#ifndef DROPZONE_H
#define DROPZONE_H

#include <QWidget>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;

/**
 * @brief 拖拽接收区控件
 *
 * 通过「重写虚函数」实现文件拖拽交互（本作业对事件处理的考察点）：
 *   - dragEnterEvent ：文件拖入时接受拖拽，并进入高亮状态；
 *   - dragLeaveEvent ：文件拖出时取消高亮；
 *   - dropEvent      ：松开鼠标时读取拖入的文件路径并发出 fileDropped 信号。
 *
 * 拖入高亮通过切换 dynamic property "dragHover" 实现，配合 QSS 中的
 *   QWidget#dropZone[dragHover="true"] { ... }
 * 选择器即可获得高亮边框/背景反馈。
 */
class DropZone : public QWidget
{
    Q_OBJECT
public:
    explicit DropZone(QWidget *parent = nullptr);

    /// 设置提示文字
    void setHintText(const QString &text);

signals:
    /// 用户拖入并松开了文件（参数为所有拖入文件的完整路径列表）
    void filesDropped(const QStringList &paths);

protected:
    // —— 以下三个是 QWidget 的虚函数，这里重写实现自定义拖拽逻辑 ——
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    // 重写绘制事件，画一个居中的提示文字（不引入子控件，纯事件绘制）
    void paintEvent(QPaintEvent *event) override;

private:
    void setDragHover(bool on); ///< 切换拖入高亮状态

    QString m_hintText; ///< 提示文字
    bool    m_hover;    ///< 是否处于拖入高亮状态
};

#endif // DROPZONE_H
