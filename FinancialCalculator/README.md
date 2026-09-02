# 简易财务计算器（Qt 6 + C++）

一个具备「鼠标拖拽输入」特殊交互方式的 Qt 6 财务计算器，使用 CMake 构建。

- 开发环境：**Ubuntu 22.04 + Qt 6.2.4 + GCC 11 + CMake 3.22**
- 代码在 **Windows 本地编写**，通过 **VMware 共享文件夹** 同步到 Linux 虚拟机编译运行

---

## 版本更新记录（v1 → v2）

v1 为首个可正常编译运行的版本。v2 在 v1 基础上，针对实际测试中发现的问题做了以下 6 处修复与完善，**未改变整体架构与题目要求的实现方式**：

1. **修复粘贴可绕过输入过滤的问题**
   v1 的 `eventFilter` 只拦截单个按键，`Ctrl+V` 粘贴可将非法字符（字母、中文等）直接带入输入框。
   v2 改在 `onExpressionChanged` 中做内容层过滤，任何途径进入的非法字符都会被自动剔除，且不影响光标位置、不会无限递归。

2. **修复小数值显示精度截断**
   v1 的 `formatNumber` 对小数固定保留 6 位，导致 `0.0000001` 被四舍五入成 `"0.000000"`、去掉尾零后误显示为 `0`。
   v2 改为按数值大小动态选择小数位数（大数 6 位、接近 0 的小数 12 位）。

3. **优化空本金时的提示**
   v1 在表达式为空且没有历史结果时，会以 `0` 为本金弹出一个无意义的计算结果。
   v2 改为提示「请先在表达式框中输入本金」。

4. **输入过滤与解析器对齐（支持空格）**
   v1 的 `isAllowedChar` 不含空格，而 `tokenize` 会跳过空格，二者逻辑不一致，导致键盘无法输入空格。
   v2 在 `isAllowedChar` 中加入空格，可输入 `1 + 2` 这样更可读的表达式。

5. **按键区高度随窗口自适应**
   v1 窗口拉高时按键聚在顶部、下方留出大片空白。
   v2 将按键区按钮设为 `vertical Expanding`，随窗口缩放而变大，更贴合「布局管理器自适应」的要求。

6. **结果标签支持换行**
   v1 的 `resultLabel` 在结果过长时会被截断。
   v2 为其增加 `wordWrap`，超长结果可换行完整显示。

---

## 一、目录结构

```
FinancialCalculator/
├── CMakeLists.txt              # CMake 构建脚本（主用）
├── FinancialCalculator.pro     # 备用 qmake 工程文件（若评分环境只认 .pro）
├── resources.qrc               # Qt 资源系统：QSS 样式表 + PNG 图标
├── README.md
├── .gitignore
│
├── resources/
│   ├── style.qss               # QSS 样式表（圆角、悬停变色等）
│   └── icons/
│       ├── calculator.png      # 程序图标
│       ├── clear.png           # 清除按钮图标
│       ├── backspace.png       # 退格按钮图标
│       ├── equal.png           # 等号按钮图标
│       ├── finance.png         # 财务按钮图标
│       └── background.png      # 主窗口背景图
│
├── src/
│   ├── main.cpp                # 程序入口：加载 QSS、创建主窗口
│   │
│   ├── calculator.h / .cpp     # 【Model】表达式求值器（递归下降解析器）
│   ├── financecalculator.h/.cpp# 【Model】单利 / 复利计算
│   │
│   ├── draggablelabel.h / .cpp # 【自定义控件】重写鼠标事件实现拖拽输入
│   ├── mainwindow.h / .cpp     # 【View】主窗口
│   └── financedialog.h / .cpp  # 【View】财务参数设置对话框
│
└── ui/
    ├── mainwindow.ui           # 主窗口界面（Qt Designer）
    └── financedialog.ui        # 对话框界面（Qt Designer）
```

---

## 二、功能与题目要求对照

