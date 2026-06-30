#pragma once
#include "plugin/iplugin.h"

// 文件搜索，"@" 前缀触发。内置递归扫描固定的用户目录（桌面/文档/下载…），
// 对文件名做模糊匹配。同步遍历，靠访问数硬上限兜底，避免大目录卡 UI 线程。
class FilePlugin : public IPlugin {
public:
    QString           name()  const override { return "Files"; }
    QString           triggerPrefix() const override { return "@"; }
    QList<ResultItem> query(const QString &keyword) override;
    void              execute(const ResultItem &item) override;     // 默认程序打开文件
    void              executeAlt(const ResultItem &item) override;  // Ctrl+Enter：打开所在目录

private:
    static QStringList searchRoots();  // 跨平台用户目录集（QStandardPaths）
};
