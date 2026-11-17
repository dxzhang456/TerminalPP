#ifndef SERIALTAB_H
#define SERIALTAB_H

#include <QWidget>
#include "serialsession.h"
#include "editor/thememanager.h"

class TerminalWidget;

// A single serial/UART terminal tab (PuTTY-like).
class SerialTab : public QWidget
{
    Q_OBJECT
public:
    explicit SerialTab(const SerialConfig &cfg, QWidget *parent = nullptr);

    bool openPort();
    void closePort();
    bool isOpen() const;

    const SerialConfig &config() const { return m_config; }
    TerminalWidget *terminal() const { return m_terminal; }

    // Apply the shared editor theme's background/foreground so serial tabs
    // match the application's overall look.
    void applyTheme(const EditorTheme &theme);

signals:
    void connectionClosed();

private:
    SerialConfig m_config;
    TerminalWidget *m_terminal = nullptr;
    SerialSession *m_session = nullptr;
};

#endif // SERIALTAB_H