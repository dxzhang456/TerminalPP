# Terminal++（中文完整文档）

> 本文件是 Terminal++ 的完整中文文档（安装、编译、快捷键、源码结构、设计说明）。多语言概览请见根目录 [README.md](README.md)。

---

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
- 字体缩放（Ctrl+滚轮 / Ctrl +/- / Ctrl+0；Ctrl++/Ctrl+-/Ctrl+0 均以独立 QShortcut 注册，SSH/串口标签页同样可用 Ctrl +/- 调整终端字体大小）
- 自动换行开关、行号显示开关
- 打印（Ctrl+P）
- 命令行传参直接打开文件
- 状态栏显示行列号、语言、编码、行尾
- **列编辑 / 块编辑**（Alt+拖动鼠标框选矩形区域，输入/删除/复制/粘贴同时作用于所有选中行，Esc 退出；状态栏显示 COLUMN 指示）
- **剪贴板历史**（Ctrl+Shift+V 打开历史列表，自动记录最近 50 条，双击回填并粘贴，持久化保存）
- **自动保存**（View 菜单开关，默认 30 秒静默保存所有已命名且已修改的标签，设置持久化）
- **十六进制查看/编辑/保存**（File → Open as Hex...，偏移+十六进制+ASCII 三栏，鼠标/方向键定位，直接在 Hex 或 ASCII 列输入改写字节，Ctrl+S 保存回磁盘）
- **File → Open Recent**：最近打开过的 20 个文件，持久化，含"清除最近文件"
- **标签栏右键菜单**：关闭当前 / 关闭其他 / 关闭向左 / 关闭向右；标签栏空白区域双击可新建文件标签页
- **启动即为空窗口**：不再自动打开或保留一个空白标签页，关闭最后一个标签页后窗口即为空
- **文本解码强化**：自动识别 BOM、校验"看似合法"的 UTF-8 是否实际是误判的 GBK（避免云盘下载的中文 txt 在 Terminal++ 和系统记事本中都显示乱码），必要时回退系统 ANSI 代码页
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

### 4. 文件比较
顶层 **Compare** 菜单（New Comparison = Ctrl+Shift+K；或直接选择某种比较类型，对话框会自动预选对应类型）：
- **文本比较**：左右双栏对齐视图——插入/删除行留空、按类型着色（修改/新增/删除/相同）、字符级内联高亮、同步滚动、上一处/下一处差异导航（◀ Prev / Next ▶）、整块互拷（Copy → / ← Copy）、交换、重载、保存左/右、忽略大小写/忽略空白开关、差异统计。基于 Myers O(ND) 差异算法。
- **文件夹比较**：**递归**对比两个目录的所有子文件夹，6 列（名称/大小/修改时间/状态/大小/修改时间），状态分类 Same / Left newer / Right newer / Left only / Right only / Different（MD5+大小判等）、状态列按主题着色、名称 glob 过滤（`*.cpp;*.h`）、仅显示差异开关、**文件与整个文件夹递归互拷**（Copy → / ← Copy）、刷新。双击差异文件对按类型智能分流：文本→文本比较、图片→图片比较、其它→十六进制比较。
- **十六进制比较**：左右 hex dump（偏移 | 16 字节 | ASCII），差异字节高亮、同步滚动、字节差异统计。
- **图片比较**：左右同步平移/缩放（放大/缩小/适应/1:1）、逐像素差异叠加模式、尺寸与差异像素百分比统计。**打开时图片会自动缩放到可视范围**：过大的图片按比例缩小以适配窗口，过小的图片按比例放大到清晰可见。

### 5. 连接（云盘 / 共享文件夹 / SFTP / FTP）
顶层 **Connect** 菜单提供三个独立入口，均用同一个浏览器标签页界面（与右键操作、上传/下载、传输面板完全一致）：

