#include "core/matcher.h"
#include "core/pinyin.h"
#include <QtTest>

// Matcher/Pinyin 均为纯函数，无 GUI 依赖，故用 QTEST_GUILESS_MAIN（QCoreApplication）。
class TestMatcher : public QObject {
    Q_OBJECT
private slots:
    // ── score 分档优先级 ──
    void exactIsHighest();
    void tierOrdering(); // 前缀 > 词首边界 > 普通子串 > 子序列
    void shorterPreferredInTier();
    void noMatchIsNegative();
    void caseInsensitive();

    // ── 子序列模糊 ──
    void subsequenceMatches();
    void subsequenceRespectsOrder();

    // ── matchedPositions ──
    void positionsSubstringContiguous();
    void positionsSubsequenceScattered();
    void positionsNoMatchEmpty();
    void positionsEmptyKeyword();

    // ── Pinyin::compute ──
    void pinyinComputeCjk();
    void pinyinComputeAsciiPassthrough();
    void pinyinComputePunctSkipped();
    void pinyinComputeOffsets();

    // ── Pinyin 评分 ──
    void pinyinFullScores();
    void pinyinInitialsScores();
    void pinyinDirectBeatsFullPinyin();

    // ── Pinyin 高亮映射 ──
    void pinyinHighlightFull();
    void pinyinHighlightInitials();
};

// ── Matcher 测试实现 ─────────────────────────────────────────

void TestMatcher::exactIsHighest() {
    QCOMPARE(Matcher::score("Code", "Code"), 1000);
    QCOMPARE(Matcher::score("Code", "code"), 1000); // 大小写不敏感
    QVERIFY(Matcher::score("Code", "Code") > Matcher::score("Firefox", "fire"));
}

void TestMatcher::tierOrdering() {
    const int prefix      = Matcher::score("Firefox", "fire");              // 前缀
    const int boundary    = Matcher::score("Visual Studio Code", "studio"); // 词首边界
    const int substring   = Matcher::score("Foobar", "oob");                // 普通子串
    const int subsequence = Matcher::score("Firefox", "ff");                // 子序列模糊

    QVERIFY(prefix > 0 && boundary > 0 && substring > 0 && subsequence > 0);
    QVERIFY(prefix > boundary);
    QVERIFY(boundary > substring);
    QVERIFY(substring > subsequence); // 子串恒优先于子序列（常规标题长度下）
}

void TestMatcher::shorterPreferredInTier() {
    // 同为前缀命中，短标题分更高
    QVERIFY(Matcher::score("Code", "co") > Matcher::score("Codex", "co"));
}

void TestMatcher::noMatchIsNegative() {
    QCOMPARE(Matcher::score("Code", "xyz"), -1);
    QCOMPARE(Matcher::score("", "a"), -1);
}

void TestMatcher::caseInsensitive() {
    QVERIFY(Matcher::score("FireFox", "FIREFOX") == 1000);
    QVERIFY(Matcher::score("FireFox", "ff") > 0); // 子序列亦大小写不敏感
}

void TestMatcher::subsequenceMatches() {
    QVERIFY(Matcher::score("Firefox", "ff") > 0); // F..f
    QVERIFY(Matcher::score("Visual Studio Code", "vsc") > 0);
}

void TestMatcher::subsequenceRespectsOrder() {
    // "xf" 在 Firefox 中：x 在末尾，其后无 f，不构成子序列
    QCOMPARE(Matcher::score("Firefox", "xf"), -1);
}

void TestMatcher::positionsSubstringContiguous() {
    QCOMPARE(Matcher::matchedPositions("Firefox", "fire"), (QList<int>{0, 1, 2, 3}));
}

void TestMatcher::positionsSubsequenceScattered() {
    QCOMPARE(Matcher::matchedPositions("Firefox", "ff"), (QList<int>{0, 4}));
}

void TestMatcher::positionsNoMatchEmpty() {
    QVERIFY(Matcher::matchedPositions("Code", "zzz").isEmpty());
}

void TestMatcher::positionsEmptyKeyword() {
    QVERIFY(Matcher::matchedPositions("Code", "").isEmpty());
}

// ── Pinyin 测试实现 ──────────────────────────────────────────

void TestMatcher::pinyinComputeCjk() {
    const auto r = Pinyin::compute(QString::fromUtf8("微信"));
    QCOMPARE(r.full, QString("weixin"));
    QCOMPARE(r.initials, QString("wx"));
    QCOMPARE(r.offsets.size(), 2);
}

void TestMatcher::pinyinComputeAsciiPassthrough() {
    const auto r = Pinyin::compute("WeChat");
    QCOMPARE(r.full, QString("WeChat"));
    QCOMPARE(r.initials, QString("WeChat"));
    QCOMPARE(r.offsets.size(), 6);
    QCOMPARE(r.offsets[0], 0);
}

void TestMatcher::pinyinComputePunctSkipped() {
    // "微·信"：间隔符为标点，跳过
    const auto r = Pinyin::compute(QString::fromUtf8("微·信"));
    QCOMPARE(r.full, QString("weixin"));
    QCOMPARE(r.initials, QString("wx"));
    QCOMPARE(r.offsets.size(), 3);
    QCOMPARE(r.offsets[1], -1); // '·' 被跳过
}

void TestMatcher::pinyinComputeOffsets() {
    const auto r = Pinyin::compute(QString::fromUtf8("微信"));
    // 微 → "wei" 起始偏移 0；信 → "xin" 起始偏移 3
    QCOMPARE(r.offsets[0], 0);
    QCOMPARE(r.offsets[1], 3);
}

void TestMatcher::pinyinFullScores() {
    const auto py = Pinyin::compute(QString::fromUtf8("微信"));
    QVERIFY(Matcher::score(py.full, "wei") > 0);                    // "weixin" 前缀匹配
    QCOMPARE(Matcher::score(py.full, "weixin"), 1000);              // 精确匹配
    QCOMPARE(Matcher::score(QString::fromUtf8("微信"), "wei"), -1); // 直接匹配：无
}

void TestMatcher::pinyinInitialsScores() {
    const auto py = Pinyin::compute(QString::fromUtf8("微信"));
    QCOMPARE(Matcher::score(py.initials, "wx"), 1000); // "wx" 精确
    QVERIFY(Matcher::score(py.initials, "w") > 0);     // 前缀
}

void TestMatcher::pinyinDirectBeatsFullPinyin() {
    // 英文前缀直接命中优先于同 query 的拼音全拼前缀
    const int direct = Matcher::score("WeightX", "wei"); // 800-7=793
    const int pinyin =
        Matcher::score(Pinyin::compute(QString::fromUtf8("微信")).full, "wei") -
        50; // 794-50=744
    QVERIFY(direct > pinyin);
}

void TestMatcher::pinyinHighlightFull() {
    const auto py  = Pinyin::compute(QString::fromUtf8("微信"));
    const auto pos = Pinyin::pinyinMatchedTitlePositions(py, "wei", false);
    QCOMPARE(pos, (QList<int>{0})); // "wei" 对应 微（下标 0）
}

void TestMatcher::pinyinHighlightInitials() {
    const auto py  = Pinyin::compute(QString::fromUtf8("微信"));
    const auto pos = Pinyin::pinyinMatchedTitlePositions(py, "wx", true);
    QCOMPARE(pos, (QList<int>{0, 1})); // "wx" 对应 微(0) 和 信(1)
}

QTEST_GUILESS_MAIN(TestMatcher)
#include "test_matcher.moc"
