#include "filesender.h"

#include "transferprotocol.h"

#include <QFileInfo>
#include <QTcpSocket>

FileSender::FileSender(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_totalSize(0)
    , m_sent(0)
    , m_headerSent(false)
{
}

/**
 * 开始发送。整体分两部分：
 *   1. 协议头 + 文件名（一次性写入，很小）；
 *   2. 文件正文（分块写，靠 bytesWritten 驱动）。
 */
void FileSender::start(QTcpSocket *socket, const QString &path)
{
    // 若上一次发送尚未结束，先复位，避免状态串扰
    if (isSending()) {
        reset();
    }

    if (socket == nullptr || !socket->isValid()) {
        emit error(QStringLiteral("连接不可用，无法发送文件"));
        return;
    }

    // 注意：QFile 禁止拷贝/移动赋值，因此直接用成员 m_file 打开文件
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        emit error(QStringLiteral("无法打开文件：%1").arg(path));
        return;
    }

    const qint64 fileSize = m_file.size();
    const QString fileName = QFileInfo(path).fileName();

    // 打包协议头 + 文件名。
    // 必须用 buildHeader 并传入【真实文件大小】——接收端靠这个字段判断
    // 正文有多长；若传 0，接收端会把正文误当成下一帧的头而丢弃数据。
    // 文件正文不放在头部里，由 writeMore() 分块流式写入。
    m_header = TransferProtocol::buildHeader(TransferProtocol::TypeFile,
                                             fileName, fileSize);

    m_socket     = socket;
    m_totalSize  = fileSize;
    m_sent       = 0;
    m_headerSent = false;

    // 连接一次性的 bytesWritten 信号，用于分块发送（发送结束后自动断开）
    connect(m_socket, &QTcpSocket::bytesWritten,
            this, &FileSender::onBytesWritten, Qt::UniqueConnection);

    writeMore();
}

/**
 * 把剩余数据尽量写入 socket。返回 true 表示已经全部写完。
 *
 * 这里不依赖 Qt 的 write() 返回值判断"写完没有"，而是：
 *   - 先写协议头（若还没写完）；
 *   - 再分块读文件并 write；
 *   - 每写一次都累加 m_sent，直到 m_sent 达到 m_totalSize。
 * 当 socket 内核缓冲满了 write() 会返回 -1 或写入 0，此时停止，
 * 由后续 bytesWritten 信号再次触发 writeMore() 继续。
 */
bool FileSender::writeMore()
{
    if (m_socket == nullptr) {
        return true;
    }

    // ---- 第一步：写协议头 + 文件名 ----
    if (!m_headerSent) {
        const qint64 w = m_socket->write(m_header);
        if (w < 0) {
            emit error(QStringLiteral("发送协议头失败"));
            reset();
            return true;
        }
        // 头很小，一般一次写尽；若没写尽，留在 m_header 里下次继续
        if (w < m_header.size()) {
            m_header.remove(0, static_cast<int>(w));
            return false; // 等 bytesWritten
        }
        m_header.clear();
        m_headerSent = true;
    }

    // ---- 第二步：分块写文件正文 ----
    while (m_sent < m_totalSize) {
        // 读一块
        QByteArray chunk = m_file.read(CHUNK_SIZE);
        if (chunk.isEmpty()) {
            break; // 读不到数据（理论上读满即结束）
        }

        const qint64 w = m_socket->write(chunk);
        if (w <= 0) {
            // 写不进去（缓冲满），回退文件指针，等 bytesWritten 再重读这块
            m_file.seek(m_file.pos() - chunk.size());
            return false;
        }

        m_sent += w;
        emit progress(m_sent, m_totalSize);

        // 本次没写完这一块，把剩余部分回退，等下次继续
        if (w < chunk.size()) {
            m_file.seek(m_file.pos() - (chunk.size() - w));
            return false;
        }
    }

    // ---- 全部写完 ----
    if (m_sent >= m_totalSize) {
        const QString name = QFileInfo(m_file.fileName()).fileName();
        // 必须先 reset 再 emit finished：完成信号会同步触发发送队列取下一个文件，
        // 若先发信号，槽函数里 isSending() 仍为 true，队列会误以为还在发送而跳过，
        // 导致多文件拖入时只有第一个文件被发送。
        reset();
        emit finished(name);
        return true;
    }

    return false;
}

void FileSender::onBytesWritten(qint64 /*bytes*/)
{
    // 内核缓冲被消费了一部分，尝试继续发送剩余数据
    writeMore();
}

void FileSender::cancel()
{
    // 异常中止（如断线）：只复位状态，不发 finished 信号
    reset();
}

void FileSender::reset()
{
    if (m_socket != nullptr) {
        disconnect(m_socket, &QTcpSocket::bytesWritten,
                   this, &FileSender::onBytesWritten);
    }
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_socket      = nullptr;
    m_header.clear();
    m_totalSize   = 0;
    m_sent        = 0;
    m_headerSent  = false;
}
