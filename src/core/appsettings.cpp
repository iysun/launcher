#include "appsettings.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QSettings>
#include <QStandardPaths>

static const char *kRunKey =
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char *kAppName = "launcher";

// 文件搜索默认目录：桌面/文档/下载/图片，去重（部分平台上某些 Location 可能落到同一目录）
QStringList AppSettings::defaultFileSearchPaths() {
    QStringList roots;
    const auto  types = {
        QStandardPaths::DesktopLocation,
        QStandardPaths::DocumentsLocation,
        QStandardPaths::DownloadLocation,
        QStandardPaths::PicturesLocation,
    };
    for (auto t : types) {
        const QString dir = QStandardPaths::writableLocation(t);
        if (!dir.isEmpty() && !roots.contains(dir)) roots.append(dir);
    }
    return roots;
}

AppSettings::AppSettings(QObject *parent) : QObject(parent) {
    m_fileSearchPaths = defaultFileSearchPaths(); // load() 若有存档值会覆盖
    load();
}

QStringList AppSettings::fileSearchPaths() const {
    QMutexLocker lock(&m_fileSearchPathsMutex);
    return m_fileSearchPaths;
}

void AppSettings::setHotkey(const QString &seq) {
    if (m_hotkey == seq) return;
    m_hotkey = seq;
    emit hotkeyChanged(seq);
}

void AppSettings::setAutostart(bool on) {
    m_autostart = on;
    applyAutostart(on);
}

void AppSettings::setDisabledPlugins(const QStringList &names) {
    m_disabledPlugins = names;
}

void AppSettings::setWebEngineOrder(const QStringList &order) {
    m_webEngineOrder = order;
}

void AppSettings::setCustomWebEngines(const QList<WebEngine> &engines) {
    m_customWebEngines = engines;
}

void AppSettings::setFileSearchPaths(const QStringList &paths) {
    QMutexLocker lock(&m_fileSearchPathsMutex);
    m_fileSearchPaths = paths;
}

void AppSettings::save() const {
    QJsonObject obj;
    obj["hotkey"]    = m_hotkey;
    obj["autostart"] = m_autostart;

    QJsonArray arr;
    for (const QString &n : m_disabledPlugins)
        arr.append(n);
    obj["disabledPlugins"] = arr;

    QJsonArray orderArr;
    for (const QString &id : m_webEngineOrder)
        orderArr.append(id);
    obj["webEngineOrder"] = orderArr;

    QJsonArray customArr;
    for (const WebEngine &e : m_customWebEngines) {
        QJsonObject o;
        o["id"]          = e.id;
        o["name"]        = e.name;
        o["urlTemplate"] = e.urlTemplate;
        customArr.append(o);
    }
    obj["customWebEngines"] = customArr;

    QJsonArray pathsArr;
    for (const QString &p : fileSearchPaths()) // 走加锁 getter，保持访问该字段一律过锁
        pathsArr.append(p);
    obj["fileSearchPaths"] = pathsArr;

    const QString path =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
        "/settings.json";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(obj).toJson());
}

void AppSettings::load() {
    const QString path =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
        "/settings.json";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;

    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    if (obj.contains("hotkey")) m_hotkey = obj["hotkey"].toString(m_hotkey);
    if (obj.contains("autostart")) m_autostart = obj["autostart"].toBool(m_autostart);
    if (obj.contains("disabledPlugins")) {
        m_disabledPlugins.clear();
        for (const QJsonValue &v : obj["disabledPlugins"].toArray())
            m_disabledPlugins.append(v.toString());
    }
    if (obj.contains("webEngineOrder")) {
        m_webEngineOrder.clear();
        for (const QJsonValue &v : obj["webEngineOrder"].toArray())
            m_webEngineOrder.append(v.toString());
    }
    if (obj.contains("customWebEngines")) {
        m_customWebEngines.clear();
        for (const QJsonValue &v : obj["customWebEngines"].toArray()) {
            const QJsonObject o = v.toObject();
            m_customWebEngines.append(
                {o["id"].toString(), o["name"].toString(), o["urlTemplate"].toString()});
        }
    }
    if (obj.contains("fileSearchPaths")) {
        QStringList paths;
        for (const QJsonValue &v : obj["fileSearchPaths"].toArray())
            paths.append(v.toString());
        QMutexLocker lock(&m_fileSearchPathsMutex);
        m_fileSearchPaths = paths;
    }
}

void AppSettings::applyAutostart(bool on) const {
#ifdef Q_OS_WIN
    QSettings reg(kRunKey, QSettings::NativeFormat);
    if (on)
        reg.setValue(kAppName,
                     QCoreApplication::applicationFilePath().replace("/", "\\"));
    else reg.remove(kAppName);
#else
    Q_UNUSED(on)
#endif
}
