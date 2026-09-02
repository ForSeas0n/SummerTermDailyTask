#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QPoint>

#include "calculator.h"
#include "financecalculator.h"

class DraggableLabel;
class FinanceDialog;

// 由 uic 根据 ui/mainwindow.ui 自动生成的界面类
namespace Ui
{
class MainWindow;
}

/**
 * @brief 主窗口（View 层）
 *
 * 【架构说明 —— UI 显示与计算逻辑分离】
 *   本类只负责三件事：
 *     1) 把用户在界面上的操作（点击按钮、键盘输入、鼠标拖拽）翻译成"要做什么"；
 *     2) 调用 Model 层（Calculator / FinanceCalculator）完成真正的计算；
 *     3) 把 Model 返回的结果显示到界面上，或在出错时给出提示。
 *
 *   所有数学运算都不在本类中进行，全部委托给 Model，
 *   因此计算逻辑可以脱离界面单独测试，界面也可以随时替换。
 *
 * 【事件处理的两种手法在本项目中的分工】
 *   - 重写虚函数  ：DraggableLabel 的 mousePress/Move/Release，实现拖拽输入。
 *                   原因：拖拽是该控件固有的行为，写在类内最内聚。
 *   - 事件过滤器  ：本类的 eventFilter() 安装在表达式输入框上，处理
 *                   回车计算、退格、Esc 清空以及非法字符拦截。
 *                   原因：QLineEdit 是 Qt 内置控件，无法修改其源码，
 *                   在外部安装过滤器比继承出一个新类更轻量。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    /**
     * @brief 事件过滤器
     *        安装在 ui->exprEdit 上，用于在 Qt 内置控件之外追加自定义键盘行为。
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    // ---------- 输入类 ----------
    /** 数字 / 运算符 / 小数点 / 括号按钮的统一槽，通过 sender() 的属性区分具体字符 */
    void onInsertButtonClicked();

    void onEqualClicked();     // =
    void onClearClicked();     // C
    void onBackspaceClicked(); // ←

    /** 表达式内容变化时实时预览结果（由 textChanged 信号驱动） */
    void onExpressionChanged(const QString &text);

    // ---------- 拖拽类 ----------
    /** 可拖拽标签松手时的落点判断与字符插入 */
    void onDraggedReleased(const QString &payload, const QPoint &globalPos);

    // ---------- 财务类 ----------
    void onFinanceSettingsClicked(); // 打开"财务参数设置"对话框
    void onSimpleInterestClicked();  // 单利计算
    void onCompoundInterestClicked();// 复利计算

    /** 接收对话框通过信号传来的参数（窗口间通信的接收端） */
    void onFinanceParamsChanged(double annualRatePercent, int years, int mode);

private:
    // ---------- 初始化辅助函数 ----------
    void setupUiExtras();    // 图标、事件过滤器、拖拽标签绑定等代码侧补充
    void connectSignals();   // 集中书写所有信号与槽的连接
    void setupDragLabels();  // 给 .ui 中的拖拽标签设置 payload 并连接信号

    // ---------- 业务逻辑辅助函数 ----------
    void appendToExpression(const QString &text); // 追加字符（含"结果态"处理）
    void updateResultPreview();                   // 实时预览当前表达式的值
    void updateFinanceInfoLabel();                // 刷新财务参数显示
    bool ensurePrincipal(double &principal, QString &error); // 取本金（当前表达式的值）
    void applyFinance(InterestMode mode);         // 执行单利/复利计算的公共流程
    void showError(const QString &message, bool showMessageBox = true);

    // ---------- 成员变量 ----------
    Ui::MainWindow *ui = nullptr;

    Calculator m_calculator;                  // Model：表达式求值器
    QPointer<FinanceDialog> m_financeDialog;  // 财务参数对话框（懒创建）

    double       m_rate  = 3.0;                    // 年利率（百分数）
    int          m_years = 5;                      // 年限
    InterestMode m_mode  = InterestMode::Compound; // 计息方式

    double m_lastResult  = 0.0;  // 上一次 "=" 或财务计算的结果
    bool   m_resultShown = false; // 输入框当前显示的是否为"上一次的结果"
};

#endif // MAINWINDOW_H
