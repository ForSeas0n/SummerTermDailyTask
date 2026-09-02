# ==============================================================================
#  备用 qmake 工程文件
#  说明：Qt 6 官方主推 CMake，本作业默认使用 CMakeLists.txt 构建。
#        若老师/助教的评分环境只认 .pro，可用本文件在 Qt Creator 中打开。
#        两个文件二选一即可，不要同时构建，避免生成两套产物。
# ==============================================================================

QT += core gui widgets

CONFIG += c++17

TEMPLATE = app
TARGET = FinancialCalculator

# 源码
SOURCES += \
    src/main.cpp \
    src/calculator.cpp \
    src/financecalculator.cpp \
    src/draggablelabel.cpp \
    src/financedialog.cpp \
    src/mainwindow.cpp

# 头文件
HEADERS += \
    src/calculator.h \
    src/financecalculator.h \
    src/draggablelabel.h \
    src/financedialog.h \
    src/mainwindow.h

# Qt Designer 界面文件
FORMS += \
    ui/mainwindow.ui \
    ui/financedialog.ui

# Qt 资源系统
RESOURCES += resources.qrc
