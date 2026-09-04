#include "transferprotocol.h"

#include <QDataStream>
#include <QIODevice>
#include <QtEndian>

TransferProtocol::TransferProtocol()
    : m_state(ParseState::WaitHeader)
    , m_magic(0)
    , m_type(0)
    , m_nameLen(0)
    , m_fileSize(0)
    , m_received(0)
{
}

/**
 * 打包「协议头 + 文件名」。所有整数字段统一使用「大端（网络字节序）」写入，
 * 确保收发两端无论各自 CPU 是小端（x86）还是大端都能正确解析——
 * 这是网络编程的通用约定。
 */
QByteArray TransferProtocol::buildHeader(MessageType type,
                                         const QString &fileName,
                                         qint64 fileSize)
{
    const QByteArray nameBytes = fileName.toUtf8(); // 文件名统一按 UTF-8 编码

    QByteArray header;
    header.reserve(HEADER_SIZE + nameBytes.size());

    // 用 QDataStream 配合 BigEndian 手动写头，避免 QDataStream 版本号差异
    QDataStream out(&header, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);

    out << quint32(MAGIC);             // 魔数
    out << quint32(type);              // 消息类型
    out << quint32(nameBytes.size());  // 文件名长度
    out << qint64(fileSize);           // 文件数据长度（8 字节，必须为真实大小）

    header.append(nameBytes);
    return header;
}

QByteArray TransferProtocol::buildFrame(MessageType type,
                                        const QString &fileName,
                                        const QByteArray &fileData)
{
    // 完整帧 = 头部（含真实文件大小）+ 文件正文
    QByteArray frame = buildHeader(type, fileName, fileData.size());
    frame.append(fileData);
    return frame;
}

bool TransferProtocol::isReceivingFile() const
{
    return m_state != ParseState::WaitHeader;
}

void TransferProtocol::reset()
{
    // 清空缓冲与全部帧状态，回到"等待新协议头"的干净状态
    m_buffer.clear();
    m_state     = ParseState::WaitHeader;
    m_magic     = 0;
    m_type      = 0;
    m_nameLen   = 0;
    m_fileSize  = 0;
    m_fileName.clear();
    m_received  = 0;
}

/**
 * 解析新到达的数据（流式事件模型）。
 * 核心思路：把 chunk 追加进内部缓冲，然后根据当前状态机推进，
 * 尽可能多地切出事件。文件数据【不攒在内存里】，缓冲里有多少正文
 * 就吐多少 FileData 事件，由接收方直接写盘。
 */
QVector<TransferProtocol::Event> TransferProtocol::feed(const QByteArray &chunk)
{
    m_buffer.append(chunk); // 累积到内部缓冲，天然支持"分次到达"的拼接

    QVector<Event> result;

    // 用一个"数据游标"表示当前帧已经消费到的位置，避免频繁 memmove
    int pos = 0;

    while (true) {
        switch (m_state) {
        case ParseState::WaitHeader: {
            // 需要 20 字节才能读出一个完整协议头
            if (m_buffer.size() - pos < HEADER_SIZE) {
                goto done;
            }

            // 逐字段读出（大端）
            const uchar *p = reinterpret_cast<const uchar *>(m_buffer.constData() + pos);
            m_magic    = qFromBigEndian<quint32>(p);
            m_type     = qFromBigEndian<quint32>(p + 4);
            m_nameLen  = qFromBigEndian<quint32>(p + 8);
            m_fileSize = qFromBigEndian<qint64>(p + 12);

            pos += HEADER_SIZE;

            // 校验魔数：不匹配说明数据错位（协议被破坏），丢弃整个缓冲以防死循环
            if (m_magic != MAGIC) {
                m_buffer.clear();
                goto done;
            }

            // 长度字段异常则直接丢弃，避免恶意/损坏数据导致超大分配
            if (m_nameLen > MAX_FILE_NAME_LEN
                    || m_fileSize < 0 || m_fileSize > MAX_FILE_SIZE) {
                m_buffer.clear();
                goto done;
            }

            m_fileName.clear();
            m_received = 0;
            m_state = ParseState::WaitFileName;
            break;
        }

        case ParseState::WaitFileName: {
            // 缓冲区里文件名字节还不足，等下一次
            if (m_buffer.size() - pos < static_cast<int>(m_nameLen)) {
                goto done;
            }

            m_fileName = QString::fromUtf8(m_buffer.constData() + pos,
                                           static_cast<int>(m_nameLen));
            pos += static_cast<int>(m_nameLen);

            // 吐出 FileStarted 事件：接收方据此打开临时文件开始落盘
            {
                Event e;
                e.type     = EventType::FileStarted;
                e.fileName = m_fileName;
                e.fileSize = m_fileSize;
                result.append(e);
            }

            // 0 字节空文件：直接收满
            if (m_fileSize == 0) {
                Event e;
                e.type     = EventType::FileFinished;
                e.fileName = m_fileName;
                e.fileSize = m_fileSize;
                e.received = 0;
                result.append(e);
                m_state = ParseState::WaitHeader;
            } else {
                m_state = ParseState::WaitData;
            }
            break;
        }

        case ParseState::WaitData: {
            const int available = m_buffer.size() - pos;
            if (available <= 0) {
                goto done; // 数据还没到，等下一次
            }

            // 本次最多取还需要接收的字节数，多出来的部分（可能属于下一帧）留在缓冲里
            const qint64 need = m_fileSize - m_received;
            const int take = static_cast<int>(qMin<qint64>(need, available));

            // 吐出 FileData 事件：携带本块数据与累计进度（接收方直接写盘）
            {
                Event e;
                e.type     = EventType::FileData;
                e.data     = QByteArray(m_buffer.constData() + pos, take);
                e.fileSize = m_fileSize;
                e.received = m_received + take;
                result.append(e);
            }
            pos += take;
            m_received += take;

            // 文件数据收满 → FileFinished，回到 WaitHeader 继续解析下一帧
            if (m_received >= m_fileSize) {
                Event e;
                e.type     = EventType::FileFinished;
                e.fileName = m_fileName;
                e.fileSize = m_fileSize;
                e.received = m_received;
                result.append(e);
                m_state = ParseState::WaitHeader;
            }
            break;
        }
        }
    }

done:
    // 把已经消费掉的字节从缓冲中移除，只保留未消费的尾巴
    if (pos > 0) {
        m_buffer.remove(0, pos);
    }
    return result;
}
