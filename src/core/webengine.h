#pragma once
#include <QString>

// 网页搜索引擎描述（内置与用户自定义共用）。
struct WebEngine {
    QString id;
    QString name;
    QString urlTemplate; // %1 为 URL 编码后的关键词
};
