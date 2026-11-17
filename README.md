# Terminal++

**Terminal++** 是一款用 **Qt 6** 开发的一体化终端与编辑器工具，在同一个带标签页的窗口里集成了三大功能模块：

- **代码编辑器** — 多标签代码/文本编辑
- **SSH 终端** — 远程 SSH 会话（内置 tmux 支持）
- **串口 / UART 终端** — 本地串口调试控制台

三个模块共用同一套 VT100/ANSI 终端渲染引擎，可在一个窗口内自由混排标签页。

---

## 一、功能清单

### 1. 代码编辑器
- 多标签编辑（可移动、可关闭，未保存自动提示）
- 行号栏 + 当前行高亮
- 多语言语法高亮：C/C++、C#、Java、Python、JavaScript、JSON、XML、HTML、SQL、Shell、INI
- 查找 / 替换 / 全部替换 / 计数：支持大小写、全词、正则、循环查找
- 跳转到行（Ctrl+G）
- 撤销 / 重做 / 剪切 / 复制 / 粘贴 / 全选
- 缩进控制：Tab→空格、块缩进 / 反缩进（Shift+Tab）、回车自动缩进
- 编码支持：UTF-8 / UTF-16LE / UTF-16BE / ISO-8859-1 / GB18030，自动 BOM 识别
- 行尾转换：Windows (CRLF) / Unix (LF) / Mac (CR)，打开时自动检测
- 内置多套配色主题：Default / Monokai / Deep Black / Blue light / Twilight / Choco
- 字体缩放（Ctrl+滚轮 / Ctrl +/- / Ctrl+0）
- 自动换行开关、行号显示开关
- 打印（Ctrl+P）
- 命令行传参直接打开文件
- 状态栏显示行列号、语言、编码、行尾
- **列编辑 / 块编辑**（Alt+拖动鼠标框选矩形区域，输入/删除/复制/粘贴同时作用于所有选中行，Esc 退出；状态栏显示 COLUMN 指示）
- **剪贴板历史**（Ctrl+Shift+V 打开历史列表，自动记录最近 50 条，双击回填并粘贴，持久化保存）
- **自动保存**（View 菜单开关，默认 30 秒静默保存所有已命名且已修改的标签，设置持久化）
- **多语言界面**：English / 简体中文 / 繁體中文 / Español / Français / Deutsch / Русский / 日本語 / 한국어，首次启动默认英文，可在 "UI Language" 菜单切换，选择后立即生效并持久化，下次启动沿用

### 2. SSH 终端
- 多 SSH 会话，每个会话一个标签页
- 左侧会话侧边栏（Ctrl+B 显隐），双击连接
- 会话配置持久化到 `sessions.json`
- 密码 / 私钥认证、自定义端口、额外连接参数
- tmux 自动挂载：`tmux new-session -A -s <name>`，断线不丢工作
- VT100 / xterm-256color 全 ANSI 转义渲染、256 色 + 真彩色
- 中文输入法：候选框跟随光标位置（IME `ImCursorRectangle`）
- 复制（Ctrl+Shift+C / 鼠标选取）、粘贴（Ctrl+Shift+V / 中键）
- 滚动回看缓冲（scrollback 5000 行）
- 标签标题随远端 OSC 标题自动更新
- 重连（Ctrl+Shift+R）
- **原生 SSH 实现**：基于 libssh2 自建客户端（独立工作线程 + socket + 握手 + 认证 + PTY/shell 通道），不依赖任何外部 ssh / plink 程序，完全自包含。

### 3. 串口 / UART 终端
- 串口选择（自动枚举可用串口 + 刷新）
- 波特率：300 ~ 921600（可手填）
- 数据位 5/6/7/8、校验 None/Even/Odd/Mark/Space、停止位 1/1.5/2
- 流控：None / RTS-CTS / XON-XOFF
- 回车发送：CR / LF / CRLF / None
- 本地回显（Local echo）
- 与 SSH 共用同一 ANSI 终端渲染，支持彩色日志
- 设备拔出自动检测

### 4. 共用框架
- 统一标签页：编辑器 / SSH / 串口可任意混排
- 统一菜单栏、工具栏、状态栏
- 关闭时统一提示保存、统一断开所有连接
- 跨平台：Windows 与 Linux（及 macOS）均可编译运行

---

## 二、编译环境要求

| 组件 | 版本 | 说明 |
|------|------|------|
| Qt | **6.3+**（推荐 6.5 / 6.6） | 需含 `Widgets`、`Network`、`PrintSupport`、**`SerialPort`** 模块 |
| **libssh2** | 1.9+ | SSH 功能依赖，原生客户端 |
| 编译器 | Windows: **MinGW-w64**（Qt 自带）；Linux: **g++** / clang | |
| 构建工具 | CMake 3.19+ **或** qmake | 两种都提供 |
| 系统 | Windows 10/11 x64、Linux、macOS | 全平台通用 |