- **Connect Cloud Drive...**：**百度网盘** 与通用 **WebDAV**（坚果云/Nutstore、自建服务器等）。认证：百度用 OAuth2 访问令牌，WebDAV 用 URL + 账号密码。
- **Connect Shared Folder...**：**Windows / Samba 共享文件夹**，填入 UNC 路径（如 `\\server\share` 或 `//server/share`）。Windows 对 SMB 协议版本（SMB1/CIFS、SMB2、SMB3）的协商完全透明，应用层无需选择版本即可兼容任意服务器支持的 SMB 方言；若需认证会用 `WNetAddConnection2` 以提供的用户名密码临时建立连接。
- **Connect SFTP/FTP...**：**SFTP**（基于项目已内置的原生 libssh2，与 SSH 终端共用同一套库，无需外部 sftp.exe，支持密码或私钥文件认证）与 **FTP**（纯 `QTcpSocket` 实现 RFC 959 控制通道 + PASV 被动模式数据通道，Qt6 已移除 QFtp，故自实现，不依赖外部 ftp.exe）。

每种连接的浏览器标签页功能完全一致：
- **浏览**：树状列出文件夹与文件（名称/大小/修改时间），双击进入子文件夹，支持后退/上级/刷新。
- **上传**：工具栏「Upload Files...」多选文件上传到当前目录；「Upload Folder...」整个本地文件夹**递归**上传（自动在远端建同名子目录）；「New Folder...」新建远端文件夹。百度网盘走官方 xpan 三步流程（precreate → superfile2 → create）。
- **右键菜单**：
  - **文本文件** → 内部编辑器打开（含编码自动识别，避免中文乱码，各类源代码均可编辑）。
  - **图片** → 内部图片查看/编辑器；**无论图片过大或过小都会在打开时自动缩放到可视范围**，支持手动放大/缩小。
  - **视频/音频** → 「Stream（边下边播）」；共享文件夹/SFTP/FTP 的本地/原生路径可直接播放，百度网盘等需特殊请求头的场景通过本机 `StreamProxy` 中转；无法流式时自动回退为先下载再播放。
  - **其它类型文件** → 「Open With...」选择外部程序打开，或「Open（系统默认程序）」；未知扩展名的文件会先下载后按字节内容嗅探是否为文本。
  - **Download...** / **Download Folder...** → 下载到指定本地路径（会在 Transfers 面板中跟踪进度）。
  - **Rename...** → 重命名文件或文件夹。
  - **Delete** → 删除前弹出确认框（文件夹会提示"及其全部内容"），确认后递归删除并刷新。
- **传输面板（Transfers）**：仅当当前标签页是浏览页时自动显示的可停靠面板，列出所有上传/下载任务（类型/名称/连接名/大小/进度条/速度/状态），右键菜单支持取消、重试、移除、打开、打开所在文件夹、复制名称/本地路径/远端路径、清除已完成。
- **已保存连接统一管理**：无论哪种类型，连接后都会按 `后端名-用户命名` 的格式（如 `BaiduPan-dx`、`SharedFolder-nas`、`SFTP-server1`）列在 **Connect** 菜单的三个入口下方，每条均可 **打开 / 重新连接 / 编辑... / 删除**（编辑会按原后端类型自动重新打开匹配的对话框）。
- 所有连接标签页同样跟随统一主题，界面全程多语言。

### 6. 图片编辑器
File → Open as Image... 可将任意图片打开为完整图片编辑器标签页：
- **自动适配**：打开时**根据当前窗口自动缩放到可视范围**——大图按比例缩小，小图按比例放大。工具栏 + / − / Fit / 100% 随时手动改变缩放。
- 工具：画笔、橡皮擦、直线、矩形、椭圆、箭头、文字、裁剪、马赛克、魔棒
- 调整：亮度、对比度、饱和度、色温、曝光、阴影、高光、锐化（滑块实时预览）
- 滤镜：灰度、复古、反色、暖色、冷色、Vintage、高对比、模糊
- 几何：左转 / 右转 / 自由旋转 / 拉直 / 水平或垂直翻转 / 裁剪 / 改尺寸 / 缩放 / DPI / 扩画布
- 水印：文字水印、图片水印
- 导出：Export As / Compress & Export（可选质量 1-100）

