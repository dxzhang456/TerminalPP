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
- **Multi-language UI (9 languages)** — English / 简体中文 / 繁體中文 / Español / Français / Deutsch / Русский / 日本語 / 한국어. First launch defaults to English; switch via the **UI Language** menu. Choice is persisted.
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
- **多语言界面（9 种语言）** — English / 简体中文 / 繁體中文 / Español / Français / Deutsch / Русский / 日本語 / 한국어。首次启动默认英文，可通过 **UI Language** 菜单随时切换，选择持久化保存。
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
- **多語言介面（9 種語言）** — English / 简体中文 / 繁體中文 / Español / Français / Deutsch / Русский / 日本語 / 한국어。第一次啟動預設英文，可透過 **UI Language** 選單切換，選擇會持久化。
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
- **Interfaz multilingüe (9 idiomas)** — inglés / chino simplificado / chino tradicional / español / francés / alemán / ruso / japonés / coreano. El primer inicio es en inglés; cambie desde el menú **UI Language**. La elección se guarda.
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
- **Interface multilingue (9 langues)** — anglais / chinois simplifié / chinois traditionnel / espagnol / français / allemand / russe / japonais / coréen. Le premier lancement est en anglais ; changez via le menu **UI Language**. Le choix est enregistré.
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
- **Mehrsprachige Oberfläche (9 Sprachen)** — Englisch / Vereinfachtes Chinesisch / Traditionelles Chinesisch / Spanisch / Französisch / Deutsch / Russisch / Japanisch / Koreanisch. Erststart ist Englisch; Wechsel über das Menü **UI Language**. Die Wahl wird gespeichert.
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
- **Многоязычный интерфейс (9 языков)** — английский / упрощённый китайский / традиционный китайский / испанский / французский / немецкий / русский / японский / корейский. Первый запуск — на английском; переключение через меню **UI Language**. Выбор сохраняется.
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
- **多言語 UI（9 言語）** — 英語 / 簡体字中国語 / 繁體字中国語 / スペイン語 / フランス語 / ドイツ語 / ロシア語 / 日本語 / 韓国語。初回は英語で起動し、**UI Language** メニューから切り替え可能。選択は保存されます。
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
- **다국어 UI(9개 언어)** — 영어 / 简体中文 / 繁體中文 / Español / Français / Deutsch / Русский / 日本語 / 한국어. 처음에는 영어로 실행되며 **UI Language** 메뉴에서 전환 가능. 선택 사항은 저장됩니다.
- **빠른 시작** — 번역 테이블은 지연 로딩(영어 실행은 완전히 건너뜀); 중요하지 않은 작업(클립보드 리스너 등)은 메인 창이 그려진 뒤로 미룹니다.

자세한 빌드 안내는 [README.zh-CN.md](README.zh-CN.md) 를 참조하세요.