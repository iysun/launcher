#include "core/matcher.h"

namespace {

// 词首/驼峰/连续 命中的加分；返回 -1 表示 keyword 不是 text 的子序列。
int subsequenceBonus(const QString &text, const QString &kw) {
    int ti = 0, ki = 0, bonus = 0, prev = -2;
    while (ti < text.length() && ki < kw.length()) {
        if (text.at(ti).toLower() == kw.at(ki).toLower()) {
            if (ti == prev + 1) bonus += 5; // 连续命中
            if (ti == 0) {
                bonus += 10; // 句首
            } else {
                const QChar pc = text.at(ti - 1);
                if (pc == ' ' || pc == '-' || pc == '_') bonus += 10;       // 词首
                else if (pc.isLower() && text.at(ti).isUpper()) bonus += 8; // 驼峰边界
            }
            prev = ti;
            ++ki;
        }
        ++ti;
    }
    return ki == kw.length() ? bonus : -1;
}

} // namespace

// 完全相等 1000 > 前缀 800 > 词首边界 600 > 普通子串 300，同档减文本长度以偏好短文本；
// 以上均为连续子串命中。都不命中时退化为子序列模糊匹配（固定低档 + 质量加分，上限 220）。
// 普通子串档下限为 300-len，故标题不太长（<80 字符，应用名几乎都满足）时子串恒优先于
// 子序列；超长标题的子串命中理论上可能被短标题的强子序列命中反超，属可接受的边缘取舍。
// 无匹配返回 -1。
int Matcher::score(const QString &text, const QString &keyword) {
    const int idx = text.indexOf(keyword, 0, Qt::CaseInsensitive);
    if (idx >= 0) {
        if (text.compare(keyword, Qt::CaseInsensitive) == 0) return 1000;
        if (idx == 0) return 800 - text.length();
        const QChar prev     = text.at(idx - 1);
        const QChar cur      = text.at(idx);
        const bool  boundary = prev == ' ' || prev == '-' || prev == '_' ||
                               (prev.isLower() && cur.isUpper()); // 空格/连字符/驼峰边界
        return (boundary ? 600 : 300) - text.length();
    }

    const int bonus = subsequenceBonus(text, keyword);
    if (bonus < 0) return -1;
    return 100 + qMin(bonus, 120); // 上限 220，绝大多数标题长度下低于普通子串档
}

QList<int> Matcher::matchedPositions(const QString &text, const QString &keyword) {
    if (keyword.isEmpty()) return {};

    QList<int> pos;
    const int  idx = text.indexOf(keyword, 0, Qt::CaseInsensitive);
    if (idx >= 0) {
        for (int i = 0; i < keyword.length(); ++i)
            pos.append(idx + i);
        return pos;
    }

    int ti = 0, ki = 0;
    while (ti < text.length() && ki < keyword.length()) {
        if (text.at(ti).toLower() == keyword.at(ki).toLower()) {
            pos.append(ti);
            ++ki;
        }
        ++ti;
    }
    if (ki < keyword.length()) return {}; // 未全部命中
    return pos;
}
