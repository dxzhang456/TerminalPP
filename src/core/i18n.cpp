#include "i18n.h"

#include <QSettings>
#include <QLocale>

// ---------------------------------------------------------------------------
// The translation table is embedded directly in C++ so the application is fully
// self-contained (no external .qm / lrelease dependency). Each supported
// language maps an English source string to its localized form. Missing entries
// gracefully fall back to the English source.
// ---------------------------------------------------------------------------

I18n *I18n::instance()
{
    static I18n inst;
    return &inst;
}

I18n::I18n(QObject *parent) : QObject(parent)
{
}

// Lightweight bootstrap — only installs the language code / display-name
// metadata and reads the persisted preference. The (large) translation table
// itself is loaded lazily the first time a non-English translation is actually
// requested, so launching in English (the default for first-run users) stays
// as fast as possible.
void I18n::initialize()
{
    // Language codes and their self-localized display names (needed for the
    // UI Language menu, which is built up front).
    // Order shown in the UI Language menu: Simplified Chinese first, then
    // Traditional Chinese, then English, then the rest.
    m_codes = {
        QStringLiteral("zh_CN"), QStringLiteral("zh_TW"), QStringLiteral("en"),
        QStringLiteral("es"),    QStringLiteral("fr"),    QStringLiteral("de"),
        QStringLiteral("ru"),    QStringLiteral("ja"),    QStringLiteral("ko"),
    };
    m_displayNames = {
        {QStringLiteral("en"),    QStringLiteral("English")},
        {QStringLiteral("zh_CN"), QStringLiteral("简体中文")},
        {QStringLiteral("zh_TW"), QStringLiteral("繁體中文")},
        {QStringLiteral("es"),    QStringLiteral("Español")},
        {QStringLiteral("fr"),    QStringLiteral("Français")},
        {QStringLiteral("de"),    QStringLiteral("Deutsch")},
        {QStringLiteral("ru"),    QStringLiteral("Русский")},
        {QStringLiteral("ja"),    QStringLiteral("日本語")},
        {QStringLiteral("ko"),    QStringLiteral("한국어")},
    };

    // Restore the persisted language, defaulting to English on first launch.
    QSettings s;
    QString saved = s.value(QStringLiteral("ui/language"), QString()).toString();
    if (saved.isEmpty() || !m_codes.contains(saved))
        saved = QStringLiteral("en");
    m_current = saved;

    // Only pay the cost of building the full translation table when the user
    // has actually chosen a non-English language. English launches skip this
    // entirely for a faster startup.
    if (m_current != QStringLiteral("en"))
        ensureTableLoaded();
}

QStringList I18n::languageCodes() const
{
    return m_codes;
}

QString I18n::languageDisplayName(const QString &code) const
{
    return m_displayNames.value(code, code);
}

void I18n::setLanguage(const QString &code)
{
    if (!m_codes.contains(code) || code == m_current)
        return;
    m_current = code;
    QSettings s;
    s.setValue(QStringLiteral("ui/language"), code);
    // Make sure the translation table is populated before any widget
    // re-translates itself on the languageChanged() signal.
    if (m_current != QStringLiteral("en"))
        ensureTableLoaded();
    emit languageChanged(code);
}

QString I18n::tr(const QString &source) const
{
    if (m_current == QStringLiteral("en"))
        return source;
    const auto it = m_table.constFind(m_current);
    if (it == m_table.constEnd())
        return source;
    return it->value(source, source);
}

void I18n::ensureTableLoaded()
{
    if (m_tableLoaded)
        return;
    m_tableLoaded = true;
    loadTable();
}

