#ifndef IMAGECOMPARETAB_H
#define IMAGECOMPARETAB_H

#include <QWidget>
#include <QImage>
#include "editor/thememanager.h"

class QLabel;
class QSlider;
class QScrollArea;
class ImageView;   // internal (defined in .cpp)

// side-by-side picture comparison tab.
//  * side-by-side image view with synchronized pan/zoom
//  * zoom in/out/fit, 1:1
//  * a "difference" overlay mode that highlights per-pixel differences
//  * stats: dimensions, differing pixel count/percentage
class ImageCompareTab : public QWidget
{
    Q_OBJECT
public:
    explicit ImageCompareTab(QWidget *parent = nullptr);

    bool loadFiles(const QString &leftPath, const QString &rightPath);
    void applyTheme(const EditorTheme &theme);

    QString leftPath() const { return m_leftPath; }
    QString rightPath() const { return m_rightPath; }

signals:
    void statusMessage(const QString &msg);
    // Request the host to reopen the current two files as another comparison
    // type (0=Text, 1=Folder, 2=Hex, 3=Image).
    void requestCompareAs(int type, const QString &left, const QString &right);

public slots:
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomReset();
    void toggleDiffOverlay(bool on);
    void reload();

private:
    void rebuildDiff();
    void updateStats();

    ImageView *m_left = nullptr;
    ImageView *m_right = nullptr;
    QLabel *m_leftLabel = nullptr;
    QLabel *m_rightLabel = nullptr;
    QLabel *m_statsLabel = nullptr;

    QImage m_leftImg;
    QImage m_rightImg;
    QImage m_diffImg;
    QString m_leftPath;
    QString m_rightPath;

    double m_zoom = 1.0;
    bool m_diffMode = false;
};

#endif // IMAGECOMPARETAB_H