#pragma once
#include <QIcon>
#include <QList>
#include <QMetaType>
#include <QString>

class IPlugin; // 仅作非拥有指针使用，避免与 iplugin.h 形成包含环

struct ResultItem {
    QString  title;
    QString  subtitle;
    QString  action; // 执行路径或命令
    QIcon    icon;
    IPlugin *owner = nullptr; // 产出该结果的插件，由 MainWindow 收集时标记
    int      score = 0;       // 匹配分，插件填写，MainWindow 据此跨插件统一排序
    QList<int>
        matchHighlight; // 拼音匹配时预计算的 title 字符下标；空则 delegate 动态计算
};

Q_DECLARE_METATYPE(ResultItem)
