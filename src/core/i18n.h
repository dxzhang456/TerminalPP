#ifndef I18N_H
#define I18N_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

// A lightweight, self-contained UI translation manager.
//
// Instead of relying on the external lrelease/.qm toolchain, Terminal++ keeps
// its UI strings in an embedded JSON table (:/i18n/strings.json) keyed by a
// language code ("en", "zh_CN", "zh_TW", "es", "fr", "de", "ru", "ja", "ko").
// Widgets call I18n::tr("Some English source") and get back the translated
// string for the current language (falling back to the English source if no
// translation exists).
//
// The chosen language is persisted via QSettings, so the first launch defaults
// to English and subsequent launches restore the user's preference.
class I18n : public QObject
{
    Q_OBJECT
public:
    static I18n *instance();

    // Load the embedded translation table and the saved language preference.
    void initialize();

    // All supported language codes, in display order.
    QStringList languageCodes() const;
    // Human-readable, self-localized name for a code (e.g. "zh_CN" -> "简体中文").
    QString languageDisplayName(const QString &code) const;

    QString currentLanguage() const { return m_current; }
    void setLanguage(const QString &code);   // persists + emits languageChanged

    // Translate an English source string into the current language.
    QString tr(const QString &source) const;

signals:
    void languageChanged(const QString &code);

private:
    explicit I18n(QObject *parent = nullptr);
    void loadTable();

    QString m_current = QStringLiteral("en");
    // code -> (English source -> translated)
    QHash<QString, QHash<QString, QString>> m_table;
    QStringList m_codes;
    QHash<QString, QString> m_displayNames;
};

// Convenience macro mirroring Qt's tr() but routed through our runtime table.
#define TPTR(s) (I18n::instance()->tr(QStringLiteral(s)))

#endif // I18N_H