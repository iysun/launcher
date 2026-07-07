#include "i18n.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

I18n &I18n::instance() {
    static I18n inst;
    return inst;
}

QString I18n::i18nDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/i18n";
}

QHash<QString, QString> I18n::loadStrings(const QString &jsonPath) {
    QHash<QString, QString> result;
    QFile                   f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) return result;

    const QJsonObject root    = QJsonDocument::fromJson(f.readAll()).object();
    const QJsonObject strings = root["strings"].toObject();
    for (auto it = strings.constBegin(); it != strings.constEnd(); ++it)
        result.insert(it.key(), it.value().toString());
    return result;
}

// 把内置 :/i18n/zh.json、:/i18n/en.json 落盘到 datadir/i18n/，仅当目标文件不存在时才写入，
// 不用 QFile::copy 是为了避免继承 Qt 资源文件的只读属性——用户需要能直接编辑/复制这些文件
void I18n::ensureDefaultFiles() const {
    const QString dir = i18nDir();
    QDir().mkpath(dir);

    for (const char *code : {"zh", "en"}) {
        const QString target = dir + "/" + code + ".json";
        if (QFile::exists(target)) continue;

        QFile src(QString(":/i18n/%1.json").arg(code));
        if (!src.open(QIODevice::ReadOnly)) continue;
        const QByteArray content = src.readAll();

        QFile out(target);
        if (out.open(QIODevice::WriteOnly)) out.write(content);
    }
}

void I18n::init(const QString &languageCode) {
    ensureDefaultFiles();

    m_code             = languageCode.isEmpty() ? "zh" : languageCode;
    m_englishFallback  = loadStrings(":/i18n/en.json");

    m_strings = loadStrings(i18nDir() + "/" + m_code + ".json");
    if (m_strings.isEmpty()) // 内置语言的 datadir 副本理论上落盘后必然存在；此处兜底覆盖异常场景
        m_strings = loadStrings(QString(":/i18n/%1.json").arg(m_code));
}

QString I18n::text(const QString &key) const {
    if (const auto it = m_strings.constFind(key); it != m_strings.constEnd()) return it.value();
    if (const auto it = m_englishFallback.constFind(key); it != m_englishFallback.constEnd())
        return it.value();
    return key;
}

QString I18n::t(const QString &key) {
    return instance().text(key);
}

QList<QPair<QString, QString>> I18n::availableLanguages() const {
    QList<QPair<QString, QString>> result;
    QDir                            dir(i18nDir());
    for (const QString &fileName : dir.entryList({"*.json"}, QDir::Files)) {
        QFile f(dir.filePath(fileName));
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QJsonObject meta = QJsonDocument::fromJson(f.readAll()).object()["meta"].toObject();
        const QString     code = meta["code"].toString();
        const QString     name = meta["name"].toString();
        if (code.isEmpty() || name.isEmpty()) continue; // 缺元信息的文件跳过，避免下拉框出现空项
        result.append({code, name});
    }
    return result;
}
