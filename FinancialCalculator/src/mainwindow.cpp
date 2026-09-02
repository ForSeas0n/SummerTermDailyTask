#include "mainwindow.h"
#include "ui_mainwindow.h" // uic 由 ui/mainwindow.ui 生成

#include "draggablelabel.h"
#include "financedialog.h"

#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPair>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QSizePolicy>
#include <QStatusBar>

#include <QtGlobal> // qMin, qFuzzyIsNull

#include <cmath>

// ==============================================================================
//  文件内部工具函数
// ==============================================================================
namespace
{

/**
 * @brief 把 double 格式化成适合显示的字符串
 *        - 整数值不加小数点
 *        - 小数最多保留 6 位，并去掉末尾无意义的 0
 */
QString formatNumber(double value)
{
    if (std::isnan(value) || std::isinf(value)) {
        return QStringLiteral("溢出");
    }

    // 接近整数时按整数输出，避免出现 "5.000000"
    if (std::fabs(value - std::round(value)) < 1e-9) {
        return QString::number(std::round(value), 'f', 0);
    }

    // 根据数值大小动态选择小数位数：
    // 大数用 6 位足够，但接近 0 的小数若仍用 6 位，
    // 像 0.0000001 会被四舍五入成 "0.000000"，再去掉尾零就误显示为 "0"
    const int decimals = (std::fabs(value) < 0.001) ? 12 : 6;
    QString text = QString::number(value, 'f', decimals);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    return text;
}

} // namespace

// ==============================================================================
//  构造 / 析构
// ==============================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    // 1. 加载 .ui 文件中设计的界面（全部使用布局管理器，窗口缩放时自动适配）
    ui->setupUi(this);

    // 2. 界面之外的补充设置（图标、事件过滤器、信号槽等）
    setupUiExtras();
}

MainWindow::~MainWindow()
{
    delete ui;
}

/**
 * @brief 代码侧补充初始化
 *
 * 说明：能用 Qt Designer 完成的部分（控件摆放、布局、objectName）
 *       都写在 .ui 里；需要写代码才能完成的部分集中在本函数中，
 *       保持 .ui 与 .cpp 的职责清晰。
 */
void MainWindow::setupUiExtras()
{
    // ---------- 1. 从 Qt 资源系统（:/）加载图标 ----------
    // 图标全部使用 PNG 格式，QtGui 原生支持，不依赖 Qt SVG 模块
    setWindowIcon(QIcon(QStringLiteral(":/icons/calculator.png")));
    ui->btnClear->setIcon(QIcon(QStringLiteral(":/icons/clear.png")));
    ui->btnBackspace->setIcon(QIcon(QStringLiteral(":/icons/backspace.png")));
    ui->btnEqual->setIcon(QIcon(QStringLiteral(":/icons/equal.png")));
    ui->btnFinanceSettings->setIcon(QIcon(QStringLiteral(":/icons/finance.png")));

    // ---------- 2. 安装事件过滤器 ----------
    // 给表达式输入框安装过滤器，从而在 Qt 内置控件的行为之外，
    // 追加"回车计算 / 退格 / Esc 清空 / 拦截非法字符"等自定义键盘处理。
    ui->exprEdit->installEventFilter(this);

    // ---------- 3. 集中连接所有信号与槽 ----------
    connectSignals();

    // ---------- 4. 绑定 .ui 中摆放的可拖拽标签 ----------
    setupDragLabels();

    // ---------- 5. 让按键区的按钮高度随窗口自适应 ----------
    // 窗口被拉高时，按键会随之变大，而不是聚在顶部、下方留出大片空白
    const QList<QPushButton *> padButtons = ui->buttonGroup->findChildren<QPushButton *>();
    for (QPushButton *btn : padButtons) {
        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    }

    // ---------- 6. 初始显示 ----------
    updateFinanceInfoLabel();
    ui->resultLabel->setText(QStringLiteral("0"));
    statusBar()->showMessage(
        QStringLiteral("就绪：可点击按钮输入，也可用鼠标把拖拽区的标签拖入输入框"), 8000);
}

// ==============================================================================
//  信号与槽连接（集中书写，便于阅读与维护）
// ==============================================================================

