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

void I18n::initialize()
{
    loadTable();

    // Restore the persisted language, defaulting to English on first launch.
    QSettings s;
    QString saved = s.value(QStringLiteral("ui/language"), QString()).toString();
    if (saved.isEmpty() || !m_codes.contains(saved))
        saved = QStringLiteral("en");
    m_current = saved;
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

void I18n::loadTable()
{
    // Language codes and their self-localized display names.
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

    // ---- Cloud drives ----
    add("Clou&d",            "云盘(&D)",  "雲端(&D)",  "&Nube",        "&Cloud",        "&Cloud",        "&Облако",         "クラウド(&D)",  "클라우드(&D)");
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
    add("Cancel",            "取消",      "取消",      "Cancelar",     "Annuler",       "Abbrechen",     "Отмена",          "キャンセル",    "취소");
    add("Retry",             "重试",      "重試",      "Reintentar",   "Réessayer",     "Wiederholen",   "Повторить",       "再試行",        "다시 시도");
    add("Remove",            "移除",      "移除",      "Quitar",       "Retirer",       "Entfernen",     "Убрать",          "削除",          "제거");
    add("Clear Finished",    "清除已完成","清除已完成","Borrar completados","Effacer les terminés","Abgeschlossene löschen","Очистить завершённые","完了分をクリア","완료 항목 지우기");
    add("Copy",              "复制",      "複製",      "Copiar",       "Copier",        "Kopieren",      "Копировать",      "コピー",        "복사");
    add("Copy Name",         "复制名称",  "複製名稱",  "Copiar nombre","Copier le nom", "Namen kopieren","Копировать имя",  "名前をコピー",  "이름 복사");
    add("Copy Local Path",   "复制本地路径","複製本機路徑","Copiar ruta local","Copier le chemin local","Lokalen Pfad kopieren","Копировать локальный путь","ローカルパスをコピー","로컬 경로 복사");
    add("Copy Remote Path",  "复制远程路径","複製遠端路徑","Copiar ruta remota","Copier le chemin distant","Remote-Pfad kopieren","Копировать удалённый путь","リモートパスをコピー","원격 경로 복사");
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
    add("multi-tab editor with syntax highlighting, themes, find/replace, encodings, column/block edit, clipboard history, auto-save.",
        "多标签编辑器，含语法高亮、主题、查找/替换、编码、列/块编辑、剪贴板历史、自动保存。",
        "多分頁編輯器，含語法高亮、佈景主題、尋找/取代、編碼、欄/區塊編輯、剪貼簿歷史、自動儲存。",
        "editor de varias pestañas con resaltado de sintaxis, temas, buscar/reemplazar, codificaciones, edición de columna/bloque, historial del portapapeles, autoguardado.",
        "éditeur multi-onglets avec coloration syntaxique, thèmes, rechercher/remplacer, encodages, édition colonne/bloc, historique du presse-papiers, enregistrement auto.",
        "Multi-Tab-Editor mit Syntaxhervorhebung, Designs, Suchen/Ersetzen, Kodierungen, Spalten-/Blockbearbeitung, Zwischenablage-Verlauf, Autospeichern.",
        "многовкладочный редактор с подсветкой синтаксиса, темами, поиском/заменой, кодировками, редактированием столбцов/блоков, историей буфера, автосохранением.",
        "構文強調・テーマ・検索/置換・エンコード・列/ブロック編集・クリップボード履歴・自動保存を備えたマルチタブエディタ。",
        "구문 강조, 테마, 찾기/바꾸기, 인코딩, 열/블록 편집, 클립보드 기록, 자동 저장을 갖춘 다중 탭 편집기.");
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