#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "dropzone.h"
#include "filereceiver.h"
#include "filesender.h"
#include "settingsdialog.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHostAddress>
#include <QMessageBox>
#include <QTcpServer>
#include <QTcpSocket>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_server(nullptr)
    , m_socket(nullptr)
    , m_sender(nullptr)
    , m_receiver(nullptr)
    , m_ip(QStringLiteral("127.0.0.1"))
    , m_port(8888)
    , m_saveDir(QStringLiteral("."))
{
    ui->setupUi(this);

    // 创建网络与传输对象
    m_server   = new QTcpServer(this);
    m_socket   = new QTcpSocket(this);
    m_sender   = new FileSender(this);
    m_receiver = new FileReceiver(this);
    m_receiver->attach(m_socket, m_saveDir);

    // ================= 信号与槽连接 =================

    // 模式切换（两个单选按钮都连到同一个槽）
    connect(ui->serverRadio, &QRadioButton::toggled, this, &MainWindow::onModeChanged);
    connect(ui->clientRadio, &QRadioButton::toggled, this, &MainWindow::onModeChanged);

    // 动作 / 打开文件 / 设置按钮
    connect(ui->actionButton, &QPushButton::clicked, this, &MainWindow::onActionClicked);
    connect(ui->openFileButton, &QPushButton::clicked, this, &MainWindow::onOpenFileClicked);
    connect(ui->settingsButton, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);

    // 服务端：有新连接接入
    connect(m_server, &QTcpServer::newConnection, this, &MainWindow::onNewConnection);

    // 客户端 socket 状态变化（抽成函数，服务端接管新连接时也要重新连一次）
    setupSocketConnections(m_socket);

    // 拖拽区 → 发送文件（支持一次拖入多个文件）
    connect(ui->dropZone, &DropZone::filesDropped, this, &MainWindow::onFilesDropped);

    // 发送器信号 → 界面反馈
    connect(m_sender, &FileSender::progress, this, &MainWindow::onSendProgress);
    connect(m_sender, &FileSender::finished, this, &MainWindow::onSendFinished);
    connect(m_sender, &FileSender::error, this, &MainWindow::onTransferError);

    // 接收器信号 → 界面反馈（实时进度）
    connect(m_receiver, &FileReceiver::fileStarted, this, &MainWindow::onFileReceiveStarted);
    connect(m_receiver, &FileReceiver::progress, this, &MainWindow::onReceiveProgress);
    connect(m_receiver, &FileReceiver::fileSaved, this, &MainWindow::onFileSaved);
    connect(m_receiver, &FileReceiver::error, this, &MainWindow::onTransferError);

    // 初始化界面状态
    // 注意：本程序一个实例只能扮演一个角色，测试需要开【两个窗口】：
    // 一个作为服务端监听，另一个作为客户端连接（另开终端再运行一次本程序）。
    appendLog(QStringLiteral("信息"),
              QStringLiteral("程序已启动：一个窗口只能扮演一个角色，测试请【打开两个窗口】，一个选服务端、一个选客户端"));
    updateSavePathLabel();
    updateUiState();
}

MainWindow::~MainWindow()
{
    // 关闭连接，释放由 Qt 父子关系自动管理
    if (m_server->isListening()) {
        m_server->close();
    }
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    delete ui;
}

// ================= 模式切换 =================

void MainWindow::onModeChanged()
{
    // 切换模式前若已有连接，先断开，避免角色混乱
    if (m_server->isListening() || m_socket->state() != QAbstractSocket::UnconnectedState) {
        appendLog(QStringLiteral("信息"),
                  QStringLiteral("已切换模式并停止当前的监听/连接。注意：单个窗口只能扮演一个角色，测试请开两个窗口分别作为服务端和客户端"));
        disconnectAll();
    }
    updateUiState();
}

// ================= 动作按钮 =================

