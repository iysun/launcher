#pragma once
#include <QList>
#include <QString>

// 关键字匹配打分：完全相等 > 前缀 > 词首边界 > 普通子串 > 子序列模糊；同档优先短文本。
// 无匹配返回 -1。供各插件统一调用，保证跨插件排序口径一致。
namespace Matcher {
int score(const QString &text, const QString &keyword);

// 返回 text 中与 keyword 命中的字符下标（升序，用于高亮）：
// 连续子串命中返回该区间下标，否则返回子序列命中位置；无命中返回空。
QList<int> matchedPositions(const QString &text, const QString &keyword);
}
