#include "dropzone.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QStyle>
#include <QUrl>

DropZone::DropZone(QWidget *parent)
    : QWidget(parent)
    , m_hintText(QStringLiteral("将文件拖拽到此处自动发送"))
    , m_hover(false)
{
    // 开启拖拽接收（必须，否则 dragEnterEvent 不会被触发）
    setAcceptDrops(true);

    // 设置最小尺寸，保证在布局里有合理的高度
    setMinimumHeight(120);
}

void DropZone::setHintText(const QString &text)
{
    m_hintText = text;
    update(); // 触发重绘
}

/**
 * 拖入事件：判断拖入内容里是否包含本地文件，
 * 有则接受（acceptProposedAction），并进入高亮状态。
 */
void DropZone::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        setDragHover(true);
    } else {
        event->ignore();
    }
}

/// 拖出事件：取消高亮
void DropZone::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    setDragHover(false);
}

/**
 * 松开事件：取出拖入的所有本地文件路径，
 * 发出 filesDropped 信号交由主窗口处理，同时取消高亮。
 */
void DropZone::dropEvent(QDropEvent *event)
{
    setDragHover(false);

    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) {
        return;
    }

    // 收集所有本地文件路径（支持一次拖入多个文件）
    QStringList paths;
    for (const QUrl &url : urls) {
        const QString path = url.toLocalFile();
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    if (paths.isEmpty()) {
        return;
    }

    event->acceptProposedAction();
    emit filesDropped(paths);
}

/// 切换高亮状态：通过 dynamic property 通知 QSS 改变外观
void DropZone::setDragHover(bool on)
{
    if (m_hover == on) {
        return;
    }
    m_hover = on;
    setProperty("dragHover", on);
    // 属性变化后需要刷新样式（QSS 按属性选择器匹配）
    style()->unpolish(this);
    style()->polish(this);
    update();
}

/// 绘制事件：居中显示提示文字
void DropZone::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(palette().color(QPalette::PlaceholderText));
    p.drawText(rect(), Qt::AlignCenter, m_hintText);
}