void MainWindow::onActionClicked()
{
    if (ui->serverRadio->isChecked()) {
        // 服务端模式：按钮在「开始监听 / 停止监听并断开」间切换
        if (m_server->isListening()) {
            disconnectAll();
        } else {
            startServer();
        }
    } else {
        // 客户端模式：按钮在「连接 / 断开」间切换
        if (m_socket->state() == QAbstractSocket::UnconnectedState) {
            startClient();
        } else {
            disconnectAll();
        }
    }
    updateUiState();
}

void MainWindow::onOpenFileClicked()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择要发送的文件（可多选）"), QString(),
        QStringLiteral("文本/图片文件 (*.txt *.png *.jpg *.jpeg *.bmp);;所有文件 (*.*)"));
    if (!paths.isEmpty()) {
        onFilesDropped(paths);
    }
}

void MainWindow::onSettingsClicked()
{
    SettingsDialog dlg(this);
    dlg.setValues(m_ip, m_port, m_saveDir);

    // 窗口通信：对话框确认后把参数传回主窗口
    connect(&dlg, &SettingsDialog::settingsConfirmed,
            this, &MainWindow::onSettingsConfirmed);

    dlg.exec();
}

// ================= 服务端网络事件 =================

void MainWindow::startServer()
{
    if (!m_server->listen(QHostAddress::Any, m_port)) {
        QMessageBox::critical(this, QStringLiteral("错误"),
                              QStringLiteral("监听失败：%1").arg(m_server->errorString()));
        return;
    }
    appendLog(QStringLiteral("信息"),
              QStringLiteral("服务端已开始监听端口 %1").arg(m_port));
}