void MainWindow::connectSignals()
{
    // ---------- (1) 数字按钮：0-9 ----------
    const QList<QPushButton *> digitButtons = {
        ui->btnNum0, ui->btnNum1, ui->btnNum2, ui->btnNum3, ui->btnNum4,
        ui->btnNum5, ui->btnNum6, ui->btnNum7, ui->btnNum8, ui->btnNum9
    };
    for (QPushButton *button : digitButtons) {
        // 用动态属性记录该按钮代表的字符，槽函数中通过 sender() 取回，
        // 这样 10 个按钮可以共用同一个槽函数，避免写 10 段重复代码
        button->setProperty("insertText", button->text());
        connect(button, &QPushButton::clicked, this, &MainWindow::onInsertButtonClicked);
    }

    // ---------- (2) 小数点按钮 ----------
    ui->btnDot->setProperty("insertText", QStringLiteral("."));
    connect(ui->btnDot, &QPushButton::clicked, this, &MainWindow::onInsertButtonClicked);

    // ---------- (3) 运算符与括号按钮 ----------
    // 界面上为美观显示 ×  ÷，但表达式解析器只识别 *  /，因此这里做一次映射
    const QList<QPair<QPushButton *, QString>> operatorButtons = {
        { ui->btnAdd,     QStringLiteral("+") },
        { ui->btnSub,     QStringLiteral("-") },
        { ui->btnMul,     QStringLiteral("*") }, // 显示 ×，实际插入 *
        { ui->btnDiv,     QStringLiteral("/") }, // 显示 ÷，实际插入 /
        { ui->btnLParen,  QStringLiteral("(") },
        { ui->btnRParen,  QStringLiteral(")") }
    };
    for (const QPair<QPushButton *, QString> &pair : operatorButtons) {
        pair.first->setProperty("insertText", pair.second);
        connect(pair.first, &QPushButton::clicked, this, &MainWindow::onInsertButtonClicked);
    }

    // ---------- (4) 功能按钮 ----------
    connect(ui->btnEqual,     &QPushButton::clicked, this, &MainWindow::onEqualClicked);
    connect(ui->btnClear,     &QPushButton::clicked, this, &MainWindow::onClearClicked);
    connect(ui->btnBackspace, &QPushButton::clicked, this, &MainWindow::onBackspaceClicked);

    // ---------- (5) 财务计算按钮 ----------
    connect(ui->btnFinanceSettings,    &QPushButton::clicked, this, &MainWindow::onFinanceSettingsClicked);
    connect(ui->btnSimpleInterest,     &QPushButton::clicked, this, &MainWindow::onSimpleInterestClicked);
    connect(ui->btnCompoundInterest,   &QPushButton::clicked, this, &MainWindow::onCompoundInterestClicked);

    // ---------- (6) 表达式变化 -> 实时预览结果 ----------
    // 只要输入框内容发生变化（点击按钮、键盘输入、拖拽插入、退格、清空），
    // 都会自动触发预览，无需在每个槽里重复调用
    connect(ui->exprEdit, &QLineEdit::textChanged,
            this,         &MainWindow::onExpressionChanged);
}

/**
 * @brief 给 .ui 中摆放的所有 DraggableLabel 设置负载字符并连接拖拽信号
 *
 * findChildren 会自动递归查找主窗口下所有 DraggableLabel 实例，
 * 因此后续在 Designer 里新增标签也无需修改本函数。
 */
void MainWindow::setupDragLabels()
{
    const QList<DraggableLabel *> labels = findChildren<DraggableLabel *>();

    for (DraggableLabel *label : labels) {
        // 标签上显示的文本就是它代表的字符（÷ × 需要映射为 / *）
        QString payload = label->text();
        if (payload == QStringLiteral("÷")) {
            payload = QStringLiteral("/");
        } else if (payload == QStringLiteral("×")) {
            payload = QStringLiteral("*");
        }
        label->setPayload(payload);

        // 拖拽松手 -> 由主窗口判断落点并决定是否插入
        connect(label, &DraggableLabel::dragReleased,
                this,  &MainWindow::onDraggedReleased);
    }
}

// ==============================================================================
//  事件过滤器
// ==============================================================================

