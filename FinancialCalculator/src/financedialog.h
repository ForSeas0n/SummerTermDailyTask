#ifndef FINANCEDIALOG_H
#define FINANCEDIALOG_H

#include <QDialog>

#include "financecalculator.h" // InterestMode

// 由 uic 根据 ui/financedialog.ui 自动生成的界面类，前置声明避免头文件互相包含
namespace Ui
{
class FinanceDialog;
}

/**
 * @brief 财务参数设置对话框（View 层）
 *
 * 【职责】
 *   采集三个参数：年利率、年限、计息方式（单利 / 复利）。
 *
 * 【与主窗口的通信方式】
 *   采用"信号 -> 槽"完成跨窗口数据传递，而不是让主窗口直接读取对话框的控件，
 *   也不是让对话框去操作主窗口。这样做的好处是对话框完全不知道主窗口的存在，
 *   耦合度最低，符合 Qt 推荐的组件化设计。
 *
 *   流程：
 *     用户点确定 -> onAccepted() 校验 -> emit parametersAccepted(...) -> accept()
 *     主窗口用 connect 捕获 parametersAccepted，把参数保存到自己的成员变量中。
 */
class FinanceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FinanceDialog(QWidget *parent = nullptr);
    ~FinanceDialog() override;

    /**
     * @brief 每次打开对话框前，把主窗口当前的参数回填到控件上，保证界面显示与实际一致
     */
    void setParameters(double annualRatePercent, int years, InterestMode mode);

signals:
    /**
     * @brief 参数校验通过后发出，把结果传给主窗口
     * @param annualRatePercent 年利率（百分数，如 3.5 表示 3.5%）
     * @param years             年限
     * @param mode              计息方式，取值来自 InterestMode（Simple=0, Compound=1）
     */
    void parametersAccepted(double annualRatePercent, int years, int mode);

private slots:
    /**
     * @brief "确定"按钮槽函数
     *        先校验参数 -> 再发信号 -> 最后关闭对话框
     *        （在构造函数中把 buttonBox 的 accepted() 连到这里，
     *          而不是直接连到 accept()，就是为了留出校验与传参的时机）
     */
    void onAccepted();

private:
    Ui::FinanceDialog *ui;
};

#endif // FINANCEDIALOG_H