void I18n::loadTable()
{
    // NOTE: m_codes / m_displayNames are already populated in initialize() so
    // the UI Language menu can be built without paying for the full table.
    // Here we only populate the (source -> translated) rows.

    // Helper to register a row of translations for one English source string.
    auto add = [this](const char *en,
                      const char *zhCN, const char *zhTW, const char *es,
                      const char *fr, const char *de, const char *ru,
                      const char *ja, const char *ko) {
        const QString key = QString::fromUtf8(en);
        m_table[QStringLiteral("zh_CN")][key] = QString::fromUtf8(zhCN);
        m_table[QStringLiteral("zh_TW")][key] = QString::fromUtf8(zhTW);
        m_table[QStringLiteral("es")][key]    = QString::fromUtf8(es);
        m_table[QStringLiteral("fr")][key]    = QString::fromUtf8(fr);
        m_table[QStringLiteral("de")][key]    = QString::fromUtf8(de);
        m_table[QStringLiteral("ru")][key]    = QString::fromUtf8(ru);
        m_table[QStringLiteral("ja")][key]    = QString::fromUtf8(ja);
        m_table[QStringLiteral("ko")][key]    = QString::fromUtf8(ko);
    };

    //   English              zh_CN        zh_TW        es              fr               de               ru                 ja              ko
    // ---- Menus ----
    add("&File",             "文件(&F)",  "檔案(&F)",  "&Archivo",     "&Fichier",      "&Datei",        "&Файл",           "ファイル(&F)",  "파일(&F)");
    add("&Edit",             "编辑(&E)",  "編輯(&E)",  "&Editar",      "&Édition",      "&Bearbeiten",   "&Правка",         "編集(&E)",      "편집(&E)");
    add("&View",             "视图(&V)",  "檢視(&V)",  "&Ver",         "&Affichage",    "&Ansicht",      "&Вид",            "表示(&V)",      "보기(&V)");
    add("&Language",         "语言(&L)",  "語言(&L)",  "&Idioma",      "&Langue",       "&Sprache",      "&Язык",           "言語(&L)",      "언어(&L)");
    add("E&ncoding",         "编码(&N)",  "編碼(&N)",  "&Codificación","&Encodage",     "&Kodierung",    "&Кодировка",      "エンコード(&N)","인코딩(&N)");
    add("&Terminal",         "终端(&T)",  "終端(&T)",  "&Terminal",    "&Terminal",     "&Terminal",     "&Терминал",       "端末(&T)",      "터미널(&T)");
    add("&Help",             "帮助(&H)",  "說明(&H)",  "A&yuda",       "&Aide",         "&Hilfe",        "&Справка",        "ヘルプ(&H)",    "도움말(&H)");
    add("&UI Language",      "界面语言(&U)","介面語言(&U)","&Idioma de interfaz","&Langue de l'interface","&Oberflächensprache","&Язык интерфейса","UI言語(&U)","UI 언어(&U)");
    add("&Preferences",      "首选项(&P)","偏好設定(&P)","&Preferencias","&Préférences","&Einstellungen","&Настройки",      "設定(&P)",      "환경 설정(&P)");

    // ---- File menu ----
    add("&New",              "新建(&N)",  "新增(&N)",  "&Nuevo",       "&Nouveau",      "&Neu",          "&Создать",        "新規(&N)",      "새로 만들기(&N)");
    add("&Open...",          "打开(&O)...","開啟(&O)...","&Abrir...",   "&Ouvrir...",    "&Öffnen...",    "&Открыть...",     "開く(&O)...",   "열기(&O)...");
    add("Open as &Hex...",   "以十六进制打开(&H)...","以十六進位開啟(&H)...","Abrir como &hex...","Ouvrir en &hex...","Als &Hex öffnen...","Открыть как &hex...","&Hexで開く...","&Hex로 열기...");
    add("Open as Hex",       "以十六进制打开","以十六進位開啟","Abrir como hex","Ouvrir en hex","Als Hex öffnen","Открыть как hex","Hexで開く","Hex로 열기");
    add("Open &Recent",      "打开最近(&R)","開啟最近(&R)","Abrir &reciente","Récemment &ouverts","&Zuletzt geöffnet","Открыть &недавние","最近開いた(&R)","최근 &열기");
    add("(no recent files)", "(无最近文件)","(無最近檔案)","(sin archivos recientes)","(aucun fichier récent)","(keine zuletzt geöffneten Dateien)","(нет недавних файлов)","(最近のファイルなし)","(최근 파일 없음)");
    add("Clear Recent Files","清除最近文件","清除最近檔案","Borrar archivos recientes","Effacer les fichiers récents","Zuletzt geöffnete löschen","Очистить недавние файлы","最近のファイルをクリア","최근 파일 지우기");
    add("Hex: %1",           "十六进制：%1","十六進位：%1","Hex: %1",   "Hex : %1",      "Hex: %1",       "Hex: %1",         "Hex: %1",       "Hex: %1");
    add("&Save",             "保存(&S)",  "儲存(&S)",  "&Guardar",     "&Enregistrer",  "&Speichern",    "&Сохранить",      "保存(&S)",      "저장(&S)");
    add("Save &As...",       "另存为(&A)...","另存為(&A)...","Guardar &como...","Enregistrer &sous...","Speichern &unter...","Сохранить &как...","名前を付けて保存(&A)...","다른 이름으로 저장(&A)...");
    add("Save A&ll",         "全部保存(&L)","全部儲存(&L)","Guardar &todo","&Tout enregistrer","&Alle speichern","Сохранить &всё","すべて保存(&L)","모두 저장(&L)");
    add("&Print...",         "打印(&P)...","列印(&P)...","&Imprimir...","&Imprimer...", "&Drucken...",   "&Печать...",      "印刷(&P)...",   "인쇄(&P)...");
    add("&Close Tab",        "关闭标签(&C)","關閉分頁(&C)","&Cerrar pestaña","&Fermer l'onglet","Tab &schließen","&Закрыть вкладку","タブを閉じる(&C)","탭 닫기(&C)");
    add("Close",             "关闭",      "關閉",      "Cerrar",       "Fermer",        "Schließen",     "Закрыть",         "閉じる",        "닫기");
    add("Close Others",      "关闭其他",  "關閉其他",  "Cerrar otras", "Fermer les autres","Andere schließen","Закрыть другие","他を閉じる","다른 탭 닫기");
    add("Close to the Right","关闭右侧",  "關閉右側",  "Cerrar a la derecha","Fermer à droite","Rechts schließen","Закрыть справа","右側を閉じる","오른쪽 닫기");
    add("Close to the Left", "关闭左侧",  "關閉左側",  "Cerrar a la izquierda","Fermer à gauche","Links schließen","Закрыть слева","左側を閉じる","왼쪽 닫기");
    add("E&xit",             "退出(&X)",  "結束(&X)",  "&Salir",       "&Quitter",      "&Beenden",      "&Выход",          "終了(&X)",      "종료(&X)");

    // ---- Edit menu ----
    add("&Undo",             "撤销(&U)",  "復原(&U)",  "&Deshacer",    "&Annuler",      "&Rückgängig",   "&Отменить",       "元に戻す(&U)",  "실행 취소(&U)");
    add("&Redo",             "重做(&R)",  "重做(&R)",  "&Rehacer",     "&Rétablir",     "&Wiederholen",  "&Повторить",      "やり直し(&R)",  "다시 실행(&R)");
    add("Cu&t",              "剪切(&T)",  "剪下(&T)",  "Cor&tar",      "Co&uper",       "&Ausschneiden", "&Вырезать",       "切り取り(&T)",  "잘라내기(&T)");
    add("&Copy",             "复制(&C)",  "複製(&C)",  "&Copiar",      "&Copier",       "&Kopieren",     "&Копировать",     "コピー(&C)",    "복사(&C)");
    add("&Paste",            "粘贴(&P)",  "貼上(&P)",  "&Pegar",       "C&oller",       "&Einfügen",     "&Вставить",       "貼り付け(&P)",  "붙여넣기(&P)");
    add("Select &All",       "全选(&A)",  "全選(&A)",  "Seleccionar &todo","Tout &sélectionner","Alles &auswählen","&Выделить всё","すべて選択(&A)","모두 선택(&A)");
    add("&Find...",          "查找(&F)...","尋找(&F)...","&Buscar...",  "&Rechercher...","&Suchen...",    "&Найти...",       "検索(&F)...",   "찾기(&F)...");
    add("&Replace...",       "替换(&R)...","取代(&R)...","&Reemplazar...","&Remplacer...","&Ersetzen...", "&Заменить...",    "置換(&R)...",   "바꾸기(&R)...");
    add("&Go To Line...",    "跳转到行(&G)...","跳至行(&G)...","&Ir a la línea...","&Aller à la ligne...","&Gehe zu Zeile...","&Перейти к строке...","行へ移動(&G)...","줄로 이동(&G)...");
    add("Clipboard &History","剪贴板历史(&H)","剪貼簿歷史(&H)","&Historial del portapapeles","&Historique du presse-papiers","Zwischenablage-&Verlauf","&История буфера обмена","クリップボード履歴(&H)","클립보드 기록(&H)");
    add("Column / Block Edit (Alt+Drag)","列/块编辑 (Alt+拖动)","欄/區塊編輯 (Alt+拖曳)","Edición de columna/bloque (Alt+Arrastrar)","Édition colonne/bloc (Alt+Glisser)","Spalten-/Blockbearbeitung (Alt+Ziehen)","Редактирование столбца/блока (Alt+перетаскивание)","列/ブロック編集 (Alt+ドラッグ)","열/블록 편집 (Alt+드래그)");
    add("Column / Block Edit (Alt+Drag or Ctrl+Shift+Up/Down)",
        "列/块编辑（Alt+拖动 或 Ctrl+Shift+上/下）",
        "欄/區塊編輯（Alt+拖曳 或 Ctrl+Shift+上/下）",
        "Edición de columna/bloque (Alt+Arrastrar o Ctrl+Shift+Arriba/Abajo)",
        "Édition colonne/bloc (Alt+Glisser ou Ctrl+Maj+Haut/Bas)",
        "Spalten-/Blockbearbeitung (Alt+Ziehen oder Strg+Umschalt+Hoch/Runter)",
        "Редактирование столбца/блока (Alt+перетаскивание или Ctrl+Shift+Вверх/Вниз)",
        "列/ブロック編集（Alt+ドラッグ または Ctrl+Shift+上/下）",
        "열/블록 편집(Alt+드래그 또는 Ctrl+Shift+위/아래)");

    // ---- View menu ----
    add("Show &Line Numbers","显示行号(&L)","顯示行號(&L)","Mostrar números de &línea","Afficher les numéros de &ligne","&Zeilennummern anzeigen","Показать &номера строк","行番号を表示(&L)","줄 번호 표시(&L)");
    add("&Word Wrap",        "自动换行(&W)","自動換行(&W)","&Ajuste de línea","&Retour à la ligne","&Zeilenumbruch","&Перенос слов","右端で折り返す(&W)","자동 줄 바꿈(&W)");
    add("Zoom &In",          "放大(&I)",  "放大(&I)",  "&Acercar",     "&Zoom avant",   "Ver&größern",   "&Увеличить",      "拡大(&I)",      "확대(&I)");
    add("Zoom &Out",         "缩小(&O)",  "縮小(&O)",  "A&lejar",      "Zoom a&rrière", "Ver&kleinern",  "У&меньшить",      "縮小(&O)",      "축소(&O)");
    add("&Reset Zoom",       "重置缩放(&R)","重設縮放(&R)","&Restablecer zoom","&Réinitialiser le zoom","Zoom &zurücksetzen","&Сбросить масштаб","ズームをリセット(&R)","확대/축소 초기화(&R)");
    add("&Theme",            "主题(&T)",  "佈景主題(&T)","&Tema",       "&Thème",        "&Design",       "&Тема",           "テーマ(&T)",    "테마(&T)");
    add("&Auto Save",        "自动保存(&A)","自動儲存(&A)","&Autoguardado","&Enregistrement auto","&Automatisch speichern","&Автосохранение","自動保存(&A)","자동 저장(&A)");

    // ---- Terminal menu ----
    add("New &SSH Session...","新建SSH会话(&S)...","新增SSH工作階段(&S)...","Nueva sesión &SSH...","Nouvelle session &SSH...","Neue &SSH-Sitzung...","Новый &SSH-сеанс...","新規SSHセッション(&S)...","새 SSH 세션(&S)...");
    add("New Serial / &UART Session...","新建串口/UART会话(&U)...","新增序列/UART工作階段(&U)...","Nueva sesión serie / &UART...","Nouvelle session série / &UART...","Neue Seriell-/&UART-Sitzung...","Новый сеанс Serial / &UART...","新規シリアル/UARTセッション(&U)...","새 시리얼/UART 세션(&U)...");
    add("&Reconnect Current","重连当前(&R)","重新連線目前(&R)","&Reconectar actual","&Reconnecter l'actuel","Aktuelle &neu verbinden","&Переподключить текущий","現在の接続を再接続(&R)","현재 다시 연결(&R)");
    add("SSH / Serial &Sessions","SSH/串口会话(&S)","SSH/序列工作階段(&S)","&Sesiones SSH / Serie","&Sessions SSH / Série","SSH-/Seriell-&Sitzungen","SSH / Serial &сеансы","SSH/シリアルセッション(&S)","SSH/시리얼 세션(&S)");
    add("SSH",               "SSH",       "SSH",       "SSH",          "SSH",           "SSH",           "SSH",             "SSH",           "SSH");
    add("Serial / UART",     "串口 / UART","序列 / UART","Serie / UART", "Série / UART",  "Seriell / UART","Serial / UART",   "シリアル / UART","시리얼 / UART");
    add("+ New SSH Session...","+ 新建SSH会话...","+ 新增SSH工作階段...","+ Nueva sesión SSH...","+ Nouvelle session SSH...","+ Neue SSH-Sitzung...","+ Новый SSH-сеанс...","+ 新規SSHセッション...","+ 새 SSH 세션...");
    add("+ New Serial Session...","+ 新建串口会话...","+ 新增序列工作階段...","+ Nueva sesión serie...","+ Nouvelle session série...","+ Neue Seriell-Sitzung...","+ Новый Serial-сеанс...","+ 新規シリアルセッション...","+ 새 시리얼 세션...");
    add("(no saved SSH sessions)","(无已保存的SSH会话)","(無已儲存的SSH工作階段)","(sin sesiones SSH guardadas)","(aucune session SSH enregistrée)","(keine gespeicherten SSH-Sitzungen)","(нет сохранённых SSH-сеансов)","(保存されたSSHセッションなし)","(저장된 SSH 세션 없음)");
    add("(no saved serial sessions)","(无已保存的串口会话)","(無已儲存的序列工作階段)","(sin sesiones serie guardadas)","(aucune session série enregistrée)","(keine gespeicherten Seriell-Sitzungen)","(нет сохранённых Serial-сеансов)","(保存されたシリアルセッションなし)","(저장된 시리얼 세션 없음)");
    add("Open",              "打开",      "開啟",      "Abrir",        "Ouvrir",        "Öffnen",        "Открыть",         "開く",          "열기");
    add("Delete",            "删除",      "刪除",      "Eliminar",     "Supprimer",     "Löschen",       "Удалить",         "削除",          "삭제");

    // ---- Compare menu ----
    add("&Compare",          "比较(&C)",  "比較(&C)",  "&Comparar",    "&Comparer",     "&Vergleichen",  "&Сравнить",       "比較(&C)",      "비교(&C)");
    add("&New Comparison...","新建比较(&N)...","新增比較(&N)...","&Nueva comparación...","&Nouvelle comparaison...","&Neuer Vergleich...","&Новое сравнение...","新規比較(&N)...","새 비교(&N)...");
    add("New Comparison",    "新建比较",  "新增比較",  "Nueva comparación","Nouvelle comparaison","Neuer Vergleich","Новое сравнение","新規比較","새 비교");
    add("&Text Compare...",  "文本比较(&T)...","文字比較(&T)...","Comparar &texto...","Comparer le &texte...","&Textvergleich...","Сравнение &текста...","テキスト比較(&T)...","텍스트 비교(&T)...");
    add("&Folder Compare...","文件夹比较(&F)...","資料夾比較(&F)...","Comparar &carpetas...","Comparer les &dossiers...","&Ordnervergleich...","Сравнение &папок...","フォルダ比較(&F)...","폴더 비교(&F)...");
    add("&Hex Compare...",   "十六进制比较(&H)...","十六進位比較(&H)...","Comparación &hex...","Comparaison &hex...","&Hex-Vergleich...","&Hex-сравнение...","Hex比較(&H)...","Hex 비교(&H)...");
    add("&Picture Compare...","图片比较(&P)...","圖片比較(&P)...","Comparar &imagen...","Comparer les &images...","&Bildvergleich...","Сравнение &изображений...","画像比較(&P)...","사진 비교(&P)...");
    add("Text Compare",      "文本比较",  "文字比較",  "Comparar texto","Comparer le texte","Textvergleich","Сравнение текста","テキスト比較","텍스트 비교");
    add("Folder Compare",    "文件夹比较","資料夾比較","Comparar carpetas","Comparer les dossiers","Ordnervergleich","Сравнение папок","フォルダ比較","폴더 비교");
    add("Hex Compare",       "十六进制比较","十六進位比較","Comparación hex","Comparaison hex","Hex-Vergleich","Hex-сравнение","Hex比較","Hex 비교");
    add("Picture Compare",   "图片比较",  "圖片比較",  "Comparar imagen","Comparer les images","Bildvergleich","Сравнение изображений","画像比較","사진 비교");
    add("Type:",             "类型：",    "類型：",    "Tipo:",        "Type :",        "Typ:",          "Тип:",            "種類：",        "유형:");
    add("Left:",             "左侧：",    "左側：",    "Izquierda:",   "Gauche :",      "Links:",        "Слева:",          "左：",          "왼쪽:");
    add("Right:",            "右侧：",    "右側：",    "Derecha:",     "Droite :",      "Rechts:",       "Справа:",         "右：",          "오른쪽:");
    add("Browse...",         "浏览...",   "瀏覽...",   "Examinar...",  "Parcourir...",  "Durchsuchen...","Обзор...",        "参照...",       "찾아보기...");
    add("Select Left File",  "选择左侧文件","選擇左側檔案","Seleccionar archivo izquierdo","Sélectionner le fichier gauche","Linke Datei wählen","Выбрать левый файл","左のファイルを選択","왼쪽 파일 선택");
    add("Select Right File", "选择右侧文件","選擇右側檔案","Seleccionar archivo derecho","Sélectionner le fichier droit","Rechte Datei wählen","Выбрать правый файл","右のファイルを選択","오른쪽 파일 선택");
    add("Select Left Folder","选择左侧文件夹","選擇左側資料夾","Seleccionar carpeta izquierda","Sélectionner le dossier gauche","Linken Ordner wählen","Выбрать левую папку","左のフォルダを選択","왼쪽 폴더 선택");
    add("Select Right Folder","选择右侧文件夹","選擇右側資料夾","Seleccionar carpeta derecha","Sélectionner le dossier droit","Rechten Ordner wählen","Выбрать правую папку","右のフォルダを選択","오른쪽 폴더 선택");

    // ---- Compare tabs (toolbars / labels / stats) ----
    add("◀ Prev Diff",      "◀ 上一处差异","◀ 上一處差異","◀ Dif. ant.","◀ Diff préc.","◀ Vorige Diff","◀ Пред. различие","◀ 前の差分","◀ 이전 차이");
    add("Next Diff ▶",      "下一处差异 ▶","下一處差異 ▶","Dif. sig. ▶","Diff suiv. ▶","Nächste Diff ▶","След. различие ▶","次の差分 ▶","다음 차이 ▶");
    add("Copy →",            "复制 →",    "複製 →",    "Copiar →",     "Copier →",      "Kopieren →",    "Копировать →",    "コピー →",      "복사 →");
    add("← Copy",            "← 复制",    "← 複製",    "← Copiar",     "← Copier",      "← Kopieren",    "← Копировать",    "← コピー",      "← 복사");
    add("Swap",              "交换",      "交換",      "Intercambiar", "Échanger",      "Tauschen",      "Поменять",        "入れ替え",      "교체");
    add("Reload",            "重载",      "重新載入",  "Recargar",     "Recharger",     "Neu laden",     "Перезагрузить",   "再読み込み",    "다시 로드");
    add("Save L",            "保存左",    "儲存左",    "Guardar Izq",  "Enreg. G",      "Links speichern","Сохр. лев.",     "左を保存",      "왼쪽 저장");
    add("Save R",            "保存右",    "儲存右",    "Guardar Der",  "Enreg. D",      "Rechts speichern","Сохр. прав.",   "右を保存",      "오른쪽 저장");
    add("As Text",           "作为文本",  "作為文字",  "Como texto",   "En texte",      "Als Text",      "Как текст",       "テキストで",    "텍스트로");
    add("As Hex",            "作为十六进制","作為十六進位","Como hex",   "En hexa",       "Als Hex",       "Как hex",         "Hexで",         "Hex로");
    add("As Picture",        "作为图片",  "作為圖片",  "Como imagen",  "En image",      "Als Bild",      "Как изображение", "画像で",        "사진으로");
    add("Ignore Case",       "忽略大小写","忽略大小寫","Ignorar mayús.","Ignorer la casse","Groß/Klein ignorieren","Игнор. регистр","大文字小文字無視","대소문자 무시");
    add("Ignore Whitespace", "忽略空白",  "忽略空白",  "Ignorar espacios","Ignorer espaces","Leerraum ignorieren","Игнор. пробелы","空白を無視","공백 무시");
    add("(left)",            "（左）",    "（左）",    "(izquierda)",  "(gauche)",      "(links)",       "(слева)",         "（左）",        "(왼쪽)");
    add("(right)",           "（右）",    "（右）",    "(derecha)",    "(droite)",      "(rechts)",      "(справа)",        "（右）",        "(오른쪽)");
    add("(left folder)",     "（左侧文件夹）","（左側資料夾）","(carpeta izquierda)","(dossier gauche)","(linker Ordner)","(левая папка)","（左フォルダ）","(왼쪽 폴더)");
    add("(right folder)",    "（右侧文件夹）","（右側資料夾）","(carpeta derecha)","(dossier droit)","(rechter Ordner)","(правая папка)","（右フォルダ）","(오른쪽 폴더)");
    add("Refresh",           "刷新",      "重新整理",  "Actualizar",   "Rafraîchir",    "Aktualisieren", "Обновить",        "更新",          "새로 고침");
    add("Differences Only",  "仅差异",    "僅差異",    "Solo diferencias","Différences seules","Nur Unterschiede","Только различия","差分のみ","차이만");
    add("Name",              "名称",      "名稱",      "Nombre",       "Nom",           "Name",          "Имя",             "名前",          "이름");
    add("Size",              "大小",      "大小",      "Tamaño",       "Taille",        "Größe",         "Размер",          "サイズ",        "크기");
    add("Modified",          "修改时间",  "修改時間",  "Modificado",   "Modifié",       "Geändert",      "Изменён",         "更新日時",      "수정됨");
    add("Status",            "状态",      "狀態",      "Estado",       "État",          "Status",        "Статус",          "状態",          "상태");
    add("Same",              "相同",      "相同",      "Igual",        "Identique",     "Gleich",        "Одинаково",       "同一",          "동일");
    add("Left newer",        "左较新",    "左較新",    "Izq. más nuevo","Gauche + récent","Links neuer",  "Левее новее",     "左が新しい",    "왼쪽 최신");
    add("Right newer",       "右较新",    "右較新",    "Der. más nuevo","Droite + récent","Rechts neuer", "Правее новее",    "右が新しい",    "오른쪽 최신");
    add("Left only",         "仅左侧",    "僅左側",    "Solo izquierda","Gauche seul.",  "Nur links",     "Только слева",    "左のみ",        "왼쪽만");
    add("Right only",        "仅右侧",    "僅右側",    "Solo derecha", "Droite seul.",  "Nur rechts",    "Только справа",   "右のみ",        "오른쪽만");
    add("Different",         "不同",      "不同",      "Diferente",    "Différent",     "Unterschiedlich","Различается",    "相違",          "다름");
    add("Folder",            "文件夹",    "資料夾",    "Carpeta",      "Dossier",       "Ordner",        "Папка",           "フォルダ",      "폴더");
    add("Zoom In",           "放大",      "放大",      "Acercar",      "Zoom avant",    "Vergrößern",    "Увеличить",       "拡大",          "확대");
    add("Zoom Out",          "缩小",      "縮小",      "Alejar",       "Zoom arrière",  "Verkleinern",   "Уменьшить",       "縮小",          "축소");
    add("Fit",               "适应",      "適應",      "Ajustar",      "Ajuster",       "Anpassen",      "Вместить",        "全体表示",      "맞춤");
    add("Difference Overlay","差异叠加",  "差異疊加",  "Superposición dif.","Superpos. diff.","Differenz-Overlay","Наложение различий","差分オーバーレイ","차이 오버레이");

    // ---- Compare / find status messages ----
    add("Same: %1   Changed: %2   Inserted: %3   Removed: %4",
        "相同：%1   修改：%2   新增：%3   删除：%4",
        "相同：%1   修改：%2   新增：%3   刪除：%4",
        "Iguales: %1   Cambiadas: %2   Insertadas: %3   Eliminadas: %4",
        "Identiques : %1   Modifiées : %2   Insérées : %3   Supprimées : %4",
        "Gleich: %1   Geändert: %2   Eingefügt: %3   Entfernt: %4",
        "Одинаково: %1   Изменено: %2   Добавлено: %3   Удалено: %4",
        "同一: %1   変更: %2   挿入: %3   削除: %4",
        "동일: %1   변경: %2   삽입: %3   제거: %4");
    add("Same: %1   Different: %2   Left only: %3   Right only: %4",
        "相同：%1   不同：%2   仅左侧：%3   仅右侧：%4",
        "相同：%1   不同：%2   僅左側：%3   僅右側：%4",
        "Iguales: %1   Diferentes: %2   Solo izq.: %3   Solo der.: %4",
        "Identiques : %1   Différents : %2   Gauche seul. : %3   Droite seul. : %4",
        "Gleich: %1   Unterschiedlich: %2   Nur links: %3   Nur rechts: %4",
        "Одинаково: %1   Различается: %2   Только слева: %3   Только справа: %4",
        "同一: %1   相違: %2   左のみ: %3   右のみ: %4",
        "동일: %1   다름: %2   왼쪽만: %3   오른쪽만: %4");
    add("Left: %1 bytes   Right: %2 bytes   Differing: %3 bytes",
        "左：%1 字节   右：%2 字节   差异：%3 字节",
        "左：%1 位元組   右：%2 位元組   差異：%3 位元組",
        "Izq.: %1 bytes   Der.: %2 bytes   Difieren: %3 bytes",
        "Gauche : %1 octets   Droite : %2 octets   Diff. : %3 octets",
        "Links: %1 Bytes   Rechts: %2 Bytes   Abweichend: %3 Bytes",
        "Слева: %1 байт   Справа: %2 байт   Отличаются: %3 байт",
        "左: %1 バイト   右: %2 バイト   相違: %3 バイト",
        "왼쪽: %1 바이트   오른쪽: %2 바이트   차이: %3 바이트");
    add("Left: %1x%2   Right: %3x%4   Differing pixels: %5 (%6%)",
        "左：%1x%2   右：%3x%4   差异像素：%5 (%6%)",
        "左：%1x%2   右：%3x%4   差異像素：%5 (%6%)",
        "Izq.: %1x%2   Der.: %3x%4   Píxeles dif.: %5 (%6%)",
        "Gauche : %1x%2   Droite : %3x%4   Pixels diff. : %5 (%6%)",
        "Links: %1x%2   Rechts: %3x%4   Abw. Pixel: %5 (%6%)",
        "Слева: %1x%2   Справа: %3x%4   Отлич. пикселей: %5 (%6%)",
        "左: %1x%2   右: %3x%4   相違ピクセル: %5 (%6%)",
        "왼쪽: %1x%2   오른쪽: %3x%4   차이 픽셀: %5 (%6%)");
    add("Copied block to the right side.","已复制块到右侧。","已複製區塊到右側。","Bloque copiado al lado derecho.","Bloc copié à droite.","Block nach rechts kopiert.","Блок скопирован вправо.","ブロックを右側にコピーしました。","블록을 오른쪽으로 복사함.");
    add("Copied block to the left side.","已复制块到左侧。","已複製區塊到左側。","Bloque copiado al lado izquierdo.","Bloc copié à gauche.","Block nach links kopiert.","Блок скопирован влево.","ブロックを左側にコピーしました。","블록을 왼쪽으로 복사함.");
    add("Saved left file: %1","已保存左侧文件：%1","已儲存左側檔案：%1","Archivo izquierdo guardado: %1","Fichier gauche enregistré : %1","Linke Datei gespeichert: %1","Левый файл сохранён: %1","左ファイルを保存: %1","왼쪽 파일 저장: %1");
    add("Saved right file: %1","已保存右侧文件：%1","已儲存右側檔案：%1","Archivo derecho guardado: %1","Fichier droit enregistré : %1","Rechte Datei gespeichert: %1","Правый файл сохранён: %1","右ファイルを保存: %1","오른쪽 파일 저장: %1");
    add("Copied to right: %1","已复制到右侧：%1","已複製到右側：%1","Copiado a la derecha: %1","Copié à droite : %1","Nach rechts kopiert: %1","Скопировано вправо: %1","右にコピー: %1","오른쪽으로 복사: %1");
    add("Copied to left: %1","已复制到左侧：%1","已複製到左側：%1","Copiado a la izquierda: %1","Copié à gauche : %1","Nach links kopiert: %1","Скопировано влево: %1","左にコピー: %1","왼쪽으로 복사: %1");
    add("Difference overlay ON (right pane).","差异叠加已开启（右侧）。","差異疊加已開啟（右側）。","Superposición de diferencias ACTIVADA (panel derecho).","Superposition des différences ACTIVÉE (volet droit).","Differenz-Overlay EIN (rechtes Fenster).","Наложение различий ВКЛ (правая панель).","差分オーバーレイ ON（右ペイン）。","차이 오버레이 켜짐(오른쪽).");
    add("Difference overlay OFF.","差异叠加已关闭。","差異疊加已關閉。","Superposición de diferencias DESACTIVADA.","Superposition des différences DÉSACTIVÉE.","Differenz-Overlay AUS.","Наложение различий ВЫКЛ.","差分オーバーレイ OFF。","차이 오버레이 꺼짐.");
    add("One or both images failed to load.","一张或两张图片加载失败。","一張或兩張圖片載入失敗。","Una o ambas imágenes no se pudieron cargar.","Une ou les deux images n'ont pas pu être chargées.","Ein oder beide Bilder konnten nicht geladen werden.","Одно или оба изображения не загрузились.","片方または両方の画像の読み込みに失敗しました。","하나 또는 두 이미지를 불러오지 못했습니다.");
    add("Cannot find \"%1\"","找不到 \"%1\"","找不到 \"%1\"","No se encuentra \"%1\"","Introuvable : \"%1\"","\"%1\" nicht gefunden","Не найдено \"%1\"","\"%1\" が見つかりません","\"%1\" 을(를) 찾을 수 없음");
    add("Replaced %1 occurrence(s)","已替换 %1 处","已取代 %1 處","%1 reemplazo(s)","%1 remplacement(s)","%1 Vorkommen ersetzt","Заменено: %1","%1 件を置換","%1개 바꿈");
    add("Count: %1 occurrence(s)","计数：%1 处","計數：%1 處","Conteo: %1","Nombre : %1","Anzahl: %1","Количество: %1","件数: %1","개수: %1");

    // ---- Connect menu (cloud drives / shared folders / SFTP-FTP) ----
    add("Co&nnect",          "连接(&N)",  "連接(&N)",  "C&onectar",    "Co&nnecter",    "&Verbinden",    "&Подключить",     "接続(&N)",      "연결(&N)");
    add("Connect S&hared Folder...","连接共享文件夹(&H)...","連接共用資料夾(&H)...","Conectar carpeta c&ompartida...","Connecter un dossier &partagé...","Freigegebenen Ordner &verbinden...","Подключить &общую папку...","共有フォルダに接続(&H)...","공유 폴더 연결(&H)...");
    add("Connect SFTP/&FTP...","连接 SFTP/FTP(&F)...","連接 SFTP/FTP(&F)...","Conectar SFTP/&FTP...","Connecter SFTP/&FTP...","SFTP/&FTP verbinden...","Подключить SFTP/&FTP...","SFTP/FTPに接続(&F)...","SFTP/FTP 연결(&F)...");
    add("Connect Shared Folder","连接共享文件夹","連接共用資料夾","Conectar carpeta compartida","Connecter un dossier partagé","Freigegebenen Ordner verbinden","Подключить общую папку","共有フォルダに接続","공유 폴더 연결");
    add("Connect SFTP/FTP",  "连接 SFTP/FTP","連接 SFTP/FTP","Conectar SFTP/FTP","Connecter SFTP/FTP","SFTP/FTP verbinden","Подключить SFTP/FTP","SFTP/FTPに接続","SFTP/FTP 연결");
    add("Shared Folder (SMB/Samba)","共享文件夹（SMB/Samba）","共用資料夾（SMB/Samba）","Carpeta compartida (SMB/Samba)","Dossier partagé (SMB/Samba)","Freigegebener Ordner (SMB/Samba)","Общая папка (SMB/Samba)","共有フォルダ（SMB/Samba）","공유 폴더(SMB/Samba)");
    add("Share Path (UNC):", "共享路径（UNC）：","共用路徑（UNC）：","Ruta compartida (UNC):","Chemin partagé (UNC) :","Freigabepfad (UNC):","Путь к общей папке (UNC):","共有パス（UNC）：","공유 경로(UNC):");
    add("e.g. \\\\server\\share or //server/share",
        "例如 \\\\server\\share 或 //server/share",
        "例如 \\\\server\\share 或 //server/share",
        "p. ej. \\\\server\\share o //server/share",
        "p. ex. \\\\server\\share ou //server/share",
        "z. B. \\\\server\\share oder //server/share",
        "например \\\\server\\share или //server/share",
        "例: \\\\server\\share または //server/share",
        "예: \\\\server\\share 또는 //server/share");
    add("server address, e.g. 192.168.1.10 or ftp.example.com",
        "服务器地址，例如 192.168.1.10 或 ftp.example.com",
        "伺服器位址，例如 192.168.1.10 或 ftp.example.com",
        "dirección del servidor, p. ej. 192.168.1.10 o ftp.example.com",
        "adresse du serveur, p. ex. 192.168.1.10 ou ftp.example.com",
        "Serveradresse, z. B. 192.168.1.10 oder ftp.example.com",
        "адрес сервера, например 192.168.1.10 или ftp.example.com",
        "サーバーアドレス、例: 192.168.1.10 または ftp.example.com",
        "서버 주소, 예: 192.168.1.10 또는 ftp.example.com");
    add("Private Key (optional):","私钥（可选）：","私密金鑰（選用）：","Clave privada (opcional):","Clé privée (facultative) :","Privater Schlüssel (optional):","Закрытый ключ (необязательно):","秘密鍵（任意）：","개인 키(선택):");
    add("Enter a Windows/Samba share path, e.g. \\\\server\\share or //server/share. Works against any SMB dialect (SMB1/CIFS, SMB2, SMB3) the server offers — Windows negotiates it automatically. Username/password are used to authenticate the share if needed.",
        "输入 Windows/Samba 共享路径，例如 \\\\server\\share 或 //server/share。可与服务器提供的任意 SMB 协议版本（SMB1/CIFS、SMB2、SMB3）通信——Windows 会自动协商；如需要可用用户名/密码验证该共享。",
        "輸入 Windows/Samba 共用路徑，例如 \\\\server\\share 或 //server/share。可與伺服器提供的任意 SMB 版本（SMB1/CIFS、SMB2、SMB3）通訊——Windows 會自動協商；如需要可用使用者名稱/密碼驗證該共用。",
        "Introduzca una ruta de compartición Windows/Samba, p. ej. \\\\server\\share o //server/share. Funciona con cualquier dialecto SMB (SMB1/CIFS, SMB2, SMB3) que ofrezca el servidor — Windows lo negocia automáticamente. Usuario/contraseña se usan para autenticar la compartición si es necesario.",
        "Entrez un chemin de partage Windows/Samba, p. ex. \\\\server\\share ou //server/share. Fonctionne avec tout dialecte SMB (SMB1/CIFS, SMB2, SMB3) offert par le serveur — Windows le négocie automatiquement. Le nom d'utilisateur/mot de passe servent à authentifier le partage si nécessaire.",
        "Geben Sie einen Windows/Samba-Freigabepfad ein, z. B. \\\\server\\share oder //server/share. Funktioniert mit jedem vom Server angebotenen SMB-Dialekt (SMB1/CIFS, SMB2, SMB3) — Windows handelt dies automatisch aus. Benutzername/Passwort dienen bei Bedarf zur Authentifizierung der Freigabe.",
        "Введите путь общей папки Windows/Samba, например \\\\server\\share или //server/share. Работает с любым диалектом SMB (SMB1/CIFS, SMB2, SMB3), который предлагает сервер — Windows согласовывает это автоматически. Имя пользователя/пароль используются для аутентификации общей папки при необходимости.",
        "Windows/Samba の共有パスを入力してください。例: \\\\server\\share または //server/share。サーバーが提供する任意の SMB 方式（SMB1/CIFS、SMB2、SMB3）に対応——Windows が自動的に調整します。必要に応じてユーザー名/パスワードで共有を認証します。",
        "Windows/Samba 공유 경로를 입력하세요. 예: \\\\server\\share 또는 //server/share. 서버가 제공하는 모든 SMB 방식(SMB1/CIFS, SMB2, SMB3)과 작동합니다 — Windows가 자동으로 협상합니다. 필요한 경우 사용자 이름/비밀번호로 공유를 인증합니다.");
    add("Enter the SFTP server host and port (default 22), your username, and either a password or a private key file. Uses the same native libssh2 client as the SSH terminal feature — no external sftp.exe.",
        "输入 SFTP 服务器地址和端口（默认 22）、用户名，以及密码或私钥文件。使用与 SSH 终端功能相同的原生 libssh2 客户端——无需外部 sftp.exe。",
        "輸入 SFTP 伺服器位址與連接埠（預設 22）、使用者名稱，以及密碼或私密金鑰檔案。使用與 SSH 終端功能相同的原生 libssh2 客戶端——不需要外部 sftp.exe。",
        "Introduzca el host y puerto del servidor SFTP (predet. 22), su usuario, y una contraseña o un archivo de clave privada. Usa el mismo cliente nativo libssh2 que la función de terminal SSH — sin sftp.exe externo.",
        "Entrez l'hôte et le port du serveur SFTP (défaut 22), votre nom d'utilisateur, et un mot de passe ou un fichier de clé privée. Utilise le même client natif libssh2 que la fonction terminal SSH — pas de sftp.exe externe.",
        "Geben Sie Host und Port des SFTP-Servers (Standard 22), Ihren Benutzernamen sowie ein Passwort oder eine private Schlüsseldatei ein. Nutzt denselben nativen libssh2-Client wie die SSH-Terminal-Funktion — kein externes sftp.exe.",
        "Введите хост и порт SFTP-сервера (по умолчанию 22), имя пользователя, и пароль или файл закрытого ключа. Использует тот же нативный клиент libssh2, что и функция SSH-терминала — без внешнего sftp.exe.",
        "SFTP サーバーのホストとポート（既定 22）、ユーザー名、パスワードまたは秘密鍵ファイルを入力してください。SSH 端末機能と同じネイティブ libssh2 クライアントを使用——外部の sftp.exe は不要です。",
        "SFTP 서버 호스트와 포트(기본 22), 사용자 이름, 비밀번호 또는 개인 키 파일을 입력하세요. SSH 터미널 기능과 동일한 네이티브 libssh2 클라이언트를 사용합니다 — 외부 sftp.exe가 필요 없습니다.");
    add("Enter the FTP server host and port (default 21) and your account credentials. Uses passive-mode (PASV) data connections.",
        "输入 FTP 服务器地址和端口（默认 21）以及账号密码。使用被动模式（PASV）数据连接。",
        "輸入 FTP 伺服器位址與連接埠（預設 21）以及帳號密碼。使用被動模式（PASV）資料連線。",
        "Introduzca el host y puerto del servidor FTP (predet. 21) y sus credenciales. Usa conexiones de datos en modo pasivo (PASV).",
        "Entrez l'hôte et le port du serveur FTP (défaut 21) et vos identifiants. Utilise des connexions de données en mode passif (PASV).",
        "Geben Sie Host und Port des FTP-Servers (Standard 21) und Ihre Zugangsdaten ein. Nutzt passive (PASV) Datenverbindungen.",
        "Введите хост и порт FTP-сервера (по умолчанию 21) и учётные данные. Использует пассивные (PASV) подключения данных.",
        "FTP サーバーのホストとポート（既定 21）とアカウント情報を入力してください。パッシブモード（PASV）のデータ接続を使用します。",
        "FTP 서버 호스트와 포트(기본 21), 계정 정보를 입력하세요. 패시브 모드(PASV) 데이터 연결을 사용합니다.");
    add("&Connect Cloud Drive...","连接云盘(&C)...","連接雲端(&C)...","&Conectar unidad en la nube...","&Connecter un disque cloud...","Cloud-Laufwerk &verbinden...","&Подключить облако...","クラウドに接続(&C)...","클라우드 연결(&C)...");
    add("Connect to Cloud Drive","连接到云盘","連接到雲端","Conectar a la nube","Connexion au cloud","Mit Cloud verbinden","Подключение к облаку","クラウドに接続","클라우드에 연결");
    add("Cloud",             "云盘",      "雲端",      "Nube",         "Cloud",         "Cloud",         "Облако",          "クラウド",      "클라우드");
    add("Provider:",         "服务商：",  "服務商：",  "Proveedor:",   "Fournisseur :", "Anbieter:",     "Провайдер:",      "プロバイダ：",  "제공자:");
    add("Drive Name:",       "云盘名称：","雲端名稱：","Nombre de unidad:","Nom du disque :","Laufwerksname:","Имя диска:","ドライブ名：","드라이브 이름:");
    add("e.g. work, photos (shown as BaiduPan-work)",
        "例如 work、照片（显示为 BaiduPan-work）",
        "例如 work、照片（顯示為 BaiduPan-work）",
        "p. ej. work, fotos (se muestra como BaiduPan-work)",
        "p. ex. work, photos (affiché comme BaiduPan-work)",
        "z. B. work, Fotos (angezeigt als BaiduPan-work)",
        "например work, фото (отображается как BaiduPan-work)",
        "例: work、写真（BaiduPan-work と表示）",
        "예: work, 사진 (BaiduPan-work 으로 표시)");
    add("(no connected cloud drives)","(无已连接的云盘)","(無已連接的雲端)","(sin unidades en la nube conectadas)","(aucun disque cloud connecté)","(keine verbundenen Cloud-Laufwerke)","(нет подключённых облаков)","(接続済みのクラウドなし)","(연결된 클라우드 없음)");
    add("Reconnect",         "重新连接",  "重新連線",  "Reconectar",   "Reconnecter",   "Neu verbinden", "Переподключить",  "再接続",        "다시 연결");
    add("Edit...",           "编辑...",   "編輯...",   "Editar...",    "Modifier...",   "Bearbeiten...", "Изменить...",     "編集...",       "편집...");
    add("Delete Cloud Drive","删除云盘",  "刪除雲端",  "Eliminar unidad en la nube","Supprimer le disque cloud","Cloud-Laufwerk löschen","Удалить облако","クラウドを削除","클라우드 삭제");
    add("Delete cloud drive \"%1\"?","确定删除云盘 \"%1\"？","確定刪除雲端 \"%1\"？","¿Eliminar la unidad en la nube \"%1\"?","Supprimer le disque cloud \"%1\" ?","Cloud-Laufwerk \"%1\" löschen?","Удалить облако \"%1\"?","クラウド \"%1\" を削除しますか？","클라우드 \"%1\" 을(를) 삭제할까요?");
    add("Reconnected %1",    "已重新连接 %1","已重新連線 %1","Reconectado %1","Reconnecté %1","%1 neu verbunden","Переподключено %1","%1 を再接続しました","%1 다시 연결됨");
    add("Updated %1",        "已更新 %1", "已更新 %1", "Actualizado %1","Mis à jour %1","%1 aktualisiert","Обновлено %1",   "%1 を更新しました","%1 업데이트됨");
    add("Deleted %1",        "已删除 %1", "已刪除 %1", "Eliminado %1", "Supprimé %1",   "%1 gelöscht",   "Удалено %1",      "%1 を削除しました","%1 삭제됨");
    add("Access Token:",     "访问令牌：","存取權杖：","Token de acceso:","Jeton d'accès :","Zugriffstoken:","Токен доступа:","アクセストークン：","액세스 토큰:");
    add("Drive ID:",         "网盘 ID：", "雲端 ID：", "ID de unidad:","ID du disque :","Laufwerk-ID:",  "ID диска:",       "ドライブID：",  "드라이브 ID:");
    add("WebDAV URL:",       "WebDAV 地址：","WebDAV 網址：","URL de WebDAV:","URL WebDAV :","WebDAV-URL:","URL WebDAV:","WebDAV URL：","WebDAV URL:");
    add("Username:",         "用户名：",  "使用者名稱：","Usuario:",    "Utilisateur :", "Benutzername:", "Имя пользователя:","ユーザー名：", "사용자 이름:");
    add("Password:",         "密码：",    "密碼：",    "Contraseña:",  "Mot de passe :","Passwort:",     "Пароль:",         "パスワード：",  "비밀번호:");
    add("Missing required connection details.","缺少必要的连接信息。","缺少必要的連線資訊。","Faltan datos de conexión obligatorios.","Détails de connexion requis manquants.","Erforderliche Verbindungsdetails fehlen.","Отсутствуют обязательные данные подключения.","必要な接続情報が不足しています。","필수 연결 정보가 없습니다.");
    add("Login in Browser (get token)","浏览器登录（获取令牌）","瀏覽器登入（取得權杖）","Iniciar sesión en el navegador (obtener token)","Se connecter dans le navigateur (obtenir le jeton)","Im Browser anmelden (Token holen)","Войти в браузере (получить токен)","ブラウザでログイン（トークン取得）","브라우저에서 로그인(토큰 받기)");
    add("Proxy Server (optional)","代理服务器（可选）","代理伺服器（選用）","Servidor proxy (opcional)","Serveur proxy (facultatif)","Proxyserver (optional)","Прокси-сервер (необязательно)","プロキシサーバー（任意）","프록시 서버(선택)");
    add("Type:",             "类型：",    "類型：",    "Tipo:",        "Type :",        "Typ:",          "Тип:",            "種類：",        "유형:");
    add("None",              "无",        "無",        "Ninguno",      "Aucun",         "Keiner",        "Нет",             "なし",          "없음");
    add("Host:",             "主机：",    "主機：",    "Host:",        "Hôte :",        "Host:",         "Хост:",           "ホスト：",      "호스트:");
    add("Port:",             "端口：",    "連接埠：",  "Puerto:",      "Port :",        "Port:",         "Порт:",           "ポート：",      "포트:");
    add("WebDAV uses URL + account credentials, no OAuth login.","WebDAV 使用 URL + 账号密码，无需 OAuth 登录。","WebDAV 使用 URL + 帳號密碼，無需 OAuth 登入。","WebDAV usa URL + credenciales, sin OAuth.","WebDAV utilise URL + identifiants, sans OAuth.","WebDAV nutzt URL + Zugangsdaten, kein OAuth.","WebDAV использует URL + учётные данные, без OAuth.","WebDAV は URL + 認証情報を使用し、OAuth ログインは不要です。","WebDAV는 URL+계정 자격증명을 사용하며 OAuth 로그인이 없습니다.");
    add("A browser window opened for login (supports web / QR code). After authorizing, copy the access_token (and drive_id for Aliyun) from the redirect URL and paste it above. Replace YOUR_CLIENT_ID with your registered app id. The manual token field remains available.",
        "已打开浏览器登录（支持网页/扫二维码）。授权后，从跳转 URL 中复制 access_token（阿里云盘还需 drive_id）粘贴到上方。请将 YOUR_CLIENT_ID 替换为你注册的应用 id。手动输入令牌的方式仍然保留。",
        "已開啟瀏覽器登入（支援網頁/掃 QR）。授權後，從跳轉 URL 複製 access_token（阿里雲端另需 drive_id）貼到上方。請將 YOUR_CLIENT_ID 換成你註冊的應用 id。手動輸入權杖的方式仍保留。",
        "Se abrió el navegador para iniciar sesión (web / código QR). Tras autorizar, copie el access_token (y drive_id para Aliyun) de la URL de redirección y péguelo arriba. Reemplace YOUR_CLIENT_ID por su id de app. El campo manual sigue disponible.",
        "Un navigateur s'est ouvert pour la connexion (web / QR). Après autorisation, copiez l'access_token (et drive_id pour Aliyun) depuis l'URL de redirection et collez-le ci-dessus. Remplacez YOUR_CLIENT_ID par votre id d'app. Le champ manuel reste disponible.",
        "Ein Browser wurde zum Anmelden geöffnet (Web / QR-Code). Nach der Autorisierung access_token (und drive_id für Aliyun) aus der Weiterleitungs-URL kopieren und oben einfügen. YOUR_CLIENT_ID durch Ihre App-ID ersetzen. Das manuelle Feld bleibt verfügbar.",
        "Открыто окно браузера для входа (веб / QR-код). После авторизации скопируйте access_token (и drive_id для Aliyun) из URL перенаправления и вставьте выше. Замените YOUR_CLIENT_ID на свой id приложения. Поле ручного ввода остаётся доступным.",
        "ログイン用のブラウザが開きました（Web / QR コード対応）。認可後、リダイレクト URL から access_token（Aliyun は drive_id も）をコピーして上に貼り付けてください。YOUR_CLIENT_ID は登録済みアプリ id に置き換えてください。手動入力欄も引き続き利用できます。",
        "로그인용 브라우저 창이 열렸습니다(웹/QR 코드 지원). 인증 후 리디렉션 URL에서 access_token(아리윈은 drive_id도)을 복사해 위에 붙여넣으세요. YOUR_CLIENT_ID를 등록한 앱 id로 바꾸세요. 수동 토큰 입력란도 그대로 사용할 수 있습니다.");
    // Browser tab actions
    add("Back",              "后退",      "上一頁",    "Atrás",        "Retour",        "Zurück",        "Назад",           "戻る",          "뒤로");
    add("Up",                "上级",      "上層",      "Arriba",       "Haut",          "Nach oben",     "Вверх",           "上へ",          "위로");
    add("Open",              "打开",      "開啟",      "Abrir",        "Ouvrir",        "Öffnen",        "Открыть",         "開く",          "열기");
    add("Open Folder",       "打开文件夹","開啟資料夾","Abrir carpeta","Ouvrir le dossier","Ordner öffnen","Открыть папку","フォルダを開く","폴더 열기");
    add("Download...",       "下载...",   "下載...",   "Descargar...", "Télécharger...","Herunterladen...","Скачать...",   "ダウンロード...","다운로드...");
    add("Download Folder...","下载文件夹...","下載資料夾...","Descargar carpeta...","Télécharger le dossier...","Ordner herunterladen...","Скачать папку...","フォルダをダウンロード...","폴더 다운로드...");
    add("Open With...",      "打开方式...","開啟方式...","Abrir con...","Ouvrir avec...","Öffnen mit...","Открыть с помощью...","プログラムから開く...","연결 프로그램...");
    add("Open with VS Code", "用 VS Code 打开","用 VS Code 開啟","Abrir con VS Code","Ouvrir avec VS Code","Mit VS Code öffnen","Открыть в VS Code","VS Codeで開く","VS Code로 열기");
    add("Open as Text",      "作为文本打开","以文字開啟","Abrir como texto","Ouvrir comme texte","Als Text öffnen","Открыть как текст","テキストとして開く","텍스트로 열기");
    add("Open (system default)","用系统默认程序打开","用系統預設程式開啟","Abrir (predeterminado del sistema)","Ouvrir (par défaut du système)","Öffnen (Systemstandard)","Открыть (по умолчанию)","既定のアプリで開く","시스템 기본으로 열기");
    add("Stream (play while downloading)","流式播放（边下边播）","串流播放（邊下邊播）","Transmitir (reproducir mientras descarga)","Diffuser (lire pendant le téléchargement)","Streamen (beim Herunterladen abspielen)","Потоково (воспроизвод. при загрузке)","ストリーム再生（ダウンロードしながら）","스트리밍(다운로드하며 재생)");
    add("Choose Program to Open With","选择打开程序","選擇開啟程式","Elegir programa para abrir","Choisir le programme","Programm zum Öffnen wählen","Выбрать программу","開くプログラムを選択","열 프로그램 선택");
    add("Programs (*.exe);;All Files (*)","程序 (*.exe);;所有文件 (*)","程式 (*.exe);;所有檔案 (*)","Programas (*.exe);;Todos (*)","Programmes (*.exe);;Tous (*)","Programme (*.exe);;Alle (*)","Программы (*.exe);;Все (*)","プログラム (*.exe);;すべて (*)","프로그램 (*.exe);;모든 파일 (*)");
    add(" Path: ",           " 路径： ",  " 路徑： ",  " Ruta: ",      " Chemin : ",    " Pfad: ",       " Путь: ",         " パス： ",      " 경로: ");
    add("Loading…",          "加载中…",   "載入中…",   "Cargando…",    "Chargement…",   "Laden…",        "Загрузка…",       "読み込み中…",   "불러오는 중…");
    add("%1 item(s)",        "%1 项",     "%1 項",     "%1 elemento(s)","%1 élément(s)","%1 Element(e)", "%1 элем.",        "%1 件",         "%1개 항목");
    add("Error: %1",         "错误：%1",  "錯誤：%1",  "Error: %1",    "Erreur : %1",   "Fehler: %1",    "Ошибка: %1",      "エラー: %1",    "오류: %1");
    add("Downloading %1…",   "正在下载 %1…","正在下載 %1…","Descargando %1…","Téléchargement de %1…","%1 wird heruntergeladen…","Загрузка %1…","%1 をダウンロード中…","%1 다운로드 중…");
    add("Downloading… %1 / %2","下载中… %1 / %2","下載中… %1 / %2","Descargando… %1 / %2","Téléchargement… %1 / %2","Herunterladen… %1 / %2","Загрузка… %1 / %2","ダウンロード中… %1 / %2","다운로드 중… %1 / %2");
    add("Downloaded to %1",  "已下载到 %1","已下載到 %1","Descargado en %1","Téléchargé vers %1","Heruntergeladen nach %1","Скачано в %1","%1 にダウンロード完了","%1 에 다운로드됨");
    add("Streaming via system player…","正在用系统播放器流式播放…","正在用系統播放器串流…","Transmitiendo con el reproductor del sistema…","Diffusion via le lecteur système…","Streaming über Systemplayer…","Потоковое воспроизведение системным плеером…","システムプレーヤーでストリーミング中…","시스템 플레이어로 스트리밍 중…");
    add("Streaming unavailable (%1); downloading first…","无法流式播放（%1）；改为先下载…","無法串流（%1）；改為先下載…","Transmisión no disponible (%1); descargando primero…","Diffusion indisponible (%1) ; téléchargement d'abord…","Streaming nicht verfügbar (%1); lade zuerst herunter…","Потоковое недоступно (%1); сначала загрузка…","ストリーミング不可（%1）。先にダウンロードします…","스트리밍 불가(%1); 먼저 다운로드 중…");
    add("Local stream relay unavailable; downloading first…",
        "本地流式中转启动失败；改为先下载…",
        "本機串流中轉啟動失敗；改為先下載…",
        "Relé de transmisión local no disponible; descargando primero…",
        "Relais de diffusion local indisponible ; téléchargement d'abord…",
        "Lokales Stream-Relay nicht verfügbar; lade zuerst herunter…",
        "Локальный ретранслятор недоступен; сначала загрузка…",
        "ローカル中継の起動に失敗しました。先にダウンロードします…",
        "로컬 스트림 중계를 사용할 수 없음; 먼저 다운로드 중…");
    add("VS Code (code) not found on PATH; opening internally.","PATH 中未找到 VS Code (code)；改为内部打开。","PATH 中找不到 VS Code (code)；改為內部開啟。","VS Code (code) no está en PATH; abriendo internamente.","VS Code (code) introuvable dans PATH ; ouverture interne.","VS Code (code) nicht im PATH; intern geöffnet.","VS Code (code) не найден в PATH; открываю внутри.","VS Code (code) が PATH にありません。内部で開きます。","PATH에 VS Code(code)가 없어 내부에서 엽니다.");

    // ---- Cloud transfers (upload/download manager + panel) ----
    add("Transfers",         "传输",      "傳輸",      "Transferencias","Transferts",   "Übertragungen", "Передачи",        "転送",          "전송");
    add("Show &Transfers Panel","显示传输面板(&T)","顯示傳輸面板(&T)","Mostrar panel de &transferencias","Afficher le panneau de &transferts","&Übertragungspanel anzeigen","Показать панель &передач","転送パネルを表示(&T)","전송 패널 표시(&T)");
    add("Upload Files...",   "上传文件...","上傳檔案...","Subir archivos...","Téléverser des fichiers...","Dateien hochladen...","Загрузить файлы...","ファイルをアップロード...","파일 업로드...");
    add("Upload Folder...",  "上传文件夹...","上傳資料夾...","Subir carpeta...","Téléverser un dossier...","Ordner hochladen...","Загрузить папку...","フォルダをアップロード...","폴더 업로드...");
    add("New Folder...",     "新建文件夹...","新增資料夾...","Nueva carpeta...","Nouveau dossier...","Neuer Ordner...","Новая папка...","新規フォルダ...","새 폴더...");
    add("New Folder",        "新建文件夹","新增資料夾","Nueva carpeta","Nouveau dossier","Neuer Ordner","Новая папка","新規フォルダ","새 폴더");
    add("Folder name:",      "文件夹名称：","資料夾名稱：","Nombre de carpeta:","Nom du dossier :","Ordnername:","Имя папки:","フォルダ名：","폴더 이름:");
    add("Select Files to Upload","选择要上传的文件","選擇要上傳的檔案","Seleccionar archivos para subir","Sélectionner les fichiers à téléverser","Hochzuladende Dateien wählen","Выберите файлы для загрузки","アップロードするファイルを選択","업로드할 파일 선택");
    add("Select Folder to Upload","选择要上传的文件夹","選擇要上傳的資料夾","Seleccionar carpeta para subir","Sélectionner le dossier à téléverser","Hochzuladenden Ordner wählen","Выберите папку для загрузки","アップロードするフォルダを選択","업로드할 폴더 선택");
    add("Uploading %1 file(s)…","正在上传 %1 个文件…","正在上傳 %1 個檔案…","Subiendo %1 archivo(s)…","Téléversement de %1 fichier(s)…","%1 Datei(en) werden hochgeladen…","Загрузка %1 файла(ов)…","%1 件のファイルをアップロード中…","%1개 파일 업로드 중…");
    add("Uploading folder %1…","正在上传文件夹 %1…","正在上傳資料夾 %1…","Subiendo la carpeta %1…","Téléversement du dossier %1…","Ordner %1 wird hochgeladen…","Загрузка папки %1…","フォルダ %1 をアップロード中…","폴더 %1 업로드 중…");
    add("Uploaded %1 file(s).","已上传 %1 个文件。","已上傳 %1 個檔案。","Se subieron %1 archivo(s).","%1 fichier(s) téléversé(s).","%1 Datei(en) hochgeladen.","Загружено файлов: %1.","%1 件をアップロードしました。","%1개 파일 업로드됨.");
    add("Downloaded %1 file(s).","已下载 %1 个文件。","已下載 %1 個檔案。","Se descargaron %1 archivo(s).","%1 fichier(s) téléchargé(s).","%1 Datei(en) heruntergeladen.","Скачано файлов: %1.","%1 件をダウンロードしました。","%1개 파일 다운로드됨.");
    add("A folder upload is already in progress.","已有文件夹上传正在进行中。","已有資料夾上傳正在進行中。","Ya hay una subida de carpeta en curso.","Un téléversement de dossier est déjà en cours.","Ein Ordner-Upload läuft bereits.","Загрузка папки уже выполняется.","フォルダのアップロードが既に進行中です。","폴더 업로드가 이미 진행 중입니다.");
    add("A folder download is already in progress.","已有文件夹下载正在进行中。","已有資料夾下載正在進行中。","Ya hay una descarga de carpeta en curso.","Un téléchargement de dossier est déjà en cours.","Ein Ordner-Download läuft bereits.","Скачивание папки уже выполняется.","フォルダのダウンロードが既に進行中です。","폴더 다운로드가 이미 진행 중입니다.");
    add("Cancel",            "取消",      "取消",      "Cancelar",     "Annuler",       "Abbrechen",     "Отмена",          "キャンセル",    "취소");
    add("Retry",             "重试",      "重試",      "Reintentar",   "Réessayer",     "Wiederholen",   "Повторить",       "再試行",        "다시 시도");
    add("Remove",            "移除",      "移除",      "Quitar",       "Retirer",       "Entfernen",     "Убрать",          "削除",          "제거");
    add("Clear Finished",    "清除已完成","清除已完成","Borrar completados","Effacer les terminés","Abgeschlossene löschen","Очистить завершённые","完了分をクリア","완료 항목 지우기");
    add("Max Tasks:",        "最大任务数：","最大任務數：","Tareas máx.:","Tâches max :","Max. Aufgaben:","Макс. задач:","最大タスク数:","최대 작업 수:");
    add("Maximum concurrent transfer tasks (1 - 1000)",
        "最大并发传输任务数（1 - 1000）",
        "最大並發傳輸任務數（1 - 1000）",
        "Tareas de transferencia concurrentes máximas (1 - 1000)",
        "Tâches de transfert simultanées maximales (1 - 1000)",
        "Maximale gleichzeitige Übertragungsaufgaben (1 - 1000)",
        "Максимальное количество одновременных задач (1 - 1000)",
        "最大同時転送タスク数（1 - 1000）",
        "최대 동시 전송 작업 수 (1 - 1000)");
    add("Rename...",         "重命名...", "重新命名...","Renombrar...", "Renommer...",   "Umbenennen...", "Переименовать...","名前を変更...","이름 바꾸기...");
    add("Rename",            "重命名",    "重新命名",  "Renombrar",    "Renommer",      "Umbenennen",    "Переименовать",   "名前を変更",    "이름 바꾸기");
    add("New name:",         "新名称：",  "新名稱：",  "Nuevo nombre:","Nouveau nom :", "Neuer Name:",   "Новое имя:",      "新しい名前：",  "새 이름:");
    add("Delete folder \"%1\" and all its contents?",
        "确定删除文件夹 \"%1\" 及其全部内容？",
        "確定刪除資料夾 \"%1\" 及其所有內容？",
        "¿Eliminar la carpeta \"%1\" y todo su contenido?",
        "Supprimer le dossier \"%1\" et tout son contenu ?",
        "Ordner \"%1\" und seinen gesamten Inhalt löschen?",
        "Удалить папку \"%1\" и всё её содержимое?",
        "フォルダ \"%1\" とその中身をすべて削除しますか？",
        "폴더 \"%1\" 와 그 안의 모든 내용을 삭제할까요?");
    add("Delete \"%1\"?",    "确定删除 \"%1\"？","確定刪除 \"%1\"？","¿Eliminar \"%1\"?","Supprimer \"%1\" ?","\"%1\" löschen?","Удалить \"%1\"?","\"%1\" を削除しますか？","\"%1\" 을(를) 삭제할까요?");
    add("Deleted \"%1\".",   "已删除 \"%1\"。","已刪除 \"%1\"。","Se eliminó \"%1\".","\"%1\" supprimé.","\"%1\" gelöscht.","Удалено \"%1\".","\"%1\" を削除しました。","\"%1\" 삭제됨.");
    add("Delete failed: %1", "删除失败：%1","刪除失敗：%1","Error al eliminar: %1","Échec de la suppression : %1","Löschen fehlgeschlagen: %1","Ошибка удаления: %1","削除に失敗しました：%1","삭제 실패: %1");
    add("Save to Remote?",   "保存到远程？","儲存到遠端？","¿Guardar en remoto?","Enregistrer à distance ?","Auf Remote speichern?","Сохранить на удалённый сервер?","リモートに保存しますか？","원격에 저장할까요?");
    add("\"%1\" was opened from %2.\nDo you also want to update it there?",
        "\"%1\" 是从 %2 打开的。\n是否同时更新到那里？",
        "\"%1\" 是從 %2 開啟的。\n是否同時更新到那裡？",
        "\"%1\" se abrió desde %2.\n¿Desea también actualizarlo allí?",
        "\"%1\" a été ouvert depuis %2.\nVoulez-vous aussi le mettre à jour là-bas ?",
        "\"%1\" wurde von %2 geöffnet.\nMöchten Sie es dort auch aktualisieren?",
        "\"%1\" был открыт из %2.\nХотите также обновить его там?",
        "\"%1\" は %2 から開かれました。\nそこも更新しますか？",
        "\"%1\" 은(는) %2 에서 열렸습니다.\n그곳도 업데이트할까요?");
    add("Updated %1 on %2",  "已更新 %1 到 %2","已更新 %1 到 %2","Se actualizó %1 en %2","%1 mis à jour sur %2","%1 auf %2 aktualisiert","Обновлено %1 на %2","%1 を %2 で更新しました","%1 을(를) %2 에 업데이트함");
    add("Updating %1 on %2…","正在更新 %1 到 %2…","正在更新 %1 到 %2…","Actualizando %1 en %2…","Mise à jour de %1 sur %2…","%1 wird auf %2 aktualisiert…","Обновление %1 на %2…","%1 を %2 で更新中…","%1 을(를) %2 에 업데이트 중…");
    add("Failed to update the remote copy:\n%1",
        "更新远程副本失败：\n%1",
        "更新遠端副本失敗：\n%1",
        "No se pudo actualizar la copia remota:\n%1",
        "Échec de la mise à jour de la copie distante :\n%1",
        "Aktualisierung der Remote-Kopie fehlgeschlagen:\n%1",
        "Не удалось обновить удалённую копию:\n%1",
        "リモートコピーの更新に失敗しました：\n%1",
        "원격 복사본 업데이트 실패:\n%1");
    add("Renamed \"%1\" to \"%2\".",
        "已将 \"%1\" 重命名为 \"%2\"。",
        "已將 \"%1\" 重新命名為 \"%2\"。",
        "Se renombró \"%1\" a \"%2\".",
        "\"%1\" renommé en \"%2\".",
        "\"%1\" wurde in \"%2\" umbenannt.",
        "\"%1\" переименовано в \"%2\".",
        "\"%1\" を \"%2\" に名前変更しました。",
        "\"%1\" 을(를) \"%2\" 로 이름을 바꿨습니다.");
    add("Rename failed: %1", "重命名失败：%1","重新命名失敗：%1","Error al renombrar: %1","Échec du renommage : %1","Umbenennen fehlgeschlagen: %1","Ошибка переименования: %1","名前変更に失敗しました：%1","이름 바꾸기 실패: %1");
    add("Copy",              "复制",      "複製",      "Copiar",       "Copier",        "Kopieren",      "Копировать",      "コピー",        "복사");
    add("Copy Name",         "复制名称",  "複製名稱",  "Copiar nombre","Copier le nom", "Namen kopieren","Копировать имя",  "名前をコピー",  "이름 복사");
    add("Copy Local Path",   "复制本地路径","複製本機路徑","Copiar ruta local","Copier le chemin local","Lokalen Pfad kopieren","Копировать локальный путь","ローカルパスをコピー","로컬 경로 복사");
    add("Copy Remote Path",  "复制远程路径","複製遠端路徑","Copiar ruta remota","Copier le chemin distant","Remote-Pfad kopieren","Копировать удалённый путь","リモートパスをコピー","원격 경로 복사");
    add("Copy Status",       "复制状态",  "複製狀態",  "Copiar estado","Copier le statut","Status kopieren","Копировать статус","状態をコピー","상태 복사");
    add("Copy All",          "复制全部",  "複製全部",  "Copiar todo",  "Tout copier",   "Alles kopieren","Копировать всё",  "すべてコピー",  "모두 복사");
    add("Type",              "类型",      "類型",      "Tipo",         "Type",          "Typ",           "Тип",             "種類",          "유형");
    add("Progress",          "进度",      "進度",      "Progreso",     "Progression",   "Fortschritt",   "Прогресс",        "進捗",          "진행률");
    add("Speed",             "速度",      "速度",      "Velocidad",    "Vitesse",       "Geschwindigkeit","Скорость",       "速度",          "속도");
    add("Upload",            "上传",      "上傳",      "Subir",        "Téléverser",    "Hochladen",     "Загрузка",        "アップロード",  "업로드");
    add("Download",          "下载",      "下載",      "Descargar",    "Télécharger",   "Herunterladen", "Скачивание",      "ダウンロード",  "다운로드");
    add("Delete",            "删除",      "刪除",      "Eliminar",     "Supprimer",     "Löschen",       "Удалить",         "削除",          "삭제");
    add("Deleting %1…",      "正在删除 %1…","正在刪除 %1…","Eliminando %1…","Suppression de %1…","Lösche %1…","Удаление %1…","%1 を削除中…","%1 삭제 중…");
    add("Deleted %1 file(s).","已删除 %1 个文件。","已刪除 %1 個檔案。","%1 archivo(s) eliminado(s).","%1 fichier(s) supprimé(s).","%1 Datei(en) gelöscht.","Удалено файлов: %1.","%1 個のファイルを削除しました。","%1개 파일 삭제됨.");
    add("A folder deletion is already in progress.","已有文件夹删除任务在进行中。","已有資料夾刪除任務在進行中。","Ya hay una eliminación de carpeta en curso.","Une suppression de dossier est déjà en cours.","Eine Ordnerlöschung läuft bereits.","Удаление папки уже выполняется.","フォルダの削除が既に進行中です。","폴더 삭제가 이미 진행 중입니다.");
    add("Queued",            "排队中",    "排隊中",    "En cola",      "En file",       "In Warteschlange","В очереди",     "待機中",        "대기 중");
    add("Running",           "进行中",    "進行中",    "En curso",     "En cours",      "Läuft",         "Выполняется",     "実行中",        "진행 중");
    add("Paused",            "已暂停",    "已暫停",    "Pausado",      "En pause",      "Pausiert",      "Приостановлено",  "一時停止",      "일시 중지");
    add("Completed",         "已完成",    "已完成",    "Completado",   "Terminé",       "Abgeschlossen", "Завершено",       "完了",          "완료");
    add("Failed",            "失败",      "失敗",      "Fallido",      "Échoué",        "Fehlgeschlagen","Ошибка",          "失敗",          "실패");
    add("Canceled",          "已取消",    "已取消",    "Cancelado",    "Annulé",        "Abgebrochen",   "Отменено",        "キャンセル済み","취소됨");
    add("Active: %1   Completed: %2   Failed: %3",
        "进行中：%1   已完成：%2   失败：%3",
        "進行中：%1   已完成：%2   失敗：%3",
        "Activos: %1   Completados: %2   Fallidos: %3",
        "Actifs : %1   Terminés : %2   Échoués : %3",
        "Aktiv: %1   Abgeschlossen: %2   Fehlgeschlagen: %3",
        "Активно: %1   Завершено: %2   Ошибки: %3",
        "実行中: %1   完了: %2   失敗: %3",
        "진행 중: %1   완료: %2   실패: %3");

    // ---- SSH session dialog ----
    add("SSH Session",       "SSH 会话",  "SSH 工作階段","Sesión SSH",  "Session SSH",   "SSH-Sitzung",   "SSH-сеанс",       "SSHセッション", "SSH 세션");
    add("Auto-attach tmux session","自动接入 tmux 会话","自動接入 tmux 工作階段","Adjuntar sesión tmux automáticamente","Attacher auto. la session tmux","tmux-Sitzung automatisch verbinden","Авто-подключение к сессии tmux","tmux セッションに自動接続","tmux 세션 자동 연결");
    add("tmux session name (default: muxterm)","tmux 会话名（默认：muxterm）","tmux 工作階段名稱（預設：muxterm）","Nombre de sesión tmux (predet.: muxterm)","Nom de session tmux (défaut : muxterm)","tmux-Sitzungsname (Standard: muxterm)","Имя сессии tmux (по умолч.: muxterm)","tmux セッション名（既定: muxterm）","tmux 세션 이름(기본: muxterm)");
    add("extra ssh arguments (optional)","额外 ssh 参数（可选）","額外 ssh 參數（選用）","Argumentos ssh adicionales (opcional)","Arguments ssh supplémentaires (facultatif)","Zusätzliche ssh-Argumente (optional)","Доп. аргументы ssh (необязательно)","追加の ssh 引数（任意）","추가 ssh 인자(선택)");
    add("Name:",             "名称：",    "名稱：",    "Nombre:",      "Nom :",         "Name:",         "Имя:",            "名前：",        "이름:");
    add("Private key:",      "私钥：",    "私鑰：",    "Clave privada:","Clé privée :", "Privater Schlüssel:","Закрытый ключ:","秘密鍵：",      "개인 키:");
    add("tmux name:",        "tmux 名称：","tmux 名稱：","Nombre tmux:", "Nom tmux :",    "tmux-Name:",    "Имя tmux:",       "tmux 名：",     "tmux 이름:");
    add("Extra args:",       "额外参数：","額外參數：","Args. extra:", "Args suppl. :", "Extra-Args:",   "Доп. аргументы:", "追加引数：",    "추가 인자:");
    add("Select private key","选择私钥",  "選擇私鑰",  "Seleccionar clave privada","Sélectionner la clé privée","Privaten Schlüssel wählen","Выбрать закрытый ключ","秘密鍵を選択","개인 키 선택");

    // ---- Serial / UART dialog ----
    add("Serial / UART Configuration","串口 / UART 配置","序列 / UART 設定","Configuración serie / UART","Configuration série / UART","Seriell-/UART-Konfiguration","Настройка Serial / UART","シリアル / UART 設定","시리얼/UART 구성");
    add("Even",              "偶校验",    "偶同位",    "Par",          "Paire",         "Gerade",        "Чётный",          "偶数",          "짝수");
    add("Odd",               "奇校验",    "奇同位",    "Impar",        "Impaire",       "Ungerade",      "Нечётный",        "奇数",          "홀수");
    add("Mark",              "标记",      "標記",      "Marca",        "Marque",        "Mark",          "Метка",           "マーク",        "마크");
    add("Space",             "空格",      "空格",      "Espacio",      "Espace",        "Leerzeichen",   "Пробел",          "スペース",      "스페이스");
    add("RTS/CTS",           "RTS/CTS",   "RTS/CTS",   "RTS/CTS",      "RTS/CTS",       "RTS/CTS",       "RTS/CTS",         "RTS/CTS",       "RTS/CTS");
    add("XON/XOFF",          "XON/XOFF",  "XON/XOFF",  "XON/XOFF",     "XON/XOFF",      "XON/XOFF",      "XON/XOFF",        "XON/XOFF",      "XON/XOFF");
    add("Local echo",        "本地回显",  "本機回應",  "Eco local",    "Écho local",    "Lokales Echo",  "Локальное эхо",   "ローカルエコー","로컬 에코");
    add("Serial line:",      "串口线路：","序列線路：","Línea serie:", "Ligne série :", "Serielle Leitung:","Serial-линия:","シリアル回線：","시리얼 회선:");
    add("Speed (baud):",     "速率（波特）：","速率（鮑率）：","Velocidad (baudios):","Vitesse (bauds) :","Geschwindigkeit (Baud):","Скорость (бод):","速度（ボー）：","속도(보드):");
    add("Data bits:",        "数据位：",  "資料位元：","Bits de datos:","Bits de données :","Datenbits:",  "Биты данных:",    "データビット：","데이터 비트:");
    add("Parity:",           "校验：",    "同位：",    "Paridad:",     "Parité :",      "Parität:",      "Чётность:",       "パリティ：",    "패리티:");
    add("Stop bits:",        "停止位：",  "停止位元：","Bits de parada:","Bits d'arrêt :","Stoppbits:",  "Стоп-биты:",      "ストップビット：","정지 비트:");
    add("Flow control:",     "流控：",    "流量控制：","Control de flujo:","Contrôle de flux :","Flusskontrolle:","Управление потоком:","フロー制御：","흐름 제어:");
    add("Enter sends:",      "回车发送：","Enter 送出：","Enter envía:","Entrée envoie :","Enter sendet:", "Enter отправляет:","Enter 送信：", "Enter 전송:");

    // ---- Find / Replace dialog ----
    add("Find / Replace",    "查找 / 替换","尋找 / 取代","Buscar / Reemplazar","Rechercher / Remplacer","Suchen / Ersetzen","Найти / Заменить","検索 / 置換","찾기/바꾸기");
    add("Match case",        "区分大小写","區分大小寫","Coincidir mayúsculas","Respecter la casse","Groß-/Kleinschreibung","Учитывать регистр","大文字小文字を区別","대소문자 구분");
    add("Whole word only",   "全字匹配",  "全字匹配",  "Solo palabra completa","Mot entier seulement","Nur ganzes Wort","Только слово целиком","単語全体のみ","단어 전체만");
    add("Regular expression","正则表达式","正規表達式","Expresión regular","Expression régulière","Regulärer Ausdruck","Регулярное выражение","正規表現","정규식");
    add("Wrap around",       "循环查找",  "循環尋找",  "Buscar en bucle","Recherche circulaire","Umlaufend suchen","Искать по кругу","折り返して検索","순환 검색");
    add("Find what:",        "查找内容：","尋找內容：","Buscar:",      "Rechercher :",  "Suchen nach:",  "Найти:",          "検索する文字列：","찾을 내용:");
    add("Find Next",         "查找下一个","尋找下一個","Buscar siguiente","Suivant",     "Weitersuchen",  "Найти далее",     "次を検索",      "다음 찾기");
    add("Find Previous",     "查找上一个","尋找上一個","Buscar anterior","Précédent",    "Vorheriges",    "Найти ранее",     "前を検索",      "이전 찾기");
    add("Replace with:",     "替换为：",  "取代為：",  "Reemplazar con:","Remplacer par :","Ersetzen durch:","Заменить на:","置換後の文字列：","바꿀 내용:");
    add("Replace",           "替换",      "取代",      "Reemplazar",   "Remplacer",     "Ersetzen",      "Заменить",        "置換",          "바꾸기");
    add("Replace All",       "全部替换",  "全部取代",  "Reemplazar todo","Tout remplacer","Alle ersetzen","Заменить всё","すべて置換","모두 바꾸기");
    add("Find",              "查找",      "尋找",      "Buscar",       "Rechercher",    "Suchen",        "Найти",           "検索",          "찾기");
    add("Options",           "选项",      "選項",      "Opciones",     "Options",       "Optionen",      "Параметры",       "オプション",    "옵션");
    add("Close",             "关闭",      "關閉",      "Cerrar",       "Fermer",        "Schließen",     "Закрыть",         "閉じる",        "닫기");
    add("Invalid regular expression.","正则表达式无效。","正規表達式無效。","Expresión regular no válida.","Expression régulière non valide.","Ungültiger regulärer Ausdruck.","Недопустимое регулярное выражение.","正規表現が無効です。","잘못된 정규식입니다.");
    add("Count",             "计数",      "計數",      "Conteo",       "Nombre",        "Anzahl",        "Количество",      "件数",          "개수");
    add("%1 occurrence(s) found.","找到 %1 处。","找到 %1 處。","%1 coincidencia(s) encontrada(s).","%1 occurrence(s) trouvée(s).","%1 Treffer gefunden.","Найдено совпадений: %1.","%1 件見つかりました。","%1개를 찾았습니다.");

    // ---- Go To Line dialog ----
    add("Go To Line",        "跳转到行",  "跳至行",    "Ir a la línea","Aller à la ligne","Gehe zu Zeile","Перейти к строке","行へ移動","줄로 이동");
    add("Line number (1 - %1):","行号（1 - %1）：","行號（1 - %1）：","Número de línea (1 - %1):","Numéro de ligne (1 - %1) :","Zeilennummer (1 - %1):","Номер строки (1 - %1):","行番号（1 - %1）：","줄 번호(1 - %1):");

    // ---- Help ----
    add("&About Terminal++", "关于 Terminal++(&A)","關於 Terminal++(&A)","&Acerca de Terminal++","À &propos de Terminal++","&Über Terminal++","&О программе Terminal++","Terminal++について(&A)","Terminal++ 정보(&A)");
    add("About Terminal++",  "关于 Terminal++","關於 Terminal++","Acerca de Terminal++","À propos de Terminal++","Über Terminal++","О программе Terminal++","Terminal++について","Terminal++ 정보");

    // ---- About dialog body ----
    add("A unified Qt 6 application combining:",
        "一款集成以下功能的 Qt 6 应用：","一款整合以下功能的 Qt 6 應用：",
        "Una aplicación Qt 6 unificada que combina:","Une application Qt 6 unifiée combinant :",
        "Eine einheitliche Qt-6-Anwendung mit:","Единое Qt 6 приложение, объединяющее:",
        "次を統合した Qt 6 アプリケーション：","다음을 결합한 통합 Qt 6 애플리케이션:");
    add("Code editor", "代码编辑器","程式碼編輯器","Editor de código","Éditeur de code","Code-Editor","Редактор кода","コードエディタ","코드 편집기");
    add("multi-tab editor with syntax highlighting, themes, find/replace, encodings, column/block edit, clipboard history, auto-save, a built-in hex viewer/editor, and a tab-bar context menu (close / close others / close to the left or right).",
        "多标签编辑器，含语法高亮、主题、查找/替换、编码、列/块编辑、剪贴板历史、自动保存、内置十六进制查看/编辑器，以及标签右键菜单（关闭/关闭其他/关闭左侧或右侧）。",
        "多分頁編輯器，含語法高亮、佈景主題、尋找/取代、編碼、欄/區塊編輯、剪貼簿歷史、自動儲存、內建十六進位檢視/編輯器，以及分頁右鍵選單（關閉/關閉其他/關閉左側或右側）。",
        "editor de varias pestañas con resaltado de sintaxis, temas, buscar/reemplazar, codificaciones, edición de columna/bloque, historial del portapapeles, autoguardado, un visor/editor hexadecimal integrado y un menú contextual de pestañas (cerrar / cerrar otras / cerrar a la izquierda o derecha).",
        "éditeur multi-onglets avec coloration syntaxique, thèmes, rechercher/remplacer, encodages, édition colonne/bloc, historique du presse-papiers, enregistrement auto, un visualiseur/éditeur hexadécimal intégré et un menu contextuel d'onglets (fermer / fermer les autres / fermer à gauche ou à droite).",
        "Multi-Tab-Editor mit Syntaxhervorhebung, Designs, Suchen/Ersetzen, Kodierungen, Spalten-/Blockbearbeitung, Zwischenablage-Verlauf, Autospeichern, integriertem Hex-Viewer/-Editor und einem Tab-Kontextmenü (Schließen / Andere schließen / Links oder rechts schließen).",
        "многовкладочный редактор с подсветкой синтаксиса, темами, поиском/заменой, кодировками, редактированием столбцов/блоков, историей буфера, автосохранением, встроенным hex-просмотрщиком/редактором и контекстным меню вкладок (закрыть / закрыть другие / закрыть слева или справа).",
        "構文強調・テーマ・検索/置換・エンコード・列/ブロック編集・クリップボード履歴・自動保存・内蔵Hexビューア/エディタ・タブ右クリックメニュー（閉じる/他を閉じる/左右を閉じる）を備えたマルチタブエディタ。",
        "구문 강조, 테마, 찾기/바꾸기, 인코딩, 열/블록 편집, 클립보드 기록, 자동 저장, 내장 Hex 뷰어/편집기, 탭 우클릭 메뉴(닫기/다른 탭 닫기/왼쪽·오른쪽 닫기)를 갖춘 다중 탭 편집기.");
    add("SSH terminal", "SSH 终端","SSH 終端","Terminal SSH","Terminal SSH","SSH-Terminal","SSH-терминал","SSH 端末","SSH 터미널");
    add("native libssh2 SSH sessions with tmux support and VT100/256-color rendering; passwords encrypted with Windows DPAPI.",
        "基于 libssh2 的原生 SSH 会话，支持 tmux 与 VT100/256 色渲染；密码使用 Windows DPAPI 加密。",
        "基於 libssh2 的原生 SSH 工作階段，支援 tmux 與 VT100/256 色渲染；密碼以 Windows DPAPI 加密。",
        "sesiones SSH nativas con libssh2, soporte tmux y renderizado VT100/256 colores; contraseñas cifradas con Windows DPAPI.",
        "sessions SSH natives via libssh2, prise en charge tmux et rendu VT100/256 couleurs ; mots de passe chiffrés avec Windows DPAPI.",
        "native SSH-Sitzungen mit libssh2, tmux-Unterstützung und VT100/256-Farben; Passwörter mit Windows DPAPI verschlüsselt.",
        "нативные SSH-сеансы на libssh2 с поддержкой tmux и рендерингом VT100/256 цветов; пароли шифруются Windows DPAPI.",
        "libssh2 ネイティブ SSH セッション（tmux 対応・VT100/256 色描画）、パスワードは Windows DPAPI で暗号化。",
        "libssh2 기반 네이티브 SSH 세션(tmux 지원, VT100/256색 렌더링); 비밀번호는 Windows DPAPI로 암호화.");
    add("configurable serial console sharing the same ANSI engine.",
        "可配置的串口控制台，与终端共用同一 ANSI 引擎。",
        "可設定的序列主控台，與終端共用同一 ANSI 引擎。",
        "consola serie configurable que comparte el mismo motor ANSI.",
        "console série configurable partageant le même moteur ANSI.",
        "konfigurierbare serielle Konsole mit derselben ANSI-Engine.",
        "настраиваемая последовательная консоль с тем же движком ANSI.",
        "同じ ANSI エンジンを共有する設定可能なシリアルコンソール。",
        "동일한 ANSI 엔진을 공유하는 구성 가능한 시리얼 콘솔.");
    add("File Compare", "文件比较","檔案比較","Comparación de archivos","Comparaison de fichiers","Dateivergleich","Сравнение файлов","ファイル比較","파일 비교");
    add("Side-by-side text, folder, hex and picture comparison.",
        "并排的文本、文件夹、十六进制与图片比较。",
        "並排的文字、資料夾、十六進位與圖片比較。",
        "comparación en paralelo de texto, carpetas, hex e imágenes.",
        "comparaison côte à côte de texte, dossier, hexadécimal et image.",
        "Text-, Ordner-, Hex- und Bildvergleich nebeneinander.",
        "параллельное сравнение текста, папок, hex и изображений.",
        "テキスト・フォルダ・Hex・画像の並列比較。",
        "텍스트, 폴더, 16진수, 사진의 나란히 비교.");
    add("Cloud Drives",      "云盘",      "雲端",      "Unidades en la nube","Disques cloud","Cloud-Laufwerke","Облачные диски","クラウドドライブ","클라우드 드라이브");
    add("Baidu Pan and WebDAV browsing, upload/download (files and folders), rename, delete, and streaming playback, with a Transfers panel showing live progress and history.",
        "支持百度网盘与 WebDAV 浏览、上传/下载（文件与文件夹）、重命名、删除与边下边播，并配有实时显示进度与历史的传输面板。",
        "支援百度網盤與 WebDAV 瀏覽、上傳/下載（檔案與資料夾）、重新命名、刪除與串流播放，並配有即時顯示進度與歷史的傳輸面板。",
        "navegación por Baidu Pan y WebDAV, subida/descarga (archivos y carpetas), renombrar, eliminar y reproducción en streaming, con un panel de transferencias que muestra progreso e historial en vivo.",
        "navigation Baidu Pan et WebDAV, téléversement/téléchargement (fichiers et dossiers), renommage, suppression et lecture en streaming, avec un panneau de transferts affichant la progression et l'historique en direct.",
        "Baidu Pan- und WebDAV-Browsing, Hoch-/Herunterladen (Dateien und Ordner), Umbenennen, Löschen und Streaming-Wiedergabe, mit einem Übertragungspanel für Live-Fortschritt und Verlauf.",
        "просмотр Baidu Pan и WebDAV, загрузка/скачивание (файлов и папок), переименование, удаление и потоковое воспроизведение, с панелью передач, показывающей прогресс и историю в реальном времени.",
        "Baidu Pan と WebDAV の参照、アップロード/ダウンロード（ファイル・フォルダ）、名前変更、削除、ストリーミング再生に対応し、進捗と履歴をリアルタイム表示する転送パネルを備える。",
        "Baidu Pan 및 WebDAV 탐색, 업로드/다운로드(파일 및 폴더), 이름 바꾸기, 삭제, 스트리밍 재생을 지원하며 실시간 진행률과 기록을 보여주는 전송 패널을 제공합니다.");
    add("a built-in, dependency-free PDF engine: page navigation (first / previous / next / last, jump-to-page) with zoom in / zoom out, actual size, fit width and fit page; pages are always scaled proportionally (the original aspect ratio is preserved) and /Rotate is honoured. Reads PDF 1.0-2.0 including compressed object streams, cross-reference streams and the FlateDecode, LZWDecode, RunLengthDecode, ASCIIHex and ASCII85 filters with PNG/TIFF predictors; renders DeviceRGB / DeviceGray / DeviceCMYK / Indexed / ICCBased images at 1, 2, 4, 8 and 16 bits per component. Annotate (highlight, underline, strikethrough, text box, sticky note, pen, stamp), manage pages (merge, split, delete / insert / extract, rotate, reorder), hand-drawn and image signatures, PDF <-> image conversion, PDF -> Word (RTF text, with layout loss), compression, text and image watermarks, open-password protection, and AcroForm filling. Every PDF control is fully localized in all 9 UI languages and follows the language switch live.",
        "内置、无外部依赖的 PDF 引擎：页面导航（首页/上一页/下一页/末页、跳转到指定页），并提供放大/缩小、实际大小、适合宽度与适合页面；页面始终按比例缩放（保留原始宽高比），并遵循 /Rotate 旋转设置。可读取 PDF 1.0-2.0，包括压缩对象流、交叉引用流，以及 FlateDecode、LZWDecode、RunLengthDecode、ASCIIHex 与 ASCII85 过滤器和 PNG/TIFF 预测器；可渲染 DeviceRGB / DeviceGray / DeviceCMYK / Indexed / ICCBased 图像，支持每分量 1、2、4、8、16 位。支持批注（高亮、下划线、删除线、文本框、便笺、画笔、图章）、页面管理（合并、拆分、删除/插入/提取、旋转、重排）、手绘与图片签名、PDF ↔ 图片转换、PDF → Word（RTF 文本，有排版损耗）、压缩、文字与图片水印、打开密码保护以及 AcroForm 表单填写。所有 PDF 控件均已完整本地化为 9 种界面语言，并随语言切换实时更新。",
        "內建、無外部依賴的 PDF 引擎：頁面導覽（第一頁/上一頁/下一頁/最後一頁、跳至指定頁），並提供放大/縮小、實際大小、符合寬度與符合頁面；頁面一律等比例縮放（保留原始長寬比），並遵循 /Rotate 旋轉設定。可讀取 PDF 1.0-2.0，包括壓縮物件流、交互參照流，以及 FlateDecode、LZWDecode、RunLengthDecode、ASCIIHex 與 ASCII85 過濾器和 PNG/TIFF 預測器；可算繪 DeviceRGB / DeviceGray / DeviceCMYK / Indexed / ICCBased 圖像，支援每分量 1、2、4、8、16 位元。支援註解（螢光標示、底線、刪除線、文字方塊、便利貼、畫筆、圖章）、頁面管理（合併、分割、刪除/插入/擷取、旋轉、重排）、手繪與圖片簽名、PDF ↔ 圖片轉換、PDF → Word（RTF 文字，有排版損耗）、壓縮、文字與圖片水印、開啟密碼保護以及 AcroForm 表單填寫。所有 PDF 控件均已完整本地化為 9 種介面語言，並隨語言切換即時更新。",
        "un motor PDF integrado y sin dependencias: navegación de páginas (primera / anterior / siguiente / última, ir a página) con acercar / alejar, tamaño real, ajustar al ancho y ajustar a la página; las páginas siempre se escalan proporcionalmente (se conserva la relación de aspecto original) y se respeta /Rotate. Lee PDF 1.0-2.0 incluyendo flujos de objetos comprimidos, flujos de referencias cruzadas y los filtros FlateDecode, LZWDecode, RunLengthDecode, ASCIIHex y ASCII85 con predictores PNG/TIFF; representa imágenes DeviceRGB / DeviceGray / DeviceCMYK / Indexed / ICCBased a 1, 2, 4, 8 y 16 bits por componente. Anotaciones (resaltar, subrayar, tachar, cuadro de texto, nota adhesiva, lápiz, sello), gestión de páginas (combinar, dividir, eliminar / insertar / extraer, rotar, reordenar), firmas dibujadas y de imagen, conversión PDF ↔ imagen, PDF → Word (texto RTF, con pérdida de diseño), compresión, marcas de agua de texto e imagen, protección con contraseña de apertura y relleno de AcroForm. Todos los controles PDF están totalmente localizados en los 9 idiomas y siguen el cambio de idioma en vivo.",
        "un moteur PDF intégré et sans dépendance : navigation entre les pages (première / précédente / suivante / dernière, aller à la page) avec zoom avant / arrière, taille réelle, ajuster à la largeur et ajuster à la page ; les pages sont toujours mises à l'échelle proportionnellement (le rapport d'aspect d'origine est préservé) et /Rotate est respecté. Lit les PDF 1.0-2.0 y compris les flux d'objets compressés, les flux de références croisées et les filtres FlateDecode, LZWDecode, RunLengthDecode, ASCIIHex et ASCII85 avec prédicteurs PNG/TIFF ; restitue les images DeviceRGB / DeviceGray / DeviceCMYK / Indexed / ICCBased à 1, 2, 4, 8 et 16 bits par composante. Annotations (surligner, souligner, barrer, zone de texte, pense-bête, stylo, tampon), gestion des pages (fusionner, diviser, supprimer / insérer / extraire, pivoter, réordonner), signatures dessinées et image, conversion PDF ↔ image, PDF → Word (texte RTF, avec perte de mise en page), compression, filigranes texte et image, protection par mot de passe d'ouverture et remplissage d'AcroForm. Tous les contrôles PDF sont entièrement localisés dans les 9 langues et suivent le changement de langue en direct.",
        "eine integrierte, abhängigkeitsfreie PDF-Engine: Seitennavigation (erste / vorherige / nächste / letzte, Gehe zu Seite) mit Vergrößern / Verkleinern, Originalgröße, Breite anpassen und Seite anpassen; Seiten werden stets proportional skaliert (das ursprüngliche Seitenverhältnis bleibt erhalten) und /Rotate wird beachtet. Liest PDF 1.0-2.0 einschließlich komprimierter Objektströme, Querverweisströme sowie der Filter FlateDecode, LZWDecode, RunLengthDecode, ASCIIHex und ASCII85 mit PNG-/TIFF-Prädiktoren; rendert DeviceRGB- / DeviceGray- / DeviceCMYK- / Indexed- / ICCBased-Bilder mit 1, 2, 4, 8 und 16 Bit pro Komponente. Anmerkungen (Hervorheben, Unterstreichen, Durchstreichen, Textfeld, Notizzettel, Stift, Stempel), Seitenverwaltung (Zusammenführen, Teilen, Löschen / Einfügen / Extrahieren, Drehen, Neuordnen), handgezeichnete und Bildsignaturen, PDF-↔-Bild-Konvertierung, PDF → Word (RTF-Text, mit Layoutverlust), Komprimierung, Text- und Bildwasserzeichen, Öffnen-Kennwortschutz und AcroForm-Ausfüllen. Alle PDF-Bedienelemente sind in allen 9 Sprachen vollständig lokalisiert und folgen dem Sprachwechsel live.",
        "встроенный PDF-движок без внешних зависимостей: навигация по страницам (первая / предыдущая / следующая / последняя, переход к странице) с увеличением / уменьшением, реальным размером, по ширине и по размеру страницы; страницы всегда масштабируются пропорционально (исходное соотношение сторон сохраняется), учитывается /Rotate. Читает PDF 1.0-2.0, включая сжатые объектные потоки, потоки перекрёстных ссылок и фильтры FlateDecode, LZWDecode, RunLengthDecode, ASCIIHex и ASCII85 с предикторами PNG/TIFF; отрисовывает изображения DeviceRGB / DeviceGray / DeviceCMYK / Indexed / ICCBased при 1, 2, 4, 8 и 16 битах на компонент. Аннотации (выделение, подчёркивание, зачёркивание, текстовое поле, заметка, карандаш, штамп), управление страницами (объединение, разделение, удаление / вставка / извлечение, поворот, переупорядочивание), рисованные и графические подписи, преобразование PDF ↔ изображение, PDF → Word (текст RTF, с потерей макета), сжатие, текстовые и графические водяные знаки, защита паролем открытия и заполнение AcroForm. Все элементы управления PDF полностью локализованы на все 9 языков и мгновенно следуют смене языка.",
        "依存関係のない内蔵 PDF エンジン：ページ移動（最初/前/次/最後のページ、ページ番号へジャンプ）に加え、拡大/縮小、実際のサイズ、幅に合わせる、ページに合わせるに対応。ページは常に縦横比を保ったまま比例拡大縮小され、/Rotate も反映されます。圧縮オブジェクトストリーム、相互参照ストリーム、FlateDecode・LZWDecode・RunLengthDecode・ASCIIHex・ASCII85 フィルターと PNG/TIFF 予測器を含む PDF 1.0-2.0 を読み取り、DeviceRGB / DeviceGray / DeviceCMYK / Indexed / ICCBased 画像を 1・2・4・8・16 ビット/成分で描画します。注釈（ハイライト、下線、取り消し線、テキストボックス、付箋、ペン、スタンプ）、ページ管理（結合、分割、削除/挿入/抽出、回転、並べ替え）、手書きおよび画像署名、PDF ↔ 画像変換、PDF → Word（RTF テキスト、レイアウトは失われます）、圧縮、テキストおよび画像の透かし、開くパスワード保護、AcroForm 入力に対応。すべての PDF コントロールは 9 言語に完全対応し、言語切り替えに即座に追従します。",
        "의존성 없는 내장 PDF 엔진: 페이지 이동(첫/이전/다음/마지막 페이지, 페이지로 이동)과 확대/축소, 실제 크기, 너비 맞춤, 페이지 맞춤을 제공합니다. 페이지는 항상 원본 가로세로 비율을 유지하며 비례 축척되고 /Rotate 값도 반영됩니다. 압축 객체 스트림, 상호 참조 스트림, FlateDecode·LZWDecode·RunLengthDecode·ASCIIHex·ASCII85 필터와 PNG/TIFF 예측기를 포함한 PDF 1.0-2.0을 읽고, DeviceRGB / DeviceGray / DeviceCMYK / Indexed / ICCBased 이미지를 성분당 1, 2, 4, 8, 16비트로 렌더링합니다. 주석(형광펜, 밑줄, 취소선, 텍스트 상자, 메모, 펜, 스탬프), 페이지 관리(병합, 분할, 삭제/삽입/추출, 회전, 재배열), 손그림 및 이미지 서명, PDF ↔ 이미지 변환, PDF → Word(RTF 텍스트, 레이아웃 손실), 압축, 텍스트·이미지 워터마크, 열기 비밀번호 보호, AcroForm 채우기를 지원합니다. 모든 PDF 컨트롤은 9개 언어로 완전히 현지화되어 언어 전환에 즉시 반영됩니다.");

    add("Multi-language UI (9 languages), VS Code-like themes applied across all tabs.",
        "多语言界面（9 种语言），VS Code 系主题应用于所有标签页。",
        "多語言介面（9 種語言），VS Code 系佈景主題套用於所有分頁。",
        "Interfaz multilingüe (9 idiomas), temas estilo VS Code en todas las pestañas.",
        "Interface multilingue (9 langues), thèmes façon VS Code sur tous les onglets.",
        "Mehrsprachige Oberfläche (9 Sprachen), VS-Code-artige Designs auf allen Tabs.",
        "Многоязычный интерфейс (9 языков), темы в стиле VS Code во всех вкладках.",
        "多言語 UI（9 言語）、VS Code 風テーマを全タブに適用。",
        "다국어 UI(9개 언어), 모든 탭에 적용되는 VS Code 스타일 테마.");
    add("Built with MinGW + Qt 6 + libssh2.",
        "使用 MinGW + Qt 6 + libssh2 构建。","以 MinGW + Qt 6 + libssh2 建置。",
        "Compilado con MinGW + Qt 6 + libssh2.","Compilé avec MinGW + Qt 6 + libssh2.",
        "Erstellt mit MinGW + Qt 6 + libssh2.","Собрано с MinGW + Qt 6 + libssh2.",
        "MinGW + Qt 6 + libssh2 でビルド。","MinGW + Qt 6 + libssh2로 빌드.");

    // ---- Status bar / misc ----
    add("Ready",             "就绪",      "就緒",      "Listo",        "Prêt",          "Bereit",        "Готово",          "準備完了",      "준비됨");
    add("Plain Text",        "纯文本",    "純文字",    "Texto plano",  "Texte brut",    "Nur-Text",      "Простой текст",   "プレーンテキスト","일반 텍스트");
    add("Terminal",          "终端",      "終端",      "Terminal",     "Terminal",      "Terminal",      "Терминал",        "端末",          "터미널");
    add("Line Ending",       "行尾",      "行尾",      "Fin de línea", "Fin de ligne",  "Zeilenende",    "Конец строки",    "改行コード",    "줄 끝");
    add("Reopened with encoding: %1",
        "已用编码重新打开：%1",
        "已用編碼重新開啟：%1",
        "Reabierto con la codificación: %1",
        "Rouvert avec l'encodage : %1",
        "Mit Kodierung neu geöffnet: %1",
        "Открыт заново в кодировке: %1",
        "エンコードを変えて再読み込み: %1",
        "인코딩으로 다시 열기: %1");
    add("Windows (CRLF)",    "Windows (CRLF)","Windows (CRLF)","Windows (CRLF)","Windows (CRLF)","Windows (CRLF)","Windows (CRLF)","Windows (CRLF)","Windows (CRLF)");
    add("Unix (LF)",         "Unix (LF)", "Unix (LF)", "Unix (LF)",    "Unix (LF)",     "Unix (LF)",     "Unix (LF)",       "Unix (LF)",     "Unix (LF)");
    add("Mac (CR)",          "Mac (CR)",  "Mac (CR)",  "Mac (CR)",     "Mac (CR)",      "Mac (CR)",      "Mac (CR)",        "Mac (CR)",      "Mac (CR)");
    add("COLUMN",            "列编辑",    "欄編輯",    "COLUMNA",      "COLONNE",       "SPALTE",        "СТОЛБЕЦ",         "列編集",        "열 편집");
    add("Ln %1, Col %2",     "行 %1, 列 %2","行 %1, 欄 %2","Ln %1, Col %2","Lg %1, Col %2","Z %1, Sp %2","Стр %1, Стлб %2","行 %1, 列 %2","행 %1, 열 %2");
    add("new",               "新建",      "新增",      "nuevo",        "nouveau",       "neu",           "новый",           "新規",          "새 파일");
    add("untitled",          "未命名",    "未命名",    "sin título",   "sans titre",    "unbenannt",     "без имени",       "無題",          "제목 없음");

    // ---- Editor/save/session messages (status bar + message boxes) ----
    add("Open",              "打开",      "開啟",      "Abrir",        "Ouvrir",        "Öffnen",        "Открыть",         "開く",          "열기");
    add("Save",              "保存",      "儲存",      "Guardar",      "Enregistrer",   "Speichern",     "Сохранить",       "保存",          "저장");
    add("Cannot open file:\n%1",
        "无法打开文件：\n%1","無法開啟檔案：\n%1","No se puede abrir el archivo:\n%1",
        "Impossible d'ouvrir le fichier :\n%1","Datei kann nicht geöffnet werden:\n%1",
        "Не удалось открыть файл:\n%1","ファイルを開けません：\n%1","파일을 열 수 없습니다:\n%1");
    add("Cannot write file.",
        "无法写入文件。","無法寫入檔案。","No se puede escribir el archivo.",
        "Impossible d'écrire le fichier.","Datei kann nicht geschrieben werden.",
        "Не удалось записать файл.","ファイルを書き込めません。","파일을 쓸 수 없습니다.");
    add("Saved %1",          "已保存 %1", "已儲存 %1", "Guardado %1",  "Enregistré %1", "%1 gespeichert","Сохранено %1",    "%1 を保存しました","%1 저장됨");
    add("Terminal++",        "Terminal++","Terminal++","Terminal++",  "Terminal++",    "Terminal++",    "Terminal++",      "Terminal++",    "Terminal++");
    add("The document \"%1\" has unsaved changes.\nDo you want to save?",
        "文档 \"%1\" 有未保存的更改。\n是否要保存？",
        "文件 \"%1\" 有未儲存的變更。\n是否要儲存？",
        "El documento \"%1\" tiene cambios sin guardar.\n¿Desea guardar?",
        "Le document \"%1\" a des modifications non enregistrées.\nVoulez-vous enregistrer ?",
        "Das Dokument \"%1\" hat ungespeicherte Änderungen.\nMöchten Sie speichern?",
        "Документ \"%1\" содержит несохранённые изменения.\nСохранить?",
        "ドキュメント \"%1\" に未保存の変更があります。\n保存しますか？",
        "문서 \"%1\" 에 저장되지 않은 변경 사항이 있습니다.\n저장할까요?");
    add("Host is required.", "必须填写主机。","必須填寫主機。","El host es obligatorio.","L'hôte est requis.","Host ist erforderlich.","Требуется указать хост.","ホストは必須です。","호스트는 필수입니다.");
    add("Serial",            "串口",      "序列",      "Serie",        "Série",         "Seriell",       "Serial",          "シリアル",      "시리얼");
    add("No serial port selected.","未选择串口。","未選擇序列埠。","No se seleccionó ningún puerto serie.","Aucun port série sélectionné.","Kein serieller Port ausgewählt.","Последовательный порт не выбран.","シリアルポートが選択されていません。","시리얼 포트가 선택되지 않았습니다.");
    add("Delete Session",    "删除会话",  "刪除工作階段","Eliminar sesión","Supprimer la session","Sitzung löschen","Удалить сеанс","セッションを削除","세션 삭제");
    add("Delete SSH session \"%1\"?",
        "确定删除 SSH 会话 \"%1\"？","確定刪除 SSH 工作階段 \"%1\"？","¿Eliminar la sesión SSH \"%1\"?",
        "Supprimer la session SSH \"%1\" ?","SSH-Sitzung \"%1\" löschen?","Удалить SSH-сеанс \"%1\"?",
        "SSH セッション \"%1\" を削除しますか？","SSH 세션 \"%1\" 을(를) 삭제할까요?");
    add("Delete serial session \"%1\"?",
        "确定删除串口会话 \"%1\"？","確定刪除序列工作階段 \"%1\"？","¿Eliminar la sesión serie \"%1\"?",
        "Supprimer la session série \"%1\" ?","Seriell-Sitzung \"%1\" löschen?","Удалить Serial-сеанс \"%1\"?",
        "シリアルセッション \"%1\" を削除しますか？","시리얼 세션 \"%1\" 을(를) 삭제할까요?");
    add(" [closed]",         " [已关闭]", " [已關閉]", " [cerrado]",   " [fermé]",      " [geschlossen]"," [закрыто]",      " [切断]",       " [닫힘]");
    add("Theme: %1",         "主题：%1",  "佈景主題：%1","Tema: %1",    "Thème : %1",    "Design: %1",    "Тема: %1",        "テーマ: %1",    "테마: %1");
    add("Auto-save enabled (every %1s)",
        "已启用自动保存（每 %1 秒）","已啟用自動儲存（每 %1 秒）","Autoguardado activado (cada %1s)",
        "Enregistrement auto activé (toutes les %1s)","Autospeichern aktiviert (alle %1s)",
        "Автосохранение включено (каждые %1с)","自動保存を有効化（%1 秒ごと）","자동 저장 켜짐(%1초마다)");
    add("Auto-save disabled","已禁用自动保存","已停用自動儲存","Autoguardado desactivado","Enregistrement auto désactivé","Autospeichern deaktiviert","Автосохранение выключено","自動保存を無効化","자동 저장 꺼짐");
    add("Auto-saved %1 file(s)","已自动保存 %1 个文件","已自動儲存 %1 個檔案","Autoguardado %1 archivo(s)","%1 fichier(s) enregistré(s) auto","%1 Datei(en) automatisch gespeichert","Автосохранено файлов: %1","%1 件を自動保存しました","%1개 파일 자동 저장됨");
    add("Column (block) edit mode — type/Delete/Ctrl+C/Ctrl+V across lines, Esc to exit",
        "列（块）编辑模式 — 可跨行 输入/删除/Ctrl+C/Ctrl+V，按 Esc 退出",
        "欄（區塊）編輯模式 — 可跨行 輸入/刪除/Ctrl+C/Ctrl+V，按 Esc 離開",
        "Modo de edición de columna (bloque) — escribir/Supr/Ctrl+C/Ctrl+V entre líneas, Esc para salir",
        "Mode d'édition colonne (bloc) — saisir/Suppr/Ctrl+C/Ctrl+V sur plusieurs lignes, Échap pour quitter",
        "Spalten-/Blockbearbeitungsmodus — Tippen/Entf/Strg+C/Strg+V über Zeilen, Esc zum Beenden",
        "Режим редактирования столбца (блока) — ввод/Delete/Ctrl+C/Ctrl+V по строкам, Esc для выхода",
        "列（ブロック）編集モード — 複数行で 入力/Delete/Ctrl+C/Ctrl+V、Esc で終了",
        "열(블록) 편집 모드 — 여러 줄에서 입력/Delete/Ctrl+C/Ctrl+V, Esc로 종료");
    add("Cloud",             "云盘",      "雲端",      "Nube",         "Cloud",         "Cloud",         "Облако",          "クラウド",      "클라우드");
    add("&Copy",             "复制(&C)",  "複製(&C)",  "&Copiar",      "&Copier",       "&Kopieren",     "&Копировать",     "コピー(&C)",    "복사(&C)");
    add("&Paste",            "粘贴(&P)",  "貼上(&P)",  "&Pegar",       "C&oller",       "&Einfügen",     "&Вставить",       "貼り付け(&P)",  "붙여넣기(&P)");

    // ---- Image editor ----
    add("image",             "图片",      "圖片",      "imagen",       "image",         "Bild",          "изображение",     "画像",          "이미지");
    add("Image: %1",         "图片：%1",  "圖片：%1",  "Imagen: %1",   "Image : %1",    "Bild: %1",      "Изображение: %1", "画像: %1",      "이미지: %1");
    add("Open as &Image...", "以图片打开(&I)...","以圖片開啟(&I)...","Abrir como &imagen...","Ouvrir comme &image...","Als &Bild öffnen...","Открыть как &изображение...","画像として開く(&I)...","이미지로 열기(&I)...");
    add("Open as Image",     "以图片打开","以圖片開啟","Abrir como imagen","Ouvrir comme image","Als Bild öffnen","Открыть как изображение","画像として開く","이미지로 열기");
    add("Tools",             "工具",      "工具",      "Herramientas", "Outils",        "Werkzeuge",     "Инструменты",     "ツール",        "도구");
    add("Style",             "样式",      "樣式",      "Estilo",       "Style",         "Stil",          "Стиль",           "スタイル",      "스타일");
    add("Adjustments",       "调整",      "調整",      "Ajustes",      "Réglages",      "Anpassungen",   "Настройки",       "調整",          "조정");
    add("Filters",           "滤镜",      "濾鏡",      "Filtros",      "Filtres",       "Filter",        "Фильтры",         "フィルター",    "필터");
    add("Brush",             "画笔",      "畫筆",      "Pincel",       "Pinceau",       "Pinsel",        "Кисть",           "ブラシ",        "브러시");
    add("Eraser",            "橡皮擦",    "橡皮擦",    "Borrador",     "Gomme",         "Radierer",      "Ластик",          "消しゴム",      "지우개");
    add("Line",              "直线",      "直線",      "Línea",        "Ligne",         "Linie",         "Линия",           "直線",          "선");
    add("Rect",              "矩形",      "矩形",      "Rectángulo",   "Rectangle",     "Rechteck",      "Прямоугольник",   "矩形",          "사각형");
    add("Ellipse",           "椭圆",      "橢圓",      "Elipse",       "Ellipse",       "Ellipse",       "Эллипс",          "楕円",          "타원");
    add("Arrow",             "箭头",      "箭頭",      "Flecha",       "Flèche",        "Pfeil",         "Стрелка",         "矢印",          "화살표");
    add("Text",              "文字",      "文字",      "Texto",        "Texte",         "Text",          "Текст",           "テキスト",      "텍스트");
    add("Crop",              "裁剪",      "裁剪",      "Recortar",     "Rogner",        "Zuschneiden",   "Обрезать",        "切り抜き",      "자르기");
    add("Mosaic",            "马赛克",    "馬賽克",    "Mosaico",      "Mosaïque",      "Mosaik",        "Мозаика",         "モザイク",      "모자이크");
    add("Magic Wand",        "魔棒",      "魔術棒",    "Varita mágica","Baguette magique","Zauberstab",  "Волшебная палочка","マジックワンド","요술봉");
    add("Brush Color...",    "画笔颜色...","畫筆顏色...","Color de pincel...","Couleur du pinceau...","Pinselfarbe...","Цвет кисти...","ブラシの色...","브러시 색...");
    add("Text Color...",     "文字颜色...","文字顏色...","Color del texto...","Couleur du texte...","Textfarbe...","Цвет текста...","文字色...","텍스트 색...");
    add("Brush Color",       "画笔颜色",  "畫筆顏色",  "Color de pincel","Couleur du pinceau","Pinselfarbe","Цвет кисти","ブラシの色","브러시 색");
    add("Text Color",        "文字颜色",  "文字顏色",  "Color del texto","Couleur du texte","Textfarbe","Цвет текста","文字色","텍스트 색");
    add("Brush Width:",      "画笔宽度：","畫筆寬度：","Grosor del pincel:","Épaisseur du pinceau :","Pinselbreite:","Ширина кисти:","ブラシの太さ：","브러시 굵기:");
    add("Text Size:",        "文字大小：","文字大小：","Tamaño del texto:","Taille du texte :","Textgröße:","Размер текста:","文字サイズ：","텍스트 크기:");
    add("Mosaic Block:",     "马赛克块：","馬賽克區塊：","Bloque de mosaico:","Bloc mosaïque :","Mosaikblock:","Блок мозаики:","モザイクブロック：","모자이크 블록:");
    add("Wand Tolerance:",   "魔棒容差：","魔術棒容差：","Tolerancia de varita:","Tolérance baguette :","Zauberstab-Toleranz:","Допуск палочки:","ワンド許容値：","요술봉 허용치:");
    add("Brightness",        "亮度",      "亮度",      "Brillo",       "Luminosité",    "Helligkeit",    "Яркость",         "明るさ",        "밝기");
    add("Contrast",          "对比度",    "對比度",    "Contraste",    "Contraste",     "Kontrast",      "Контраст",        "コントラスト",  "대비");
    add("Saturation",        "饱和度",    "飽和度",    "Saturación",   "Saturation",    "Sättigung",     "Насыщенность",    "彩度",          "채도");
    add("Temperature",       "色温",      "色溫",      "Temperatura",  "Température",    "Temperatur",    "Температура",      "色温度",        "색온도");
    add("Exposure",          "曝光",      "曝光",      "Exposición",   "Exposition",    "Belichtung",    "Экспозиция",      "露出",          "노출");
    add("Shadows",           "阴影",      "陰影",      "Sombras",      "Ombres",        "Schatten",      "Тени",            "シャドウ",      "그림자");
    add("Highlights",        "高光",      "高光",      "Luces",        "Hautes lumières","Lichter",      "Света",           "ハイライト",    "하이라이트");
    add("Sharpen",           "锐化",      "銳化",      "Nitidez",      "Netteté",       "Schärfen",      "Резкость",        "シャープ",      "선명화");
    add("Apply",             "应用",      "套用",      "Aplicar",      "Appliquer",     "Anwenden",      "Применить",       "適用",          "적용");
    add("Reset",             "重置",      "重設",      "Restablecer",  "Réinitialiser", "Zurücksetzen",  "Сбросить",        "リセット",      "초기화");
    add("Adjustments applied.","已应用调整。","已套用調整。","Ajustes aplicados.","Réglages appliqués.","Anpassungen angewendet.","Настройки применены.","調整を適用しました。","조정이 적용되었습니다.");
    add("Undo",              "撤销",      "復原",      "Deshacer",     "Annuler",       "Rückgängig",    "Отменить",        "元に戻す",      "실행 취소");
    add("Redo",              "重做",      "重做",      "Rehacer",      "Rétablir",      "Wiederholen",   "Повторить",       "やり直し",      "다시 실행");
    add("Rotate Left",       "向左旋转",  "向左旋轉",  "Girar a la izq.","Pivoter à gauche","Links drehen","Повернуть влево","左に回転","왼쪽 회전");
    add("Rotate Right",      "向右旋转",  "向右旋轉",  "Girar a la der.","Pivoter à droite","Rechts drehen","Повернуть вправо","右に回転","오른쪽 회전");
    add("Free Rotate...",    "自由旋转...","自由旋轉...","Rotación libre...","Rotation libre...","Freies Drehen...","Свободный поворот...","自由回転...","자유 회전...");
    add("Free Rotate",       "自由旋转",  "自由旋轉",  "Rotación libre","Rotation libre","Freies Drehen","Свободный поворот","自由回転","자유 회전");
    add("Angle (degrees):",  "角度（度）：","角度（度）：","Ángulo (grados):","Angle (degrés) :","Winkel (Grad):","Угол (градусы):","角度（度）：","각도(도):");
    add("Straighten...",     "拉直...",   "拉直...",   "Enderezar...", "Redresser...",  "Begradigen...", "Выпрямить...",    "傾き補正...",   "수평 맞추기...");
    add("Straighten",        "拉直",      "拉直",      "Enderezar",    "Redresser",     "Begradigen",    "Выпрямить",       "傾き補正",      "수평 맞추기");
    add("Tilt (degrees):",   "倾斜（度）：","傾斜（度）：","Inclinación (grados):","Inclinaison (degrés) :","Neigung (Grad):","Наклон (градусы):","傾き（度）：","기울기(도):");
    add("Flip H",            "水平翻转",  "水平翻轉",  "Voltear H",    "Miroir H",      "H spiegeln",    "Отразить Г",      "左右反転",      "좌우 반전");
    add("Flip V",            "垂直翻转",  "垂直翻轉",  "Voltear V",    "Miroir V",      "V spiegeln",    "Отразить В",      "上下反転",      "상하 반전");
    add("Select a crop region first (use the Crop tool).",
        "请先用裁剪工具框选裁剪区域。","請先用裁剪工具框選裁剪區域。","Seleccione primero una región de recorte (herramienta Recortar).",
        "Sélectionnez d'abord une zone de rognage (outil Rogner).","Wählen Sie zuerst einen Zuschneidebereich (Werkzeug Zuschneiden).",
        "Сначала выделите область обрезки (инструмент Обрезать).","先に切り抜き領域を選択してください（切り抜きツール）。","먼저 자를 영역을 선택하세요(자르기 도구).");
    add("Resize...",         "改尺寸...", "改尺寸...", "Redimensionar...","Redimensionner...","Größe ändern...","Изменить размер...","サイズ変更...","크기 조정...");
    add("Resize",            "改尺寸",    "改尺寸",    "Redimensionar","Redimensionner","Größe ändern",  "Изменить размер", "サイズ変更",    "크기 조정");
    add("Scale...",          "缩放...",   "縮放...",   "Escalar...",   "Mettre à l'échelle...","Skalieren...","Масштаб...","拡大縮小...","배율...");
    add("Scale",             "缩放",      "縮放",      "Escalar",      "Échelle",       "Skalieren",     "Масштаб",         "拡大縮小",      "배율");
    add("Scale (%):",        "缩放（%）：","縮放（%）：","Escala (%):",  "Échelle (%) :", "Skalierung (%):","Масштаб (%):",   "拡大縮小（%）：","배율(%):");
    add("Width:",            "宽度：",    "寬度：",    "Ancho:",       "Largeur :",     "Breite:",       "Ширина:",         "幅：",          "너비:");
    add("Height:",           "高度：",    "高度：",    "Alto:",        "Hauteur :",     "Höhe:",         "Высота:",         "高さ：",        "높이:");
    add("DPI...",            "DPI...",    "DPI...",    "PPP...",       "PPP...",        "DPI...",        "DPI...",          "DPI...",        "DPI...");
    add("DPI",               "DPI",       "DPI",       "PPP",          "PPP",           "DPI",           "DPI",             "DPI",           "DPI");
    add("DPI:",              "DPI：",     "DPI：",     "PPP:",         "PPP :",         "DPI:",          "DPI:",            "DPI：",         "DPI:");
    add("DPI set to %1.",    "DPI 已设为 %1。","DPI 已設為 %1。","PPP establecido en %1.","PPP réglé sur %1.","DPI auf %1 gesetzt.","DPI установлен в %1.","DPI を %1 に設定しました。","DPI를 %1(으)로 설정함.");
    add("Expand Canvas...",  "扩画布...", "擴畫布...", "Ampliar lienzo...","Agrandir le canevas...","Leinwand erweitern...","Расширить холст...","キャンバス拡張...","캔버스 확장...");
    add("Expand Canvas",     "扩画布",    "擴畫布",    "Ampliar lienzo","Agrandir le canevas","Leinwand erweitern","Расширить холст","キャンバス拡張","캔버스 확장");
    add("Border (px):",      "边框（像素）：","邊框（像素）：","Borde (px):","Bordure (px) :","Rand (px):","Граница (px):","枠（px）：","테두리(px):");
    add("Auto Cutout",       "自动抠图",  "自動去背",  "Recorte automático","Détourage auto","Auto-Freistellen","Авто-вырезание","自動切り抜き","자동 누끼");
    add("Background removed (auto cutout).","已移除背景（自动抠图）。","已移除背景（自動去背）。","Fondo eliminado (recorte automático).","Arrière-plan supprimé (détourage auto).","Hintergrund entfernt (Auto-Freistellen).","Фон удалён (авто-вырезание).","背景を削除しました（自動切り抜き）。","배경 제거됨(자동 누끼).");
    add("Text Watermark...", "文字水印...","文字浮水印...","Marca de agua de texto...","Filigrane texte...","Text-Wasserzeichen...","Текстовый водяной знак...","文字透かし...","텍스트 워터마크...");
    add("Text Watermark",    "文字水印",  "文字浮水印","Marca de agua de texto","Filigrane texte","Text-Wasserzeichen","Текстовый водяной знак","文字透かし","텍스트 워터마크");
    add("Image Watermark...","图片水印...","圖片浮水印...","Marca de agua de imagen...","Filigrane image...","Bild-Wasserzeichen...","Водяной знак (изобр.)...","画像透かし...","이미지 워터마크...");
    add("Watermark text:",   "水印文字：","浮水印文字：","Texto de marca de agua:","Texte du filigrane :","Wasserzeichentext:","Текст водяного знака:","透かし文字：","워터마크 텍스트:");
    add("Choose Watermark Image","选择水印图片","選擇浮水印圖片","Elegir imagen de marca de agua","Choisir l'image de filigrane","Wasserzeichenbild wählen","Выбрать изображение водяного знака","透かし画像を選択","워터마크 이미지 선택");
    add("Export As...",      "导出为...", "匯出為...", "Exportar como...","Exporter sous...","Exportieren als...","Экспорт как...","エクスポート...","내보내기...");
    add("Export As",         "导出为",    "匯出為",    "Exportar como","Exporter sous", "Exportieren als","Экспорт как",   "エクスポート",  "내보내기");
    add("Exported to %1",    "已导出到 %1","已匯出到 %1","Exportado a %1","Exporté vers %1","Exportiert nach %1","Экспортировано в %1","%1 にエクスポートしました","%1 로 내보냄");
    add("Compress & Export...","压缩并导出...","壓縮並匯出...","Comprimir y exportar...","Compresser et exporter...","Komprimieren & exportieren...","Сжать и экспортировать...","圧縮してエクスポート...","압축 후 내보내기...");
    add("Compress & Export", "压缩并导出","壓縮並匯出","Comprimir y exportar","Compresser et exporter","Komprimieren & exportieren","Сжать и экспортировать","圧縮してエクスポート","압축 후 내보내기");
    add("Quality (1-100):",  "质量（1-100）：","品質（1-100）：","Calidad (1-100):","Qualité (1-100) :","Qualität (1-100):","Качество (1-100):","品質（1-100）：","품질(1-100):");
    add("Exported (quality %1) to %2","已以质量 %1 导出到 %2","已以品質 %1 匯出到 %2","Exportado (calidad %1) a %2","Exporté (qualité %1) vers %2","Exportiert (Qualität %1) nach %2","Экспортировано (качество %1) в %2","品質 %1 で %2 にエクスポート","품질 %1(으)로 %2 에 내보냄");
    add("Filter applied: %1","已应用滤镜：%1","已套用濾鏡：%1","Filtro aplicado: %1","Filtre appliqué : %1","Filter angewendet: %1","Фильтр применён: %1","フィルター適用: %1","필터 적용: %1");
    add("Add Text",          "添加文字",  "新增文字",  "Agregar texto","Ajouter du texte","Text hinzufügen","Добавить текст","テキスト追加","텍스트 추가");
    add("Text:",             "文字：",    "文字：",    "Texto:",       "Texte :",       "Text:",         "Текст:",          "テキスト：",    "텍스트:");
    add("Images (*.png *.jpg *.jpeg *.bmp *.webp)","图片 (*.png *.jpg *.jpeg *.bmp *.webp)","圖片 (*.png *.jpg *.jpeg *.bmp *.webp)","Imágenes (*.png *.jpg *.jpeg *.bmp *.webp)","Images (*.png *.jpg *.jpeg *.bmp *.webp)","Bilder (*.png *.jpg *.jpeg *.bmp *.webp)","Изображения (*.png *.jpg *.jpeg *.bmp *.webp)","画像 (*.png *.jpg *.jpeg *.bmp *.webp)","이미지 (*.png *.jpg *.jpeg *.bmp *.webp)");

    // ---- File associations ----
    add("File &Associations...","文件关联(&A)...","檔案關聯(&A)...","&Asociaciones de archivo...","&Associations de fichiers...","Datei&zuordnungen...","&Ассоциации файлов...","ファイル関連付け(&A)...","파일 연결(&A)...");
    add("File Associations", "文件关联",  "檔案關聯",  "Asociaciones de archivo","Associations de fichiers","Dateizuordnungen","Ассоциации файлов","ファイル関連付け","파일 연결");
    add("File associations are only supported on Windows.","文件关联仅在 Windows 上支持。","檔案關聯僅在 Windows 上支援。","Las asociaciones de archivo solo se admiten en Windows.","Les associations de fichiers ne sont prises en charge que sous Windows.","Dateizuordnungen werden nur unter Windows unterstützt.","Ассоциации файлов поддерживаются только в Windows.","ファイル関連付けは Windows のみ対応です。","파일 연결은 Windows에서만 지원됩니다.");
    add("Choose which file groups Terminal++ should handle, then pick an action. All changes are per-user and fully reversible.",
        "选择要让 Terminal++ 处理的文件分组，然后选择一个操作。所有更改均为当前用户级且完全可撤销。",
        "選擇要讓 Terminal++ 處理的檔案分組，然後選擇一個動作。所有變更皆為目前使用者層級且完全可還原。",
        "Elija qué grupos de archivos debe manejar Terminal++ y luego una acción. Todos los cambios son por usuario y totalmente reversibles.",
        "Choisissez les groupes de fichiers que Terminal++ doit gérer, puis une action. Tous les changements sont par utilisateur et réversibles.",
        "Wählen Sie, welche Dateigruppen Terminal++ behandeln soll, dann eine Aktion. Alle Änderungen sind benutzerbezogen und umkehrbar.",
        "Выберите группы файлов для Terminal++, затем действие. Все изменения на уровне пользователя и обратимы.",
        "Terminal++ が処理するファイルグループを選び、操作を選択してください。すべての変更はユーザー単位で、完全に元に戻せます。",
        "Terminal++가 처리할 파일 그룹을 선택한 뒤 작업을 고르세요. 모든 변경은 사용자 단위이며 되돌릴 수 있습니다.");
    add("Text files (.txt, .md, .log, .csv, .ini, .json, .xml, …)","文本文件 (.txt, .md, .log, .csv, .ini, .json, .xml, …)","文字檔 (.txt, .md, .log, .csv, .ini, .json, .xml, …)","Archivos de texto (.txt, .md, .log, .csv, .ini, .json, .xml, …)","Fichiers texte (.txt, .md, .log, .csv, .ini, .json, .xml, …)","Textdateien (.txt, .md, .log, .csv, .ini, .json, .xml, …)","Текстовые файлы (.txt, .md, .log, .csv, .ini, .json, .xml, …)","テキストファイル (.txt, .md, .log, .csv, .ini, .json, .xml, …)","텍스트 파일 (.txt, .md, .log, .csv, .ini, .json, .xml, …)");
    add("Source code (.c, .cpp, .h, .py, .js, .java, .cs, .go, …)","源代码 (.c, .cpp, .h, .py, .js, .java, .cs, .go, …)","原始碼 (.c, .cpp, .h, .py, .js, .java, .cs, .go, …)","Código fuente (.c, .cpp, .h, .py, .js, .java, .cs, .go, …)","Code source (.c, .cpp, .h, .py, .js, .java, .cs, .go, …)","Quellcode (.c, .cpp, .h, .py, .js, .java, .cs, .go, …)","Исходный код (.c, .cpp, .h, .py, .js, .java, .cs, .go, …)","ソースコード (.c, .cpp, .h, .py, .js, .java, .cs, .go, …)","소스 코드 (.c, .cpp, .h, .py, .js, .java, .cs, .go, …)");
    add("Images (.png, .jpg, .bmp, .gif, .webp, .tiff, .ico, .svg)","图片 (.png, .jpg, .bmp, .gif, .webp, .tiff, .ico, .svg)","圖片 (.png, .jpg, .bmp, .gif, .webp, .tiff, .ico, .svg)","Imágenes (.png, .jpg, .bmp, .gif, .webp, .tiff, .ico, .svg)","Images (.png, .jpg, .bmp, .gif, .webp, .tiff, .ico, .svg)","Bilder (.png, .jpg, .bmp, .gif, .webp, .tiff, .ico, .svg)","Изображения (.png, .jpg, .bmp, .gif, .webp, .tiff, .ico, .svg)","画像 (.png, .jpg, .bmp, .gif, .webp, .tiff, .ico, .svg)","이미지 (.png, .jpg, .bmp, .gif, .webp, .tiff, .ico, .svg)");
    add("Add to \"Open with\"","加入“打开方式”","加入「開啟方式」","Añadir a \"Abrir con\"","Ajouter à « Ouvrir avec »","Zu \"Öffnen mit\" hinzufügen","Добавить в «Открыть с помощью»","「プログラムから開く」に追加","\"연결 프로그램\"에 추가");
    add("Remove from \"Open with\"","从“打开方式”移除","從「開啟方式」移除","Quitar de \"Abrir con\"","Retirer de « Ouvrir avec »","Aus \"Öffnen mit\" entfernen","Убрать из «Открыть с помощью»","「プログラムから開く」から削除","\"연결 프로그램\"에서 제거");
    add("Set as Default Program","设为默认程序","設為預設程式","Establecer como programa predeterminado","Définir comme programme par défaut","Als Standardprogramm festlegen","Сделать программой по умолчанию","既定のプログラムに設定","기본 프로그램으로 설정");
    add("Unset as Default Program","取消默认程序","取消預設程式","Quitar como programa predeterminado","Retirer comme programme par défaut","Als Standardprogramm entfernen","Убрать как программу по умолчанию","既定のプログラムを解除","기본 프로그램 해제");
    add("Added Terminal++ to \"Open with\" for %1 file type(s).","已为 %1 种文件类型将 Terminal++ 加入“打开方式”。","已為 %1 種檔案類型將 Terminal++ 加入「開啟方式」。","Terminal++ añadido a \"Abrir con\" para %1 tipo(s) de archivo.","Terminal++ ajouté à « Ouvrir avec » pour %1 type(s) de fichier.","Terminal++ zu \"Öffnen mit\" für %1 Dateityp(en) hinzugefügt.","Terminal++ добавлен в «Открыть с помощью» для %1 типов файлов.","%1 種類のファイルで Terminal++ を「プログラムから開く」に追加しました。","%1개 파일 형식에 대해 Terminal++를 \"연결 프로그램\"에 추가함.");
    add("Removed Terminal++ from \"Open with\" for %1 file type(s).","已为 %1 种文件类型将 Terminal++ 从“打开方式”移除。","已為 %1 種檔案類型將 Terminal++ 從「開啟方式」移除。","Terminal++ quitado de \"Abrir con\" para %1 tipo(s) de archivo.","Terminal++ retiré de « Ouvrir avec » pour %1 type(s) de fichier.","Terminal++ aus \"Öffnen mit\" für %1 Dateityp(en) entfernt.","Terminal++ убран из «Открыть с помощью» для %1 типов файлов.","%1 種類のファイルで Terminal++ を「プログラムから開く」から削除しました。","%1개 파일 형식에서 Terminal++를 \"연결 프로그램\"에서 제거함.");
    add("Terminal++ set as default for %1 file type(s).","已为 %1 种文件类型将 Terminal++ 设为默认程序。","已為 %1 種檔案類型將 Terminal++ 設為預設程式。","Terminal++ establecido como predeterminado para %1 tipo(s).","Terminal++ défini par défaut pour %1 type(s).","Terminal++ als Standard für %1 Dateityp(en) festgelegt.","Terminal++ установлен по умолчанию для %1 типов.","%1 種類のファイルで Terminal++ を既定に設定しました。","%1개 파일 형식의 기본 프로그램으로 설정함.");
    add("Terminal++ removed as default for %1 file type(s).","已为 %1 种文件类型取消 Terminal++ 的默认程序。","已為 %1 種檔案類型取消 Terminal++ 的預設程式。","Terminal++ quitado como predeterminado para %1 tipo(s).","Terminal++ retiré par défaut pour %1 type(s).","Terminal++ als Standard für %1 Dateityp(en) entfernt.","Terminal++ убран из программ по умолчанию для %1 типов.","%1 種類のファイルで Terminal++ の既定を解除しました。","%1개 파일 형식에서 Terminal++ 기본 설정을 해제함.");

    // ---- Dialogs / messages ----
    add("Open Files",        "打开文件",  "開啟檔案",  "Abrir archivos","Ouvrir des fichiers","Dateien öffnen","Открыть файлы","ファイルを開く","파일 열기");
    add("Save As",           "另存为",    "另存為",    "Guardar como", "Enregistrer sous","Speichern unter","Сохранить как","名前を付けて保存","다른 이름으로 저장");
    add("Clipboard History", "剪贴板历史","剪貼簿歷史","Historial del portapapeles","Historique du presse-papiers","Zwischenablage-Verlauf","История буфера обмена","クリップボード履歴","클립보드 기록");
    add("Clear",             "清空",      "清除",      "Borrar",       "Effacer",       "Leeren",        "Очистить",        "クリア",        "지우기");
    add("Cancel",            "取消",      "取消",      "Cancelar",     "Annuler",       "Abbrechen",     "Отмена",          "キャンセル",    "취소");
    add("Language changed. Some elements update immediately; reopen dialogs to fully refresh.",
        "语言已更改。部分界面立即更新；重新打开对话框以完全刷新。",
        "語言已變更。部分介面立即更新；重新開啟對話方塊以完全重新整理。",
        "Idioma cambiado. Algunos elementos se actualizan de inmediato; vuelva a abrir los diálogos para refrescar.",
        "Langue modifiée. Certains éléments se mettent à jour immédiatement ; rouvrez les dialogues pour tout rafraîchir.",
        "Sprache geändert. Einige Elemente werden sofort aktualisiert; Dialoge erneut öffnen zum vollständigen Aktualisieren.",
        "Язык изменён. Некоторые элементы обновляются сразу; откройте диалоги заново для полного обновления.",
        "言語が変更されました。一部はすぐに更新されます。ダイアログを再度開くと完全に反映されます。",
        "언어가 변경되었습니다. 일부 요소는 즉시 업데이트됩니다. 대화상자를 다시 열면 완전히 새로 고쳐집니다.");

    // ---- Round-2 fixes: short About, PDF password, compression ----
    add("Editor, hex, image, PDF, SSH and serial terminal, file compare and cloud drives in one tabbed window.", "编辑器、十六进制、图片、PDF、SSH 与串口终端、文件比较与云盘，全部集成在一个多标签窗口里。", "編輯器、十六進位、圖片、PDF、SSH 與序列終端、檔案比較與雲端，全部整合在一個多分頁視窗裡。", "Editor, hex, imágenes, PDF, terminal SSH y serie, comparación de archivos y unidades en la nube en una sola ventana con pestañas.", "Éditeur, hex, images, PDF, terminal SSH et série, comparaison de fichiers et disques cloud dans une seule fenêtre à onglets.", "Editor, Hex, Bilder, PDF, SSH- und Seriell-Terminal, Dateivergleich und Cloud-Laufwerke in einem Fenster mit Tabs.", "Редактор, hex, изображения, PDF, SSH- и Serial-терминал, сравнение файлов и облачные диски в одном окне с вкладками.", "エディタ、Hex、画像、PDF、SSH ・ シリアル端末、ファイル比較、クラウドドライブを 1 つのタブウィンドウに統合。", "에디토, Hex, 이미지, PDF, SSH 및 시리얼 토밍널, 파일 변교, 클라우드 드라이버를 하나의 탭 창에 통합.");
    add("Qt 6 / MinGW. 9 UI languages.", "Qt 6 / MinGW 构建，支持 9 种界面语言。", "Qt 6 / MinGW 建置，支援 9 種介面語言。", "Qt 6 / MinGW. 9 idiomas de interfaz.", "Qt 6 / MinGW. 9 langues d'interface.", "Qt 6 / MinGW. 9 Oberflächensprachen.", "Qt 6 / MinGW. 9 языков интерфейса.", "Qt 6 / MinGW。UI 9 言語対応。", "Qt 6 / MinGW. UI 9개 언어.");
    add("Password Required", "需要密码", "需要密碼", "Se requiere contraseña", "Mot de passe requis", "Kennwort erforderlich", "Требуется пароль", "パスワードが必要です", "보호 번호 필요");
    add("This PDF is password-protected.\nEnter the password to open it:", "此 PDF 已用密码保护。\n请输入打开密码：", "此 PDF 已用密碼保護。\n請輸入開啟密碼：", "Este PDF está protegido con contraseña.\nIntroduzca la contraseña para abrirlo:", "Ce PDF est protégé par mot de passe.\nSaisissez le mot de passe pour l'ouvrir :", "Dieses PDF ist kennwortgeschützt.\nGeben Sie das Kennwort zum Öffnen ein:", "Этот PDF защищён паролем.\nВведите пароль для открытия:", "この PDF はパスワードで保護されています。\n開くにはパスワードを入力してください：", "이 PDF는 보호 번호로 보호되어 있습니다.\n여는 보호 번호를 입력하세요:");
    add("Wrong password. Please try again:", "密码错误，请重试：", "密碼錯誤，請重試：", "Contraseña incorrecta. Inténtelo de nuevo:", "Mot de passe incorrect. Veuillez réessayer :", "Falsches Kennwort. Bitte erneut versuchen:", "Неверный пароль. Попробуйте снова:", "パスワードが違います。もう一度入力してください：", "보호 번호가 틀렸습니다. 다시 시도하세요:");
    add("This PDF is password-protected and could not be opened without the correct password.", "此 PDF 已用密码保护，没有正确密码无法打开。", "此 PDF 已用密碼保護，沒有正確密碼無法開啟。", "Este PDF está protegido con contraseña y no se pudo abrir sin la contraseña correcta.", "Ce PDF est protégé par mot de passe et n'a pas pu être ouvert sans le bon mot de passe.", "Dieses PDF ist kennwortgeschützt und konnte ohne das richtige Kennwort nicht geöffnet werden.", "Этот PDF защищён паролем и не может быть открыт без верного пароля.", "この PDF はパスワードで保護されており、正しいパスワードがなければ開けません。", "이 PDF는 보호 번호로 보호되어 있어 정확한 보호 번호 없이는 여릴 수 없습니다.");
    add("encrypted: view / copy only, editing is disabled", "已加密：仅可查看 / 复制，编辑已禁用", "已加密：僅可檢視 / 複製，編輯已停用", "cifrado: solo ver / copiar, la edición está deshabilitada", "chiffré : lecture / copie seulement, l'édition est désactivée", "verschlüsselt: nur Ansehen / Kopieren, Bearbeiten ist deaktiviert", "зашифровано: только просмотр / копирование, редактирование отключено", "暗号化済み：閲覧 / コピーのみ可、編集は無効", "암호화됨: 보기 / 복사만 가능, 편집 부가능");
    add("Already fully compressed: %1 KB → %2 KB (no further reduction possible).", "已完全压缩：%1 KB → %2 KB（无法再缩小）。", "已完全壓縮：%1 KB → %2 KB（無法再縮小）。", "Ya está completamente comprimido: %1 KB → %2 KB (no es posible reducirlo más).", "Déjà entièrement compressé : %1 KB → %2 KB (aucune réduction supplémentaire possible).", "Bereits vollständig komprimiert: %1 KB → %2 KB (keine weitere Verkleinerung möglich).", "Уже полностью сжато: %1 КБ → %2 КБ (дальнейшее уменьшение невозможно).", "すでに完全に圧縮されています：%1 KB → %2 KB（これ以上小さくできません）。", "이미 완전히 압술됨: %1 KB → %2 KB (더 이상 줄이려가 없습니다).");

    // ---- Real text: select & copy the text that IS text ----
    add("Copy Text", "复制文字", "複製文字", "Copiar texto", "Copier le texte", "Text kopieren", "Копировать текст", "テキストをコピー", "텍스트 복사");
    add("Select All Text", "全选文字", "全選文字", "Seleccionar todo el texto", "Tout sélectionner", "Gesamten Text auswählen", "Выделить весь текст", "すべてのテキストを選択", "모든 텍스트 여기");
    add("Copy Page Text", "复制本页文字", "複製本頁文字", "Copiar el texto de la página", "Copier le texte de la page", "Seitentext kopieren", "Копировать текст страницы", "パージ全文をコピー", "해당 페이지 텍스트 복사");
    add("Copy All Text", "复制全文", "複製全文", "Copiar todo el texto", "Copier tout le texte", "Gesamten Text kopieren", "Копировать всё", "全文をコピー", "전짜 텍스트 복사");
    add("Copied %1 characters of selected text.", "已复制所选文字的 %1 个字符。", "已複製所選文字的 %1 個字元。", "Se copiaron %1 caracteres del texto seleccionado.", "%1 caractères du texte sélectionné copiés.", "%1 Zeichen des markierten Textes kopiert.", "Скопировано символов выделенного текста: %1.", "選択したテキスト %1 文字をコピーしました。", "여기한 텍스트 %1자를 복사했습니다.");
    add("Copied the whole page text (%1 characters).", "已复制本页全部文字（%1 个字符）。", "已複製本頁全部文字（%1 個字元）。", "Se copió todo el texto de la página (%1 caracteres).", "Tout le texte de la page a été copié (%1 caractères).", "Der gesamte Seitentext wurde kopiert (%1 Zeichen).", "Скопирован весь текст страницы (%1 симв.).", "パージ全文（%1 文字）をコピーしました。", "해당 페이지 전짜 텍스트(%1자)를 복사했습니다.");
    add("Copied the text of %1 page(s) (%2 characters).", "已复制 %1 页的文字（%2 个字符）。", "已複製 %1 頁的文字（%2 個字元）。", "Se copió el texto de %1 página(s) (%2 caracteres).", "Texte de %1 page(s) copié (%2 caractères).", "Text von %1 Seite(n) kopiert (%2 Zeichen).", "Скопирован текст страниц: %1 (%2 симв.).", "%1 パージ分のテキスト（%2 文字）をコピーしました。", "%1개 페이지의 텍스트(%2자)를 복사했습니다.");
    add("Selected all %1 characters on this page.", "已选中本页全部 %1 个字符。", "已選取本頁全部 %1 個字元。", "Se seleccionaron los %1 caracteres de esta página.", "Les %1 caractères de cette page sont sélectionnés.", "Alle %1 Zeichen dieser Seite ausgewählt.", "Выделены все символы страницы: %1.", "このパージの %1 文字をすべて選択しました。", "이 페이지의 %1자를 모두 여기했습니다.");
    add("This page has no text layer — its text is part of an image, so there is nothing to copy. Use OCR or \"To Images\" instead.", "本页没有文字图层——其文字属于图片的一部分，因此没有可复制的文字。请使用 OCR 或“转为图片”。", "本頁沒有文字圖層——其文字屬於圖片的一部分，因此沒有可複製的文字。請使用 OCR 或「轉為圖片」。", "Esta página no tiene capa de texto: su texto forma parte de una imagen, así que no hay nada que copiar. Use OCR o \"A imágenes\".", "Cette page n'a pas de couche de texte : son texte fait partie d'une image, il n'y a donc rien à copier. Utilisez l'OCR ou « Vers images ».", "Diese Seite hat keine Textebene — ihr Text ist Teil eines Bildes, es gibt also nichts zu kopieren. Verwenden Sie OCR oder „Zu Bildern“.", "На этой странице нет текстового слоя — текст является частью изображения, копировать нечего. Используйте OCR или «В изображения».", "このパージにはテキストレイヤーがありません——文字は画像の一部なのでコピーできるテキストはありません。OCR または「画像に変換」をご利用ください。", "이 페이지에는 텍스트 레이어가 없습니다 — 문자가 이미지의 일부이뭀로 복사할 텍스트가 없습니다. OCR 또는 “이미지로 변환”을 사용하십시오.");
    add("This page has no text layer — its text is part of an image, so there is nothing to select.", "本页没有文字图层——其文字属于图片的一部分，因此没有可选中的文字。", "本頁沒有文字圖層——其文字屬於圖片的一部分，因此沒有可選取的文字。", "Esta página no tiene capa de texto: su texto forma parte de una imagen, así que no hay nada que seleccionar.", "Cette page n'a pas de couche de texte : son texte fait partie d'une image, il n'y a donc rien à sélectionner.", "Diese Seite hat keine Textebene — ihr Text ist Teil eines Bildes, es gibt also nichts auszuwählen.", "На этой странице нет текстового слоя — текст является частью изображения, выделять нечего.", "このパージにはテキストレイヤーがありません——文字は画像の一部なので選択できるテキストはありません。", "이 페이지에는 텍스트 레이어가 없습니다 — 문자가 이미지의 일부이뭀로 여기할 텍스트가 없습니다.");
    add("No extractable text was found in this document.", "在此文档中未找到可提取的文字。", "在此文件中找不到可擷取的文字。", "No se encontró texto extraíble en este documento.", "Aucun texte extractible n'a été trouvé dans ce document.", "In diesem Dokument wurde kein extrahierbarer Text gefunden.", "В этом документе не найдено извлекаемого текста.", "この文書から抽出できるテキストは見つかりませんでした。", "이 발서에서 추출 가능한 텍스트를 찾지 못했습니다.");
    add("selectable text: %1 characters (drag to select, Ctrl+C to copy)", "可选文字：%1 个字符（拖动选中，Ctrl+C 复制）", "可選文字：%1 個字元（拖曳選取，Ctrl+C 複製）", "texto seleccionable: %1 caracteres (arrastre para seleccionar, Ctrl+C para copiar)", "texte sélectionnable : %1 caractères (glisser pour sélectionner, Ctrl+C pour copier)", "auswählbarer Text: %1 Zeichen (ziehen zum Auswählen, Ctrl+C zum Kopieren)", "текст доступен для выделения: %1 симв. (выделите мышью, Ctrl+C — копировать)", "選択可能なテキスト：%1 文字（ドラッグで選択、Ctrl+C でコピー）", "여기 가능한 텍스트: %1자 (드래게하여 여기, Ctrl+C로 복사)");
    add("text can be copied whole-page only", "仅支持整页复制文字", "僅支援整頁複製文字", "el texto solo se puede copiar por página completa", "le texte ne peut être copié que page entière", "Text kann nur seitenweise kopiert werden", "текст можно скопировать только страницей целиком", "テキストはパージ全体しかコピーできません", "텍스트는 페이지 전짜로만 복사할 수 있습니다");
    add("no text layer (image-only page)", "无文字图层（线图片页面）", "無文字圖層（純圖片頁面）", "sin capa de texto (página solo de imagen)", "aucune couche de texte (page image uniquement)", "keine Textebene (Seite besteht nur aus einem Bild)", "текстового слоя нет (страница — только изображение)", "テキストレイヤーなし（画像のみのパージ）", "텍스트 레이어 없음(이미지 전용 페이지)");
    add("(this page has no text layer — its text is part of an image)", "（本页没有文字图层——其文字属于图片的一部分）", "（本頁沒有文字圖層——其文字屬於圖片的一部分）", "(esta página no tiene capa de texto: su texto forma parte de una imagen)", "(cette page n'a pas de couche de texte : son texte fait partie d'une image)", "(diese Seite hat keine Textebene — ihr Text ist Teil eines Bildes)", "(на этой странице нет текстового слоя — текст является частью изображения)", "（このパージにはテキストレイヤーがありません——文字は画像の一部です）", "(이 페이지에는 텍스트 레이어가 없습니다 — 문자가 이미지의 일부입니다)");

    // ---- PDF viewer / editor (toolbar, dialogs, status messages) ----
    add("PDF viewer / editor", "PDF 查看 / 编辑", "PDF 檢視 / 編輯", "Visor / editor de PDF", "Visionneuse / éditeur PDF", "PDF-Betrachter / -Editor", "Просмотр / редактирование PDF", "PDF ビューアー / エディタ", "PDF 뷰어 / 에디터");

    // Every string emitted by src/pdf/pdftab.cpp goes through TR(), and the
    // PdfTab toolbar retranslates itself live on I18n::languageChanged, so all
    // of the following entries take effect without reopening the tab.

    // --- MainWindow "Open as PDF" entry points (new request #1) ----------
    add("Open as &PDF...",   "以 PDF 打开(&P)...","以 PDF 開啟(&P)...","Abrir como &PDF...","Ouvrir comme &PDF...","Als &PDF öffnen...","Открыть как &PDF...","PDF として開く(&P)...","PDF로 열기(&P)...");
    add("Open as PDF",       "以 PDF 打开","以 PDF 開啟","Abrir como PDF","Ouvrir comme PDF","Als PDF öffnen","Открыть как PDF","PDF として開く","PDF로 열기");
    add("PDF or Images (*.pdf *.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;PDF Files (*.pdf);;Images (*.png *.jpg *.jpeg *.bmp *.webp);;All Files (*)",
        "PDF 或图片 (*.pdf *.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;PDF 文件 (*.pdf);;图片 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*)",
        "PDF 或圖片 (*.pdf *.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;PDF 檔案 (*.pdf);;圖片 (*.png *.jpg *.jpeg *.bmp *.webp);;所有檔案 (*)",
        "PDF o imágenes (*.pdf *.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;Archivos PDF (*.pdf);;Imágenes (*.png *.jpg *.jpeg *.bmp *.webp);;Todos los archivos (*)",
        "PDF ou images (*.pdf *.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;Fichiers PDF (*.pdf);;Images (*.png *.jpg *.jpeg *.bmp *.webp);;Tous les fichiers (*)",
        "PDF oder Bilder (*.pdf *.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;PDF-Dateien (*.pdf);;Bilder (*.png *.jpg *.jpeg *.bmp *.webp);;Alle Dateien (*)",
        "PDF или изображения (*.pdf *.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;Файлы PDF (*.pdf);;Изображения (*.png *.jpg *.jpeg *.bmp *.webp);;Все файлы (*)",
        "PDF または画像 (*.pdf *.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;PDF ファイル (*.pdf);;画像 (*.png *.jpg *.jpeg *.bmp *.webp);;すべてのファイル (*)",
        "PDF 또는 이미지 (*.pdf *.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;PDF 파일 (*.pdf);;이미지 (*.png *.jpg *.jpeg *.bmp *.webp);;모든 파일 (*)");
    add("Could not build a PDF: %1","无法生成 PDF：%1","無法產生 PDF：%1","No se pudo crear el PDF: %1","Impossible de créer le PDF : %1","PDF konnte nicht erstellt werden: %1","Не удалось создать PDF: %1","PDF を作成できませんでした：%1","PDF를 만들 수 없습니다: %1");
    add("Save new PDF",      "保存新的 PDF","儲存新的 PDF","Guardar el nuevo PDF","Enregistrer le nouveau PDF","Neues PDF speichern","Сохранить новый PDF","新しい PDF を保存","새 PDF 저장");
    add("Cannot open PDF:\n%1","无法打开 PDF：\n%1","無法開啟 PDF：\n%1","No se puede abrir el PDF:\n%1","Impossible d'ouvrir le PDF :\n%1","PDF kann nicht geöffnet werden:\n%1","Не удалось открыть PDF:\n%1","PDF を開けません：\n%1","PDF를 열 수 없습니다:\n%1");
    add("PDF: %1",           "PDF：%1",   "PDF：%1",   "PDF: %1",      "PDF : %1",      "PDF: %1",       "PDF: %1",         "PDF: %1",       "PDF: %1");
    add("PDF documents (.pdf)","PDF 文档 (.pdf)","PDF 文件 (.pdf)","Documentos PDF (.pdf)","Documents PDF (.pdf)","PDF-Dokumente (.pdf)","Документы PDF (.pdf)","PDF 文書 (.pdf)","PDF 문서 (.pdf)");
    add("Print to PDF",      "打印为 PDF","列印為 PDF","Imprimir a PDF","Imprimer en PDF","Als PDF drucken","Печать в PDF","PDF に印刷",    "PDF로 인쇄");

    // --- Rendering backend status (new request #2: PDFium) ----------------
    // About-dialog sentence describing the PDFium rendering backend, so the
    // whole About text stays translated in every language.
    add("a dual-engine PDF module. Pages are rendered by PDFium (the same engine Chrome/Edge use), loaded dynamically at run time from pdfium.dll with no compile-time dependency, so text, vector graphics, embedded fonts and transparency render exactly as in a browser and are re-rasterised at the current zoom to stay crisp. A copy of pdfium.dll ships in dist/pdfium/ as the last-resort fallback; if no PDFium is found the viewer degrades gracefully to the built-in engine. The portrait / landscape layout is chosen automatically from the PDF's own page box, so a landscape slide deck lays out landscape with no user action.",
        "双引擎 PDF 模块。页面由 PDFium（与 Chrome / Edge 同款引擎）渲染，运行时从 pdfium.dll 动态加载、无任何编译期依赖，因此文字、矩量图形、嵌入字体与透明度都能像浏览器一样完整呈现，并在当前缩放级别重新栅格化以保持清晰。构建时会把一份 pdfium.dll 打包进 dist/pdfium/ 作为最终兜底；若找不到 PDFium，则优雅降级到内置引擎。纵向 / 横向版式根据 PDF 自身页面框自动判定，因此横版幻灯片无需任何操作即以横向呈现。",
        "雙引擎 PDF 模組。頁面由 PDFium（與 Chrome / Edge 同款引擎）算繪，執行時從 pdfium.dll 動態載入、無任何編譯期依賴，因此文字、矩量圖形、嵌入字型與透明度都能像瀏覽器一樣完整呈现，並在目前縮放級別重新算繪以保持清晰。建置時會把一份 pdfium.dll 打包進 dist/pdfium/ 作為最終備援；若找不到 PDFium，則優雅降級到内建引擎。縱向 / 橪向版式依 PDF 自身頁面框自動判定，因此橪版简報無需任何操作即以橪向呈现。",
        "un módulo PDF de doble motor. Las páginas se representan con PDFium (el mismo motor que usan Chrome/Edge), cargado dinámicamente en tiempo de ejecución desde pdfium.dll sin dependencia de compilación, por lo que el texto, los gráficos vectoriales, las fuentes incrustadas y la transparencia se muestran exactamente como en un navegador y se vuelven a rasterizar al zoom actual para mantenerse nítidos. Una copia de pdfium.dll se incluye en dist/pdfium/ como último recurso; si no se encuentra PDFium, el visor recurre al motor integrado. La orientación vertical / horizontal se elige automáticamente a partir del propio cuadro de página del PDF.",
        "un module PDF à double moteur. Les pages sont rendues par PDFium (le moteur utilisé par Chrome/Edge), chargé dynamiquement à l'exécution depuis pdfium.dll sans dépendance de compilation, de sorte que le texte, les graphiques vectoriels, les polices intégrées et la transparence s'affichent exactement comme dans un navigateur et sont ré-rastérisés au zoom courant pour rester nets. Une copie de pdfium.dll est livrée dans dist/pdfium/ en dernier recours ; si aucun PDFium n'est trouvé, la visionneuse revient au moteur intégré. L'orientation portrait / paysage est choisie automatiquement d'après la boîte de page du PDF.",
        "ein PDF-Modul mit zwei Engines. Seiten werden von PDFium gerendert (derselben Engine, die Chrome/Edge verwenden), zur Laufzeit dynamisch aus pdfium.dll geladen und ohne Abhängigkeit zur Kompilierzeit, sodass Text, Vektorgrafiken, eingebettete Schriften und Transparenz exakt wie im Browser erscheinen und beim aktuellen Zoom neu gerastert werden. Eine Kopie von pdfium.dll liegt als letzte Rückfalloption in dist/pdfium/; wird kein PDFium gefunden, weicht der Betrachter elegant auf die integrierte Engine aus. Hoch- / Querformat wird automatisch aus dem Seitenrahmen des PDFs bestimmt.",
        "модуль PDF с двумя движками. Страницы отрисовывает PDFium (тот же движок, что в Chrome/Edge), загружаемый динамически во время выполнения из pdfium.dll без зависимости на этапе сборки, поэтому текст, векторная графика, встроенные шрифты и прозрачность отображаются точно как в браузере и повторно растеризуются при текущем масштабе. Копия pdfium.dll поставляется в dist/pdfium/ как последний резерв; если PDFium не найден, просмотрщик корректно переходит на встроенный движок. Книжная / альбомная ориентация выбирается автоматически по странице самого PDF.",
        "デュアルエンジンの PDF モジュール。プージは PDFium（Chrome/Edge と同じエンジン）で描画されます。pdfium.dll から実行時に動的読み込みされ、コンパイル時の依存関係はありません。そのためテキスト・ベクター画像・埋め込みフォント・透明度がブラウザと同じように描画され、現在のズームで再ラスタライズされて鮮明さを保ちます。pdfium.dll のコピーは最終フォールバックとして dist/pdfium/ に同梁されます。PDFium が見つからない場合は内蔵エンジンへ適切に切り替わります。縦向き / 横向きのレイアウトは PDF 自身のプージ枠から自動判定されます。",
        "듀얼 엔진 PDF 모듈입니다. 페이지는 PDFium(Chrome/Edge와 동일한 엔진)으로 렌더링되며, 추촜 시 의존성 없이 실행 시점에 pdfium.dll에서 동적으로 로드됩니다. 따라서 텍스트, 벡터 그래픽, 임베드 폰트, 투명도가 브라우자와 동일하게 표시되고 현재 확대 수준에서 다시 래스터화되어 선명하게 유지됩니다. pdfium.dll 사본은 최후의 대체 수단으로 dist/pdfium/ 에 포함됩니다. PDFium을 찾지 못하면 뷰어는 내장 엔진으로 자연스럽게 전환됩니다. 세로 / 가로 레이아웃은 PDF 자제 페이지 박스에서 자동으로 결정됩니다.");

    add("rendered by PDFium","由 PDFium 渲染","由 PDFium 算繪","renderizado por PDFium","rendu par PDFium","gerendert von PDFium","отрисовано PDFium","PDFium で描画","PDFium으로 렌더링");
    add("built-in renderer (install pdfium.dll for full page rendering)",
        "内置渲染器（安装 pdfium.dll 可完整渲染页面）",
        "內建算繪器（安裝 pdfium.dll 可完整算繪頁面）",
        "renderizador integrado (instale pdfium.dll para renderizar páginas completas)",
        "moteur de rendu intégré (installez pdfium.dll pour un rendu complet des pages)",
        "integrierter Renderer (pdfium.dll installieren für vollständiges Seiten-Rendering)",
        "встроенный визуализатор (установите pdfium.dll для полной отрисовки страниц)",
        "内蔵レンダラー（ページ全体を描画するには pdfium.dll を導入してください）",
        "내장 렌더러 (전체 페이지 렌더링에는 pdfium.dll 필요)");
    add("Full page rendering needs pdfium.dll. Place it next to TerminalPP.exe to render every page; text extraction and all page / annotation tools work without it.",
        "完整页面渲染需要 pdfium.dll。将其放在 TerminalPP.exe 同目录即可渲染所有页面；没有它时，文本提取以及所有页面/批注工具仍可正常使用。",
        "完整頁面算繪需要 pdfium.dll。將其放在 TerminalPP.exe 同目錄即可算繪所有頁面；沒有它時，文字擷取以及所有頁面/註解工具仍可正常使用。",
        "La representación completa de páginas necesita pdfium.dll. Colóquelo junto a TerminalPP.exe para representar todas las páginas; la extracción de texto y todas las herramientas de página / anotación funcionan sin él.",
        "Le rendu complet des pages nécessite pdfium.dll. Placez-le à côté de TerminalPP.exe pour afficher toutes les pages ; l'extraction de texte et tous les outils de page / annotation fonctionnent sans lui.",
        "Für vollständiges Seiten-Rendering wird pdfium.dll benötigt. Legen Sie sie neben TerminalPP.exe, um jede Seite zu rendern; Textextraktion und alle Seiten-/Anmerkungswerkzeuge funktionieren auch ohne sie.",
        "Для полной отрисовки страниц нужна pdfium.dll. Поместите её рядом с TerminalPP.exe, чтобы отрисовывать все страницы; извлечение текста и все инструменты страниц / аннотаций работают и без неё.",
        "ページ全体の描画には pdfium.dll が必要です。TerminalPP.exe と同じ場所に置くとすべてのページを描画できます。なくてもテキスト抽出とすべてのページ/注釈ツールは動作します。",
        "전체 페이지 렌더링에는 pdfium.dll이 필요합니다. TerminalPP.exe 옆에 두면 모든 페이지를 렌더링합니다. 없어도 텍스트 추출과 모든 페이지/주석 도구는 작동합니다.");
    add("portrait",          "纵向",      "縱向",      "vertical",     "portrait",      "Hochformat",    "книжная",         "縦向き",        "세로");
    add("landscape",         "横向",      "橫向",      "horizontal",   "paysage",       "Querformat",    "альбомная",       "横向き",        "가로");
    add("Page %1 / %2   —   %3 × %4 pt (%5)   —   rotation %6°   —   zoom %7%",
        "第 %1 / %2 页   —   %3 × %4 pt（%5）   —   旋转 %6°   —   缩放 %7%",
        "第 %1 / %2 頁   —   %3 × %4 pt（%5）   —   旋轉 %6°   —   縮放 %7%",
        "Página %1 / %2   —   %3 × %4 pt (%5)   —   rotación %6°   —   zoom %7%",
        "Page %1 / %2   —   %3 × %4 pt (%5)   —   rotation %6°   —   zoom %7%",
        "Seite %1 / %2   —   %3 × %4 pt (%5)   —   Drehung %6°   —   Zoom %7%",
        "Страница %1 / %2   —   %3 × %4 pt (%5)   —   поворот %6°   —   масштаб %7%",
        "%1 / %2 ページ   —   %3 × %4 pt（%5）   —   回転 %6°   —   ズーム %7%",
        "%1 / %2 페이지   —   %3 × %4 pt (%5)   —   회전 %6°   —   확대 %7%");

    // Navigation + zoom (Prev / Next page, Zoom Out / Zoom In, fit modes).
    add("First Page",        "首页",      "第一頁",    "Primera página","Première page","Erste Seite",  "Первая страница", "最初のページ",  "첫 페이지");
    add("Previous Page",     "上一页",    "上一頁",    "Página anterior","Page précédente","Vorherige Seite","Предыдущая страница","前のページ","이전 페이지");
    add("Next Page",         "下一页",    "下一頁",    "Página siguiente","Page suivante","Nächste Seite","Следующая страница","次のページ",  "다음 페이지");
    add("Last Page",         "末页",      "最後一頁",  "Última página","Dernière page", "Letzte Seite",  "Последняя страница","最後のページ","마지막 페이지");
    add("Go to page",        "跳转到页",  "跳至頁面",  "Ir a la página","Aller à la page","Gehe zu Seite","Перейти к странице","ページへ移動","페이지로 이동");
    add("Actual Size",       "实际大小",  "實際大小",  "Tamaño real",  "Taille réelle", "Originalgröße", "Реальный размер", "実際のサイズ",  "실제 크기");
    add("Fit Width",         "适合宽度",  "符合寬度",  "Ajustar al ancho","Ajuster à la largeur","Breite anpassen","По ширине",     "幅に合わせる",  "너비 맞춤");
    add("Fit Page",          "适合页面",  "符合頁面",  "Ajustar a la página","Ajuster à la page","Seite anpassen","По размеру страницы","ページに合わせる","페이지 맞춤");
    add("Page %1",           "第 %1 页",  "第 %1 頁",  "Página %1",    "Page %1",       "Seite %1",      "Страница %1",     "%1 ページ",     "%1 페이지");
    add("Page %1 / %2   —   %3 × %4 pt   —   rotation %5°   —   zoom %6%",
        "第 %1 / %2 页   —   %3 × %4 pt   —   旋转 %5°   —   缩放 %6%",
        "第 %1 / %2 頁   —   %3 × %4 pt   —   旋轉 %5°   —   縮放 %6%",
        "Página %1 / %2   —   %3 × %4 pt   —   rotación %5°   —   zoom %6%",
        "Page %1 / %2   —   %3 × %4 pt   —   rotation %5°   —   zoom %6%",
        "Seite %1 / %2   —   %3 × %4 pt   —   Drehung %5°   —   Zoom %6%",
        "Страница %1 / %2   —   %3 × %4 pt   —   поворот %5°   —   масштаб %6%",
        "%1 / %2 ページ   —   %3 × %4 pt   —   回転 %5°   —   ズーム %6%",
        "%1 / %2 페이지   —   %3 × %4 pt   —   회전 %5°   —   확대 %6%");
    // Placeholder drawn on pages the engine could not rasterise.
    add("%1 × %2 pt",        "%1 × %2 pt","%1 × %2 pt","%1 × %2 pt",   "%1 × %2 pt",    "%1 × %2 pt",    "%1 × %2 pt",      "%1 × %2 pt",    "%1 × %2 pt");
    add("   (rotated %1°)",
        "   （已旋转 %1°）",
        "   （已旋轉 %1°）",
        "   (rotado %1°)",
        "   (pivoté de %1°)",
        "   (um %1° gedreht)",
        "   (повёрнуто на %1°)",
        "   （%1° 回転）",
        "   (%1° 회전됨)");
    add("This page could not be rendered as an image. Text extraction and all page / annotation tools still work.",
        "此页无法渲染为图像。文本提取以及所有页面/批注工具仍可正常使用。",
        "此頁無法算繪為圖像。文字擷取以及所有頁面/註解工具仍可正常使用。",
        "Esta página no se pudo representar como imagen. La extracción de texto y todas las herramientas de página / anotación siguen funcionando.",
        "Cette page n'a pas pu être rendue en image. L'extraction de texte et tous les outils de page / annotation fonctionnent toujours.",
        "Diese Seite konnte nicht als Bild gerendert werden. Textextraktion und alle Seiten-/Anmerkungswerkzeuge funktionieren weiterhin.",
        "Эту страницу не удалось отрисовать как изображение. Извлечение текста и все инструменты страниц / аннотаций по-прежнему работают.",
        "このページは画像としてレンダリングできませんでした。テキスト抽出とすべてのページ/注釈ツールは引き続き使用できます。",
        "이 페이지를 이미지로 렌더링할 수 없습니다. 텍스트 추출과 모든 페이지/주석 도구는 계속 사용할 수 있습니다.");

    // Annotation tools.
    add("Annotate",          "批注",      "註解",      "Anotar",       "Annoter",       "Anmerken",      "Аннотации",       "注釈",          "주석");
    add("Pages",             "页面",      "頁面",      "Páginas",      "Pages",         "Seiten",        "Страницы",        "ページ",        "페이지");
    add("More",              "更多",      "更多",      "Más",          "Plus",          "Mehr",          "Ещё",             "その他",        "기타");
    add("Select",            "选择",      "選擇",      "Seleccionar",  "Sélection",     "Auswählen",     "Выделить",        "選択",          "선택");
    add("Choose stamp image","选择图章图片","選擇圖章圖片","Elegir imagen de sello","Choisir une image de tampon","Stempelbild auswählen","Выбрать изображение штампа","スタンプ画像を選択","스탬프 이미지 선택");
    add("Could not load the image.","无法加载图片。","無法載入圖片。","No se pudo cargar la imagen.","Impossible de charger l'image.","Bild konnte nicht geladen werden.","Не удалось загрузить изображение.","画像を読み込めませんでした。","이미지를 불러올 수 없습니다.");
    add("Stamp tool: drag a rectangle on the page to place the image.",
        "图章工具：在页面上拖动一个矩形来放置图片。",
        "圖章工具：在頁面上拖動一個矩形以放置圖片。",
        "Herramienta de sello: arrastre un rectángulo en la página para colocar la imagen.",
        "Outil tampon : glissez un rectangle sur la page pour placer l'image.",
        "Stempel-Werkzeug: Ziehen Sie ein Rechteck auf der Seite, um das Bild zu platzieren.",
        "Инструмент штампа: перетащите прямоугольник на странице, чтобы разместить изображение.",
        "スタンプツール：ページ上に矩形をドラッグして画像を配置します。",
        "스탬프 도구: 페이지에 사각형을 드래그하여 이미지를 배치합니다.");
    add("Edit Text",         "编辑文字",  "編輯文字",  "Editar texto", "Modifier le texte","Text bearbeiten","Изменить текст",  "テキストを編集", "텍스트 편집");
    add("Edit annotation text:","编辑批注文本：","編輯註解文字：","Editar el texto de la anotación:","Modifier le texte de l'annotation :","Anmerkungstext bearbeiten:","Изменить текст аннотации:","注釈テキストを編集：","주석 텍스트 편집:");
    add("Signature added. Drag or resize it, then Save.",
        "签名已添加。可拖动或缩放后再保存。",
        "簽名已加入。可拖動或縮放後再儲存。",
        "Firma añadida. Arrástrela o cámbiele el tamaño y luego guarde.",
        "Signature ajoutée. Déplacez-la ou redimensionnez-la, puis enregistrez.",
        "Signatur hinzugefügt. Verschieben oder skalieren, dann speichern.",
        "Подпись добавлена. Переместите или измените размер, затем сохраните.",
        "署名を追加しました。移動またはサイズ変更して保存してください。",
        "서명이 추가되었습니다. 이동하거나 크기를 조절한 후 저장하세요.");
    add("Image signature added. Drag or resize it, then Save.",
        "图片签名已添加。可拖动或缩放后再保存。",
        "圖片簽名已加入。可拖動或縮放後再儲存。",
        "Firma de imagen añadida. Arrástrela o cámbiele el tamaño y luego guarde.",
        "Signature image ajoutée. Déplacez-la ou redimensionnez-la, puis enregistrez.",
        "Bildsignatur hinzugefügt. Verschieben oder skalieren, dann speichern.",
        "Подпись-изображение добавлена. Переместите или измените размер, затем сохраните.",
        "画像署名を追加しました。移動またはサイズ変更して保存してください。",
        "이미지 서명이 추가되었습니다. 이동하거나 크기를 조절한 후 저장하세요.");
    add("Draw your signature below, then click OK. You can drag / resize it on the page before saving.",
        "在下方绘制签名，然后点击确定。保存前可在页面上拖动或缩放。",
        "在下方繪製簽名，然後按一下確定。儲存前可在頁面上拖動或縮放。",
        "Dibuje su firma a continuación y haga clic en Aceptar. Puede arrastrarla o cambiar su tamaño en la página antes de guardar.",
        "Dessinez votre signature ci-dessous, puis cliquez sur OK. Vous pouvez la déplacer / redimensionner sur la page avant d'enregistrer.",
        "Zeichnen Sie unten Ihre Signatur und klicken Sie dann auf OK. Sie können sie auf der Seite verschieben / skalieren, bevor Sie speichern.",
        "Нарисуйте подпись ниже, затем нажмите ОК. Перед сохранением её можно перемещать и изменять размер на странице.",
        "下に署名を描き、OK をクリックしてください。保存する前にページ上で移動・サイズ変更できます。",
        "아래에 서명을 그린 다음 확인을 클릭하세요. 저장하기 전에 페이지에서 이동하거나 크기를 조절할 수 있습니다.");
    add("Split into 2 files and opened them in new tabs.",
        "已拆分为 2 个文件并在新标签页中打开。",
        "已拆分為 2 個檔案並在新分頁中開啟。",
        "Dividido en 2 archivos y abiertos en pestañas nuevas.",
        "Scindé en 2 fichiers et ouverts dans de nouveaux onglets.",
        "In 2 Dateien aufgeteilt und in neuen Tabs geöffnet.",
        "Разделено на 2 файла и открыто в новых вкладках.",
        "2 つのファイルに分割し、新しいタブで開きました。",
        "2개 파일로 분할하여 새 탭에서 열었습니다.");
    add("Extracted %1 page(s) to %2 and opened it in a new tab.",
        "已提取 %1 页到 %2 并在新标签页中打开。",
        "已擷取 %1 頁到 %2 並在新分頁中開啟。",
        "Extraído %1 página(s) a %2 y abierto en una pestaña nueva.",
        "Extrait %1 page(s) vers %2 et ouvert dans un nouvel onglet.",
        "%1 Seite(n) nach %2 extrahiert und in einem neuen Tab geöffnet.",
        "Извлечено %1 страниц в %2 и открыто в новой вкладке.",
        "%1 ページを %2 に抽出し、新しいタブで開きました。",
        "%1 페이지를 %2(으)로 추출하여 새 탭에서 열었습니다.");
    add("Save first part","保存第一部分","儲存第一部分","Guardar primera parte","Enregistrer la première partie","Ersten Teil speichern","Сохранить первую часть","最初の部分を保存","첫 번째 부분 저장");
    add("Save second part","保存第二部分","儲存第二部分","Guardar segunda parte","Enregistrer la deuxième partie","Zweiten Teil speichern","Сохранить вторую часть","2 番目の部分を保存","두 번째 부분 저장");

    // --- TextCompareTab row context menu + row-operation messages --------
    add("Selected line(s): %1","已选中 %1 行","已選取 %1 行","Línea(s) seleccionada(s): %1","Ligne(s) sélectionnée(s) : %1","Ausgewählte Zeile(n): %1","Выбрано строк: %1","選択した行: %1","선택한 줄: %1");
    add("Copy Left → Right","复制 左 → 右","複製 左 → 右","Copiar izq. → der.","Copier gauche → droite","Links → Rechts kopieren","Копировать слева → направо","左 → 右 にコピー","왼쪽 → 오른쪽 복사");
    add("Copy Right → Left","复制 右 → 左","複製 右 → 左","Copiar der. → izq.","Copier droite → gauche","Rechts → Links kopieren","Копировать справа → налево","右 → 左 にコピー","오른쪽 → 왼쪽 복사");
    add("Move Left → Right","移动 左 → 右","移動 左 → 右","Mover izq. → der.","Déplacer gauche → droite","Links → Rechts verschieben","Переместить слева → направо","左 → 右 に移動","왼쪽 → 오른쪽 이동");
    add("Move Right → Left","移动 右 → 左","移動 右 → 左","Mover der. → izq.","Déplacer droite → gauche","Rechts → Links verschieben","Переместить справа → налево","右 → 左 に移動","오른쪽 → 왼쪽 이동");
    add("Edit Line...","编辑行...","編輯行...","Editar línea...","Modifier la ligne...","Zeile bearbeiten...","Изменить строку...","行を編集...","줄 편집...");
    add("Edit Left Line %1","编辑左侧第 %1 行","編輯左側第 %1 行","Editar línea izq. %1","Modifier la ligne gauche %1","Linke Zeile %1 bearbeiten","Изменить левую строку %1","左の行 %1 を編集","왼쪽 %1번째 줄 편집");
    add("Edit Right Line %1","编辑右侧第 %1 行","編輯右側第 %1 行","Editar línea der. %1","Modifier la ligne droite %1","Rechte Zeile %1 bearbeiten","Изменить правую строку %1","右の行 %1 を編集","오른쪽 %1번째 줄 편집");
    add("Line content:","行内容：","行內容：","Contenido de la línea:","Contenu de la ligne :","Zeileninhalt:","Содержимое строки:","行の内容：","줄 내용:");
    add("Click a line first, then right-click it.","先点击一行，然后对其右键单击。","先點擊一行，然後對其右鍵點擊。","Haga clic en una línea y luego clic derecho sobre ella.","Cliquez d'abord sur une ligne, puis faites un clic droit dessus.","Zuerst eine Zeile anklicken, dann Rechtsklick.","Сначала выберите строку, затем щёлкните по ней правой кнопкой.","まず行をクリックし、その上で右クリックしてください。","먼저 줄을 클릭한 뒤 마우스 오른쪽 버튼을 클릭하세요.");
    add("Select one or more lines first.","请先选择一行或多行。","請先選擇一行或多行。","Seleccione primero una o más líneas.","Sélectionnez d'abord une ou plusieurs lignes.","Zuerst eine oder mehrere Zeilen auswählen.","Сначала выберите одну или несколько строк.","まず 1 行以上を選択してください。","먼저 한 줄 이상 선택하세요.");
    add("Nothing to delete on that side.","该侧没有可删除的内容。","該側沒有可刪除的內容。","Nada que eliminar en ese lado.","Rien à supprimer de ce côté.","Nichts auf dieser Seite zu löschen.","На этой стороне нечего удалять.","その側には削除するものがありません。","그쪽에 삭제할 내용이 없습니다.");
    add("That row is a gap on this side — nothing to edit.","此行在该侧为空，无法编辑。","此行在該側為空，無法編輯。","Esa fila está vacía en este lado — nada que editar.","Cette ligne est vide de ce côté — rien à modifier.","Diese Zeile ist auf dieser Seite leer — nichts zu bearbeiten.","Эта строка на этой стороне пустая — редактировать нечего.","この行はこちら側が空です — 編集するものがありません。","이 행은 이쪽에 비어 있어 편집할 내용이 없습니다.");
    add("Moved","已移动","已移動","Movido","Déplacé","Verschoben","Перемещено","移動しました","이동됨");
    add("Copied","已复制","已複製","Copiado","Copié","Kopiert","Скопировано","コピーしました","복사됨");
    add("Deleted","已删除","已刪除","Eliminado","Supprimé","Gelöscht","Удалено","削除しました","삭제됨");
    add("%1 %2 line(s).","%1 %2 行。","%1 %2 行。","%1 %2 línea(s).","%1 %2 ligne(s).","%1 %2 Zeile(n).","%1 %2 строк.","%1 行を %2 。","%1 %2 줄.");
    add("Edited %1 line %2. Use \"Save L\" / \"Save R\" to write the file.",
        "已编辑 %1 第 %2 行。使用 \"Save L\" / \"Save R\" 写入文件。",
        "已編輯 %1 第 %2 行。使用 \"Save L\" / \"Save R\" 寫入檔案。",
        "Se editó la línea %2 de %1. Use \"Save L\" / \"Save R\" para escribir el archivo.",
        "Ligne %2 de %1 modifiée. Utilisez \"Save L\" / \"Save R\" pour écrire le fichier.",
        "Zeile %2 (%1) bearbeitet. Verwenden Sie \"Save L\" / \"Save R\", um die Datei zu schreiben.",
        "Изменена строка %2 (%1). Используйте \"Save L\" / \"Save R\", чтобы записать файл.",
        "%1 の %2 行目を編集しました。「Save L」/「Save R」でファイルに書き込んでください。",
        "%1 의 %2번째 줄을 편집했습니다. \"Save L\" / \"Save R\" 로 파일에 기록하세요.");
    add("left","左侧","左側","izquierdo","gauche","links","левой","左","왼쪽");
    add("right","右侧","右側","derecho","droite","rechts","правой","右","오른쪽");

    add("Highlight",         "高亮",      "螢光標示",  "Resaltar",     "Surligner",     "Hervorheben",   "Выделение",       "ハイライト",    "형광펜");
    add("Underline",         "下划线",    "底線",      "Subrayar",     "Souligner",     "Unterstreichen","Подчёркивание",   "下線",          "밑줄");
    add("Strike",            "删除线",    "刪除線",    "Tachar",       "Barrer",        "Durchstreichen","Зачёркивание",    "取り消し線",    "취소선");
    add("Text Box",          "文本框",    "文字方塊",  "Cuadro de texto","Zone de texte","Textfeld",      "Текстовое поле",  "テキストボックス","텍스트 상자");
    add("Sticky Note",       "便笺",      "便利貼",    "Nota adhesiva","Pense-bête",    "Notizzettel",   "Заметка",         "付箋",          "메모");
    add("Pen",               "画笔",      "畫筆",      "Lápiz",        "Stylo",         "Stift",         "Карандаш",        "ペン",          "펜");
    add("Stamp",             "图章",      "圖章",      "Sello",        "Tampon",        "Stempel",       "Штамп",           "スタンプ",      "스탬프");
    add("Color...",          "颜色...",   "顏色...",   "Color...",     "Couleur...",    "Farbe...",      "Цвет...",         "色...",         "색상...");
    add("Annotation Color",  "批注颜色",  "註解顏色",  "Color de anotación","Couleur d'annotation","Anmerkungsfarbe","Цвет аннотации","注釈の色",      "주석 색상");
    add("Box Width...",      "文本框线宽...","文字方塊線寬...","Grosor del borde...","Épaisseur de bordure...","Rahmenbreite...","Толщина рамки...","枠線の太さ...","테두리 두께...");
    add("Text Box Line Width","文本框线宽","文字方塊線寬","Grosor del cuadro de texto","Épaisseur de la zone de texte","Textfeld-Rahmenbreite","Толщина рамки текстового поля","テキストボックス枠の太さ","텍스트 상자 테두리 두께");
    add("Border thickness (points, 0 = none):","边框粗细（点，0 = 无边框）：","邊框粗細（點，0 = 無邊框）：","Grosor del borde (puntos, 0 = ninguno):","Épaisseur de la bordure (points, 0 = aucune) :","Rahmendicke (Punkte, 0 = keine):","Толщина рамки (пт, 0 = нет):","枠線の太さ（ポイント、0 = なし）：","테두리 두께(포인트, 0 = 없음):");
    add("Text box border set to %1 pt.","文本框边框已设为 %1 pt。","文字方塊邊框已設為 %1 pt。","Borde del cuadro de texto establecido en %1 pt.","Bordure de la zone de texte réglée sur %1 pt.","Textfeld-Rahmen auf %1 pt gesetzt.","Толщина рамки текстового поля: %1 пт.","テキストボックス枠を %1 pt に設定しました。","텍스트 상자 테두리를 %1 pt 로 설정했습니다.");
    add("Enter annotation text:","输入批注文本：","輸入註解文字：","Introduzca el texto de la anotación:","Saisissez le texte de l'annotation :","Anmerkungstext eingeben:","Введите текст аннотации:","注釈テキストを入力：","주석 텍스트 입력:");

    // Page management.
    add("Merge",             "合并",      "合併",      "Combinar",     "Fusionner",     "Zusammenführen","Объединить",      "結合",          "병합");
    add("Merged %1 file(s).","已合并 %1 个文件。","已合併 %1 個檔案。","Se combinaron %1 archivo(s).","%1 fichier(s) fusionné(s).","%1 Datei(en) zusammengeführt.","Объединено файлов: %1.","%1 件のファイルを結合しました。","%1개 파일을 병합했습니다.");
    add("Split",             "拆分",      "分割",      "Dividir",      "Diviser",       "Teilen",        "Разделить",       "分割",          "분할");
    add("Split PDF",         "拆分 PDF",  "分割 PDF",  "Dividir PDF",  "Diviser le PDF","PDF teilen",    "Разделить PDF",   "PDF を分割",    "PDF 분할");
    add("Split AFTER page number:","在此页码之后拆分：","在此頁碼之後分割：","Dividir DESPUÉS de la página número:","Diviser APRÈS la page numéro :","Teilen NACH Seitennummer:","Разделить ПОСЛЕ страницы номер:","このページ番号の後で分割：","다음 페이지 번호 뒤에서 분할:");
    add("Nothing to split.", "没有可拆分的内容。","沒有可分割的內容。","No hay nada que dividir.","Rien à diviser.","Nichts zu teilen.","Нечего разделять.","分割するものがありません。","분할할 내용이 없습니다.");
    add("Split into 2 files in %1","已拆分为 2 个文件，位于 %1","已分割為 2 個檔案，位於 %1","Dividido en 2 archivos en %1","Divisé en 2 fichiers dans %1","In 2 Dateien geteilt in %1","Разделено на 2 файла в %1","%1 に 2 つのファイルへ分割しました","%1 에 2개 파일로 분할했습니다");
    add("Delete Page",       "删除页面",  "刪除頁面",  "Eliminar página","Supprimer la page","Seite löschen","Удалить страницу","ページを削除",  "페이지 삭제");
    add("A PDF must have at least one page.","PDF 至少必须包含一页。","PDF 至少必須包含一頁。","Un PDF debe tener al menos una página.","Un PDF doit comporter au moins une page.","Ein PDF muss mindestens eine Seite haben.","В PDF должна быть хотя бы одна страница.","PDF には少なくとも 1 ページが必要です。","PDF에는 최소 한 페이지가 있어야 합니다.");
    add("Insert",            "插入",      "插入",      "Insertar",     "Insérer",       "Einfügen",      "Вставить",        "挿入",          "삽입");
    add("Inserted %1 file(s).","已插入 %1 个文件。","已插入 %1 個檔案。","Se insertaron %1 archivo(s).","%1 fichier(s) inséré(s).","%1 Datei(en) eingefügt.","Вставлено файлов: %1.","%1 件のファイルを挿入しました。","%1개 파일을 삽입했습니다.");
    add("Extract",           "提取",      "擷取",      "Extraer",      "Extraire",      "Extrahieren",   "Извлечь",         "抽出",          "추출");
    add("Extract Pages",     "提取页面",  "擷取頁面",  "Extraer páginas","Extraire des pages","Seiten extrahieren","Извлечь страницы","ページを抽出",  "페이지 추출");
    add("Pages to extract (e.g. 1-3,5,8):","要提取的页面（例如 1-3,5,8）：","要擷取的頁面（例如 1-3,5,8）：","Páginas a extraer (p. ej. 1-3,5,8):","Pages à extraire (ex. 1-3,5,8) :","Zu extrahierende Seiten (z. B. 1-3,5,8):","Страницы для извлечения (напр. 1-3,5,8):","抽出するページ（例: 1-3,5,8）：","추출할 페이지 (예: 1-3,5,8):");
    add("Extracted %1 page(s) to %2","已提取 %1 页到 %2","已擷取 %1 頁至 %2","Se extrajeron %1 página(s) a %2","%1 page(s) extraite(s) vers %2","%1 Seite(n) extrahiert nach %2","Извлечено страниц: %1 в %2","%1 ページを %2 に抽出しました","%1개 페이지를 %2 에 추출했습니다");
    add("Save extracted PDF","保存提取的 PDF","儲存擷取的 PDF","Guardar el PDF extraído","Enregistrer le PDF extrait","Extrahiertes PDF speichern","Сохранить извлечённый PDF","抽出した PDF を保存","추출한 PDF 저장");
    add("Move Up",           "上移",      "上移",      "Subir",        "Monter",        "Nach oben",     "Вверх",           "上へ移動",      "위로 이동");
    add("Move Down",         "下移",      "下移",      "Bajar",        "Descendre",     "Nach unten",    "Вниз",            "下へ移動",      "아래로 이동");
    add("Select PDF(s) to append","选择要追加的 PDF","選擇要附加的 PDF","Seleccione el/los PDF a añadir","Sélectionnez le(s) PDF à ajouter","PDF(s) zum Anhängen auswählen","Выберите PDF для добавления","追加する PDF を選択","추가할 PDF 선택");
    add("Select PDF(s) to insert","选择要插入的 PDF","選擇要插入的 PDF","Seleccione el/los PDF a insertar","Sélectionnez le(s) PDF à insérer","PDF(s) zum Einfügen auswählen","Выберите PDF для вставки","挿入する PDF を選択","삽입할 PDF 선택");
    add("Output folder",     "输出文件夹","輸出資料夾","Carpeta de salida","Dossier de sortie","Ausgabeordner","Папка вывода",    "出力フォルダー","출력 폴더");
    add("PDF Files (*.pdf)", "PDF 文件 (*.pdf)","PDF 檔案 (*.pdf)","Archivos PDF (*.pdf)","Fichiers PDF (*.pdf)","PDF-Dateien (*.pdf)","Файлы PDF (*.pdf)","PDF ファイル (*.pdf)","PDF 파일 (*.pdf)");
    add("Images (*.png *.jpg *.jpeg *.bmp)","图像 (*.png *.jpg *.jpeg *.bmp)","圖像 (*.png *.jpg *.jpeg *.bmp)","Imágenes (*.png *.jpg *.jpeg *.bmp)","Images (*.png *.jpg *.jpeg *.bmp)","Bilder (*.png *.jpg *.jpeg *.bmp)","Изображения (*.png *.jpg *.jpeg *.bmp)","画像 (*.png *.jpg *.jpeg *.bmp)","이미지 (*.png *.jpg *.jpeg *.bmp)");

    // Open / save status messages.
    add("Failed to open PDF: %1","打开 PDF 失败：%1","開啟 PDF 失敗：%1","No se pudo abrir el PDF: %1","Échec de l'ouverture du PDF : %1","PDF konnte nicht geöffnet werden: %1","Не удалось открыть PDF: %1","PDF を開けませんでした：%1","PDF를 열지 못했습니다: %1");
    add("Opened %1 (%2 pages)","已打开 %1（%2 页）","已開啟 %1（%2 頁）","Se abrió %1 (%2 páginas)","%1 ouvert (%2 pages)","%1 geöffnet (%2 Seiten)","Открыт %1 (%2 стр.)","%1 を開きました（%2 ページ）","%1 을 열었습니다 (%2 페이지)");
    add("Cannot write %1",   "无法写入 %1","無法寫入 %1","No se puede escribir %1","Impossible d'écrire %1","%1 kann nicht geschrieben werden","Не удаётся записать %1","%1 に書き込めません","%1 에 쓸 수 없습니다");
    add("untitled.pdf",      "未命名.pdf","未命名.pdf","sin_titulo.pdf","sans-titre.pdf","unbenannt.pdf", "без_имени.pdf",   "無題.pdf",      "제목없음.pdf");

    // Signatures.
    add("Sign (draw)",       "签名（手绘）","簽名（手繪）","Firmar (dibujar)","Signer (dessiner)","Signieren (zeichnen)","Подпись (рисунок)","署名（手書き）","서명 (그리기)");
    add("Sign (image)",      "签名（图片）","簽名（圖片）","Firmar (imagen)","Signer (image)","Signieren (Bild)","Подпись (изображение)","署名（画像）","서명 (이미지)");
    add("Draw Signature",    "绘制签名",  "繪製簽名",  "Dibujar firma","Dessiner la signature","Signatur zeichnen","Нарисовать подпись","署名を描く",    "서명 그리기");
    add("Draw your signature below, then click OK.","请在下方绘制签名，然后点击确定。","請在下方繪製簽名，然後按一下確定。","Dibuje su firma abajo y luego haga clic en Aceptar.","Dessinez votre signature ci-dessous, puis cliquez sur OK.","Zeichnen Sie unten Ihre Signatur und klicken Sie dann auf OK.","Нарисуйте подпись ниже, затем нажмите OK.","下に署名を描いてから OK をクリックしてください。","아래에 서명을 그린 다음 확인을 클릭하세요.");
    add("Signature added to page %1.","已将签名添加到第 %1 页。","已將簽名新增至第 %1 頁。","Firma añadida a la página %1.","Signature ajoutée à la page %1.","Signatur zu Seite %1 hinzugefügt.","Подпись добавлена на страницу %1.","%1 ページに署名を追加しました。","%1 페이지에 서명을 추가했습니다.");
    add("Choose signature image","选择签名图片","選擇簽名圖片","Elegir imagen de firma","Choisir l'image de signature","Signaturbild auswählen","Выберите изображение подписи","署名画像を選択","서명 이미지 선택");
    add("Image signature added.","已添加图片签名。","已新增圖片簽名。","Firma de imagen añadida.","Signature image ajoutée.","Bildsignatur hinzugefügt.","Подпись-изображение добавлена.","画像署名を追加しました。","이미지 서명을 추가했습니다.");

    // Conversion / export.
    add("To Images",         "转为图片",  "轉為圖片",  "A imágenes",   "Vers images",   "Zu Bildern",    "В изображения",   "画像へ",        "이미지로");
    add("Image Format",      "图片格式",  "圖片格式",  "Formato de imagen","Format d'image","Bildformat",  "Формат изображения","画像形式",      "이미지 형식");
    add("Export each page as:","将每页导出为：","將每頁匯出為：","Exportar cada página como:","Exporter chaque page en :","Jede Seite exportieren als:","Экспортировать каждую страницу как:","各ページを次の形式でエクスポート：","각 페이지를 다음으로 내보내기:");
    add("Exported %1 page image(s) to %2","已导出 %1 张页面图片到 %2","已匯出 %1 張頁面圖片至 %2","Se exportaron %1 imagen(es) de página a %2","%1 image(s) de page exportée(s) vers %2","%1 Seitenbild(er) exportiert nach %2","Экспортировано изображений страниц: %1 в %2","%1 枚のページ画像を %2 にエクスポートしました","%1개 페이지 이미지를 %2 에 내보냈습니다");
    add("To Word",           "转为 Word", "轉為 Word", "A Word",       "Vers Word",     "Zu Word",       "В Word",          "Word へ",       "Word로");
    add("Export to Word (RTF)","导出为 Word (RTF)","匯出為 Word (RTF)","Exportar a Word (RTF)","Exporter vers Word (RTF)","Nach Word (RTF) exportieren","Экспорт в Word (RTF)","Word (RTF) にエクスポート","Word (RTF)로 내보내기");
    add("Word / RTF document (*.rtf)","Word / RTF 文档 (*.rtf)","Word / RTF 文件 (*.rtf)","Documento Word / RTF (*.rtf)","Document Word / RTF (*.rtf)","Word-/RTF-Dokument (*.rtf)","Документ Word / RTF (*.rtf)","Word / RTF 文書 (*.rtf)","Word / RTF 문서 (*.rtf)");
    add("Exported text to %1 (layout not preserved).","已将文本导出到 %1（未保留版面）。","已將文字匯出至 %1（未保留版面）。","Texto exportado a %1 (no se conserva el diseño).","Texte exporté vers %1 (mise en page non conservée).","Text nach %1 exportiert (Layout nicht erhalten).","Текст экспортирован в %1 (макет не сохранён).","テキストを %1 にエクスポートしました（レイアウトは保持されません）。","텍스트를 %1 에 내보냈습니다 (레이아웃은 유지되지 않음).");
    add("No extractable text was found. This PDF may be a scanned image; use \"To Images\" instead.",
        "未找到可提取的文本。此 PDF 可能是扫描图片；请改用“转为图片”。",
        "找不到可擷取的文字。此 PDF 可能是掃描圖片；請改用「轉為圖片」。",
        "No se encontró texto extraíble. Este PDF puede ser una imagen escaneada; utilice \"A imágenes\" en su lugar.",
        "Aucun texte extractible trouvé. Ce PDF est peut-être une image numérisée ; utilisez « Vers images » à la place.",
        "Kein extrahierbarer Text gefunden. Dieses PDF ist möglicherweise ein Scan; verwenden Sie stattdessen \"Zu Bildern\".",
        "Извлекаемый текст не найден. Возможно, это отсканированный PDF; используйте «В изображения».",
        "抽出可能なテキストが見つかりません。この PDF はスキャン画像の可能性があります。「画像へ」をご利用ください。",
        "추출 가능한 텍스트가 없습니다. 이 PDF는 스캔 이미지일 수 있습니다. 대신 \"이미지로\"를 사용하세요.");

    // Compression.
    add("Compress",          "压缩",      "壓縮",      "Comprimir",    "Compresser",    "Komprimieren",  "Сжать",           "圧縮",          "압축");
    add("Save compressed PDF","保存压缩后的 PDF","儲存壓縮後的 PDF","Guardar el PDF comprimido","Enregistrer le PDF compressé","Komprimiertes PDF speichern","Сохранить сжатый PDF","圧縮した PDF を保存","압축한 PDF 저장");
    add("Compressed: %1 KB → %2 KB (%3% smaller)","已压缩：%1 KB → %2 KB（减小 %3%）","已壓縮：%1 KB → %2 KB（縮小 %3%）","Comprimido: %1 KB → %2 KB (%3% menos)","Compressé : %1 KB → %2 KB (%3% de moins)","Komprimiert: %1 KB → %2 KB (%3% kleiner)","Сжато: %1 КБ → %2 КБ (на %3% меньше)","圧縮：%1 KB → %2 KB（%3% 削減）","압축: %1 KB → %2 KB (%3% 감소)");

    // Watermark.
    add("Text WM",           "文字水印",  "文字水印",  "Marca de agua (texto)","Filigrane texte","Text-WZ",   "Текст. водяной знак","テキスト透かし","텍스트 워터마크");
    add("Image WM",          "图片水印",  "圖片水印",  "Marca de agua (imagen)","Filigrane image","Bild-WZ",  "Изобр. водяной знак","画像透かし",    "이미지 워터마크");
    add("Text watermark applied to all pages.","已为所有页面添加文字水印。","已為所有頁面套用文字水印。","Marca de agua de texto aplicada a todas las páginas.","Filigrane texte appliqué à toutes les pages.","Textwasserzeichen auf alle Seiten angewendet.","Текстовый водяной знак применён ко всем страницам.","すべてのページにテキスト透かしを適用しました。","모든 페이지에 텍스트 워터마크를 적용했습니다.");
    add("Choose watermark image","选择水印图片","選擇水印圖片","Elegir imagen de marca de agua","Choisir l'image du filigrane","Wasserzeichenbild auswählen","Выберите изображение водяного знака","透かし画像を選択","워터마크 이미지 선택");
    add("Image watermark applied to all pages.","已为所有页面添加图片水印。","已為所有頁面套用圖片水印。","Marca de agua de imagen aplicada a todas las páginas.","Filigrane image appliqué à toutes les pages.","Bildwasserzeichen auf alle Seiten angewendet.","Водяной знак-изображение применён ко всем страницам.","すべてのページに画像透かしを適用しました。","모든 페이지에 이미지 워터마크를 적용했습니다.");
    add("CONFIDENTIAL",      "机密",      "機密",      "CONFIDENCIAL", "CONFIDENTIEL",  "VERTRAULICH",   "КОНФИДЕНЦИАЛЬНО", "機密",          "기밀");

    // Protection.
    add("Password",          "密码",      "密碼",      "Contraseña",   "Mot de passe",  "Kennwort",      "Пароль",          "パスワード",    "비밀번호");
    add("Set Open Password", "设置打开密码","設定開啟密碼","Establecer contraseña de apertura","Définir le mot de passe d'ouverture","Öffnen-Kennwort festlegen","Задать пароль открытия","開くパスワードを設定","열기 비밀번호 설정");
    add("Password required to open the PDF:","打开此 PDF 所需的密码：","開啟此 PDF 所需的密碼：","Contraseña necesaria para abrir el PDF:","Mot de passe requis pour ouvrir le PDF :","Kennwort zum Öffnen des PDFs:","Пароль, необходимый для открытия PDF:","PDF を開くのに必要なパスワード：","PDF를 열기 위한 비밀번호:");
    add("Save encrypted PDF","保存加密的 PDF","儲存加密的 PDF","Guardar el PDF cifrado","Enregistrer le PDF chiffré","Verschlüsseltes PDF speichern","Сохранить зашифрованный PDF","暗号化した PDF を保存","암호화된 PDF 저장");
    add("Encrypted PDF written to %1","已将加密的 PDF 写入 %1","已將加密的 PDF 寫入 %1","PDF cifrado escrito en %1","PDF chiffré écrit dans %1","Verschlüsseltes PDF geschrieben nach %1","Зашифрованный PDF записан в %1","暗号化した PDF を %1 に書き込みました","암호화된 PDF를 %1 에 저장했습니다");

    // About dialog: the Drag & Drop feature paragraph.
    add("drop a text file, a source file, an image, a PDF or any binary anywhere on the window and it opens in the matching tab: the format is detected from the file's own content (libmagic sniffing, not the extension), so text and source code go to the editor, images to the image editor, PDFs to the PDF viewer, and everything else to the hex editor. Several files can be dropped at once (each one is routed individually and a per-type summary is reported), and dropping exactly two folders starts a folder comparison.", "将文本文件、源代码、图片、PDF 或任意二进制文件拖放到窗口的任意位置，就会在对应的标签页中打开：格式根据文件自身的内容识别（libmagic 内容嗅探，而不是扩展名），所以文本和源代码进入代码编辑器，图片进入图片编辑器，PDF 进入 PDF 阅读器，其余全部进入十六进制编辑器。可以一次拖入多个文件（每个文件单独分派，并按类型给出汇总）；恰好拖入两个文件夹则开始文件夹比较。", "將文字檔案、原始程式碼、圖片、PDF 或任何二進位檔案拖放到視窗的任何位置，就會在對應的標籤頁中開啟：格式依檔案本身的內容偵測（libmagic 內容嗅探，而非擴展名），所以文字與原始程式碼進入程式碼編輯器，圖片進入圖片編輯器，PDF 進入 PDF 閱讀器，其餘全部進入十六進位編輯器。可以一次拖入多個檔案（每個檔案分別分派，並依類型給出彙總）；恰好拖入兩個資料夾則開始資料夾比較。", "suelte un archivo de texto, un archivo fuente, una imagen, un PDF o cualquier binario en cualquier parte de la ventana y se abrirá en la pestaña correspondiente: el formato se detecta a partir del propio contenido del archivo (análisis con libmagic, no la extensión), de modo que el texto y el código fuente van al editor, las imágenes al editor de imágenes, los PDF al visor de PDF y todo lo demás al editor hexadecimal. Se pueden soltar varios archivos a la vez (cada uno se enruta por separado y se informa un resumen por tipo) y soltar exactamente dos carpetas inicia una comparación de carpetas.", "déposez un fichier texte, un fichier source, une image, un PDF ou n'importe quel binaire n'importe où sur la fenêtre et il s'ouvre dans l'onglet correspondant : le format est détecté à partir du contenu du fichier lui-même (analyse libmagic, pas l'extension), si bien que le texte et le code source vont à l'éditeur, les images à l'éditeur d'images, les PDF à la visionneuse PDF et tout le reste à l'éditeur hexadécimal. Plusieurs fichiers peuvent être déposés en une fois (chacun est orienté individuellement et un résumé par type est affiché) et déposer exactement deux dossiers lance une comparaison de dossiers.", "Legen Sie eine Textdatei, eine Quelldatei, ein Bild, ein PDF oder eine beliebige Binärdatei irgendwo auf dem Fenster ab, und sie wird im passenden Tab geöffnet: das Format wird aus dem Inhalt der Datei selbst erkannt (libmagic-Analyse, nicht die Dateiendung), sodass Text und Quellcode in den Editor, Bilder in den Bildeditor, PDFs in den PDF-Betrachter und alles andere in den Hex-Editor gehen. Mehrere Dateien können gleichzeitig abgelegt werden (jede wird einzeln zugeordnet und eine Zusammenfassung pro Typ wird gemeldet), und das Ablegen von genau zwei Ordnern startet einen Ordnervergleich.", "перетащите текстовый файл, исходный код, изображение, PDF или любой бинарный файл в любое место окна — и он откроется в соответствующей вкладке: формат определяется по содержимому самого файла (анализ libmagic, а не расширение), поэтому текст и код попадают в редактор, изображения — в редактор изображений, PDF — в просмотрщик PDF, а всё остальное — в HEX-редактор. Можно перетащить сразу несколько файлов (каждый обрабатывается отдельно, с итоговой сводкой по типам), а перетаскивание ровно двух папок запускает сравнение папок.", "テキストファイル、ソースファイル、画像、PDF、任意のバイナリをウィンドウのどこにドロップしても、対応するタブで開きます。形式は拡張子ではなくファイル自身の内容（libmagic による判定）から検出されるため、テキストとソースコードはエディタ、画像は画像エディタ、PDF は PDF ビューアー、その他はすべて 16 進エディタへ振り分けられます。複数のファイルを一度にドロップでき（それぞれ別々に振り分けられ、種類ごとの集計が表示されます）、フォルダーをちょうど 2 つドロップするとフォルダー比較が始まります。", "텍스트 파일, 소스 파일, 이미지, PDF 또는 임의의 바이너리를 창의 아무 위치에 드롭하면 해당 탭에서 열립니다. 형식은 확장자가 아니라 파일 자신의 내용(libmagic 분석)에서 검지되므로 텍스트와 소스 코드는 에디터로, 이미지는 이미지 에디터로, PDF는 PDF 뷰어로, 그 외 모든 것은 16진수 에디터로 열립니다. 여러 파일을 한 번에 드롭할 수 있으며(개각이 개별로 처리되고 유형별 요약이 표시됩니다), 폴더를 정확히 도 개 드롭하면 폴더 바교가 시작됩니다.");

    // Drag & drop (open any dropped file by its detected format).
    add("Drag & Drop", "拖放", "拖放", "Arrastrar y soltar", "Glisser-deposer", "Drag & Drop", "Перетаскивание", "ドラッグ＆ドロップ", "끌어오기");
    add("Open Dropped Files", "打开拖入的文件", "開啟拖入的檔案", "Abrir los archivos soltados", "Ouvrir les fichiers deposes", "Abgelegte Dateien öffnen", "Открыть перетащенные файлы", "ドロップしたファイルを開く", "드롭한 파일 여기");
    add("Open all %1 dropped files in separate tabs?", "将拖入的全部 %1 个文件分别在新标签页中打开？", "將拖入的全部 %1 個檔案分別在新標籤頁中開啟？", "¿Abrir los %1 archivos soltados en pestañas separadas?", "Ouvrir les %1 fichiers déposés dans des onglets séparés ?", "Alle %1 abgelegten Dateien in eigenen Tabs öffnen?", "Открыть все %1 перетащенных файлов в отдельных вкладках?", "ドロップした %1 件のファイルをすべて別のタブで開きますか？", "드롭한 %1개 파일을 모두 개별 탭에서 여시게요?");
    add("Opened %1 dropped file(s) by detected format: %2", "已按检测到的格式打开 %1 个拖入的文件：%2", "已按偵測到的格式開啟 %1 個拖入的檔案：%2", "Se abrieron %1 archivo(s) soltado(s) segun el formato detectado: %2", "%1 fichier(s) déposé(s) ouvert(s) selon le format détecté : %2", "%1 abgelegte Datei(en) nach erkanntem Format geöffnet: %2", "Открыто файлов: %1 (по распознанному формату): %2", "検出した形式に従って %1 件のファイルを開きました：%2", "검지된 형식에 따라 드롭한 파일 %1개를 여었습니다: %2");
    add("%1 dropped file(s) could not be opened.", "有 %1 个拖入的文件无法打开。", "有 %1 個拖入的檔案無法開啟。", "No se pudieron abrir %1 archivo(s) soltado(s).", "%1 fichier(s) déposé(s) n'ont pas pu être ouverts.", "%1 abgelegte Datei(en) konnten nicht geöffnet werden.", "Не удалось открыть файлов: %1.", "%1 件のドロップしたファイルを開けませんでした。", "드롭한 파일 %1개를 여지 못했습니다.");
    add("Comparing the two dropped folders.", "正在比较拖入的两个文件夹。", "正在比較拖入的兩個資料夾。", "Comparando las dos carpetas soltadas.", "Comparaison des deux dossiers déposés.", "Die beiden abgelegten Ordner werden verglichen.", "Сравнение двух перетащенных папок.", "ドロップした 2 つのフォルダーを比較しています。", "드롭한 도 폴더를 바교하고 있어요.");
    add("Skipped %1 folder(s) — drop exactly two folders to compare them.", "已跳过 %1 个文件夹 — 请恰好拖入两个文件夹以进行比较。", "已跳過 %1 個資料夾 — 請恰好拖入兩個資料夾以進行比較。", "Se omitieron %1 carpeta(s) — suelte exactamente dos carpetas para compararlas.", "%1 dossier(s) ignoré(s) — déposez exactement deux dossiers pour les comparer.", "%1 Ordner übersprungen — legen Sie genau zwei Ordner ab, um sie zu vergleichen.", "Пропущено папок: %1 — перетащите ровно две папки для сравнения.", "%1 件のフォルダーをスキップしました — 比較するにはフォルダーをちょうど 2 つドロップしてください。", "폴더 %1개를 건넌여었습니다 — 바교하려면 폴더를 정확히 도 개 드롭하십시오.");
    add("%1 text", "%1 个文本", "%1 個文字", "%1 de texto", "%1 texte", "%1 Text", "%1 текстовых", "テキスト %1 件", "텍스트 %1개");
    add("%1 image", "%1 个图片", "%1 個圖片", "%1 de imagen", "%1 image", "%1 Bild", "%1 изображений", "画像 %1 件", "이미지 %1개");
    add("%1 PDF", "%1 个 PDF", "%1 個 PDF", "%1 PDF", "%1 PDF", "%1 PDF", "%1 PDF", "PDF %1 件", "PDF %1개");
    add("%1 binary", "%1 个二进制", "%1 個二進位", "%1 binario(s)", "%1 binaire", "%1 Binär", "%1 бинарных", "バイナリ %1 件", "바이너리 %1개");

    // Forms.
    add("Fill Form",         "填写表单",  "填寫表單",  "Rellenar formulario","Remplir le formulaire","Formular ausfüllen","Заполнить форму","フォームに入力","양식 채우기");
    add("No fillable text fields were found in this PDF.","此 PDF 中未找到可填写的文本字段。","此 PDF 中找不到可填寫的文字欄位。","No se encontraron campos de texto rellenables en este PDF.","Aucun champ de texte remplissable trouvé dans ce PDF.","In diesem PDF wurden keine ausfüllbaren Textfelder gefunden.","В этом PDF не найдено заполняемых текстовых полей.","この PDF には入力可能なテキストフィールドがありません。","이 PDF에서 입력 가능한 텍스트 필드를 찾을 수 없습니다.");
    add("Filled %1 field(s).","已填写 %1 个字段。","已填寫 %1 個欄位。","Se rellenaron %1 campo(s).","%1 champ(s) rempli(s).","%1 Feld(er) ausgefüllt.","Заполнено полей: %1.","%1 件のフィールドに入力しました。","%1개 필드를 채웠습니다.");
}
