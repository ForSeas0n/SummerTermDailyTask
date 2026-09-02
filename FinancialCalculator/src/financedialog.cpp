#include "financedialog.h"
#include "ui_financedialog.h" // uic 由 ui/financedialog.ui 生成

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>

FinanceDialog::FinanceDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FinanceDialog)
{
    // 把 .ui 文件中设计的界面"安装"到本对话框上
    ui->setupUi(this);

    // 默认选中"复利"，与主窗口的初始值保持一致
    ui->modeComboBox->setCurrentIndex(1);

    // ---------- "确定"按钮的信号与槽连接 ----------
    // 这条连接必须写在代码里，不能写在 .ui 的 <connections> 中：
    // uic 生成的 setupUi() 形参是 QDialog*，无法识别本类的自定义槽 onAccepted()。
    // 这里 this 的真实类型是 FinanceDialog，类型匹配且具备编译期检查。
    //
    // 另外，不能把 accepted() 直接连到内置的 accept()，
    // 否则对话框会立刻关闭，就没机会做参数校验和向主窗口传参了。
    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this,          &FinanceDialog::onAccepted);
}

FinanceDialog::~FinanceDialog()
{
    delete ui;
}

void FinanceDialog::setParameters(double annualRatePercent, int years, InterestMode mode)
{
    ui->rateSpinBox->setValue(annualRatePercent);
    ui->yearsSpinBox->setValue(years);
    ui->modeComboBox->setCurrentIndex(mode == InterestMode::Simple ? 0 : 1);
}

/**
 * @brief 确定按钮：校验 -> 发信号 -> 关闭
 *
 * 这里体现了"窗口间通信"的完整链路：
 *   本对话框只负责产出数据（emit 信号），
 *   主窗口负责消费数据（连接信号到自己的槽并更新界面）。
 */
void FinanceDialog::onAccepted()
{
    const double rate  = ui->rateSpinBox->value();
    const int    years = ui->yearsSpinBox->value();
    const int    mode  = ui->modeComboBox->currentIndex();

    // ---------- 参数校验（对应题目"错误处理"要求） ----------
    if (years <= 0) {
        QMessageBox::warning(this, QStringLiteral("参数错误"),
                             QStringLiteral("年限必须大于 0，请重新设置。"));
        ui->yearsSpinBox->setFocus();
        return; // 不关闭对话框，让用户继续修改
    }

    if (rate < 0.0) {
        QMessageBox::warning(this, QStringLiteral("参数错误"),
                             QStringLiteral("年利率不能为负数，请重新设置。"));
        ui->rateSpinBox->setFocus();
        return;
    }

    // ---------- 通过信号把参数发送给主窗口 ----------
    emit parametersAccepted(rate, years, mode);

    // ---------- 校验通过，关闭对话框并返回 Accepted ----------
    accept();
}
