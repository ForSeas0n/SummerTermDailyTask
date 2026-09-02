#include "draggablelabel.h"

#include <QApplication>
#include <QMouseEvent>

// ==============================================================================
//  构造 / 析构
// ==============================================================================

DraggableLabel::DraggableLabel(QWidget *parent)
    : QLabel(parent)
{
    // 让光标在悬停时显示为"小手"，暗示该控件可以被抓取
    setCursor(Qt::OpenHandCursor);
    setAlignment(Qt::AlignCenter);
}

DraggableLabel::DraggableLabel(const QString &text, QWidget *parent)
    : QLabel(text, parent)
{
    setCursor(Qt::OpenHandCursor);
    setAlignment(Qt::AlignCenter);
    m_payload = text;
}

DraggableLabel::~DraggableLabel()
{
    // 若用户拖拽过程中关闭了窗口，确保影子窗口被正确释放
    destroyGhost();
}

void DraggableLabel::setPayload(const QString &text)
{
    m_payload = text;
}

// ==============================================================================
//  鼠标事件重写
// ==============================================================================

/**
 * @brief 鼠标按下：进入拖拽状态并生成影子
 *
 * 注意：Qt 在控件接受了 mousePressEvent 之后会自动"抓取"鼠标，
 *       因此后续即使光标移出本控件，move / release 事件仍然会投递给本控件，
 *       这正是实现拖拽跟随的基础，无需手动调用 grabMouse()。
 */
void DraggableLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        setCursor(Qt::ClosedHandCursor); // 光标变为"握紧"，反馈已抓取

        createGhost();
        // Qt 6 推荐使用 globalPosition()（返回 QPointF），Qt 5 使用 globalPos()
        moveGhost(event->globalPosition().toPoint());

        event->accept(); // 明确接受事件，阻止继续向父控件传播
        return;
    }

    QLabel::mousePressEvent(event); // 非左键交给基类处理
}

/**
 * @brief 鼠标移动：只在拖拽状态下更新影子位置
 */
void DraggableLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        moveGhost(event->globalPosition().toPoint());
        event->accept();
        return;
    }

    QLabel::mouseMoveEvent(event);
}

/**
 * @brief 鼠标释放：销毁影子，并把"要插入的字符 + 释放位置"交给主窗口判断
 *
 * 这里刻意不在本类内部判断落点：本控件只负责"报告"发生了什么，
 * 是否接受由 MainWindow 决定，符合单一职责原则。
 */
void DraggableLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        const QPoint globalPos = event->globalPosition().toPoint();

        destroyGhost();
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);

        event->accept();

        // 发出信号 -> MainWindow::onDraggedReleased 判断落点是否在输入框内
        emit dragReleased(m_payload, globalPos);
        return;
    }

    QLabel::mouseReleaseEvent(event);
}

// ==============================================================================
//  影子（Ghost）窗口管理
// ==============================================================================

/**
 * @brief 创建一个无边框、置顶、完全不接收鼠标事件的小窗口作为"影子"
 *
 * 关键标志说明：
 *   Qt::FramelessWindowHint        —— 去掉系统标题栏和边框
 *   Qt::WindowStaysOnTopHint       —— 始终浮在最上层
 *   Qt::WindowTransparentForInput  —— 鼠标事件直接穿透，避免影子挡住落点判断
 *   Qt::WA_TransparentForMouseEvents —— 与上一项配合，双重保险
 *   Qt::WA_ShowWithoutActivating   —— 显示时不抢走输入焦点
 */
void DraggableLabel::createGhost()
{
    if (m_ghost) {
        return; // 防重入
    }

    m_ghost = new QLabel(m_payload.isEmpty() ? text() : m_payload);
    m_ghost->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                            | Qt::WindowTransparentForInput | Qt::Tool);
    m_ghost->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_ghost->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_ghost->setAttribute(Qt::WA_DeleteOnClose, false);

    m_ghost->setAlignment(Qt::AlignCenter);
    m_ghost->setFixedSize(52, 52);
    m_ghost->setWindowOpacity(0.75); // 半透明，形成"阴影"视觉效果

    // 影子的外观：圆角 + 主题蓝 + 阴影字，与 QSS 风格保持一致
    m_ghost->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color: #0984e3;"
        "  color: #ffffff;"
        "  border-radius: 26px;"
        "  font-size: 22px;"
        "  font-weight: bold;"
        "}"));

    m_ghost->show();
}

/** @brief 让影子中心对齐光标位置 */
void DraggableLabel::moveGhost(const QPoint &globalPos)
{
    if (!m_ghost) {
        return;
    }
    m_ghost->move(globalPos.x() - m_ghost->width() / 2,
                  globalPos.y() - m_ghost->height() / 2);
}

void DraggableLabel::destroyGhost()
{
    if (m_ghost) {
        m_ghost->hide();
        m_ghost->deleteLater(); // 交由事件循环安全销毁
        m_ghost = nullptr;
    }
}
