#include "core/usagestore.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

// frecency 加分上限。须显著小于匹配档之间的间距（子串 300 / 词首 600 / 前缀 800），
// 以保证仅在同档内重排常用项，不会让低档结果越过高档。
static constexpr int kFrecencyCap = 60;

UsageStore::UsageStore() {
    load();
}

QString UsageStore::filePath() const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + "/usage.json";
}

void UsageStore::load() {
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly)) return; // 首次运行无文件，正常
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        Entry             e;
        e.count    = o.value("count").toInt();
        e.lastUsed = o.value("lastUsed").toVariant().toLongLong();
        m_entries.insert(it.key(), e);
    }
}

void UsageStore::save() const {
    const QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonObject root;
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        QJsonObject o;
        o["count"]     = it.value().count;
        o["lastUsed"]  = it.value().lastUsed;
        root[it.key()] = o;
    }
    // 原子写：先写临时文件再 rename，避免进程在写一半时被杀导致 usage.json 截断损坏，
    // 而 load() 对损坏文件会静默回退空表——那样用户的 frecency 记录会无声丢失
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

void UsageStore::recordUse(const QString &key) {
    if (key.isEmpty()) return;
    Entry &e = m_entries[key];
    e.count += 1;
    e.lastUsed = QDateTime::currentSecsSinceEpoch();
    save();
}

int UsageStore::frecencyBonus(const QString &key) const {
    const auto it = m_entries.constFind(key);
    if (it == m_entries.constEnd()) return 0;

    const qint64 age     = QDateTime::currentSecsSinceEpoch() - it->lastUsed;
    int          recency = 0;
    if (age < 86400) recency = 30;        // < 1 天
    else if (age < 604800) recency = 20;  // < 1 周
    else if (age < 2592000) recency = 10; // < 30 天

    const int countW = qMin(it->count * 3, 30);
    return qMin(recency + countW, kFrecencyCap);
}
