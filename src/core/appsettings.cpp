#include "appsettings.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

static const char *kRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char *kAppName = "launcher";

AppSettings::AppSettings(QObject *parent) : QObject(parent) {
    load();
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

void AppSettings::save() const {
    QJsonObject obj;
    obj["hotkey"]    = m_hotkey;
    obj["autostart"] = m_autostart;

    QJsonArray arr;
    for (const QString &n : m_disabledPlugins)
        arr.append(n);
    obj["disabledPlugins"] = arr;

    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                         + "/settings.json";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(obj).toJson());
}

void AppSettings::load() {
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                         + "/settings.json";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;

    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    if (obj.contains("hotkey"))    m_hotkey    = obj["hotkey"].toString(m_hotkey);
    if (obj.contains("autostart")) m_autostart = obj["autostart"].toBool(m_autostart);
    if (obj.contains("disabledPlugins")) {
        m_disabledPlugins.clear();
        for (const QJsonValue &v : obj["disabledPlugins"].toArray())
            m_disabledPlugins.append(v.toString());
    }
}

void AppSettings::applyAutostart(bool on) const {
#ifdef Q_OS_WIN
    QSettings reg(kRunKey, QSettings::NativeFormat);
    if (on)
        reg.setValue(kAppName, QCoreApplication::applicationFilePath().replace("/", "\\"));
    else
        reg.remove(kAppName);
#else
    Q_UNUSED(on)
#endif
}
