#ifndef FILERECEIVER_H
#define FILERECEIVER_H

#include <QFile>
#include <QObject>
#include <QString>

#include "transferprotocol.h"

class QTcpSocket;

/**
 * @brief 文件接收器（流式接收 + 边收边落盘）
 *
 * 监听 QTcpSocket 的 readyRead 信号，把每次到达的数据交给 TransferProtocol
 * 做粘包/拆包解析；协议层吐出流式事件（FileStarted/FileData/FileFinished），
 * 本类据此「边收边写盘」：
 *   - FileStarted  ：确定目标文件名（带重名保护），以 .sftmp 临时文件打开；
 *   - FileData     ：把本块数据立即写入临时文件，同时发出实时进度信号；
 *   - FileFinished ：关闭临时文件并重命名为正式名。
 *
 * 关键设计：任何时刻内存里只有当前网络块的数据，与文件大小无关，
 * 大文件也不会占用大量内存，且接收进度可实时上报。
 * 传输中途断开/出错时，调用 abortCurrentFile() 删除半成品临时文件。
 */
class FileReceiver : public QObject
{
    Q_OBJECT
public:
    /// 临时文件的后缀（收满后重命名为正式文件名）
    static constexpr const char *TMP_SUFFIX = ".sftmp";

    explicit FileReceiver(QObject *parent = nullptr);

    /**
     * @brief 让接收器监听某个 socket 的数据
     * @param socket  数据来源 socket
     * @param saveDir 接收文件的保存目录（不存在会自动创建）
     */
    void attach(QTcpSocket *socket, const QString &saveDir);

    /// 设置保存目录（连接设置里改路径后调用，立即生效）
    void setSaveDir(const QString &dir);

    /// 是否正在接收一个文件
    bool isReceivingFile() const;

    /// 中止当前文件的接收：关闭并删除半成品临时文件
    void abortCurrentFile();

signals:
    /// 一个文件开始接收（fileName、总大小）
    void fileStarted(const QString &fileName, qint64 totalSize);
    /// 接收进度：已接收字节 / 总字节（实时，每收到一块就发一次）
    void progress(qint64 received, qint64 total);
    /// 一个文件已完整接收并保存
    void fileSaved(const QString &path, qint64 fileSize);
    /// 接收出错
    void error(const QString &message);

private slots:
    /// socket 有数据到达
    void onReadyRead();

private:
    void handleEvent(const TransferProtocol::Event &e);  ///< 处理单个协议事件
    QString buildSavePath(const QString &fileName) const;///< 生成不冲突的完整保存路径

private:
    QTcpSocket      *m_socket;    ///< 数据来源 socket
    QString          m_saveDir;   ///< 保存目录
    TransferProtocol m_protocol;  ///< 协议解析器（流式事件）

    QFile            m_file;      ///< 当前正在写入的临时文件
    QString          m_finalPath; ///< 当前文件收满后的正式路径
    QString          m_tmpPath;   ///< 当前文件的临时路径
    qint64           m_totalSize; ///< 当前文件的总大小
    QString          m_curName;   ///< 当前文件名
};

#endif // FILERECEIVER_H
