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
    void mouseModeTracked();
    void mouseReportEmitsBytes();
    void ansiPaletteRecolors();
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

void TestVt::mouseModeTracked() {
    TerminalCore core(5, 20);
    QVERIFY(core.mouseMode() == TerminalCore::MouseMode::None);
    QSignalSpy spy(&core, &TerminalCore::mouseModeChanged);

    core.feed("\x1b[?1000h"); // 启用点击上报（VT200 mouse）
    QVERIFY(core.mouseMode() == TerminalCore::MouseMode::Click);
    QCOMPARE(spy.count(), 1);

    core.feed("\x1b[?1003h"); // 任意移动上报
    QVERIFY(core.mouseMode() == TerminalCore::MouseMode::Move);

    core.feed("\x1b[?1000l"); // 关闭 1000（1003 也随之关，回到 None）
    QVERIFY(core.mouseMode() == TerminalCore::MouseMode::None);
}

void TestVt::mouseReportEmitsBytes() {
    TerminalCore core(5, 20);
    core.feed("\x1b[?1000h"); // 应用请求鼠标上报，否则 vterm 不产出字节
    QSignalSpy spy(&core, &TerminalCore::outputToPty);
    core.mouseMove(2, 3, Qt::NoModifier);
    core.mouseButton(1, true, Qt::NoModifier); // 左键按下
    QVERIFY(spy.count() >= 1); // 至少产出一段 CSI 鼠标上报序列
}

void TestVt::ansiPaletteRecolors() {
    TerminalCore core(3, 10);
    // 把 ANSI 索引 1（红槽）改成一个独特色，再让终端用 SGR 31 输出一个字符
    QList<QColor> pal;
    for (int i = 0; i < 16; ++i)
        pal.append(QColor(Qt::black));
    pal[1] = QColor(0x12, 0x34, 0x56); // 独特、绝不会与 libvterm 内置红撞
    core.applyAnsiPalette(pal);

    core.feed("\x1b[31mX"); // 前景 = ANSI 1
    QCOMPARE(core.cellAt(0, 0).text, QStringLiteral("X"));
    QCOMPARE(core.cellAt(0, 0).fg, QColor(0x12, 0x34, 0x56)); // 取到注入后的色
}

QTEST_GUILESS_MAIN(TestVt)
#include "test_vt.moc"
