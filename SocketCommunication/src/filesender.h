#ifndef FILESENDER_H
#define FILESENDER_H

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QString>

class QTcpSocket;

/**
 * @brief 文件发送器（发送流水线）
 *
 * 负责把一个本地文件读入并按自定义协议打包后，通过 QTcpSocket 分块发送。
 *
 * 关键设计：采用「write + bytesWritten 信号驱动」的流水线方式——
 * 不把整个大文件一次性塞进 socket，而是每次写一块，等内核缓冲被消费
 * （收到 bytesWritten 信号）后再写下一块，避免内存峰值与发送阻塞。
 */
class FileSender : public QObject
{
    Q_OBJECT
public:
    /// 单次写入 socket 的字节数（太大可能占内存，太小则频繁信号，8KB~64KB 较合适）
    static constexpr qint64 CHUNK_SIZE = 64 * 1024;

    explicit FileSender(QObject *parent = nullptr);

    /**
     * @brief 开始发送一个文件
     * @param socket 已建立连接的目标 socket（发送期间对象需保持存活）
     * @param path   本地待发送文件的完整路径
     *
     * 发送流程：先写协议头 + 文件名，再逐块写文件正文，
     * 全部写完后发出 finished() 信号。
     */
    void start(QTcpSocket *socket, const QString &path);

    /// 是否正在发送中
    bool isSending() const { return m_socket != nullptr; }

    /// 取消当前发送（断线等异常时调用）：复位状态，不发 finished 信号
    void cancel();

signals:
    /// 发送进度：已发送字节 / 总字节
    void progress(qint64 sent, qint64 total);
    /// 整个文件发送完成（数据已全部交给 socket）
    void finished(const QString &fileName);
    /// 发送出错
    void error(const QString &message);

private slots:
    /// QTcpSocket 的 bytesWritten 信号：内核缓冲被消费后可继续写下一块
    void onBytesWritten(qint64 bytes);

private:
    /// 尝试向 socket 写入尽可能多的剩余数据，返回是否已全部写完
    bool writeMore();
    /// 复位内部状态
    void reset();

private:
    QTcpSocket *m_socket;   ///< 目标 socket（发送期间有效）
    QByteArray m_header;    ///< 待发送的协议头 + 文件名部分
    QFile      m_file;      ///< 待发送的文件
    qint64     m_totalSize; ///< 文件正文总大小（用于计算进度）
    qint64     m_sent;      ///< 已发送的文件正文字节数
    bool       m_headerSent;///< 协议头部分是否已写完
};

#endif // FILESENDER_H
