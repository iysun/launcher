#include "core/pinyin.h"
#include "core/matcher.h"
#include <QSet>
#include <cstring>

namespace Pinyin {

// Declared in pinyin_data.cpp (auto-generated).
extern const char kPinyinTable[20992][7];

static constexpr int kTableFirst = 0x4E00;
static constexpr int kTableSize  = 20992;

Result compute(const QString &text) {
    Result r;
    r.offsets.reserve(text.length());

    int fullOff = 0;
    for (const QChar ch : text) {
        const int idx = static_cast<int>(ch.unicode()) - kTableFirst;
        if (idx >= 0 && idx < kTableSize && kPinyinTable[idx][0] != '\0') {
            // CJK character with a known pinyin syllable
            const char *syl = kPinyinTable[idx];
            r.offsets.append(fullOff);
            r.full += QLatin1String(syl);
            r.initials += QLatin1Char(syl[0]);
            fullOff += static_cast<int>(strlen(syl));
        } else if (ch.isLetter() && ch.unicode() < 0x80) {
            // ASCII letter: pass through verbatim
            r.offsets.append(fullOff);
            r.full += ch;
            r.initials += ch;
            fullOff += 1;
        } else if (ch.isDigit()) {
            // Digit: pass through verbatim
            r.offsets.append(fullOff);
            r.full += ch;
            r.initials += ch;
            fullOff += 1;
        } else {
            // Punctuation, space, emoji, non-CJK non-ASCII: skip
            r.offsets.append(-1);
        }
    }
    return r;
}

QList<int> pinyinMatchedTitlePositions(const Result &py, const QString &kw,
                                       bool useInitials) {
    const QString   &searchStr = useInitials ? py.initials : py.full;
    const QList<int> hits      = Matcher::matchedPositions(searchStr, kw);
    if (hits.isEmpty()) return {};

    // Build a list of (contributing_index_in_search_string, title_char_index)
    // where "contributing index" means the position in full/initials contributed by that
    // char. For initials: initials[j] = j-th contributing char → title index For full:
    // offsets[i] != -1 means char i contributes starting at offsets[i]
    QList<int>
        contributing; // title char indices that contributed to full/initials, in order
    for (int i = 0; i < py.offsets.size(); ++i) {
        if (py.offsets[i] != -1) contributing.append(i);
    }

    QSet<int> resultSet;

    if (useInitials) {
        // initials[j] corresponds to contributing[j]
        for (int hitPos : hits) {
            if (hitPos < contributing.size()) resultSet.insert(contributing[hitPos]);
        }
    } else {
        // hits are positions in py.full; map each back to the contributing char
        // whose syllable contains that position.
        // contributing[j] has its syllable starting at py.offsets[contributing[j]]
        // and ending (exclusive) at py.offsets[contributing[j+1]] or py.full.size().
        for (int hitPos : hits) {
            // Binary search: find j where offsets[contributing[j]] <= hitPos
            // and offsets[contributing[j+1]] > hitPos
            int lo = 0, hi = contributing.size() - 1;
            while (lo < hi) {
                const int mid = (lo + hi + 1) / 2;
                if (py.offsets[contributing[mid]] <= hitPos) lo = mid;
                else hi = mid - 1;
            }
            if (lo < contributing.size() && py.offsets[contributing[lo]] <= hitPos)
                resultSet.insert(contributing[lo]);
        }
    }

    QList<int> result(resultSet.begin(), resultSet.end());
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace Pinyin
