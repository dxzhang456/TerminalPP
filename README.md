# Terminal++

**Languages / 语言 / 语种:**
[English](#english) · [简体中文](#简体中文) · [繁體中文](#繁體中文) · [Español](#español) · [Français](#français) · [Deutsch](#deutsch) · [Русский](#русский) · [日本語](#日本語) · [한국어](#한국어)

> The full project documentation (install, build, shortcut reference, source-code tour, design notes) is kept in **[README.zh-CN.md](README.zh-CN.md)**. The sections below are concise multi-language summaries. The application UI itself is fully translated into all 9 languages — switch at any time via the **UI Language** menu.

---

## English

**Terminal++** is an all-in-one **Qt 6** application that combines, inside a single tabbed window:

- **Code editor** — multi-tab editor with syntax highlighting, themes, find/replace, encodings, column/block edit, clipboard history, auto-save, a built-in hex viewer/editor, and a tab-bar context menu.
- **SSH terminal** — native libssh2 SSH sessions with tmux support and VT100/256-color rendering; passwords encrypted with Windows DPAPI.
- **Serial / UART terminal** — configurable serial console sharing the same ANSI engine.
- **File Compare** — side-by-side text, folder, hex, and picture comparison (including recursive folder diff with copy/sync).
- **PDF viewer / editor** — a **dual-engine** PDF module. Pages are rendered by **PDFium** (the same engine Chrome/Edge use), loaded **dynamically at run time** from `pdfium.dll` with no compile-time dependency, so text, vector graphics, embedded fonts and transparency render exactly as in a browser and are re-rasterised at the current zoom to stay crisp (measured on 20 real PDFs: **20/20 opened, 232/232 pages rendered**). A copy of `pdfium.dll` is bundled into `dist/pdfium/` as the last-resort fallback; if no PDFium is found anywhere the viewer degrades gracefully to the **built-in, dependency-free engine** (Qt + zlib), which also always performs every *structural* edit. **Navigate** with First / Previous / Next / Last page buttons plus a page-number box, and **zoom** with Zoom Out / Zoom In, Actual Size, Fit Width and Fit Page. Pages are always scaled **proportionally** (the original aspect ratio is never distorted), `/Rotate` is honoured, and the **portrait / landscape layout is chosen automatically from the PDF's own page box** — a landscape slide deck lays out landscape with no user action, and the thumbnails follow each page's own orientation. The built-in reader additionally handles PDF 1.0–2.0 **compressed object streams** and cross-reference streams (the main reason some files used to refuse to open), the **FlateDecode, LZWDecode, RunLengthDecode, ASCIIHexDecode and ASCII85Decode** filters with **PNG/TIFF predictors**, and DeviceRGB / DeviceGray / DeviceCMYK / Indexed / ICCBased images at 1, 2, 4, 8 and 16 bits per component. You can annotate (highlight, underline, strikethrough, text box, sticky note, freehand pen, stamp); manage pages (merge, split, delete / insert / extract, rotate, reorder); add hand-drawn or image signatures; convert PDF ⇄ image (PNG/JPG); export PDF → Word (RTF text, layout not preserved); compress; add text / image watermarks; set an open password; and fill AcroForm text fields. Every PDF control is localized in all 9 UI languages and follows the language switch live, and the status bar names the active renderer. If a page still cannot be rasterised the viewer says so on the page and reports the exact reason instead of showing a silently blank sheet. Double-clicking a `.pdf` (or "Open with → Terminal++") opens it in the PDF view automatically; **File → Open as PDF…** can also build a PDF from selected images.
- **Cloud Drives** — Baidu Pan and WebDAV browsing, Windows/Samba shared folders, SFTP and FTP; upload/download (files and folders), rename, delete, and streaming playback, with a Transfers panel showing live progress and history.
- **Single-instance** — double-clicking a file or using Explorer's "Open with" opens it as a **new tab in the already-running window** (the window is raised/activated) instead of launching another process.
- **Images (view, edit, compare)** — in all image viewers (editor, compare, cloud browser), images are **auto-scaled to the visible range on open**: pictures that are too large are shrunk to fit, pictures that are too small are enlarged. You can still zoom in / out / fit / 1:1 from the toolbar.
- **Drag & drop (open by detected format)** — drop a text file, a source file, an image, a PDF or any binary **anywhere on the window** (the editor area, a terminal, the tab bar, the empty background) and it opens in the matching tab. The format is decided by the file's **own content** (libmagic content sniffing, *not* the extension), so text/source go to the code editor, images to the image editor, PDFs to the PDF viewer and everything else to the hex editor — a `.log` that is really a PNG still opens as an image, and an extension-less firmware blob still opens as hex. Multiple files can be dropped at once (each routed individually, with a per-type summary such as “2 text, 1 image, 1 PDF” on the status bar), already-open files are just focused, and dropping exactly **two folders** starts a folder comparison.
- **Multi-language UI (9 languages)** — English / 简体中文 / 繁體中文 / Español / Français / Deutsch / Русский / 日本語 / 한국어. First launch defaults to English; switch via the **UI Language** menu. Choice is persisted.
- **PDF text selection & copy** — on every page whose text really *is* text (a PDF text layer, not pixels inside a picture), the text can be **selected with the mouse and copied**: PDFium's text layer is loaded with a bounding box per character, so dragging across the page highlights the text, **double-click** picks a word, **Ctrl+A** selects the page and **Ctrl+C** copies the selection with its line breaks preserved. The same is available from the page's **right-click menu** and the **Copy Text / Select All Text / Copy Page Text** toolbar buttons, while **Copy All Text** takes the whole document; the status line always reports how many selectable characters the page has, or states plainly that the page is image-only so a scan is never mistaken for a copy failure.
- **PDF editing is WYSIWYG** — stamp, merge, rotate left/right, delete / insert / extract page, signatures, watermarks and form filling are now re-rendered immediately: the edited document is handed back to PDFium, so the page, the thumbnails and the file you save always agree.
- **PDF compression that really compresses** — every stream still stored uncompressed is Flate-compressed (losslessly), the result is compared with the **real file size on disk**, and the status bar reports the true saving — or says honestly that the document was already fully compressed instead of printing a meaningless 0%.
- **Password-protected PDFs** — opening an encrypted PDF now asks for the password (up to three attempts) instead of failing with nowhere to type it; such a document opens for viewing and copying, and the status bar states that editing is disabled.
- **Terminal copy / paste and CJK width** — **Ctrl+V** (plus Ctrl+Shift+V, Shift+Insert, middle-click and right-click) now pastes into SSH and serial sessions, with CRLF normalised to CR so pasted commands do not produce blank lines, and Chinese / Japanese / Korean / fullwidth / emoji glyphs are drawn two columns wide so they no longer overlap.
- **Encoding auto-detection** — opening a text file detects its encoding (BOM → UTF-8/16/32, otherwise a UTF-8 validity check that falls back to the local ANSI codepage, e.g. GB18030 on a Chinese Windows) and **ticks the matching entry in the Encoding menu**, so the menu doubles as the detection read-out. Tabs without a text encoding (images, PDFs, terminals) disable the menu entirely.
- **Fast startup** — the translation table is loaded lazily (English launches skip it entirely), non-critical work (clipboard listener, etc.) is deferred until after the main window is painted.

**Build:** Windows (MSYS2 / MinGW-w64, recommended) · Linux · macOS. See [README.zh-CN.md](README.zh-CN.md) for the complete step-by-step build instructions; the one-click script `build-msys2.sh` produces a portable `dist\TerminalPP.exe` with all DLLs.

---

## 简体中文

**Terminal++** 是一款用 **Qt 6** 开发的一体化终端与编辑器工具，在同一个带标签页的窗口里集成：

- **代码编辑器** — 多标签、语法高亮、主题、查找/替换、编码、列/块编辑、剪贴板历史、自动保存、内置十六进制查看/编辑器、标签右键菜单。
- **SSH 终端** — 基于 libssh2 的原生 SSH，自带 tmux、VT100/256 色；密码以 Windows DPAPI 加密。
- **串口 / UART 终端** — 可配置串口控制台，与 SSH 共用同一 ANSI 渲染引擎。
- **文件比较** — 文本、文件夹、十六进制、图片的并排比较（含递归文件夹差异与互拷）。
- **PDF 查看 / 编辑** — **双引擎** PDF 模块。页面由 **PDFium**（与 Chrome / Edge 同款引擎）渲染，运行时从 `pdfium.dll` **动态加载**、无任何编译期依赖，因此文字、矢量图形、嵌入字体与透明度都能像浏览器一样完整呈现，并在当前缩放级别重新栅格化以保持清晰（实测 20 个真实 PDF：**20/20 全部打开、232/232 页全部渲染成功**）。构建脚本会把一份 `pdfium.dll` 打包进 `dist/pdfium/` 作为最终兜底；若机器上完全找不到 PDFium，则优雅降级到**内置零依赖引擎**（仅 Qt + zlib）——所有*结构性*编辑也始终由它完成。**页面导航**：首页 / 上一页 / 下一页 / 末页按钮，并带页码输入框；**缩放**：缩小 / 放大、实际大小、适合宽度、适合页面。页面始终**按比例缩放**（绝不拉伸变形），遵循 `/Rotate`，并且**根据 PDF 自身页面框自动判定纵向 / 横向**——横版幻灯片无需任何操作即以横向呈现，左侧缩略图也遵循各页自己的方向。内置阅读器还支持 PDF 1.0–2.0 的**压缩对象流**与交叉引用流（这正是过去部分 PDF 打不开的主要原因）、**FlateDecode、LZWDecode、RunLengthDecode、ASCIIHexDecode、ASCII85Decode** 过滤器及 **PNG/TIFF 预测器**，以及 DeviceRGB / DeviceGray / DeviceCMYK / Indexed / ICCBased 图像（每分量 1、2、4、8、16 位）。另支持批注（高亮、下划线、删除线、文本框、便签、画笔、图章）；页面管理（合并、拆分、删除 / 插入 / 提取、旋转、调整页序）；手绘签名或图片签名；PDF ⇄ 图片（PNG/JPG）转换；PDF → Word（RTF 文本，有排版损耗）；压缩；文字 / 图片水印；设置打开密码；填写 AcroForm 表单。所有 PDF 控件均已本地化为 9 种界面语言并随语言切换实时更新，状态栏会明确显示当前使用的渲染器。若某页仍无法栅格化，页面上会直接给出说明并报告具体原因，而不是静默显示空白页。双击 `.pdf`（或“打开方式 → Terminal++”）会自动以 PDF 格式打开；**文件 → 以 PDF 打开…** 也可把选中的多张图片合成为 PDF。
- **云盘 / 共享 / SFTP / FTP** — 百度网盘、WebDAV、Windows/Samba 共享、SFTP、FTP，支持上传/下载（含文件夹）、重命名、删除、边下边播，统一的传输面板显示实时进度与历史。
- **单实例** — 双击文件或使用资源管理器“打开方式”时，会在**已打开的窗口中新增一个标签页**打开该文件（并把窗口置顶激活），而不是再启动一个新进程。
- **图片查看、编辑、比较** — 所有图片视图（编辑器、比较、云盘浏览）在打开时都会**自动缩放到可视范围**：过大的图片缩小到适配窗口，过小的图片放大到清晰可见；工具栏仍提供 放大 / 缩小 / 适应 / 1:1。
- **拖放打开（按识别出的格式）** — 把文本文件、源代码、图片、PDF 或任意二进制文件拖到**窗口的任意位置**（编辑区、终端、标签条、空白背景），就会在对应的标签页中打开。格式根据文件**自身的内容**判定（libmagic 内容嗅探，**而不是**扩展名），所以文本 / 源代码进代码编辑器，图片进图片编辑器，PDF 进 PDF 阅读器，其余全部进十六进制编辑器——一个实际是 PNG 的 `.log` 仍会以图片打开，没有扩展名的固件仍会以十六进制打开。可以一次拖入多个文件（逐个分派，并在状态条给出“2 个文本、1 个图片、1 个 PDF”这样的分类汇总）；已经打开的文件只会被聚焦；恰好拖入**两个文件夹**则直接开始文件夹比较。
- **多语言界面（9 种语言）** — English / 简体中文 / 繁體中文 / Español / Français / Deutsch / Русский / 日本語 / 한국어。首次启动默认英文，可通过 **UI Language** 菜单随时切换，选择持久化保存。
- **PDF 文字选取与复制** — 只要页面上的文字**真的是文字**（PDF 文字图层，而不是图片里的像素），就可以**用鼠标选中并复制**：程序会加载 PDFium 的文字图层（每个字符都带自己的边界框），因此在页面上拖动即可高亮选中，**双击**选中一个词，**Ctrl+A** 全选本页，**Ctrl+C** 复制所选内容并保留换行。页面**右键菜单**以及工具栏的**复制文字 / 全选文字 / 复制本页文字**按钮效果相同，**复制全文**则拷走整份文档；底部状态行始终显示本页有多少个可选字符，或直接说明本页是纯图片——所以扫描件不会被误认为复制失败。
- **PDF 编辑所见即所得** — 添加图章、合并、向左 / 向右旋转、删除 / 插入 / 提取页面、签名、水印、填写表单后都会立即重新渲染：编辑后的文档会交还给 PDFium，因此页面、缩略图与保存出的文件始终一致。
- **真正能压缩的 PDF 压缩** — 所有仍以未压缩方式存储的流都会被（无损地）Flate 压缩，结果与**磁盘上的真实文件大小**比较，状态栏给出真实的压缩比；若文档本来就已完全压缩，则如实说明，而不是输出毫无意义的 0%。
- **带密码的 PDF** — 打开加密 PDF 时会**弹出密码输入框**（最多三次机会），而不再是无处输入密码、直接打开失败；此类文档可查看与复制，状态栏会说明编辑功能已禁用。
- **终端复制 / 粘贴与汉字宽度** — 现在 **Ctrl+V**（以及 Ctrl+Shift+V、Shift+Insert、中键、右键）可以向 SSH 与串口会话粘贴文本，并把 CRLF 归一化为 CR，避免粘贴命令后多出空行；中日韩 / 全角 / emoji 字符按两列宽度绘制，不再互相重叠。
- **编码自动识别** — 打开文本文件会自动探测其编码（有 BOM 则识别 UTF-8/16/32，否则校验是否为合法 UTF-8，并在不是时回退到系统 ANSI 代码页，例如中文 Windows 上的 GB18030），并在**编码菜单中自动勾选对应项**，菜单因此可直接作为识别结果的参考；图片、PDF、终端等没有文本编码的标签页会整体禁用该菜单。
- **启动更快** — 翻译表采用懒加载（英文启动完全跳过），非关键初始化（剪贴板监听等）延迟到窗口显示后执行。

**完整的安装、编译、快捷键、源码结构与设计说明见 [README.zh-CN.md](README.zh-CN.md)**。Windows 推荐使用 MSYS2 / MinGW-w64，运行 `build-msys2.sh` 即可得到免安装、可直接拷走的 `dist\TerminalPP.exe`（含全部运行时 DLL）。

---

## 繁體中文

**Terminal++** 是以 **Qt 6** 開發的一體化終端與編輯器工具，在同一個分頁視窗中整合：

- **程式碼編輯器** — 多分頁、語法高亮、佈景主題、尋找/取代、編碼、欄/區塊編輯、剪貼簿歷史、自動儲存、內建十六進位檢視/編輯器、分頁右鍵選單。
- **SSH 終端** — 基於 libssh2 的原生 SSH，內建 tmux、VT100/256 色；密碼以 Windows DPAPI 加密。
- **序列 / UART 終端** — 可設定的序列主控台，與 SSH 共用同一 ANSI 引擎。
- **檔案比較** — 文字、資料夾、十六進位、圖片並排比較（含遞迴資料夾差異與互拷）。
- **雲端 / 共用 / SFTP / FTP** — 百度網盤、WebDAV、Windows/Samba 共用、SFTP、FTP；上傳/下載（含資料夾）、重新命名、刪除、串流播放，統一傳輸面板顯示即時進度與歷史。
- **圖片檢視、編輯、比較** — 所有圖片檢視（編輯器、比較、雲端瀏覽）開啟時會**自動縮放到可視範圍**：過大的圖片縮小以適配視窗，過小的圖片放大到可清楚觀察；工具列仍可 放大 / 縮小 / 適應 / 1:1。
- **拖放開啟（依偵測出的格式）** — 將文字檔案、原始程式碼、圖片、PDF 或任何二進位檔案拖到**視窗的任何位置**，就會在對應的標籤頁中開啟。格式依檔案**本身的內容**偵測（libmagic 內容嗅探，**而非**擴展名），所以文字 / 原始程式碼進程式碼編輯器，圖片進圖片編輯器，PDF 進 PDF 閱讀器，其餘全部進十六進位編輯器。可以一次拖入多個檔案（逐個分派，並在狀態列給出分類彙總）；恰好拖入**兩個資料夾**則直接開始資料夾比較。
- **多語言介面（9 種語言）** — English / 简体中文 / 繁體中文 / Español / Français / Deutsch / Русский / 日本語 / 한국어。第一次啟動預設英文，可透過 **UI Language** 選單切換，選擇會持久化。
- **PDF 文字選取與複製** — 只要頁面上的文字**真的是文字**（PDF 文字圖層，而非圖片裡的像素），就可以**用滑鼠選取並複製**：拖曳即可高亮選取，**雙擊**選取一個詞，**Ctrl+A** 全選本頁，**Ctrl+C** 複製並保留換行；右鍵選單與工具列的**複製文字 / 全選文字 / 複製本頁文字**按鈕效果相同，**複製全文**則取得整份文件，狀態列會顯示本頁可選字元數，或說明本頁為純圖片。
- **PDF 編輯所見即所得** — 圖章、合併、左 / 右旋轉、刪除 / 插入 / 擷取頁面、簽名、水印、填寫表單後立即重新算繪，頁面、縮圖與儲存結果永遠一致。
- **真正能壓縮的 PDF 壓縮** — 未壓縮的串流會被無損 Flate 壓縮，並與磁碟上的真實檔案大小比較後回報真實壓縮比；若本來已完全壓縮則如實說明。
- **帶密碼的 PDF** — 開啟加密 PDF 會彈出密碼輸入框（最多三次），可檢視與複製，狀態列會說明編輯已停用。
- **終端複製 / 貼上與漢字寬度** — **Ctrl+V**（以及 Ctrl+Shift+V、Shift+Insert、中鍵、右鍵）可向 SSH 與序列工作階段貼上文字，CRLF 會正規化為 CR；中日韓 / 全角 / emoji 字元以兩欄寬度繪製，不再重疊。
- **編碼自動識別** — 開啟文字檔會自動偵測編碼並在**編碼選單中自動勾選**對應項目；圖片、PDF、終端等沒有文字編碼的分頁會整體停用該選單。
- **啟動更快** — 翻譯表採惰性載入（英文啟動完全跳過），非關鍵初始化（剪貼簿監聽等）延遲到主視窗顯示後執行。

完整的安裝、編譯、快捷鍵、原始碼結構請見 **[README.zh-CN.md](README.zh-CN.md)**。

---

## Español

**Terminal++** es una aplicación **Qt 6** todo-en-uno que combina, dentro de una misma ventana con pestañas:

- **Editor de código** — pestañas múltiples, resaltado de sintaxis, temas, buscar/reemplazar, codificaciones, edición columna/bloque, historial del portapapeles, autoguardado, visor/editor hexadecimal integrado y menú contextual de pestañas.
- **Terminal SSH** — sesiones SSH nativas con libssh2, soporte tmux y renderizado VT100/256 colores; contraseñas cifradas con Windows DPAPI.
- **Terminal serie / UART** — consola serie configurable que comparte el mismo motor ANSI.
- **Comparación de archivos** — comparación lado a lado de texto, carpetas, hex e imágenes (con comparación recursiva de carpetas y copia entre lados).
- **Unidades en la nube / Carpetas compartidas / SFTP / FTP** — Baidu Pan, WebDAV, carpetas compartidas Windows/Samba, SFTP y FTP; subida/descarga (archivos y carpetas), renombrar, eliminar, reproducción en streaming, con un panel de Transferencias que muestra progreso e historial en vivo.
- **Imágenes (ver, editar, comparar)** — en todas las vistas de imagen, las imágenes se **ajustan automáticamente al área visible al abrirlas**: las imágenes grandes se reducen para caber, las pequeñas se amplían para verse cómodamente; la barra de herramientas sigue ofreciendo acercar / alejar / ajustar / 1:1.
- **Arrastrar y soltar (abre según el formato detectado)** — suelte un archivo de texto, un archivo fuente, una imagen, un PDF o cualquier binario **en cualquier parte de la ventana** y se abrirá en la pestaña correspondiente. El formato se decide por el **propio contenido** del archivo (análisis con libmagic, *no* la extensión): el texto y el código van al editor, las imágenes al editor de imágenes, los PDF al visor de PDF y todo lo demás al editor hexadecimal. Se pueden soltar varios archivos a la vez (cada uno se enruta por separado, con un resumen por tipo en la barra de estado) y soltar exactamente **dos carpetas** inicia una comparación de carpetas.
- **Interfaz multilingüe (9 idiomas)** — inglés / chino simplificado / chino tradicional / español / francés / alemán / ruso / japonés / coreano. El primer inicio es en inglés; cambie desde el menú **UI Language**. La elección se guarda.
- **Selección y copia del texto del PDF** — en toda página cuyo texto *sea realmente* texto (una capa de texto del PDF, no píxeles de una imagen), el texto se puede **seleccionar con el ratón y copiar**: arrastrar resalta, **doble clic** elige una palabra, **Ctrl+A** selecciona la página y **Ctrl+C** copia conservando los saltos de línea; lo mismo está en el menú contextual y en los botones **Copiar texto / Seleccionar todo el texto / Copiar el texto de la página**.
- **La edición de PDF es WYSIWYG** — sello, combinar, rotar, eliminar / insertar / extraer páginas, firmas, marcas de agua y formularios se vuelven a representar de inmediato.
- **Compresión de PDF real** — los flujos sin comprimir se comprimen con Flate sin pérdida y el ahorro se mide contra el tamaño real del archivo.
- **PDF protegidos con contraseña** — ahora se pide la contraseña (hasta tres intentos); el documento se abre para ver y copiar.
- **Copiar / pegar en el terminal y ancho CJK** — **Ctrl+V** pega en sesiones SSH y serie, y los glifos CJK / de ancho completo / emoji se dibujan con dos columnas, sin solaparse.
- **Detección automática de codificación** — al abrir un archivo de texto se detecta su codificación y se **marca la entrada correspondiente en el menú Codificación**; para imágenes, PDF y terminales el menú se deshabilita.
- **Inicio rápido** — la tabla de traducción se carga de forma perezosa (los inicios en inglés la omiten por completo); el trabajo no crítico (listener del portapapeles, etc.) se difiere hasta después de pintar la ventana principal.

Las instrucciones completas de compilación y referencia están en [README.zh-CN.md](README.zh-CN.md).

---

## Français

**Terminal++** est une application **Qt 6** tout-en-un qui combine, dans une seule fenêtre à onglets :

- **Éditeur de code** — multi-onglets, coloration syntaxique, thèmes, rechercher/remplacer, encodages, édition colonne/bloc, historique du presse-papiers, enregistrement auto, visualiseur/éditeur hexadécimal intégré et menu contextuel d'onglets.
- **Terminal SSH** — sessions SSH natives via libssh2, prise en charge tmux et rendu VT100/256 couleurs ; mots de passe chiffrés avec Windows DPAPI.
- **Terminal série / UART** — console série configurable partageant le même moteur ANSI.
- **Comparaison de fichiers** — comparaison côte à côte de texte, dossiers, hexadécimal et images (avec comparaison récursive de dossiers et copie bidirectionnelle).
- **Cloud / Dossiers partagés / SFTP / FTP** — Baidu Pan, WebDAV, dossiers partagés Windows/Samba, SFTP et FTP ; téléversement/téléchargement (fichiers et dossiers), renommage, suppression, lecture en streaming, avec un panneau de transferts affichant la progression et l'historique en direct.
- **Images (voir, éditer, comparer)** — dans toutes les vues d'image, les images sont **automatiquement mises à l'échelle à la zone visible à l'ouverture** : les grandes images sont réduites pour entrer, les petites sont agrandies pour être confortablement visibles ; la barre d'outils propose toujours zoom avant / arrière / ajuster / 1:1.
- **Glisser-déposer (ouverture selon le format détecté)** — déposez un fichier texte, un fichier source, une image, un PDF ou n'importe quel binaire **n'importe où sur la fenêtre** et il s'ouvre dans l'onglet correspondant. Le format est déterminé par le **contenu du fichier lui-même** (analyse libmagic, *pas* l'extension) : le texte et le code vont à l'éditeur, les images à l'éditeur d'images, les PDF à la visionneuse PDF et tout le reste à l'éditeur hexadécimal. Plusieurs fichiers peuvent être déposés en une fois (chacun orienté individuellement, avec un résumé par type dans la barre d'état) et déposer exactement **deux dossiers** lance une comparaison de dossiers.
- **Interface multilingue (9 langues)** — anglais / chinois simplifié / chinois traditionnel / espagnol / français / allemand / russe / japonais / coréen. Le premier lancement est en anglais ; changez via le menu **UI Language**. Le choix est enregistré.
- **Sélection et copie du texte PDF** — sur toute page dont le texte *est réellement* du texte (une couche de texte PDF, pas des pixels), il peut être **sélectionné à la souris et copié** : glisser met en surbrillance, **double-clic** choisit un mot, **Ctrl+A** sélectionne la page et **Ctrl+C** copie en conservant les retours à la ligne ; idem via le menu contextuel et les boutons **Copier le texte / Tout sélectionner / Copier le texte de la page**.
- **L'édition PDF est WYSIWYG** — tampon, fusion, rotation, suppression / insertion / extraction de pages, signatures, filigranes et formulaires sont réaffichés immédiatement.
- **Une compression PDF qui compresse vraiment** — les flux non compressés sont compressés en Flate sans perte et le gain est mesuré par rapport à la taille réelle du fichier.
- **PDF protégés par mot de passe** — le mot de passe est désormais demandé (trois tentatives) ; le document s'ouvre en lecture et copie.
- **Copier / coller dans le terminal et largeur CJK** — **Ctrl+V** colle dans les sessions SSH et série, et les glyphes CJK / pleine largeur / emoji occupent deux colonnes, sans se chevaucher.
- **Détection automatique de l'encodage** — l'encodage d'un fichier texte est détecté et **l'entrée correspondante du menu Encodage est cochée** ; pour les images, les PDF et les terminaux le menu est désactivé.
- **Démarrage rapide** — la table de traduction est chargée paresseusement (les lancements en anglais la sautent entièrement) ; les tâches non critiques (écouteur du presse-papiers, etc.) sont reportées après le premier affichage.

Les instructions complètes de compilation sont dans [README.zh-CN.md](README.zh-CN.md).

---

## Deutsch

**Terminal++** ist eine einheitliche **Qt-6**-Anwendung, die in einem einzigen Fenster mit Tabs vereint:

- **Code-Editor** — Multi-Tab, Syntaxhervorhebung, Designs, Suchen/Ersetzen, Kodierungen, Spalten-/Blockbearbeitung, Zwischenablage-Verlauf, Autospeichern, integrierter Hex-Viewer/-Editor und Tab-Kontextmenü.
- **SSH-Terminal** — native SSH-Sitzungen mit libssh2, tmux-Unterstützung und VT100/256-Farben; Passwörter mit Windows DPAPI verschlüsselt.
- **Serielles / UART-Terminal** — konfigurierbare serielle Konsole mit derselben ANSI-Engine.
- **Dateivergleich** — Text-, Ordner-, Hex- und Bildvergleich nebeneinander (einschließlich rekursivem Ordnervergleich und Kopieren zwischen Seiten).
- **Cloud / Freigegebene Ordner / SFTP / FTP** — Baidu Pan, WebDAV, Windows/Samba-Freigaben, SFTP und FTP; Hoch-/Herunterladen (Dateien und Ordner), Umbenennen, Löschen, Streaming-Wiedergabe, mit einem Übertragungspanel für Live-Fortschritt und Verlauf.
- **Bilder (anzeigen, bearbeiten, vergleichen)** — in allen Bildansichten werden Bilder **beim Öffnen automatisch auf den sichtbaren Bereich skaliert**: zu große Bilder werden verkleinert, zu kleine vergrößert; die Werkzeugleiste bietet weiterhin Vergrößern / Verkleinern / Anpassen / 1:1.
- **Drag & Drop (öffnet nach erkanntem Format)** — legen Sie eine Textdatei, eine Quelldatei, ein Bild, ein PDF oder eine beliebige Binärdatei **irgendwo auf dem Fenster** ab, und sie wird im passenden Tab geöffnet. Das Format wird aus dem **Inhalt der Datei selbst** bestimmt (libmagic-Analyse, *nicht* der Dateiendung): Text und Quellcode gehen in den Editor, Bilder in den Bildeditor, PDFs in den PDF-Betrachter und alles andere in den Hex-Editor. Mehrere Dateien können gleichzeitig abgelegt werden (jede einzeln zugeordnet, mit einer Zusammenfassung pro Typ in der Statusleiste), und das Ablegen von genau **zwei Ordnern** startet einen Ordnervergleich.
- **Mehrsprachige Oberfläche (9 Sprachen)** — Englisch / Vereinfachtes Chinesisch / Traditionelles Chinesisch / Spanisch / Französisch / Deutsch / Russisch / Japanisch / Koreanisch. Erststart ist Englisch; Wechsel über das Menü **UI Language**. Die Wahl wird gespeichert.
- **PDF-Textauswahl und -Kopie** — auf jeder Seite, deren Text *wirklich* Text ist (eine PDF-Textebene, keine Pixel), lässt sich der Text **mit der Maus markieren und kopieren**: Ziehen markiert, **Doppelklick** wählt ein Wort, **Ctrl+A** die Seite und **Ctrl+C** kopiert samt Zeilenumbrüchen; dasselbe bieten das Kontextmenü und die Schaltflächen **Text kopieren / Gesamten Text auswählen / Seitentext kopieren**.
- **PDF-Bearbeitung ist WYSIWYG** — Stempel, Zusammenführen, Drehen, Seiten löschen / einfügen / extrahieren, Signaturen, Wasserzeichen und Formulare werden sofort neu gerendert.
- **Komprimierung, die wirklich komprimiert** — unkomprimierte Streams werden verlustfrei Flate-komprimiert und die Ersparnis wird an der echten Dateigröße gemessen.
- **Kennwortgeschützte PDFs** — das Kennwort wird nun abgefragt (drei Versuche); das Dokument öffnet sich zum Ansehen und Kopieren.
- **Kopieren / Einfügen im Terminal und CJK-Breite** — **Ctrl+V** fügt in SSH- und Seriell-Sitzungen ein, und CJK-/Vollbreiten-/Emoji-Glyphen werden zwei Spalten breit gezeichnet und überlappen nicht mehr.
- **Automatische Kodierungserkennung** — die Kodierung einer Textdatei wird erkannt und **der passende Eintrag im Kodierungsmenü angehakt**; für Bilder, PDFs und Terminals ist das Menü deaktiviert.
- **Schneller Start** — die Übersetzungstabelle wird faul geladen (englische Starts überspringen sie komplett); unkritische Arbeit (Zwischenablage-Listener etc.) wird hinter das erste Zeichnen des Hauptfensters verschoben.

Vollständige Build-Anweisungen siehe [README.zh-CN.md](README.zh-CN.md).

---

## Русский

**Terminal++** — это единое приложение на **Qt 6**, объединяющее в одном окне с вкладками:

- **Редактор кода** — многовкладочный редактор с подсветкой синтаксиса, темами, поиском/заменой, кодировками, редактированием столбца/блока, историей буфера обмена, автосохранением, встроенным hex-просмотрщиком/редактором и контекстным меню вкладок.
- **SSH-терминал** — нативные SSH-сеансы на libssh2 с поддержкой tmux и рендерингом VT100/256 цветов; пароли шифруются Windows DPAPI.
- **Serial / UART-терминал** — настраиваемая последовательная консоль с тем же движком ANSI.
- **Сравнение файлов** — параллельное сравнение текста, папок, hex и изображений (с рекурсивным сравнением папок и копированием между сторонами).
- **Облако / Общие папки / SFTP / FTP** — Baidu Pan, WebDAV, Windows/Samba-ресурсы, SFTP и FTP; загрузка/скачивание (файлы и папки), переименование, удаление, потоковое воспроизведение, с панелью передач, показывающей прогресс и историю в реальном времени.
- **Изображения (просмотр, редактирование, сравнение)** — во всех режимах просмотра изображений картинки **автоматически масштабируются до видимой области при открытии**: слишком большие изображения уменьшаются, слишком маленькие увеличиваются; панель инструментов по-прежнему предоставляет увеличение / уменьшение / вместить / 1:1.
- **Перетаскивание (открытие по распознанному формату)** — перетащите текстовый файл, исходный код, изображение, PDF или любой бинарный файл **в любое место окна** — и он откроется в соответствующей вкладке. Формат определяется по **содержимому самого файла** (анализ libmagic, *а не* расширение): текст и код — в редактор, изображения — в редактор изображений, PDF — в просмотрщик PDF, всё остальное — в HEX-редактор. Можно перетащить сразу несколько файлов (каждый обрабатывается отдельно, с сводкой по типам в строке состояния), а перетаскивание ровно **двух папок** запускает сравнение папок.
- **Многоязычный интерфейс (9 языков)** — английский / упрощённый китайский / традиционный китайский / испанский / французский / немецкий / русский / японский / корейский. Первый запуск — на английском; переключение через меню **UI Language**. Выбор сохраняется.
- **Выделение и копирование текста PDF** — на любой странице, текст которой — действительно текст, его можно **выделить мышью и скопировать**: **двойной щелчок** выделяет слово, **Ctrl+A** — страницу, **Ctrl+C** копирует с сохранением переносов строк; то же доступно в контекстном меню и на панели инструментов.
- **Редактирование PDF — WYSIWYG** — штамп, объединение, поворот, удаление / вставка / извлечение страниц, подписи, водяные знаки и формы сразу перерисовываются.
- **Сжатие, которое действительно сжимает** — несжатые потоки сжимаются Flate без потерь, а выигрыш измеряется относительно реального размера файла.
- **PDF с паролем** — теперь запрашивается пароль (три попытки); документ открывается для просмотра и копирования.
- **Копирование / вставка в терминале и ширина CJK** — **Ctrl+V** вставляет в SSH- и Serial-сеансы, а CJK-/полноширинные/emoji глифы рисуются в два столбца и больше не перекрываются.
- **Автоопределение кодировки** — кодировка текстового файла определяется, и **соответствующий пункт меню «Кодировка» отмечается**; для изображений, PDF и терминалов меню отключено.
- **Быстрый запуск** — таблица переводов загружается лениво (запуски на английском полностью её пропускают); некритичная работа (слушатель буфера обмена и т. д.) откладывается до появления главного окна.

Полные инструкции по сборке см. в [README.zh-CN.md](README.zh-CN.md).

---

## 日本語

**Terminal++** は **Qt 6** で作られたオールインワンアプリで、一つのタブ付きウィンドウに以下を統合します:

- **コードエディタ** — マルチタブ、構文強調、テーマ、検索/置換、エンコーディング、列/ブロック編集、クリップボード履歴、自動保存、内蔵 Hex ビューア/エディタ、タブ右クリックメニュー。
- **SSH 端末** — libssh2 ベースのネイティブ SSH、tmux 対応、VT100/256 色描画。パスワードは Windows DPAPI で暗号化。
- **シリアル / UART 端末** — 設定可能なシリアルコンソール、SSH と同じ ANSI エンジンを共有。
- **ファイル比較** — テキスト・フォルダ・Hex・画像の並列比較（再帰的なフォルダ比較と相互コピーを含む）。
- **クラウド / 共有フォルダ / SFTP / FTP** — Baidu Pan、WebDAV、Windows/Samba 共有、SFTP、FTP。アップロード/ダウンロード（ファイル・フォルダ）、名前変更、削除、ストリーミング再生に対応し、進捗と履歴をリアルタイム表示する転送パネルを備える。
- **画像（表示・編集・比較）** — すべての画像ビューで、開いた瞬間に**画像を可視範囲に自動的にスケーリング**します: 大きすぎる画像は縮小してウィンドウに収め、小さすぎる画像は十分見える大きさまで拡大。ツールバーから拡大 / 縮小 / 全体表示 / 1:1 も可能。
- **ドラッグ＆ドロップ（検出した形式で開く）** — テキストファイル、ソースファイル、画像、PDF、任意のバイナリを**ウィンドウのどこにドロップしても**、対応するタブで開きます。形式は拡張子ではなく**ファイル自身の内容**（libmagic による判定）から決まるので、テキスト / ソースはエディタ、画像は画像エディタ、PDF は PDF ビューアー、その他は 16 進エディタへ振り分けられます。複数ファイルを一度にドロップでき（それぞれ別々に振り分け、種類ごとの集計をステータスバーに表示）、フォルダーをちょうど**2 つ**ドロップするとフォルダー比較が始まります。
- **多言語 UI（9 言語）** — 英語 / 簡体字中国語 / 繁體字中国語 / スペイン語 / フランス語 / ドイツ語 / ロシア語 / 日本語 / 韓国語。初回は英語で起動し、**UI Language** メニューから切り替え可能。選択は保存されます。
- **PDF のテキスト選択とコピー** — 文字が*本物のテキスト*（画像のピクセルではなく PDF のテキストレイヤー）であるパージでは、**マウスで選択してコピー**できます。ドラッグでハイライト、**ダブルクリック**で単語選択、**Ctrl+A** でパージ全選択、**Ctrl+C** で改行を保ったままコピーでき、右クリックメニューとツールバーボタンでも同じことができます。
- **PDF 編集は WYSIWYG** — スタンプ、結合、左 / 右回転、パージの削除 / 挿入 / 抽出、署名、透かし、フォーム入力がただちに再描画されます。
- **本当に小さくなる圧縮** — 未圧縮のストリームを可逆な Flate で圧縮し、実ファイルサイズと比較して報告します。
- **パスワード付き PDF** — パスワード入力を求めるようになり（最大 3 回）、閲覧とコピーが可能です。
- **端末のコピー / 貼り付けと CJK 幅** — **Ctrl+V** で SSH・シリアルセッションに貼り付けでき、CJK・全角・絵文字は 2 列幅で描画され重ならなくなりました。
- **エンコードの自動検出** — テキストファイルのエンコードを検出し、**エンコードメニューの該当項目にチェック**を付けます。画像・PDF・端末のタブではメニュー自体が無効になります。
- **高速起動** — 翻訳テーブルは遅延ロード（英語起動では完全にスキップ）、重要でない処理（クリップボードリスナーなど）はメインウィンドウ描画後に後回し。

詳細なビルド手順は [README.zh-CN.md](README.zh-CN.md) を参照してください。

---

## 한국어

**Terminal++**는 **Qt 6** 로 만든 올인원 애플리케이션으로, 하나의 탭 창 안에 다음을 결합합니다:

- **코드 편집기** — 멀티탭, 구문 강조, 테마, 찾기/바꾸기, 인코딩, 열/블록 편집, 클립보드 기록, 자동 저장, 내장 Hex 뷰어/편집기, 탭 우클릭 메뉴.
- **SSH 터미널** — libssh2 기반 네이티브 SSH, tmux 지원, VT100/256색 렌더링; 비밀번호는 Windows DPAPI 로 암호화.
- **시리얼 / UART 터미널** — 동일한 ANSI 엔진을 공유하는 구성 가능한 시리얼 콘솔.
- **파일 비교** — 텍스트, 폴더, 16진수, 사진의 나란히 비교(재귀 폴더 비교 및 양쪽 복사 포함).
- **클라우드 / 공유 폴더 / SFTP / FTP** — Baidu Pan, WebDAV, Windows/Samba 공유, SFTP, FTP; 업로드/다운로드(파일 및 폴더), 이름 바꾸기, 삭제, 스트리밍 재생을 지원하며 실시간 진행률과 기록을 보여주는 전송 패널을 제공합니다.
- **이미지(보기·편집·비교)** — 모든 이미지 뷰에서 열 때 **이미지를 보이는 영역에 맞춰 자동 조정**합니다: 너무 큰 이미지는 창에 맞게 축소되고, 너무 작은 이미지는 편안히 보이도록 확대됩니다; 툴바로 확대 / 축소 / 맞춤 / 1:1 도 가능.
- **드래게 앤 드롭(검지된 형식으로 열기)** — 텍스트 파일, 소스 파일, 이미지, PDF 또는 임의의 바이너리를 **창의 아무 위치에** 드롭하면 해당 탭에서 열립니다. 형식은 확장자가 아니라 **파일 자신의 내용**(libmagic 분석)으로 결정되며, 텍스트·소스는 에디터, 이미지는 이미지 에디터, PDF는 PDF 뷰어, 그 외는 16진수 에디터로 열립니다. 여러 파일을 한 번에 드롭할 수 있으며(개별 처리 및 유형별 요약을 상태 표시줄에 표시), 폴더를 정확히 **도 개** 드롭하면 폴더 바교가 시작됩니다.
- **다국어 UI(9개 언어)** — 영어 / 简体中文 / 繁體中文 / Español / Français / Deutsch / Русский / 日本語 / 한국어. 처음에는 영어로 실행되며 **UI Language** 메뉴에서 전환 가능. 선택 사항은 저장됩니다.
- **PDF 텍스트 여기 및 복사** — 문자가 *진짜 텍스트*(이미지 픽셀이 아닌 PDF 텍스트 레이어)인 페이지에서는 텍스트를 **마우스로 여기하여 복사**할 수 있습니다. 드래게하면 강조 표시되고 **더분 클릭**은 단어를, **Ctrl+A**는 페이지를 여기하며 **Ctrl+C**는 줄바꿔을 유지하여 복사합니다.
- **PDF 편집은 WYSIWYG** — 스탬프, 별합, 회전, 페이지 삭제 / 삽입 / 추출, 서명, 워타마크, 양식 작업이 즉시 다시 렌더링됩니다.
- **상짜로 줄여지는 압술** — 복원 가능한 Flate 압술을 사용하고 상짜 파일 크기와 바교하여 보고합니다.
- **보호 번호가 있는 PDF** — 보호 번호를 묻으며(최대 3회) 보기와 복사가 가능합니다.
- **토밍널 복사 / 붙이기와 CJK 폭** — **Ctrl+V**로 SSH 및 시리얼 세션에 붙여넣을 수 있고, CJK / 전자 / 이모지 문자는 두 열 폭으로 그려져 겹치지 않습니다.
- **인코딩 자동 검지** — 텍스트 파일의 인코딩을 검지하여 **인코딩 메뉴의 해당 항목에 체킬**합니다. 이미지 / PDF / 토밍널 탭에서는 메뉴가 뱄화됩니다.
- **빠른 시작** — 번역 테이블은 지연 로딩(영어 실행은 완전히 건너뜀); 중요하지 않은 작업(클립보드 리스너 등)은 메인 창이 그려진 뒤로 미룹니다.

자세한 빌드 안내는 [README.zh-CN.md](README.zh-CN.md) 를 참조하세요.