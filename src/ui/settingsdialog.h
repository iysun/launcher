#pragma once
#include "plugin/iplugin.h"
#include <QWidget>

class AppSettings;
class HotkeyEdit;
class QCheckBox;
class QLabel;
class QPoint;

class SettingsDialog : public QWidget {
    Q_OBJECT
public:
    explicit SettingsDialog(AppSettings *settings, const QList<IPlugin *> &plugins);

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    void setupUi(const QList<IPlugin *> &plugins);
    void save();
    QLabel *makeSectionLabel(const QString &text);

    AppSettings  *m_settings;
    HotkeyEdit   *m_hotkeyEdit;
    QCheckBox          *m_autostartCheck;
    QList<QCheckBox *>  m_pluginChecks;
    QList<QString>      m_pluginNames;

    QPoint m_dragPos;
    static constexpr int kTitleH = 44;
    static constexpr int kWidth  = 480;
};