| 题目要求 | 实现位置 |
| --- | --- |
| 四则运算 | `src/calculator.cpp` 递归下降解析器，支持 `+ - * /` 与括号、一元负号 |
| 单利 / 复利计算 | `src/financecalculator.cpp`；单利 `FV=P×(1+r×n)`，复利 `FV=P×(1+r)^n` |
| 清除（C）与退格（←） | `MainWindow::onClearClicked()` / `onBackspaceClicked()` |
| 实时显示表达式与结果 | `QLineEdit#exprEdit` + `QLabel#resultLabel`，由 `textChanged` 驱动预览 |
| 除零 / 格式错误提示 | 状态栏 + `QMessageBox`，见 `MainWindow::showError()` |
| 鼠标拖拽输入 | `src/draggablelabel.cpp`，重写 `mousePress/Move/ReleaseEvent` |
| 拖拽时有影子跟随 | `DraggableLabel::createGhost()` 创建半透明置顶窗口 |
| 布局管理器排版 | 全部在 `.ui` 中完成，无任何坐标硬编码 |
| QSS 美化 | `resources/style.qss`，含悬停变色、圆角边框、按下反馈 |
| 资源文件（.qrc） | `resources.qrc`，打包 QSS 与 6 个 PNG 图标（Qt 原生支持，无需 SVG 模块） |
| 窗口间通信 | `FinanceDialog::parametersAccepted` 信号 → `MainWindow::onFinanceParamsChanged` 槽 |
| 架构分离（View / Model） | Model 层（`calculator` / `financecalculator`）不含任何界面代码 |
| 虚函数重写 / 事件过滤器 | 两者都有：拖拽用**重写虚函数**，输入框键盘处理用**事件过滤器** |

---

## 三、在 Ubuntu 22.04 上安装依赖

```bash
sudo apt update

# 编译工具链
sudo apt install -y build-essential cmake ninja-build gdb

# Qt 6 开发包（Widgets 头文件 + Designer 工具 + moc/uic/rcc）
# 说明：本项目图标使用 PNG 格式，不依赖 Qt SVG 模块，无需安装 libqt6svg6-dev
sudo apt install -y qt6-base-dev qt6-base-dev-tools qt6-tools-dev qt6-tools-dev-tools
```

> 若 apt 提示某个包不存在，删掉那个包名重试即可（不同 Ubuntu 小版本的包名略有差异）。
> 核心必需的是 `qt6-base-dev`（提供 Widgets 头文件与 CMake 配置）
> 和 `qt6-tools-dev-tools`（提供 uic / moc / rcc）。

验证环境：

```bash
cmake --version          # 应 >= 3.16
qmake6 --version         # 应显示 Qt 6.x
ls /usr/lib/x86_64-linux-gnu/cmake/Qt6   # 应能看到 Qt6WidgetsConfig.cmake 等
```

想用 Qt Creator 打开项目的话：`sudo apt install -y qtcreator`。

---

## 四、编译与运行

> **重要**：必须在虚拟机的**本地磁盘**（如 `~/`）上编译，**不要**在 `/mnt/hgfs` 共享目录中编译。
> HGFS 不支持符号链接与 inotify，在其上运行 cmake 会失败。

```bash
# 1. 进入项目目录（源码已同步到虚拟机本地磁盘）
cd ~/FinancialCalculator

# 2. 配置（生成 build/ 目录）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 3. 编译
cmake --build build -j$(nproc)

# 4. 运行
./build/FinancialCalculator
```

以后每次改代码，只需重新执行第 3 步（cmake 会自动检测 CMakeLists.txt 变化并重新配置）。

---

## 五、从 Windows 同步到虚拟机

### 5.1 一次性配置共享文件夹

**VMware**：虚拟机 → 设置 → 选项 → 共享文件夹 → 总是启用 → 添加路径。
共享名与项目目录名都使用**英文**，避免 HGFS 的编码问题。例如：

- 主机文件夹：`C:\Users\<你的用户名>\Desktop\share`
- 共享名：`share`
- 项目目录：`share\FinancialCalculator`

