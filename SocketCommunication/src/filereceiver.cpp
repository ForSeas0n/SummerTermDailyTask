#include "filereceiver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTcpSocket>

FileReceiver::FileReceiver(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_saveDir(QStringLiteral("."))
    , m_totalSize(0)
{
}

void FileReceiver::attach(QTcpSocket *socket, const QString &saveDir)
{
    m_socket  = socket;
    m_saveDir = saveDir;

    // 新连接接入：复位协议解析器。
    // 旧连接可能是在收文件半途中断的，解析器会残留"正在收某个文件"的状态，
    // 不复位的话新连接的数据会被误当成旧文件的正文，导致协议错乱。
    m_protocol.reset();

    if (m_socket != nullptr) {
        // 绑定 readyRead 信号：socket 一有数据就交给协议层解析
        connect(m_socket, &QTcpSocket::readyRead,
                this, &FileReceiver::onReadyRead, Qt::UniqueConnection);
    }
}

void FileReceiver::setSaveDir(const QString &dir)
{
    m_saveDir = dir;
}

bool FileReceiver::isReceivingFile() const
{
    return m_file.isOpen();
}

void FileReceiver::abortCurrentFile()
{
    // 关闭并删除半成品临时文件，避免残留损坏文件
    if (m_file.isOpen()) {
        m_file.close();
    }
    if (!m_tmpPath.isEmpty()) {
        QFile::remove(m_tmpPath);
        m_tmpPath.clear();
    }
    m_finalPath.clear();
    m_totalSize = 0;
    m_curName.clear();

    // 同步复位协议解析器：传输是在半途中止的，解析器还停在
    // "正在收某个文件"的状态，必须清掉，否则后续数据会协议错乱
    m_protocol.reset();
}

/**
 * socket 有数据到达：读取全部可用字节，喂给协议解析器，
 * 按解析出的流式事件「边收边写盘」。
 */
void FileReceiver::onReadyRead()
{
    if (m_socket == nullptr) {
        return;
    }

    // 一次把当前可读的数据全部读走
    const QByteArray chunk = m_socket->readAll();
    if (chunk.isEmpty()) {
        return;
    }

    // 协议层做粘包/拆包，吐出事件流
    const QVector<TransferProtocol::Event> events = m_protocol.feed(chunk);
    for (const TransferProtocol::Event &e : events) {
        handleEvent(e);
    }
}

void FileReceiver::handleEvent(const TransferProtocol::Event &e)
{
    switch (e.type) {
    case TransferProtocol::EventType::FileStarted: {
        // 一个新文件开始：确定保存路径，打开临时文件
        m_finalPath = buildSavePath(e.fileName);
        m_tmpPath   = m_finalPath + QLatin1String(TMP_SUFFIX);
        m_totalSize = e.fileSize;
        m_curName   = QFileInfo(m_finalPath).fileName();

        // 确保保存目录存在
        QDir dir(m_saveDir);
        if (!dir.exists()) {
            if (!dir.mkpath(QStringLiteral("."))) {
                emit error(QStringLiteral("无法创建保存目录：%1").arg(m_saveDir));
                return;
            }
        }

        m_file.setFileName(m_tmpPath);
        if (!m_file.open(QIODevice::WriteOnly)) {
            emit error(QStringLiteral("无法写入文件：%1").arg(m_tmpPath));
            return;
        }

        emit fileStarted(m_curName, m_totalSize);
        break;
    }

    case TransferProtocol::EventType::FileData: {
        // 文件数据块：立即写入临时文件（二进制写，图片等内容不能按文本处理）
        if (!m_file.isOpen()) {
            // 理论上不会发生（FileStarted 必然先到）；
            // 万一发生（残留状态/协议错乱），报错并复位解析器以便恢复后续传输
            emit error(QStringLiteral("协议错误：收到数据块但未开始文件（可能上次传输中断留下了残留状态），已复位"));
            m_protocol.reset();
            return;
        }
        if (m_file.write(e.data) < 0) {
            emit error(QStringLiteral("写入文件失败：%1").arg(m_tmpPath));
            abortCurrentFile();
            return;
        }
        // 实时进度：每收到一块就上报一次
        emit progress(e.received, e.fileSize);
        break;
    }

    case TransferProtocol::EventType::FileFinished: {
        // 收满：关闭临时文件并重命名为正式文件名
        if (!m_file.isOpen()) {
            return;
        }
        m_file.close();

        if (QFile::exists(m_finalPath)) {
            // 收满时同名文件恰好被外部创建（罕见）：再找一个不冲突的名字
            m_finalPath = buildSavePath(m_curName);
        }
        if (!QFile::rename(m_tmpPath, m_finalPath)) {
            emit error(QStringLiteral("保存文件失败（重命名出错）：%1").arg(m_tmpPath));
            QFile::remove(m_tmpPath);
            m_tmpPath.clear();
            return;
        }

        const QString savedPath = m_finalPath;
        const qint64 savedSize = m_totalSize;
        m_tmpPath.clear();
        m_finalPath.clear();
        m_totalSize = 0;
        m_curName.clear();

        emit fileSaved(savedPath, savedSize);
        break;
    }
    }
}

/**
 * 生成不冲突的完整保存路径。
 * 文件名只取纯文件名（协议里已经不含路径），并做重名保护：
 * 若目标已存在同名文件，自动追加 " (1)" 之类的序号，避免覆盖已有文件。
 */
QString FileReceiver::buildSavePath(const QString &fileName) const
{
    QDir dir(m_saveDir);

    QString baseName = QFileInfo(fileName).fileName();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("received.bin");
    }

    QString fullPath = dir.filePath(baseName);

    // 重名保护：若已存在同名文件，追加序号
    const QString suffix = QFileInfo(baseName).suffix();
    const QString stem   = QFileInfo(baseName).completeBaseName();
    int idx = 1;
    while (QFile::exists(fullPath)) {
        const QString numbered = QStringLiteral("%1 (%2)").arg(stem).arg(idx++);
        fullPath = dir.filePath(suffix.isEmpty() ? numbered
                                                 : numbered + QLatin1Char('.') + suffix);
    }
    return fullPath;
}
