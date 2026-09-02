#ifndef DRAGGABLELABEL_H
#define DRAGGABLELABEL_H

#include <QLabel>
#include <QPoint>
#include <QString>

/**
 * @brief 可拖拽的数字 / 运算符标签（自定义控件）
 *
 * 【功能说明】
 *   界面上摆一排这样的标签（0-9、+、-、*、/ 等），用户按住其中任意一个
 *   并向"表达式输入框"拖动时，会有一个半透明的影子跟随光标移动；
 *   松开鼠标时，如果光标位于输入框上方，就把该字符追加进表达式。
 *
 * 【设计要点：为什么选择"重写鼠标事件"而不是 QDrag / eventFilter】
 *   1. 题目要求"通过重写鼠标事件来模拟拖拽体验"，即手动模拟拖拽过程，
 *      而不是使用 Qt 内置的拖放框架（QMimeData + QDrag + dropEvent）。
 *      因为拖放框架是为"跨应用程序传递数据"设计的，对本场景过重，
 *      且无法自由控制"影子"的外观与跟随行为。
 *   2. 选择"重写虚函数"而非"事件过滤器"，是因为拖拽逻辑本身就是
 *      DraggableLabel 这个控件固有的行为，写在类内部的虚函数里，
 *      内聚性最好、可读性强；事件过滤器更适合"在外部给已有控件
 *      追加行为"的场景（本项目的输入框键盘处理就是这么做的，
 *      见 MainWindow::eventFilter）。
 *
 * 【与外部的通信方式】
 *   本类不直接操作输入框，只在鼠标释放时发出 dragReleased 信号，
 *   由 MainWindow 判断落点并决定是否追加字符 —— 保证控件职责单一、可复用。
 */
class DraggableLabel : public QLabel
{
    Q_OBJECT

public:
    explicit DraggableLabel(QWidget *parent = nullptr);
    explicit DraggableLabel(const QString &text, QWidget *parent = nullptr);
    ~DraggableLabel() override;

    /** @brief 该标签代表的字符，例如 "7"、"+"、"(" */
    QString payload() const { return m_payload; }

    /** @brief 设置该标签代表的字符（通常与显示文本一致） */
    void setPayload(const QString &text);

signals:
    /**
     * @brief 拖拽结束（鼠标释放）时发出
     * @param payload   本次要插入的字符
     * @param globalPos 释放瞬间的全局屏幕坐标，由接收方判断是否落在目标区域内
     */
    void dragReleased(const QString &payload, const QPoint &globalPos);

protected:
    // ---------- 鼠标事件重写（题目要求的"虚函数重写"） ----------
    /** 按下鼠标：开始拖拽，并创建跟随光标的半透明影子 */
    void mousePressEvent(QMouseEvent *event) override;
    /** 移动鼠标：让影子跟随光标（Qt 会自动把后续 move 事件继续发给本控件） */
    void mouseMoveEvent(QMouseEvent *event) override;
    /** 释放鼠标：销毁影子并发出 dragReleased 信号 */
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void createGhost();                    // 创建影子窗口
    void moveGhost(const QPoint &globalPos); // 移动影子窗口
    void destroyGhost();                   // 销毁影子窗口

    QString m_payload;          // 该标签代表的字符
    bool    m_dragging = false; // 是否正处于拖拽状态
    QLabel *m_ghost    = nullptr; // 跟随光标移动的半透明影子（无父对象的顶层窗口）
};

#endif // DRAGGABLELABEL_H
