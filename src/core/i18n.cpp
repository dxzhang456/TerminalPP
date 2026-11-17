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
    add("&Save",             "保存(&S)",  "儲存(&S)",  "&Guardar",     "&Enregistrer",  "&Speichern",    "&Сохранить",      "保存(&S)",      "저장(&S)");
    add("Save &As...",       "另存为(&A)...","另存為(&A)...","Guardar &como...","Enregistrer &sous...","Speichern &unter...","Сохранить &как...","名前を付けて保存(&A)...","다른 이름으로 저장(&A)...");
    add("Save A&ll",         "全部保存(&L)","全部儲存(&L)","Guardar &todo","&Tout enregistrer","&Alle speichern","Сохранить &всё","すべて保存(&L)","모두 저장(&L)");
    add("&Print...",         "打印(&P)...","列印(&P)...","&Imprimir...","&Imprimer...", "&Drucken...",   "&Печать...",      "印刷(&P)...",   "인쇄(&P)...");
    add("&Close Tab",        "关闭标签(&C)","關閉分頁(&C)","&Cerrar pestaña","&Fermer l'onglet","Tab &schließen","&Закрыть вкладку","タブを閉じる(&C)","탭 닫기(&C)");
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

    // ---- Help ----
    add("&About Terminal++", "关于 Terminal++(&A)","關於 Terminal++(&A)","&Acerca de Terminal++","À &propos de Terminal++","&Über Terminal++","&О программе Terminal++","Terminal++について(&A)","Terminal++ 정보(&A)");

    // ---- Status bar / misc ----
    add("Ready",             "就绪",      "就緒",      "Listo",        "Prêt",          "Bereit",        "Готово",          "準備完了",      "준비됨");
    add("Plain Text",        "纯文本",    "純文字",    "Texto plano",  "Texte brut",    "Nur-Text",      "Простой текст",   "プレーンテキスト","일반 텍스트");
    add("Terminal",          "终端",      "終端",      "Terminal",     "Terminal",      "Terminal",      "Терминал",        "端末",          "터미널");
    add("Line Ending",       "行尾",      "行尾",      "Fin de línea", "Fin de ligne",  "Zeilenende",    "Конец строки",    "改行コード",    "줄 끝");
    add("Windows (CRLF)",    "Windows (CRLF)","Windows (CRLF)","Windows (CRLF)","Windows (CRLF)","Windows (CRLF)","Windows (CRLF)","Windows (CRLF)","Windows (CRLF)");
    add("Unix (LF)",         "Unix (LF)", "Unix (LF)", "Unix (LF)",    "Unix (LF)",     "Unix (LF)",     "Unix (LF)",       "Unix (LF)",     "Unix (LF)");
    add("Mac (CR)",          "Mac (CR)",  "Mac (CR)",  "Mac (CR)",     "Mac (CR)",      "Mac (CR)",      "Mac (CR)",        "Mac (CR)",      "Mac (CR)");
    add("COLUMN",            "列编辑",    "欄編輯",    "COLUMNA",      "COLONNE",       "SPALTE",        "СТОЛБЕЦ",         "列編集",        "열 편집");

    // ---- Dialogs / messages ----
    add("Open Files",        "打开文件",  "開啟檔案",  "Abrir archivos","Ouvrir des fichiers","Dateien öffnen","Открыть файлы","ファイルを開く","파일 열기");
    add("Save As",           "另存为",    "另存為",    "Guardar como", "Enregistrer sous","Speichern unter","Сохранить как","名前を付けて保存","다른 이름으로 저장");
    add("Clipboard History", "剪贴板历史","剪貼簿歷史","Historial del portapapeles","Historique du presse-papiers","Zwischenablage-Verlauf","История буфера обмена","クリップボード履歴","클립보드 기록");
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