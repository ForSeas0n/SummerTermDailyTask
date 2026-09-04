#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>

class QTcpServer;
class QTcpSocket;
class FileSender;
class FileReceiver;

namespace Ui {
class MainWindow;
}

/**
 * @brief 主窗口：负责模式切换、连接/监听调度、发送/接收分发、日志与进度展示
 *
 * 设计说明：网络收发逻辑已下沉到 FileSender / FileReceiver 两个类，
 * 本类只做「界面事件 → 调用传输类」的调度与状态展示，View 与 Model 分离。
 *
 * 发送队列：正在发送时又拖入/选择了新文件，新文件会进入队列，
 * 当前文件发完后自动开始下一个，保证数据流不被打断（避免协议错乱）。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // —— 模式切换 ——
    void onModeChanged();

    // —— 动作按钮（开始监听 / 连接 / 断开）——
    void onActionClicked();
    void onOpenFileClicked();
    void onSettingsClicked();

    // —— 服务端网络事件 ——
    void onNewConnection();

    // —— 客户端 socket 事件 ——
    void onConnected();
    void onDisconnected();
    void onSocketError();

    // —— 拖拽 / 发送 / 接收 ——
    void onFilesDropped(const QStringList &paths);
    void onSendProgress(qint64 sent, qint64 total);
    void onSendFinished(const QString &fileName);
    void onFileReceiveStarted(const QString &fileName, qint64 total);
    void onReceiveProgress(qint64 received, qint64 total);
    void onFileSaved(const QString &path, qint64 fileSize);
    void onTransferError(const QString &message);

    // —— 设置对话框参数回传 ——
    void onSettingsConfirmed(const QString &ip, quint16 port, const QString &saveDir);

private:
    void appendLog(const QString &level, const QString &msg); ///< 写日志（带颜色）
    void updateUiState();                                     ///< 根据连接状态刷新按钮文字
    void updateSavePathLabel();                               ///< 刷新「接收保存位置」标签
    bool ensureSocketForSend();                               ///< 发送前检查连接可用
    void startServer();                                       ///< 启动监听
    void startClient();                                       ///< 发起连接
    void disconnectAll();                                     ///< 断开/停止
    /// 给指定 socket 连接状态信号（构造时和接管新连接后都要调用）
    void setupSocketConnections(QTcpSocket *socket);
    void sendNextIfIdle();                                    ///< 空闲时从队列取下一个文件发送

    Ui::MainWindow *ui;

    QTcpServer   *m_server;    ///< 服务端监听器
    QTcpSocket   *m_socket;    ///< 当前连接（服务端接到的连接 / 客户端连接共用）
    FileSender   *m_sender;    ///< 发送器
    FileReceiver *m_receiver;  ///< 接收器

    QString m_ip;              ///< 当前配置：服务端 IP
    quint16 m_port;            ///< 当前配置：端口
    QString m_saveDir;         ///< 当前配置：保存目录
    QStringList m_pendingFiles;///< 发送队列（正在发送时新文件在此排队）
};

#endif // MAINWINDOW_H
