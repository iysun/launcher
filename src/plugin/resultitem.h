#pragma once
#include <QIcon>
#include <QMetaType>
#include <QString>

class IPlugin;  // 仅作非拥有指针使用，避免与 iplugin.h 形成包含环

struct ResultItem {
    QString  title;
    QString  subtitle;
    QString  action;          // 执行路径或命令
    QIcon    icon;
    IPlugin *owner = nullptr;  // 产出该结果的插件，由 MainWindow 收集时标记
    int      score = 0;        // 匹配分，插件填写，MainWindow 据此跨插件统一排序
};

Q_DECLARE_METATYPE(ResultItem)