### 7. 共用框架
- 统一标签页：编辑器 / SSH / 串口 / 各类比较 / 云盘可任意混排
- 统一菜单栏、工具栏、状态栏
- 关闭时统一提示保存、统一断开所有连接
- 跨平台：Windows 与 Linux（及 macOS）均可编译运行
- **全程多语言**：所有菜单、子菜单、对话框、About、比较工具栏均支持 English / 简体中文 / 繁體中文 / Español / Français / Deutsch / Русский / 日本語 / 한국어，切换即时生效并持久化
- **统一主题**：编辑器、SSH、串口、比较标签页及整个窗口（含菜单/状态栏）跟随同一主题，内置 VS Code Dark+/Light+/High Contrast、GitHub Dark、Solarized Dark 等；SSH 密码以 Windows DPAPI 加密保存
- **启动优化**：
  - 翻译表**懒加载**：英文启动时完全不构建翻译表；切换到其他语言时才一次性加载。
  - QApplication 构造前显式设置高 DPI 缩放策略，避免首帧多余的布局重排。
  - 非关键初始化（剪贴板历史监听、后续可扩展的后台任务）通过 `QTimer::singleShot(0)` 推迟到主窗口 `show()` 之后执行，不阻塞首帧绘制。

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

> **一句话总览**：装 MSYS2 → 用 `pacman` 装工具链和依赖 → 运行 `build-msys2.sh`。
> 下面是「跑 `build-msys2.sh` 之前」需要做的全部环境准备。

### 1. 安装 MSYS2
1. 从 https://www.msys2.org 下载安装器（`msys2-x86_64-*.exe`），默认安装到 `C:\msys64`。
2. 从开始菜单打开 **"MSYS2 MinGW 64-bit"** 终端（蓝色图标，**不要**用 UCRT64 / CLANG64 / MSYS 那几个）。
3. 先做一次系统更新（更新完若提示关闭终端，关掉后重新打开再执行第二条）：
   ```bash
   pacman -Syu          # 更新核心，若要求关闭终端就照做
   pacman -Su           # 重新打开终端后再更新剩余包
   ```

### 2. 安装编译工具链与依赖（跑脚本前必须完成）
在 **MSYS2 MinGW 64-bit** 终端里执行（一条命令装全）：
```bash
pacman -S --needed \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-qt6-base \
    mingw-w64-x86_64-qt6-serialport \
    mingw-w64-x86_64-libssh2 \
    mingw-w64-x86_64-zlib \
    mingw-w64-x86_64-zstd
# 可选：若还想用方式 B 的 CMake/Ninja，另装：
pacman -S --needed mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
# 可选：让生成的 exe 带文件图标（需系统 Python + Pillow）
pacman -S --needed mingw-w64-x86_64-python-pillow
```

| 包 | 提供 | 必需 |
|----|------|------|
| `mingw-w64-x86_64-gcc` | g++ / windres / objdump 等编译链 | ✅ |
| `mingw-w64-x86_64-qt6-base` | Qt6 Core/Gui/Widgets/Network/PrintSupport + moc/rcc | ✅ |
| `mingw-w64-x86_64-qt6-serialport` | 串口 / UART 模块 | ✅ |
| `mingw-w64-x86_64-libssh2` | 原生 SSH（含 libssl/libcrypto 依赖） | ✅ |
| `mingw-w64-x86_64-zlib` / `-zstd` | Qt / 工具链运行时压缩库 | ✅ |
| `mingw-w64-x86_64-cmake` / `-ninja` | 仅方式 B（CMake 构建）需要 | 可选 |
| `mingw-w64-x86_64-python-pillow` | 把 `icon.png` 转成 `icon.ico`（exe 文件图标） | 可选 |

