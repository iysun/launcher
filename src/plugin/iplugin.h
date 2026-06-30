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
};
