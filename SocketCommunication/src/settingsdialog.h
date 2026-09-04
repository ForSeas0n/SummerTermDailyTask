#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

namespace Ui {
class SettingsDialog;
}

/**
 * @brief 「连接设置」对话框
 *
 * 用于设置：服务端 IP、端口、接收文件保存路径。
 * 通过自定义信号 settingsConfirmed() 把参数回传给主窗口，
 * 体现「窗口间通信（信号与槽）」这一必做考察点。
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;

    /// 预填当前值（打开对话框时由主窗口传入）
    void setValues(const QString &ip, quint16 port, const QString &saveDir);

signals:
    /// 用户点击「确定」后发出，携带最新的连接参数
    void settingsConfirmed(const QString &ip, quint16 port, const QString &saveDir);

private slots:
    /// 浏览按钮：弹出目录选择框
    void onBrowseClicked();
    /// 确定按钮：校验并发出 settingsConfirmed 信号
    void onAccept();

private:
    Ui::SettingsDialog *ui;
};

#endif // SETTINGSDIALOG_H