void MainWindow::onNewConnection()
{
    QTcpSocket *incoming = m_server->nextPendingConnection();
    if (incoming == nullptr) {
        return;
    }

    // 若已有连接，先关闭旧的（本工具单连接简化处理）
    if (m_socket != nullptr && m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

    // 直接接管 incoming 这个 socket 对象。
    // 注意：千万不要用 setSocketDescriptor(incoming->socketDescriptor()) 去"复制"描述符，
    // 那样两个 QTcpSocket 会同时持有同一个底层 fd，incoming 被销毁时 fd 会被真正关闭，
    // 导致刚建立的连接立刻断开（表现为 "remote host closed the connection"）。
    if (m_socket != nullptr) {
        m_socket->disconnect();   // 断开旧 socket 上的所有信号连接
        m_socket->deleteLater();  // 丢弃旧对象（若有旧连接）
    }
    m_socket = incoming;
    m_socket->setParent(this);
    setupSocketConnections(m_socket);

    m_receiver->attach(m_socket, m_saveDir);

    appendLog(QStringLiteral("成功"),
              QStringLiteral("客户端已接入：%1")
                  .arg(m_socket->peerAddress().toString()));
    updateUiState();
}

// ================= 客户端 socket 事件 =================

void MainWindow::startClient()
{
    m_socket->connectToHost(m_ip, m_port);
    appendLog(QStringLiteral("信息"),
              QStringLiteral("正在连接 %1:%2 ...").arg(m_ip).arg(m_port));
}

void MainWindow::onConnected()
{
    appendLog(QStringLiteral("成功"),
              QStringLiteral("已连接到服务端 %1:%2").arg(m_ip).arg(m_port));
    m_receiver->attach(m_socket, m_saveDir);
    updateUiState();
}

void MainWindow::onDisconnected()
{
    // 传输中途断开：中止收发并清理半成品
    if (m_sender->isSending()) {
        m_sender->cancel();
        m_pendingFiles.clear();
        appendLog(QStringLiteral("错误"), QStringLiteral("连接断开，发送已中止"));
    }
    if (m_receiver->isReceivingFile()) {
        m_receiver->abortCurrentFile();
        appendLog(QStringLiteral("错误"), QStringLiteral("连接断开，接收已中止，半成品文件已清理"));
    }
    appendLog(QStringLiteral("信息"), QStringLiteral("连接已断开"));
    ui->progressBar->setValue(0);
    updateUiState();
}

void MainWindow::onSocketError()
{
    QString msg = m_socket->errorString();

    // 针对最常见的「连接被拒」给出明确的排查建议，方便调试
    if (m_socket->error() == QAbstractSocket::ConnectionRefusedError) {
        msg += QStringLiteral(" —— 请确认：另一个窗口已选【服务端】并点过【开始监听】，且两端 IP、端口一致");
    } else if (m_socket->error() == QAbstractSocket::HostNotFoundError) {
        msg += QStringLiteral(" —— 请确认 IP 地址填写正确");
    }

    appendLog(QStringLiteral("错误"), QStringLiteral("网络错误：%1").arg(msg));
    updateUiState();
}

// ================= 拖拽 / 发送 / 接收 =================

void MainWindow::onFilesDropped(const QStringList &paths)
{
    if (!ensureSocketForSend()) {
        return;
    }

    // 正在发送时，新文件进入队列；否则立刻开始发送第一个
    for (const QString &path : paths) {
        m_pendingFiles.append(path);
    }

    if (m_sender->isSending()) {
        appendLog(QStringLiteral("信息"),
                  QStringLiteral("当前正在发送，%1 个文件已加入发送队列").arg(paths.size()));
    } else {
        sendNextIfIdle();
    }
}

void MainWindow::sendNextIfIdle()
{
    if (m_sender->isSending() || m_pendingFiles.isEmpty()) {
        return;
    }

    const QString path = m_pendingFiles.takeFirst();
    const QFileInfo fi(path);

    if (!fi.isReadable()) {
        appendLog(QStringLiteral("错误"),
                  QStringLiteral("无法读取文件：%1（文件不存在或无权限）").arg(path));
        // 继续尝试队列中的下一个
        sendNextIfIdle();
        return;
    }

    // 日志带上文件大小（作业要求：文件名 + 文件大小）
    appendLog(QStringLiteral("信息"),
              QStringLiteral("开始发送文件：%1（%2 字节）")
                  .arg(fi.fileName()).arg(fi.size()));
    m_sender->start(m_socket, path);
}

void MainWindow::onSendProgress(qint64 sent, qint64 total)
{
    if (total <= 0) {
        return;
    }
    const int percent = static_cast<int>(sent * 100 / total);
    // 仅当进度变化时更新，避免高频 setValue 造成卡顿
    if (ui->progressBar->value() != percent) {
        ui->progressBar->setValue(percent);
    }
}

void MainWindow::onSendFinished(const QString &fileName)
{
    ui->progressBar->setValue(100);
    appendLog(QStringLiteral("成功"),
              QStringLiteral("文件已发送完成：%1").arg(fileName));

    // 队列里还有文件则继续发下一个
    sendNextIfIdle();
}

void MainWindow::onFileReceiveStarted(const QString &fileName, qint64 total)
{
    appendLog(QStringLiteral("信息"),
              QStringLiteral("开始接收文件：%1（%2 字节）").arg(fileName).arg(total));
}

void MainWindow::onReceiveProgress(qint64 received, qint64 total)
{
    if (total <= 0) {
        return;
    }
    const int percent = static_cast<int>(received * 100 / total);
    // 仅当进度变化时更新，避免高频 setValue 造成卡顿
    if (ui->progressBar->value() != percent) {
        ui->progressBar->setValue(percent);
    }
}

void MainWindow::onFileSaved(const QString &path, qint64 fileSize)
{
    ui->progressBar->setValue(100);
    appendLog(QStringLiteral("成功"),
              QStringLiteral("已接收并保存文件：%1（%2 字节）").arg(path).arg(fileSize));
}

void MainWindow::onTransferError(const QString &message)
{
    appendLog(QStringLiteral("错误"), message);
    QMessageBox::warning(this, QStringLiteral("传输错误"), message);
}

// ================= 设置参数回传 =================

void MainWindow::onSettingsConfirmed(const QString &ip, quint16 port, const QString &saveDir)
{
    m_ip      = ip;
    m_port    = port;
    m_saveDir = saveDir;

    m_receiver->setSaveDir(saveDir);
    updateSavePathLabel();
    appendLog(QStringLiteral("信息"),
              QStringLiteral("设置已更新：%1:%2；本窗口接收的文件将保存到：%3")
                  .arg(ip).arg(port).arg(QDir(m_saveDir).absolutePath()));
}

// ================= 工具函数 =================

void MainWindow::appendLog(const QString &level, const QString &msg)
{
    // 按级别给日志着色（日志区为白底）：成功绿色、错误红色、信息黑色
    QString color;
    if (level == QStringLiteral("成功")) {
        color = QStringLiteral("#2e7d32");   // 绿色
    } else if (level == QStringLiteral("错误")) {
        color = QStringLiteral("#c62828");   // 红色
    } else {
        color = QStringLiteral("#212121");   // 信息：黑色，白底上清晰可读
    }

    const QString html = QStringLiteral("<span style='color:%1'>[%2] %3</span>")
                             .arg(color, level, msg.toHtmlEscaped());
    ui->logEdit->append(html);
    ui->statusbar->showMessage(msg, 5000);
}

/**
 * 给一个 socket 连接上状态相关的信号与槽。
 * 服务端每接入一个新连接时，m_socket 会被换成新的 socket 对象，
 * 因此必须重新连接一次这些信号。
 */
void MainWindow::setupSocketConnections(QTcpSocket *socket)
{
    connect(socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onDisconnected);
    connect(socket, &QTcpSocket::errorOccurred, this, &MainWindow::onSocketError);
}

void MainWindow::updateSavePathLabel()
{
    // 常驻显示本窗口接收文件的保存位置（相对路径转为绝对路径更直观）
    ui->savePathLabel->setText(
        QStringLiteral("接收保存位置：%1（在「连接设置」中修改，仅对本窗口接收的文件生效）")
            .arg(QDir(m_saveDir).absolutePath()));
}

void MainWindow::updateUiState()
{
    if (ui->serverRadio->isChecked()) {
        const bool listening = m_server->isListening();
        const bool connected = m_socket->state() == QAbstractSocket::ConnectedState;
        // 已有客户端接入时按钮含义是"停止监听并断开连接"，文案更准确
        if (listening && connected) {
            ui->actionButton->setText(QStringLiteral("停止监听并断开"));
        } else {
            ui->actionButton->setText(listening ? QStringLiteral("停止监听")
                                                : QStringLiteral("开始监听"));
        }
        ui->dropZone->setHintText(QStringLiteral("服务端模式：将文件拖拽到此处发送给客户端"));
    } else {
        const bool connected = m_socket->state() == QAbstractSocket::ConnectedState;
        ui->actionButton->setText(connected ? QStringLiteral("断开连接")
                                            : QStringLiteral("连接服务端"));
        ui->dropZone->setHintText(QStringLiteral("客户端模式：将文件拖拽到此处发送给服务端"));
    }
}

bool MainWindow::ensureSocketForSend()
{
    // 服务端模式：需要已有客户端接入；客户端模式：需要已连接
    if (ui->serverRadio->isChecked()) {
        if (m_socket->state() != QAbstractSocket::ConnectedState) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("尚无客户端接入，无法发送文件"));
            return false;
        }
    } else {
        if (m_socket->state() != QAbstractSocket::ConnectedState) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("尚未连接服务端，无法发送文件"));
            return false;
        }
    }
    return true;
}

void MainWindow::disconnectAll()
{
    if (m_server->isListening()) {
        m_server->close();
        appendLog(QStringLiteral("信息"), QStringLiteral("服务端已停止监听"));
    }
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    // 中止未完成的传输并清空发送队列
    if (m_sender->isSending()) {
        m_sender->cancel();
        appendLog(QStringLiteral("错误"), QStringLiteral("传输已中止"));
    }
    if (m_receiver->isReceivingFile()) {
        m_receiver->abortCurrentFile();
        appendLog(QStringLiteral("错误"), QStringLiteral("接收已中止，半成品文件已清理"));
    }
    m_pendingFiles.clear();
    ui->progressBar->setValue(0);
    updateUiState();
}
