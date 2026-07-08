#pragma once
#include "core/webengine.h"
#include <QMutex>
#include <QObject>
#include <QStringList>

// 用户偏好设置：热键、开机自启动、插件启用状态、文件搜索目录范围、自定义网页搜索引擎。
// 持久化到 AppDataLocation/settings.json，格式与 usage.json 保持一致。
class AppSettings : public QObject {
    Q_OBJECT
public:
    explicit AppSettings(QObject *parent = nullptr);

    QString           hotkey() const { return m_hotkey; }
    QString           language() const { return m_language; }
    QString           appearanceMode() const { return m_appearanceMode; }
    QString           darkTheme() const { return m_darkTheme; }
    QString           lightTheme() const { return m_lightTheme; }
    bool              autostart() const { return m_autostart; }
    QStringList       disabledPlugins() const { return m_disabledPlugins; }
    QStringList       webEngineOrder() const { return m_webEngineOrder; }
    QList<WebEngine>  customWebEngines() const { return m_customWebEngines; }
    // 加锁访问：FilePlugin::query() 在工作线程读取，与设置页保存（主线程）可能并发
    QStringList fileSearchPaths() const;

    void setHotkey(const QString &seq);
    void setLanguage(const QString &code);
    void setAppearanceMode(const QString &mode);
    void setDarkTheme(const QString &code);
    void setLightTheme(const QString &code);
    void setAutostart(bool on);
    void setDisabledPlugins(const QStringList &names);
    void setWebEngineOrder(const QStringList &order);
    void setCustomWebEngines(const QList<WebEngine> &engines);
    void setFileSearchPaths(const QStringList &paths);

    void save() const;

    // 出厂默认值：供设置页"重置"使用
    static QString      defaultHotkey() { return "Alt+Space"; }
    static QString      defaultLanguage() { return "zh"; }
    static QString      defaultAppearanceMode() { return "dark"; }
    static QString      defaultDarkTheme() { return "mocha"; }
    static QString      defaultLightTheme() { return "latte"; }
    static bool         defaultAutostart() { return false; }
    static QStringList  defaultWebEngineOrder() { return {"google", "bing", "baidu", "github"}; }
    static QStringList  defaultFileSearchPaths();

signals:
    void hotkeyChanged(const QString &seq);

private:
    void load();
    void applyAutostart(bool on) const;

    QString          m_hotkey         = "Alt+Space";
    QString          m_language       = "zh";
    QString          m_appearanceMode = "dark";
    QString          m_darkTheme      = "mocha";
    QString          m_lightTheme     = "latte";
    bool             m_autostart      = false;
    QStringList      m_disabledPlugins;
    QStringList      m_webEngineOrder = {"google", "bing", "baidu", "github"};
    QList<WebEngine> m_customWebEngines;

    QStringList          m_fileSearchPaths; // 默认值在构造函数里用 QStandardPaths 计算
    mutable QMutex       m_fileSearchPathsMutex;
};
