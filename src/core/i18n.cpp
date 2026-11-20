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
    m_codes = {
        QStringLiteral("en"), QStringLiteral("zh_CN"), QStringLiteral("zh_TW"),
        QStringLiteral("es"), QStringLiteral("fr"),    QStringLiteral("de"),
        QStringLiteral("ru"), QStringLiteral("ja"),    QStringLiteral("ko"),
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

    // ---- Local shell terminals (cmd / PowerShell) ----
    add("New &CMD Terminal",  "新建CMD终端(&C)","新增CMD終端(&C)","Nueva terminal &CMD","Nouveau terminal &CMD","Neues &CMD-Terminal","Новый терминал &CMD","新規CMD端末(&C)","새 &CMD 터미널");
    add("New &PowerShell Terminal","新建PowerShell终端(&P)","新增PowerShell終端(&P)","Nueva terminal &PowerShell","Nouveau terminal &PowerShell","Neues &PowerShell-Terminal","Новый терминал &PowerShell","新規PowerShell端末(&P)","새 &PowerShell 터미널");
    add("CMD",               "CMD",       "CMD",       "CMD",          "CMD",           "CMD",           "CMD",             "CMD",           "CMD");
    add("PowerShell",        "PowerShell","PowerShell","PowerShell",   "PowerShell",    "PowerShell",    "PowerShell",      "PowerShell",    "PowerShell");
    add("Failed to start the shell process.",
        "无法启动 Shell 进程。","無法啟動 Shell 行程。","No se pudo iniciar el proceso del shell.",
        "Impossible de démarrer le processus du shell.","Shell-Prozess konnte nicht gestartet werden.",
        "Не удалось запустить процесс оболочки.","シェルプロセスの起動に失敗しました。","셸 프로세스를 시작하지 못했습니다.");
    add("failed to start the shell process",
        "无法启动 Shell 进程","無法啟動 Shell 行程","no se pudo iniciar el proceso del shell",
        "impossible de démarrer le processus du shell","Shell-Prozess konnte nicht gestartet werden",
        "не удалось запустить процесс оболочки","シェルプロセスの起動に失敗しました","셸 프로세스를 시작하지 못했습니다");
    add("process exited (code %1)",
        "进程已退出（代码 %1）","行程已結束（代碼 %1）","el proceso terminó (código %1)",
        "le processus s'est terminé (code %1)","Prozess beendet (Code %1)",
        "процесс завершён (код %1)","プロセスが終了しました（コード %1）","프로세스 종료됨(코드 %1)");

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
}