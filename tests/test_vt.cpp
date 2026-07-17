#include "vt/terminalcore.h"
#include <QSignalSpy>
#include <QtTest>

// TerminalCore scrollback（滚动回看）单测：喂带滚出内容的字节流，断言历史缓冲
// 行数 / 内容 / 驱逐计数 / altscreen 属性。纯逻辑无窗口，QTEST_GUILESS_MAIN 即可
// （QColor 只需链接 Qt6::Gui，不需要 QGuiApplication 实例）。
class TestVt : public QObject {
    Q_OBJECT
private:
    // 读一整行历史（拼接 text，尾部空白裁剪）
    static QString historyLine(const TerminalCore &core, int histRow, int cols) {
        QString s;
        for (int col = 0; col < cols;) {
            TerminalCore::Cell c = core.historyCellAt(histRow, col);
            s += c.text.isEmpty() ? QStringLiteral(" ") : c.text;
            col += c.width > 1 ? 2 : 1;
        }
        while (s.endsWith(QLatin1Char(' ')))
            s.chop(1);
        return s;
    }
    static QByteArray numberedLines(int from, int count) {
        QByteArray out;
        for (int i = from; i < from + count; ++i)
            out += "line" + QByteArray::number(i) + "\r\n";
        return out;
    }

private slots:
    void emptyHistoryInitially();
    void scrolledOutLinesEnterHistory();
    void pushEmitsScrollbackChanged();
    void historyIsCappedAndEvicts();
    void altScreenPropTracked();
    void altScreenDoesNotPush();
};

void TestVt::emptyHistoryInitially() {
    TerminalCore core(5, 20);
    QCOMPARE(core.historySize(), 0);
    QCOMPARE(core.historyStart(), qint64(0));
    // 越界访问安全返回空白格
    QCOMPARE(core.historyCellAt(0, 0).text, QString());
}

void TestVt::scrolledOutLinesEnterHistory() {
    TerminalCore core(5, 20);
    core.feed(numberedLines(0, 10)); // 10 行 + 末尾换行，5 行屏幕 → 滚出 6 行
    QCOMPARE(core.historySize(), 6);
    QCOMPARE(historyLine(core, 0, core.cols()), QStringLiteral("line0"));
    QCOMPARE(historyLine(core, 5, core.cols()), QStringLiteral("line5"));
    // 屏幕上剩 line6..line9
    QCOMPARE(core.cellAt(0, 4).text, QStringLiteral("6"));
}

void TestVt::pushEmitsScrollbackChanged() {
    TerminalCore core(5, 20);
    QSignalSpy spy(&core, &TerminalCore::scrollbackChanged);
    core.feed(numberedLines(0, 6)); // 恰好滚出 2 行
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).toInt(), 1);
}

void TestVt::historyIsCappedAndEvicts() {
    TerminalCore core(5, 20);
    core.feed(numberedLines(0, 5210)); // 滚出 5206 行 > 上限 5000
    QCOMPARE(core.historySize(), 5000);
    QCOMPARE(core.historyStart(), qint64(206)); // 驱逐计数 = 滚出 - 上限
    // 最旧一行已不是 line0，而是被驱逐后的首行 line206
    QCOMPARE(historyLine(core, 0, core.cols()), QStringLiteral("line206"));
}

void TestVt::altScreenPropTracked() {
    TerminalCore core(5, 20);
    QVERIFY(!core.altScreen());
    core.feed("\x1b[?1049h"); // 进入备用屏（vim/htop）
    QVERIFY(core.altScreen());
    core.feed("\x1b[?1049l");
    QVERIFY(!core.altScreen());
}

void TestVt::altScreenDoesNotPush() {
    TerminalCore core(5, 20);
    core.feed("\x1b[?1049h");
    const int before = core.historySize();
    core.feed(numberedLines(0, 20)); // 备用屏滚动不进历史
    QCOMPARE(core.historySize(), before);
    core.feed("\x1b[?1049l");
}

QTEST_GUILESS_MAIN(TestVt)
#include "test_vt.moc"
