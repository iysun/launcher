#pragma once
#include <QMutex>
#include <QObject>
#include <QStringList>

// 用户偏好设置：热键、开机自启动、插件启用状态、文件搜索目录范围。
// 持久化到 AppDataLocation/settings.json，格式与 usage.json 保持一致。
class AppSettings : public QObject {
    Q_OBJECT
public:
    explicit AppSettings(QObject *parent = nullptr);

    QString     hotkey() const { return m_hotkey; }
    bool        autostart() const { return m_autostart; }
    QStringList disabledPlugins() const { return m_disabledPlugins; }
    QStringList webEngineOrder() const { return m_webEngineOrder; }
    // 加锁访问：FilePlugin::query() 在工作线程读取，与设置页保存（主线程）可能并发
    QStringList fileSearchPaths() const;

    void setHotkey(const QString &seq);
    void setAutostart(bool on);
    void setDisabledPlugins(const QStringList &names);
    void setWebEngineOrder(const QStringList &order);
    void setFileSearchPaths(const QStringList &paths);

    void save() const;

signals:
    void hotkeyChanged(const QString &seq);

private:
    void                      load();
    void                      applyAutostart(bool on) const;
    static QStringList        defaultFileSearchPaths();

    QString     m_hotkey    = "Alt+Space";
    bool        m_autostart = false;
    QStringList m_disabledPlugins;
    QStringList m_webEngineOrder = {"google", "bing", "baidu", "github"};

    QStringList          m_fileSearchPaths; // 默认值在构造函数里用 QStandardPaths 计算
    mutable QMutex       m_fileSearchPathsMutex;
};