> DPAPI 加密（SSH 密码）用的 `crypt32` 随 MinGW 自带，无需额外安装。

### 3. 验证环境（可选但推荐）
在 **MSYS2 MinGW 64-bit** 终端确认关键工具就位（都应打印出版本/路径）：
```bash
which g++ windres objdump                         # 编译链
/mingw64/share/qt6/bin/moc.exe --version          # Qt moc
/mingw64/share/qt6/bin/rcc.exe --version          # Qt rcc
ls /mingw64/include/qt6/QtSerialPort >/dev/null && echo "SerialPort OK"
ls /mingw64/lib/libssh2* >/dev/null && echo "libssh2 OK"
```

### 4. 一键编译 + 打包
环境就绪后，运行脚本（脚本内部已自动 `export PATH=/mingw64/bin:...`，所以**用哪个 MSYS2 子终端都行**，甚至直接从 Windows CMD 调用）：
```bash
# 在 MSYS2 终端里：
bash /c/dx/myaiprojects/terminal++/TerminalPP/build-msys2.sh
```
或直接从 Windows CMD / PowerShell：
```powershell
C:\msys64\usr\bin\bash.exe -lc "bash /c/dx/myaiprojects/terminal++/TerminalPP/build-msys2.sh"
```
脚本会依次执行：`[1/4] moc` → `[2/4] rcc` → `[2b] 图标` → `[3/4] 编译+链接` → `[4/4] 打包到 dist/`，
最后打印 `DONE. Portable build in: .../dist/TerminalPP.exe (34 DLLs bundled)`。

