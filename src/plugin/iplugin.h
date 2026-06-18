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
};