> ⚠️ 安装 Qt 时请在 Qt Maintenance Tool 里勾选 **Qt Serial Port** 附加模块。
>
> ⚠️ **libssh2** 需单独安装：
> - Windows(MSYS2)：`pacman -S mingw-w64-x86_64-libssh2`，然后把安装根目录传给 `-DCMAKE_PREFIX_PATH` 或设 `LIBSSH2_DIR`
> - Debian/Ubuntu：`sudo apt install libssh2-1-dev pkg-config`
> - Fedora：`sudo dnf install libssh2-devel pkgconf`
> - Arch：`sudo pacman -S libssh2 pkgconf`
> - macOS：`brew install libssh2 pkg-config`

---

## 三、Windows 完整环境搭建（MSYS2 / MinGW-w64，推荐，已验证）

这是本项目实际用来产出 `dist\` 可执行包的完整、可复现流程。MSYS2 一次性装好
MinGW-w64 + Qt6 + libssh2 全套工具链，`build-msys2.sh` 会用 g++ 直接编译、
生成 exe 图标、并把全部运行时 DLL 打包到 `dist\`，得到免安装、可直接拷走的程序。

### 1. 安装 MSYS2
1. 从 https://www.msys2.org 下载安装器，默认安装到 `C:\msys64`。
2. 打开 **“MSYS2 MinGW 64-bit”** 终端（开始菜单里），先更新一次：
   ```bash
   pacman -Syu          # 若提示关闭终端，关掉后再次打开并重复一次
   pacman -Su
   ```

### 2. 安装编译工具链与依赖
在 MSYS2 终端里执行（一条命令装全）：
```bash
pacman -S --needed \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-qt6-base \
    mingw-w64-x86_64-qt6-serialport \
    mingw-w64-x86_64-libssh2 \
    mingw-w64-x86_64-zlib \
    mingw-w64-x86_64-zstd \
    mingw-w64-x86_64-cmake        # 可选：方式 B 的 CMake 需要