**Ubuntu 侧挂载**：

```bash
sudo apt install -y open-vm-tools open-vm-tools-desktop
sudo mkdir -p /mnt/hgfs
sudo vmhgfs-fuse .host:/ /mnt/hgfs -o allow_other
ls /mnt/hgfs/share/FinancialCalculator   # 能看到 CMakeLists.txt 即成功
```

开机自动挂载，在 `/etc/fstab` 末尾追加：

```
.host:/  /mnt/hgfs  fuse.vmhgfs-fuse  allow_other,defaults  0  0
```

### 5.2 每次改完代码，一条命令同步并编译

```bash
rsync -av --delete --exclude='build/' --exclude='.git/' \
      /mnt/hgfs/share/FinancialCalculator/ ~/FinancialCalculator/ \
  && cd ~/FinancialCalculator \
  && cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  && cmake --build build -j$(nproc) \
  && ./build/FinancialCalculator
```

把它保存成 `~/run.sh` 并 `chmod +x ~/run.sh`，以后只需敲 `~/run.sh`。

### 5.3 换行符与编码（重要）

Windows 保存的文件默认是 **CRLF**，传到 Linux 后脚本会报 `bad interpreter`。
本项目源码为 C++，CRLF 不影响编译，但为保险起见建议统一为 LF：

- VS Code：右下角点击 `CRLF` → 选择 `LF`
- 或同步后批量转换：`find . -name '*.cpp' -o -name '*.h' | xargs sed -i 's/\r$//'`

所有文件必须保存为 **UTF-8 无 BOM**（VS Code 保存时选 "UTF-8"，不要选 "UTF-8 with BOM"），
否则 GCC 可能报 `stray '\357'` 之类的错误。

---

## 六、使用说明

1. **按键输入**：点击按键区的数字与运算符，或直接用键盘输入（回车计算、Esc 清空）。
2. **拖拽输入**：按住下方「拖拽输入区」中的任意标签，会有一个蓝色半透明圆点跟随光标；
   拖到上方表达式输入框**上方松手**，该字符即被追加；松手位置不在输入框上则忽略并提示。
3. **财务计算**：
   - 点「财务参数设置...」设置年利率、年限、计息方式（对话框通过信号把参数传回主窗口）；
   - 在表达式框输入本金（如 `10000`）；
   - 点「单利计算」或「复利计算」，弹出结果详情（含公式、本息合计、利息）。

---

## 七、常见问题

**Q1：`Could NOT find a package configuration file provided by "Qt6"`**
系统缺 Qt6 开发包：
```bash
sudo apt install -y qt6-base-dev
```

**Q2：`uic: command not found` 或找不到 `ui_mainwindow.h`**
```bash
sudo apt install -y qt6-tools-dev-tools
```

**Q3：中文显示为方框**
虚拟机缺中文字体：
```bash
sudo apt install -y fonts-noto-cjk
```

**Q4：编译时报 `stray '\357' in program`**
文件被存成了 UTF-8 with BOM。用 VS Code 打开，另存为 "UTF-8"（无 BOM）。

**Q5：想改用 Qt Creator 打开**
Qt Creator → 打开文件或项目 → 选择 `CMakeLists.txt` → 配置 Kit（选 Qt 6.x）即可。
若评分环境要求 `.pro`，改用 `FinancialCalculator.pro` 打开（二选一，不要同时用）。

---

## 八、作业提交前检查清单

- [ ] 项目能在 Ubuntu 22.04 + Qt 6 下正常编译运行
- [ ] 源码包含中文注释（信号槽连接、事件处理部分已重点注释）
- [ ] 含 `.h` / `.cpp` / `.ui` / `.qrc` / `CMakeLists.txt`（+ 备用 `.pro`）
- [ ] 删除 `build/` 目录后再打包
- [ ] 若用 qmake 提交，删除 `CMakeLists.txt`，反之亦然，避免评分环境混乱
