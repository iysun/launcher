#pragma once
#include <QHash>
#include <QString>

// 记录每个结果（按 action 键）的使用次数与最近使用时间，提供 frecency 加分，
// 让常用项在同档匹配内上浮。持久化到用户数据目录下的 usage.json。
class UsageStore {
public:
    UsageStore();
    void recordUse(const QString &key);           // 执行后调用：计数 +1、刷新时间并落盘
    int  frecencyBonus(const QString &key) const; // 0..kFrecencyCap，有界以免跨匹配档

private:
    struct Entry {
        int    count    = 0;
        qint64 lastUsed = 0; // Unix 秒
    };
    void    load();
    void    save() const;
    QString filePath() const;

    QHash<QString, Entry> m_entries;
};