```
> 说明：
> - `qt6-base` 提供 Widgets/Gui/Core/Network/PrintSupport；`qt6-serialport` 提供串口模块。
> - `libssh2` 是原生 SSH 功能的依赖。
> - `zlib`/`zstd` 是 Qt 与工具链的运行时依赖。
> - exe 文件图标由 `resources/icon.png` 自动转换为 `icon.ico`；如需自动转换，系统 Python 装一下 Pillow：`pip install Pillow`（不装也能编译，只是 exe 文件图标会跳过，窗口/任务栏图标仍生效）。

### 3. 一键编译 + 打包
```bash
# 在 MSYS2 终端里（任意子系统都行，脚本内部会切到 mingw64）：
bash /c/dx/myaiprojects/terminal++/TerminalPP/build-msys2.sh
```
或从 Windows CMD / PowerShell：
```powershell
C:\msys64\usr\bin\bash.exe -lc "bash /c/dx/myaiprojects/terminal++/TerminalPP/build-msys2.sh"
```

完成后产物在 **`TerminalPP\dist\`**：
- `TerminalPP.exe`（已内嵌窗口图标 + exe 文件图标）
- 约 34 个运行时 DLL（Qt6 六模块、libssh2、libssl/libcrypto、icu、harfbuzz、freetype、gcc 运行时等）
- `platforms\qwindows.dll` + `styles\` + `imageformats\` 插件

**双击 `TerminalPP\dist\TerminalPP.exe` 即可运行，无需安装 Qt。**

---

## 四、其它编译方式（可选）

### 方式 B：CMake（MSYS2 环境内）
```bash
export PATH=/mingw64/bin:/usr/bin:/bin
cd /c/dx/myaiprojects/terminal++/TerminalPP
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/mingw64
cmake --build build -j
# 可执行文件: build/TerminalPP.exe （运行时需 /mingw64/bin 在 PATH，或用 build-msys2.sh 的打包步骤）
```

### 方式 C：官方 Qt 安装版（Qt Online Installer）
若使用官方 Qt（含 MinGW Kit）而非 MSYS2，可用自带脚本：
```powershell
cd TerminalPP
.\build.ps1 -QtDir C:\Qt\6.6.0\mingw_64 -Libssh2Dir <libssh2 安装根目录> -Deploy
```
或 CMD：先在 `build.bat` 顶部设置 `QT_DIR` / `MINGW_DIR` / `LIBSSH2_DIR`，再 `build.bat deploy`。
> 注意：官方 Qt 不含 libssh2，需另行准备（最简单是复用 MSYS2 的 `C:\msys64\mingw64`）。

### 方式 D：Linux / macOS
```bash
# Ubuntu: sudo apt install build-essential cmake qt6-base-dev qt6-serialport-dev libssh2-1-dev pkg-config
cd TerminalPP
chmod +x build.sh
./build.sh           # 或 ./build.sh run / ./build.sh clean / ./build.sh qmake
```

### 方式 E：Qt Creator
用 Qt Creator 打开 `TerminalPP/CMakeLists.txt` 或 `TerminalPP/TerminalPP.pro`，选 Qt6 Kit，点运行。

---

## 五、运行与部署

- Windows（推荐）：用 `build-msys2.sh` 产出的 `TerminalPP\dist\` 已自带全部 DLL 与插件，整个 `dist` 目录拷贝到任意 Win10/11 x64 即可运行，无需安装 Qt。
- Windows（官方 Qt）：执行 `windeployqt --release build\TerminalPP.exe` 打包 Qt6 DLL/插件，另需手动拷贝 `libssh2` 运行时 DLL。
- Linux 分发：依赖系统的 Qt6 与 libssh2 运行库，或用 `linuxdeployqt` 等工具打包。

---

## 六、键盘快捷键

| 快捷键 | 功能 |
|--------|------|
| `Ctrl+N` (File) / `Ctrl+N` (Terminal) | 新建文件 / 新建 SSH 会话 |
| `Ctrl+O` | 打开文件 |
| `Ctrl+S` / `Ctrl+Shift+S` | 保存 / 全部保存 |
| `Ctrl+P` | 打印 |
| `Ctrl+W` | 关闭当前标签 |
| `Ctrl+Q` | 退出 |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 重做 |
| `Ctrl+F` / `Ctrl+H` | 查找 / 替换 |
| `Ctrl+G` | 跳转到行 |
| `Ctrl +` / `Ctrl -` / `Ctrl 0` | 放大 / 缩小 / 重置字号 |
| `Ctrl+B` | 显隐 SSH 会话侧边栏 |
| `Ctrl+Shift+R` | 重连当前 SSH 会话 |
| `Ctrl+Shift+C` / `Ctrl+Shift+V` | 终端复制 / 粘贴 |

---

## 七、源码结构

```
TerminalPP/
├── CMakeLists.txt          # CMake 构建
├── TerminalPP.pro          # qmake 构建（备选）
├── build.bat / build.ps1   # Windows 一键编译脚本
├── build.sh                # Linux / macOS 一键编译脚本
├── resources/              # qrc 资源
└── src/
    ├── main.cpp
    ├── mainwindow.{h,cpp}           # 主窗口：菜单/工具栏/状态栏/标签页/侧边栏
    ├── editor/                      # ===== 代码编辑器 =====
    │   ├── codeeditor.{h,cpp}       # 行号 + 当前行高亮 + 缩进 + 缩放
    │   ├── syntaxhighlighter.{h,cpp}# 多语言语法高亮
    │   ├── finddialog.{h,cpp}       # 查找 / 替换
    │   ├── gotodialog.{h,cpp}       # 跳转到行
    │   ├── editortab.{h,cpp}        # 文件读写 / 编码 / 行尾 / 主题
    │   └── thememanager.{h,cpp}     # 配色主题
    └── terminal/                    # ===== SSH + 串口终端 =====
        ├── ansiparser.{h,cpp}       # VT100/xterm-256 ANSI 引擎（SSH + 串口共用）
        ├── terminalwidget.{h,cpp}   # 终端渲染 + 键盘 + IME + 选择复制
        ├── sshsession.{h,cpp}       # 原生 libssh2 SSH 客户端（工作线程）+ tmux
        ├── sshtab.{h,cpp}           # SSH 标签页
        ├── sessiondialog.{h,cpp}    # SSH 会话配置对话框
        ├── sessionmanager.{h,cpp}   # sessions.json 持久化
        ├── serialsession.{h,cpp}    # QSerialPort 封装
        ├── serialtab.{h,cpp}        # 串口标签页
        └── serialdialog.{h,cpp}     # 串口配置对话框
```

---

## 八、设计说明

- **统一终端引擎**：`AnsiParser` 维护屏幕缓冲 + 滚动回看，`TerminalWidget` 负责绘制与输入，SSH 和串口只是不同的数据源（`SshSession` / `SerialSession`），因此两边都拥有完整的 VT100 渲染、256 色、中文 IME、复制粘贴能力。
- **SSH 原生自包含**：`SshSession` 在独立工作线程里用 libssh2 完成 TCP 连接、SSH 握手、密码/公钥认证、请求 PTY(xterm-256color) 并开 shell（或 `tmux new-session -A`），再把收发字节通过队列信号与 GUI 线程的 `TerminalWidget` 交换。不调用任何外部 ssh / plink 程序，Windows(ws2_32) 与 Linux(BSD socket) 均可编译运行。
- **编辑器与终端解耦**：编辑器标签页与终端标签页共存于同一 `QTabWidget`，菜单项根据当前标签页类型智能响应。