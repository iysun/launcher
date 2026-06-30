#pragma once
#include <QString>

// 关键字匹配打分：完全相等 > 前缀 > 词首边界 > 普通子串；同档优先短文本。
// 无匹配返回 -1。供各插件统一调用，保证跨插件排序口径一致。
namespace Matcher {
int score(const QString &text, const QString &keyword);
}
