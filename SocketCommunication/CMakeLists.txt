# ==============================================================================
#  Socket 文件传输工具 · Qt 6 + CMake 构建脚本
#  环境：Ubuntu 22.04 (Qt 6.2.4 / CMake 3.22 / GCC 11)
#
#  编译方式（在虚拟机本地磁盘上执行，切勿在 /mnt/hgfs 共享目录中编译）：
#      cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
#      cmake --build build -j$(nproc)
#      ./build/SocketFileTransfer
# ==============================================================================

cmake_minimum_required(VERSION 3.16)

project(SocketFileTransfer LANGUAGES CXX)

# ---------- C++ 标准 ----------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------- 开启 Qt 的自动代码生成 ----------
# AUTOMOC : 自动对含 Q_OBJECT 宏的头文件调用 moc，生成信号槽所需的元对象代码
# AUTOUIC : 自动把 .ui 文件编译成 ui_*.h
# AUTORCC : 自动把 .qrc 资源文件编译进可执行文件
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 告诉 uic 去哪里找 .ui 文件（本项目的 .ui 全部放在 ui/ 目录下）
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# ---------- 查找 Qt6 组件 ----------
# 本项目需要 Widgets（界面）与 Network（QTcpServer / QTcpSocket）两个模块。
# 图标统一使用 PNG 格式：PNG 由 QtGui 原生内置支持，不需要任何插件或额外模块，
# 因此不依赖 Qt6::Svg，只要系统装有 qt6-base 就能编译运行。
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# ---------- 目标可执行文件 ----------
add_executable(SocketFileTransfer
    # 入口
    src/main.cpp

    # 传输逻辑层（Model）：协议编解码 + 收发流水线，不直接操作界面
    src/transferprotocol.h
    src/transferprotocol.cpp
    src/filesender.h
    src/filesender.cpp
    src/filereceiver.h
    src/filereceiver.cpp

    # 自定义控件：重写拖拽事件实现文件拖入
    src/dropzone.h
    src/dropzone.cpp

    # View 层
    src/mainwindow.h
    src/mainwindow.cpp
    src/settingsdialog.h
    src/settingsdialog.cpp

    # Qt 资源系统（QSS 样式表 + PNG 图标）
    resources.qrc
)

# 头文件所在目录，供 uic 生成的 ui_mainwindow.h 中的 #include "dropzone.h" 使用
target_include_directories(SocketFileTransfer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(SocketFileTransfer PRIVATE Qt6::Widgets Qt6::Network)

# ---------- 提交作业用：安装规则（可选） ----------
install(TARGETS SocketFileTransfer RUNTIME DESTINATION bin)
