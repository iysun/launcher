#include "core/matcher.h"
#include <QtTest>

// Matcher 是纯函数，无 GUI 依赖，故用 QTEST_GUILESS_MAIN（QCoreApplication）。
class TestMatcher : public QObject {
    Q_OBJECT
private slots:
    // ── score 分档优先级 ──
    void exactIsHighest();
    void tierOrdering();          // 前缀 > 词首边界 > 普通子串 > 子序列
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
};

void TestMatcher::exactIsHighest() {
    QCOMPARE(Matcher::score("Code", "Code"), 1000);
    QCOMPARE(Matcher::score("Code", "code"), 1000);  // 大小写不敏感
    QVERIFY(Matcher::score("Code", "Code") > Matcher::score("Firefox", "fire"));
}

void TestMatcher::tierOrdering() {
    const int prefix      = Matcher::score("Firefox", "fire");            // 前缀
    const int boundary    = Matcher::score("Visual Studio Code", "studio");  // 词首边界
    const int substring   = Matcher::score("Foobar", "oob");             // 普通子串
    const int subsequence = Matcher::score("Firefox", "ff");             // 子序列模糊

    QVERIFY(prefix > 0 && boundary > 0 && substring > 0 && subsequence > 0);
    QVERIFY(prefix > boundary);
    QVERIFY(boundary > substring);
    QVERIFY(substring > subsequence);  // 子串恒优先于子序列（常规标题长度下）
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
    QVERIFY(Matcher::score("FireFox", "ff") > 0);  // 子序列亦大小写不敏感
}

void TestMatcher::subsequenceMatches() {
    QVERIFY(Matcher::score("Firefox", "ff") > 0);       // F..f
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

QTEST_GUILESS_MAIN(TestMatcher)
#include "test_matcher.moc"