完成后产物在 **`TerminalPP\dist\`**：
- `TerminalPP.exe`（已内嵌窗口图标 + exe 文件图标）
- 约 34 个运行时 DLL（Qt6 六模块、libssh2、libssl/libcrypto、icu、harfbuzz、freetype、gcc 运行时等）
- `platforms\qwindows.dll` + `styles\` + `imageformats\` 插件

**双击 `TerminalPP\dist\TerminalPP.exe` 即可运行，无需安装 Qt。**

### 5. 常见问题
- **`moc: command not found` / `g++: command not found`**：没在 MinGW 64-bit 环境或工具链没装全。重开 "MSYS2 MinGW 64-bit" 终端并重跑第 2 步。
- **`cannot find -lssh2` / `libssh2.h: No such file`**：未装 `mingw-w64-x86_64-libssh2`，补装后重试。
- **`Qt6SerialPort` 相关链接错误**：未装 `mingw-w64-x86_64-qt6-serialport`。
- **`rm: cannot remove '.../dist': Device or resource busy`**：上一次运行的 `TerminalPP.exe` 仍开着，先关掉程序（或 `taskkill /IM TerminalPP.exe /F`）再重跑；该提示不影响编译本身。
- **exe 没有文件图标**：未装 Pillow（`icon.ico` 跳过），窗口/任务栏图标仍通过 qrc 生效；装 `mingw-w64-x86_64-python-pillow` 后重跑即可。
- **路径**：把示例中的 `/c/dx/myaiprojects/terminal++/TerminalPP` 换成你本机的实际路径（MSYS2 下 `C:\` 对应 `/c/`）。

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
若使用官方 Qt（含 MinGW Kit）而非 MSYS2，用 CMake 构建：
```powershell
cd TerminalPP
cmake -S . -B build -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="C:\Qt\6.6.0\mingw_64;<libssh2 安装根目录>"
cmake --build build -j
windeployqt --release build\TerminalPP.exe   ::（可选）打包 Qt DLL/插件
```

### 方式 D：Linux / macOS
```bash
# Ubuntu: sudo apt install build-essential cmake qt6-base-dev qt6-serialport-dev libssh2-1-dev pkg-config
cd TerminalPP
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/TerminalPP
```
> 或用 qmake：`qmake6 && make -j`。

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
| `Ctrl +` / `Ctrl -` / `Ctrl 0` | 放大 / 缩小 / 重置字号（编辑器/图片比较/SSH 与串口终端字体均响应） |
| `Ctrl+Shift+S` | 全部保存 |
| `Ctrl+Shift+R` | 重连当前 SSH 会话 |
| `Ctrl+Shift+C` / `Ctrl+Shift+V` | 终端复制 / 粘贴 |
| `Ctrl+Shift+V`（编辑器中） | 打开剪贴板历史 |
| `Ctrl+Shift+K` | 新建比较 |
| 双击标签栏空白处 | 新建文件标签页 |
| 右键单击标签 | 关闭当前/关闭其他/关闭向左/关闭向右 |

---

## 七、源码结构

```
TerminalPP/
├── CMakeLists.txt          # CMake 构建
├── TerminalPP.pro          # qmake 构建（备选）
├── build-msys2.sh          # MSYS2 / MinGW-w64 一键编译 + 打包脚本（推荐）
├── resources/              # qrc 资源
└── src/
    ├── main.cpp                        # 入口：HiDPI 设置、Fusion 风格、I18n 初始化、MainWindow 构造、延迟剪贴板监听
    ├── mainwindow.{h,cpp}              # 主窗口：菜单/工具栏/状态栏/标签页/侧边栏
    ├── core/                           # ===== 公共设施 =====
    │   ├── i18n.{h,cpp}                # 9 语言运行时翻译表（**惰性加载**：英文启动完全跳过）
    │   ├── cliphistory.{h,cpp}         # 剪贴板历史监听与持久化
    │   ├── cliphistorydialog.{h,cpp}   # 剪贴板历史选择对话框
    │   ├── fileassoc.{h,cpp}           # Windows 文件关联（Open with / 默认程序）
    │   └── secretstore.{h,cpp}         # SSH 密码加密（Windows DPAPI / XOR 回退）
    ├── cloud/                          # ===== 云盘浏览 =====
    │   ├── cloudprovider.h             # 云盘后端抽象接口（list/download/upload/remove/rename/stream...）
    │   ├── clouddrivestore.{h,cpp}     # 已连接云盘的持久化（令牌加密存储）
    │   ├── restproviders.{h,cpp}       # OneDrive/GoogleDrive/Dropbox/百度/WebDAV 具体实现
    │   ├── cloudconnectdialog.{h,cpp}  # 连接对话框（选服务商+令牌/凭据+代理）
    │   ├── cloudbrowsertab.{h,cpp}     # 树状浏览 + 右键打开方式/流式/上传/下载/重命名/删除
    │   ├── streamproxy.{h,cpp}         # 本地中转，为边下边播注入必需请求头
    │   ├── transfermanager.{h,cpp}     # 全局传输任务模型（进度/取消/重试/状态）
    │   ├── transferpanel.{h,cpp}       # 传输面板 UI（仅浏览标签页时显示）
    │   ├── smbprovider.{h,cpp}         # Windows/Samba 共享文件夹（UNC 路径）
    │   ├── sftpprovider.{h,cpp}        # SFTP（原生 libssh2，独立工作线程）
    │   └── ftpprovider.{h,cpp}         # FTP（QTcpSocket 实现 RFC959 + PASV）
    ├── compare/                        # ===== 文件比较 =====
    │   ├── diffengine.{h,cpp}          # Myers O(ND) 行差异 + 字符级内联差异
    │   ├── textcomparetab.{h,cpp}      # 文本双栏比较视图
    │   ├── foldercomparetab.{h,cpp}    # 递归文件夹比较 + 互拷
    │   ├── hexcomparetab.{h,cpp}       # 十六进制比较
    │   ├── imagecomparetab.{h,cpp}     # 图片比较 + 差异叠加；打开时自动缩放（大图缩小/小图放大）到可视范围
    │   └── comparedialog.{h,cpp}       # 新建比较启动对话框
    ├── editor/                         # ===== 代码/十六进制/图片编辑器 =====
    │   ├── codeeditor.{h,cpp}          # 行号 + 当前行高亮 + 缩进 + 缩放 + 列/块编辑
    │   ├── hexeditortab.{h,cpp}        # 十六进制查看器/编辑器（可编辑字节 + 保存）
    │   ├── imageeditortab.{h,cpp}      # 图片查看/编辑器；打开时自动缩放到可视范围
    │   ├── syntaxhighlighter.{h,cpp}   # 多语言语法高亮
    │   ├── finddialog.{h,cpp}          # 查找 / 替换
    │   ├── gotodialog.{h,cpp}          # 跳转到行
    │   ├── editortab.{h,cpp}           # 文件读写 / 编码自动识别 / 行尾 / 主题
    │   └── thememanager.{h,cpp}        # 配色主题
    └── terminal/                       # ===== SSH + 串口 + 本地 shell 终端 =====
        ├── ansiparser.{h,cpp}          # VT100/xterm-256 ANSI 引擎（SSH + 串口共用）
        ├── terminalwidget.{h,cpp}      # 终端渲染 + 键盘 + IME + 选择复制
        ├── sshsession.{h,cpp}          # 原生 libssh2 SSH 客户端（工作线程）+ tmux
        ├── sshtab.{h,cpp}              # SSH 标签页
        ├── sessiondialog.{h,cpp}       # SSH 会话配置对话框
        ├── sessionmanager.{h,cpp}      # sessions.json 持久化
        ├── serialsession.{h,cpp}       # QSerialPort 封装
        ├── serialtab.{h,cpp}           # 串口标签页
        ├── serialdialog.{h,cpp}        # 串口配置对话框
        └── localshelltab.{h,cpp}       # 本地 CMD / PowerShell 终端
