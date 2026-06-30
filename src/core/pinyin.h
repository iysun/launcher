#pragma once
#include <QList>
#include <QString>

namespace Pinyin {

// Pre-computed romanization of a title string.
// offsets.size() == text.length() always; offsets[i] == -1 when character i
// contributes nothing (punctuation, symbol, or unmapped CJK glyph).
struct Result {
    QString    full;      // concatenated syllables, lowercase (e.g. "weixin")
    QString    initials;  // one char per contributing title char (e.g. "wx")
    QList<int> offsets;   // offsets[i] = start of char i's contribution in full; -1 if skipped
};

// Compute pinyin for text. O(n), reads a static read-only table — thread-safe.
// CJK U+4E00-U+9FFF: mapped to syllable (most common pronunciation, no tones).
// ASCII letter/digit: passed through verbatim (case preserved).
// Other chars (punctuation, spaces, non-CJK non-ASCII): skipped (offset = -1).
Result compute(const QString &text);

// Map a keyword match in py.full (useInitials=false) or py.initials (true)
// back to the original title character indices for highlight rendering.
// Returns sorted, deduplicated title-char indices; empty if keyword not found.
QList<int> pinyinMatchedTitlePositions(const Result &py, const QString &kw,
                                       bool useInitials);

} // namespace Pinyin
