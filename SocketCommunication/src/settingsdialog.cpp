#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);

    // 浏览按钮 → 选择保存目录
    connect(ui->browseButton, &QPushButton::clicked,
            this, &SettingsDialog::onBrowseClicked);

    // 对话框的 OK 按钮 → 校验并发出参数信号
    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this, &SettingsDialog::onAccept);
    // Cancel 按钮直接关闭
    connect(ui->buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    // 按内容自动计算合适的窗口大小：
    // 防止 .ui 里写死的初始尺寸偏小，导致说明文字或按钮被截断显示不全
    adjustSize();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::setValues(const QString &ip, quint16 port, const QString &saveDir)
{
    ui->ipEdit->setText(ip);
    ui->portSpin->setValue(port);
    ui->dirEdit->setText(saveDir);
}

void SettingsDialog::onBrowseClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择保存目录"), ui->dirEdit->text());
    if (!dir.isEmpty()) {
        ui->dirEdit->setText(dir);
    }
}

void SettingsDialog::onAccept()
{
    const QString ip = ui->ipEdit->text().trimmed();
    if (ip.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("IP 地址不能为空"));
        return;
    }

    const quint16 port = static_cast<quint16>(ui->portSpin->value());
    const QString dir = ui->dirEdit->text().trimmed();

    // 通过信号把参数回传给主窗口（窗口通信）
    emit settingsConfirmed(ip, port, dir);
    accept(); // 关闭对话框
}
