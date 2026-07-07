#pragma once
#include <QHash>
#include <QList>
#include <QPair>
#include <QString>

// 极简 i18n：语言包是 datadir/i18n/<code>.json（meta.code/meta.name + strings 表）。
// 内置 zh/en 通过 Qt 资源 (:/i18n/zh.json, :/i18n/en.json) 首次运行时落盘到 datadir，
// 用户可在该目录直接编辑或复制出自定义语言文件。语言切换只在启动时 init() 一次生效，
// 不做运行时热切换，故本类无需 QObject/信号。
class I18n {
public:
    static I18n &instance();

    // 启动时调用一次：确保内置语言文件已落盘到 datadir，并加载 languageCode 对应的语言表
    void init(const QString &languageCode);

    QString languageCode() const { return m_code; }

    // 查表：当前语言 → 内置英文兜底 → key 本身（缺翻译时显示 key，便于发现遗漏而不是崩溃/空白）
    static QString t(const QString &key);

    // 扫描 datadir/i18n/*.json，返回 (code, displayName) 列表，供设置页语言下拉框使用
    QList<QPair<QString, QString>> availableLanguages() const;

private:
    QString text(const QString &key) const;
    void    ensureDefaultFiles() const;
    static QHash<QString, QString> loadStrings(const QString &jsonPath);
    static QString                 i18nDir();

    QHash<QString, QString> m_strings;         // 当前语言
    QHash<QString, QString> m_englishFallback; // 内置英文兜底，永远从 :/i18n/en.json 加载
    QString                 m_code = "zh";
};
