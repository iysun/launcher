#pragma once
#include "resultitem.h"
#include <QList>
#include <QString>

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual QString            name()  const = 0;
    virtual QList<ResultItem>  query(const QString &keyword) = 0;
    virtual void               execute(const ResultItem &item) = 0;

    // 触发前缀：空串=全局插件，对任意输入生效；非空时仅当输入以此前缀开头才触发，
    // 且 MainWindow 会在传入 query 前剥离前缀。默认全局。
    virtual QString            triggerPrefix() const { return {}; }

    // 次级动作：Ctrl+Enter 触发，默认无操作。插件可据此提供"复制/打开所在目录"等。
    virtual void               executeAlt(const ResultItem &item) { Q_UNUSED(item) }

    // 是否需要异步执行 query（耗时 IO）。返回 true 时 MainWindow 会把 query 派发到工作线程，
    // 该插件的 query 必须可重入/线程安全，且不得触碰 QPixmap/QWidget 等仅限 GUI 线程的资源。
    virtual bool               runsAsync() const { return false; }

    // 主线程装饰：异步插件在工作线程产出无图标结果，由 MainWindow 在主线程对将展示的结果
    // 调用本钩子补齐图标等不可跨线程的资源。默认无操作。
    virtual void               decorate(ResultItem &item) { Q_UNUSED(item) }
};