/**
 * @brief 拦截表达式输入框的键盘事件
 *
 * 【为什么这里用事件过滤器而不是继承 QLineEdit】
 *   QLineEdit 是 Qt 内置控件，若只为追加几个快捷键就派生一个新类，
 *   会引入新的类型、新的 .ui 提升配置，代价偏高；
 *   事件过滤器可以在不改动原有类的前提下"从外部"注入行为，更轻量灵活。
 */
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->exprEdit && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        // 放行 Ctrl / Alt 组合键，保证 Ctrl+C、Ctrl+V 等快捷键仍可使用
        if (keyEvent->modifiers() & Qt::ControlModifier) {
            return QMainWindow::eventFilter(watched, event);
        }

        // ---------- 自定义快捷键 ----------
        switch (keyEvent->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            onEqualClicked();     // 回车 = 计算
            return true;          // 已处理，不再向下传递
        case Qt::Key_Backspace:
            onBackspaceClicked(); // 退格
            return true;
        case Qt::Key_Escape:
            onClearClicked();     // Esc = 清空
            return true;
        default:
            break;
        }

        // ---------- 拦截非法字符 ----------
        // keyEvent->text() 对方向键、Delete 等控制键返回空串，这些应当放行
        const QString input = keyEvent->text();
        if (!input.isEmpty() && !Calculator::isAllowedChar(input.at(0))) {
            statusBar()->showMessage(
                QStringLiteral("已忽略非法输入 \"%1\"，本计算器仅支持 0-9 . + - * / ( )").arg(input),
                2500);
            return true; // 拦截：字符不会进入输入框
        }
    }

    // 其余事件交给基类处理
    return QMainWindow::eventFilter(watched, event);
}

// ==============================================================================
//  输入类槽函数
// ==============================================================================

/**
 * @brief 数字 / 运算符 / 小数点 / 括号按钮的统一槽函数
 *        通过 sender() 拿到被点击的按钮，再读取它的 insertText 属性
 */
void MainWindow::onInsertButtonClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button) {
        return;
    }
    appendToExpression(button->property("insertText").toString());
}

/**
 * @brief 把字符追加到表达式末尾
 *
 * 这里处理了计算器的经典状态机问题：
 *   刚算完一次结果后——
 *     再按运算符：用上一次的结果作为左操作数继续运算（如 2+3=5，再按 *，变成 5*）
 *     再按数字  ：认为用户要开始一个新的表达式，清空重来
 */
void MainWindow::appendToExpression(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    if (m_resultShown) {
        if (Calculator::isOperator(text.at(0))) {
            ui->exprEdit->setText(formatNumber(m_lastResult)); // 用上次结果继续
        } else {
            ui->exprEdit->clear();                             // 开启新表达式
        }
        m_resultShown = false;
    }

    ui->exprEdit->setText(ui->exprEdit->text() + text);
    ui->exprEdit->setFocus();

    // 结果预览由 textChanged 信号自动触发，此处无需手动刷新
}

void MainWindow::onExpressionChanged(const QString &text)
{
    // ---------- 防御粘贴等"非按键"途径带入的非法字符 ----------
    // eventFilter 只能拦截逐个按键的输入，但 Ctrl+V 粘贴会绕过按键检查，
    // 因此这里在内容层面再做一次过滤，只保留计算器允许的字符。
    QString clean;
    clean.reserve(text.size());
    for (const QChar c : text) {
        if (Calculator::isAllowedChar(c)) {
            clean += c;
        }
    }

    if (clean != text) {
        // 存在非法字符：把清理后的内容写回输入框。
        // setText 会再次触发 textChanged，但那时 clean == text，
        // 不会再次进入本分支，因此不会无限递归。
        const int cursorPos = ui->exprEdit->cursorPosition();
        ui->exprEdit->setText(clean);
        ui->exprEdit->setCursorPosition(qMin(cursorPos, clean.size()));
        statusBar()->showMessage(
            QStringLiteral("已自动剔除非法字符，仅支持 0-9 . + - * / ( )"), 2500);
    }

    updateResultPreview();
}

/**
 * @brief 实时预览：表达式合法就显示当前值，不合法显示占位符
 *
 * 注意：输入到一半的表达式（如 "1+"）本来就不完整，
 *       这属于正常过程，不应该弹窗报错，只在真正按下 "=" 时才提示。
 */
