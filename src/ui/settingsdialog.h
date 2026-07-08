#pragma once
#include "core/webengine.h"
#include "plugin/iplugin.h"
#include <optional>
#include <QWidget>

class AppSettings;
class HotkeyEdit;
class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
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
    void showEvent(QShowEvent *e) override;

private:
    void    setupUi(const QList<IPlugin *> &plugins);
    void    syncFormFromSettings(); // 从 m_settings 灌回所有表单控件（首次显示 + 每次重新显示）
    void    resetToDefaults();      // 表单恢复出厂默认，不碰 m_settings/磁盘
    void    save();
    QLabel *makeSectionLabel(const QString &text);
    void    populateEngineList(const QStringList &order, const QList<WebEngine> &available);
    void    populatePathList(const QStringList &paths);
    std::optional<WebEngine> promptForWebEngine();

    AppSettings       *m_settings;
    HotkeyEdit        *m_hotkeyEdit;
    QComboBox         *m_languageCombo   = nullptr;
    QComboBox         *m_appearanceCombo = nullptr;
    QComboBox         *m_darkThemeCombo  = nullptr;
    QComboBox         *m_lightThemeCombo = nullptr;
    QCheckBox         *m_autostartCheck;
    QList<QCheckBox *> m_pluginChecks;
    QList<QString>     m_pluginNames;
    QListWidget       *m_engineList = nullptr;
    QListWidget       *m_pathList   = nullptr;

    QPoint               m_dragPos;
    static constexpr int kTitleH = 44;
    static constexpr int kWidth  = 480;
};
