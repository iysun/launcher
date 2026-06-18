#pragma once
#include <QIcon>
#include <QMetaType>
#include <QString>

struct ResultItem {
    QString title;
    QString subtitle;
    QString action;  // 执行路径或命令
    QIcon   icon;
};

Q_DECLARE_METATYPE(ResultItem)