void MainWindow::updateResultPreview()
{
    const QString expression = ui->exprEdit->text().trimmed();

    if (expression.isEmpty()) {
        ui->resultLabel->setText(QStringLiteral("0"));
        return;
    }

    double  value = 0.0;
    QString error;
    if (m_calculator.evaluate(expression, value, error)) {
        ui->resultLabel->setText(formatNumber(value));
    } else {
        ui->resultLabel->setText(QStringLiteral("—"));
    }
}

// ==============================================================================
//  功能按钮槽函数
// ==============================================================================

/**
 * @brief "="：把表达式交给 Model 求值，并把结果显示出来
 */
void MainWindow::onEqualClicked()
{
    const QString expression = ui->exprEdit->text().trimmed();

    if (expression.isEmpty()) {
        showError(QStringLiteral("表达式为空，请先输入算式"), false); // 只在状态栏提示
        return;
    }

    double  value = 0.0;
    QString error;
    if (!m_calculator.evaluate(expression, value, error)) {
        // 除零、括号不匹配、格式错误等都会走到这里
        showError(error);
        return;
    }

    m_lastResult  = value;
    m_resultShown = true;

    const QString text = formatNumber(value);
    ui->resultLabel->setText(text);
    ui->exprEdit->setText(text); // 结果写入输入框，便于继续参与后续运算

    statusBar()->showMessage(QStringLiteral("计算成功：%1 = %2").arg(expression, text), 5000);
}

void MainWindow::onClearClicked()
{
    ui->exprEdit->clear();
    ui->resultLabel->setText(QStringLiteral("0"));

    m_lastResult  = 0.0;
    m_resultShown = false;

    statusBar()->showMessage(QStringLiteral("已清空"), 2000);
}

void MainWindow::onBackspaceClicked()
{
    QString text = ui->exprEdit->text();

    if (text.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("已经没有内容可以删除"), 1500);
        return;
    }

    text.chop(1); // 删除最后一个字符
    ui->exprEdit->setText(text);

    m_resultShown = false; // 手动修改后不再处于"结果态"
    ui->exprEdit->setFocus();
}

// ==============================================================================
//  拖拽输入
// ==============================================================================

/**
 * @brief 拖拽标签松手后的落点判断
 *
 * 判断方式：把输入框的矩形区域换算成屏幕坐标，检查释放点是否落在其中。
 * 这样不依赖 Qt 的拖放框架，完全由我们自己控制判定逻辑。
 */
void MainWindow::onDraggedReleased(const QString &payload, const QPoint &globalPos)
{
    const QRect targetRect(ui->exprEdit->mapToGlobal(QPoint(0, 0)), ui->exprEdit->size());

    if (!targetRect.contains(globalPos)) {
        // 没有拖到输入框上：给出友好提示，不插入字符
        statusBar()->showMessage(
            QStringLiteral("\"%1\" 未落在输入框内，已忽略（请拖到上方输入框再松手）").arg(payload),
            3000);
        return;
    }

    appendToExpression(payload);
    ui->exprEdit->setFocus();
    statusBar()->showMessage(QStringLiteral("已拖入字符 \"%1\"").arg(payload), 2000);
}

// ==============================================================================
//  财务计算
// ==============================================================================

/**
 * @brief 打开"财务参数设置"对话框
 *
 * 采用懒创建 + 复用：第一次点击时才 new，之后一直复用同一个实例，
 * 每次打开前用 setParameters 回填主窗口当前的参数，保证界面与实际一致。
 */
void MainWindow::onFinanceSettingsClicked()
{
    if (!m_financeDialog) {
        m_financeDialog = new FinanceDialog(this);

        // ============ 跨窗口通信的核心：对话框 -> 主窗口 ============
        // 对话框只负责发出信号，主窗口负责接收并处理，二者互不依赖内部实现
        connect(m_financeDialog, &FinanceDialog::parametersAccepted,
                this,            &MainWindow::onFinanceParamsChanged);
    }

    m_financeDialog->setParameters(m_rate, m_years, m_mode);

    if (m_financeDialog->exec() == QDialog::Accepted) {
        statusBar()->showMessage(QStringLiteral("财务参数已更新"), 4000);
    } else {
        statusBar()->showMessage(QStringLiteral("已取消修改，财务参数保持不变"), 3000);
    }
}

