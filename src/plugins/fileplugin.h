#pragma once
#include "plugin/iplugin.h"

class AppSettings;

// 文件搜索，"@" 前缀触发。搜索根来自 AppSettings::fileSearchPaths()（设置页可编辑，
// 默认桌面/文档/下载/图片），对文件名做模糊匹配。异步遍历，靠访问数硬上限兜底，
// 避免大目录卡 UI 线程。
class FilePlugin : public IPlugin {
public:
    explicit FilePlugin(AppSettings *settings = nullptr);

    QString name() const override { return "Files"; }
    QString triggerPrefix() const override { return "@"; }
    bool    runsAsync() const override { return true; } // 遍历挪到工作线程，不阻塞 UI
    QList<ResultItem> query(const QString &keyword) override;   // 工作线程执行，不取图标
    void              decorate(ResultItem &item) override;      // 主线程按路径补图标
    void              execute(const ResultItem &item) override; // 默认程序打开文件
    void executeAlt(const ResultItem &item) override; // Ctrl+Enter：打开所在目录

private:
    // 无设置对象时（如未来单测直接构造）的兜底默认目录集，跨平台（QStandardPaths）
    static QStringList searchRoots();

    AppSettings *m_settings;
};
