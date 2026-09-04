#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QIcon>

/**
 * @brief 从 Qt 资源系统中读取 QSS 样式表内容
 *
 * 资源路径 ":/style.qss" 对应 resources.qrc 中的 <file alias="style.qss">
 * 资源在编译期就被 rcc 打包进可执行文件，因此程序运行时不再依赖外部文件。
 */
static QString loadStyleSheet()
{
    QFile file(QStringLiteral(":/style.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString(); // 读取失败时返回空串，程序将以系统默认外观运行
    }
    return QString::fromUtf8(file.readAll());
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // ---------- 应用程序图标（来自 Qt 资源系统，PNG 格式） ----------
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));

    // ---------- 应用 QSS 样式表（来自 Qt 资源系统） ----------
    const QString styleSheet = loadStyleSheet();
    if (!styleSheet.isEmpty()) {
        app.setStyleSheet(styleSheet);
    }

    // ---------- 创建并显示主窗口 ----------
    MainWindow window;
    window.show();

    // ---------- 进入 Qt 事件循环 ----------
    return app.exec();
}