/**
 * @brief 接收对话框传来的参数并保存到主窗口（信号 -> 槽 的接收端）
 */
void MainWindow::onFinanceParamsChanged(double annualRatePercent, int years, int mode)
{
    m_rate  = annualRatePercent;
    m_years = years;
    m_mode  = static_cast<InterestMode>(mode);

    updateFinanceInfoLabel();
}

void MainWindow::updateFinanceInfoLabel()
{
    // 使用多参数 arg()：一次性替换所有占位符，避免链式调用时中间结果里的 '%' 被误解析
    ui->financeInfoLabel->setText(
        QStringLiteral("当前财务参数：年利率 %1%  |  年限 %2 年  |  计息方式：%3")
            .arg(QString::number(m_rate, 'f', 2),
                 QString::number(m_years),
                 FinanceCalculator::modeName(m_mode)));
}

void MainWindow::onSimpleInterestClicked()
{
    applyFinance(InterestMode::Simple);
}

void MainWindow::onCompoundInterestClicked()
{
    applyFinance(InterestMode::Compound);
}

/**
 * @brief 取本金：优先使用当前表达式的值，表达式为空时使用上一次的计算结果
 */
bool MainWindow::ensurePrincipal(double &principal, QString &error)
{
    const QString expression = ui->exprEdit->text().trimmed();

    if (expression.isEmpty()) {
        // 表达式为空时，用上一次计算结果作为本金；
        // 若上一次结果也是 0（程序刚启动），说明用户根本还没输入过本金
        principal = m_lastResult;
        if (qFuzzyIsNull(principal)) {
            error = QStringLiteral("请先在表达式框中输入本金（如 10000）");
            return false;
        }
        return true;
    }

    if (!m_calculator.evaluate(expression, principal, error)) {
        error = QStringLiteral("本金（当前表达式）无效：") + error;
        return false;
    }

    if (principal < 0.0) {
        error = QStringLiteral("本金不能为负数");
        return false;
    }

    return true;
}

/**
 * @brief 单利 / 复利计算的公共流程：取本金 -> 调 Model -> 显示结果
 */
void MainWindow::applyFinance(InterestMode mode)
{
    double  principal = 0.0;
    QString error;

    if (!ensurePrincipal(principal, error)) {
        showError(error);
        return;
    }

    // ---- 调用 Model 层完成真正的财务计算 ----
    const FinanceCalculator::Result result =
        FinanceCalculator::compute(principal, m_rate, m_years, mode, &error);

    if (!error.isEmpty()) {
        showError(error);
        return;
    }

    // ---- 更新显示 ----
    m_lastResult  = result.futureValue;
    m_resultShown = true;

    const QString futureText = formatNumber(result.futureValue);
    ui->resultLabel->setText(futureText);
    ui->exprEdit->setText(futureText);

    // ---- 弹窗展示完整计算过程 ----
    const QString detail = QStringLiteral("%1\n\n本金：%2\n年利率：%3%\n年限：%4 年\n\n本息合计：%5\n利息合计：%6")
                               .arg(FinanceCalculator::formulaText(principal, m_rate, m_years, mode),
                                    formatNumber(principal),
                                    QString::number(m_rate, 'f', 2),
                                    QString::number(m_years),
                                    futureText,
                                    formatNumber(result.interest));

    QMessageBox::information(this, QStringLiteral("财务计算结果"), detail);

    statusBar()->showMessage(QStringLiteral("%1计算完成：本息合计 %2")
                                 .arg(FinanceCalculator::modeName(mode), futureText), 6000);
}

// ==============================================================================
//  错误提示
// ==============================================================================

/**
 * @brief 统一的错误提示出口
 * @param message         错误内容
 * @param showMessageBox  是否弹出 QMessageBox；轻微提示只显示状态栏即可
 */
void MainWindow::showError(const QString &message, bool showMessageBox)
{
    // 状态栏提示（所有错误都会显示）
    statusBar()->showMessage(QStringLiteral("错误：%1").arg(message), 6000);

    // 严重错误再弹窗，避免频繁打断用户输入
    if (showMessageBox) {
        QMessageBox::warning(this, QStringLiteral("计算错误"), message);
    }
}