```

---

## 八、设计说明

- **统一终端引擎**：`AnsiParser` 维护屏幕缓冲 + 滚动回看，`TerminalWidget` 负责绘制与输入，SSH 和串口只是不同的数据源（`SshSession` / `SerialSession`），因此两边都拥有完整的 VT100 渲染、256 色、中文 IME、复制粘贴能力。
- **SSH 原生自包含**：`SshSession` 在独立工作线程里用 libssh2 完成 TCP 连接、SSH 握手、密码/公钥认证、请求 PTY(xterm-256color) 并开 shell（或 `tmux new-session -A`），再把收发字节通过队列信号与 GUI 线程的 `TerminalWidget` 交换。不调用任何外部 ssh / plink 程序，Windows(ws2_32) 与 Linux(BSD socket) 均可编译运行。
- **编辑器与终端解耦**：编辑器标签页与终端标签页共存于同一 `QTabWidget`，菜单项根据当前标签页类型智能响应。
- **图片打开即适配**：所有图片视图（图片编辑器、图片比较、云盘浏览）在 `loadFile()` 完成后立即调用 `zoomFit()`：计算视口与图片尺寸的最小比例并应用。关键是**同时支持向上和向下缩放**——小图不再卡在 1:1，而是会被放大到可视范围；大图依然按比例缩小以适配窗口；工具栏 `+ / − / Fit / 100%` 随时手动覆盖。
- **启动时序**：
  - `main()` 在构造 QApplication 之前就设置 HiDPI 缩放策略，避免首次布局时的策略变更导致重排。
  - `I18n::initialize()` 只读取语言偏好并填充语言码表；**完整翻译表只有在当前语言不是英文时才构建**；切换到其他语言时通过 `ensureTableLoaded()` 延迟加载（且只加载一次）。
  - 剪贴板监听、后续可扩展的后台任务通过 `QTimer::singleShot(0, ...)` 在主窗口 `show()` 之后执行，不阻塞首帧绘制。