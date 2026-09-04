# Socket 文件传输工具

基于 **Qt 6 + C++** 的 TCP Socket 文件传输工具，支持文本文件与图片文件的**双向可靠传输**，含拖拽发送、实时进度、日志反馈与 QSS 美化界面。

## 快速开始（三步编译运行）

本压缩包为纯源码交付（Qt 6 项目无法跨平台直接运行二进制），在装有 Qt 6 的 Linux 环境下：

```bash
tar -xzf SocketCommunication.tar.gz && cd SocketCommunication
cmake -S . -B build && cmake --build build -j$(nproc)
./build/SocketFileTransfer
```

- 也可用 Qt Creator 直接打开 `SocketFileTransfer.pro` 构建（qmake 方式）。
- 运行与测试步骤见下文「四、运行与测试」；若只想看协议设计，见「五、协议说明」。

## 一、功能特性

- **双角色模式**：单程序内切换「服务端（监听）」/「客户端（连接）」模式，双向收发。
- **TCP 通信**：服务端用 `QTcpServer::listen` 监听，客户端用 `QTcpSocket::connectToHost` 连接。
- **可靠传输**：自定义协议头（魔数 + 类型 + 文件名长度 + 文件大小 + 文件名 + 数据），正确处理 TCP 粘包/拆包。
- **大文件流式发送**：`write` + `bytesWritten` 信号驱动的流水线分块发送，避免内存峰值。
- **流式接收边收边落盘**：协议层以事件流（FileStarted/FileData/FileFinished）驱动，接收端内存占用与文件大小无关，接收进度实时显示。
- **发送队列**：传输中拖入的新文件自动排队，依次发送，不打断进行中的数据流。
- **拖拽发送**：重写 `dragEnterEvent` / `dropEvent`，拖入文件高亮反馈，松开自动发送（支持一次拖多个）。
- **实时反馈**：日志区（带文件名与字节数）+ `QProgressBar` 实时进度 + 状态栏提示。
- **连接设置对话框**：通过信号与槽回传 IP / 端口 / 保存路径。
- **错误处理**：连接失败、断开、文件打开失败、传输中断等均有提示并自动清理半成品文件，不崩溃。

## 二、目录结构

```
SocketCommunication/
├── CMakeLists.txt            # 主构建脚本（CMake）
├── SocketFileTransfer.pro    # 备用 qmake 工程
├── resources.qrc             # 资源清单（QSS + PNG 图标）
├── README.md
├── resources/
│   ├── style.qss             # QSS 样式表
│   └── icons/                # PNG 图标（app/connect/disconnect/listen/send/open/settings）
├── ui/
│   ├── mainwindow.ui         # 主窗口
│   └── settingsdialog.ui     # 连接设置对话框
└── src/
    ├── main.cpp              # 入口
    ├── mainwindow.h/.cpp     # 主窗口（模式切换、调度、日志、进度）
    ├── settingsdialog.h/.cpp # 连接设置对话框（窗口通信）
    ├── dropzone.h/.cpp       # 拖拽接收区（重写拖拽事件）
    ├── transferprotocol.h/.cpp # 协议编解码与分包组包
    ├── filesender.h/.cpp     # 文件发送流水线
    └── filereceiver.h/.cpp   # 文件接收落盘
```

## 三、编译（Ubuntu 22.04 / Qt 6.2.4）

> 注意：请把项目复制到虚拟机**本地磁盘**后再编译，**不要**直接在 `/mnt/hgfs` 共享目录里编译。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/SocketFileTransfer
```

若老师环境只认 `.pro`，可用 Qt Creator 打开 `SocketFileTransfer.pro` 构建（已含 `QT += network`）。

## 四、运行与测试

本工具为单程序双模式，测试时**开两个实例**（一个窗口只能扮演一个角色）：

1. **实例 A（服务端）**：选「服务端」→ 点「开始监听」（默认端口 8888）。
2. **实例 B（客户端）**：选「客户端」→ 点「连接服务端」（默认 `127.0.0.1:8888`）。
3. 任一方把文件**拖进拖拽区**（支持一次拖多个文件，或点「选择文件发送」多选），即自动发送给对端。
4. 对端自动接收并保存。

**关于保存路径（重要）**：保存路径是**接收方窗口**自己的设置——
- A 发给 B 的文件，保存在 **B 窗口**设置的路径里；
- B 发给 A 的文件，保存在 **A 窗口**设置的路径里。
两个窗口的设置互相独立，主窗口下方的「接收保存位置」标签实时显示本窗口的保存位置。
默认是程序运行时的当前目录，建议在「连接设置」里改成明确路径。

**发送队列**：正在发送时又拖入/选择了新文件，新文件会自动排队，
当前文件发完后依次发送，不会打断进行中的传输。

**传输中断**：传输中途断开连接时，发送自动中止、接收端的半成品临时文件
（`*.sftmp`）会被自动清理，不会残留损坏文件。

跨机器测试时，把客户端的 IP 改成服务端所在机器的局域网 IP，并确保防火墙放行对应端口。

## 五、协议说明

自定义协议帧（网络字节序）：

| 字段 | 长度 | 说明 |
|---|---|---|
| magic | 4 字节 | 魔数 `0x46545453`，校验数据对齐 |
| type | 4 字节 | 消息类型（1 = 文件） |
| fileNameLen | 4 字节 | 文件名（UTF-8）字节长度 |
| fileSize | 8 字节 | 文件数据总字节数 |
| fileName | 变长 | 文件名字节 |
| fileData | 变长 | 文件二进制数据 |

接收端维护缓冲状态机（`WaitHeader → WaitFileName → WaitData`），按长度字段精确切分，缓冲不足时等待下一次 `readyRead`，从而彻底解决 TCP 粘包/拆包。
