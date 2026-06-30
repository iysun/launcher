#include "core/matcher.h"

// 由 AppPlugin::matchScore 提炼而来，逻辑保持一致：
// 完全相等 1000 > 前缀 800 > 词首边界 600 > 普通子串 300，同档减去文本长度以偏好短文本。
int Matcher::score(const QString &text, const QString &keyword) {
    const int idx = text.indexOf(keyword, 0, Qt::CaseInsensitive);
    if (idx < 0) return -1;
    if (text.compare(keyword, Qt::CaseInsensitive) == 0) return 1000;
    if (idx == 0) return 800 - text.length();
    const QChar prev = text.at(idx - 1);
    const QChar cur  = text.at(idx);
    const bool boundary = prev == ' ' || prev == '-' || prev == '_' ||
                          (prev.isLower() && cur.isUpper());  // 空格/连字符/驼峰边界
    return (boundary ? 600 : 300) - text.length();
}
