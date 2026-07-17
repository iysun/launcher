#include "terminaltabs.h"
#include "core/theme.h"
#include "ui/terminalpane.h"
#include "ui/terminalview.h"
#include "vt/terminalcore.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QRegion>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
// 标签名：pwsh 初始把标题设成 exe 全路径、cd 后是目录全路径——只取最后一段，
// 去掉 .exe 后缀，超长省略，避免整条路径撑爆标签栏。空标题返回空（保留 "Terminal N"）。
QString shortTabLabel(const QString &raw) {
    QString t = raw.trimmed();
    if (t.isEmpty()) return {};
    const int slash = qMax(t.lastIndexOf('\\'), t.lastIndexOf('/'));
    if (slash >= 0 && slash < t.size() - 1) t = t.mid(slash + 1);
    if (t.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) t.chop(4);
    if (t.size() > 24) t = t.left(23) + QStringLiteral("…");
    return t;
}
} // namespace

TerminalTabs::TerminalTabs(QWidget *parent) : QWidget(parent) {
    m_bar = new QTabBar(this);
    m_bar->setTabsClosable(false); // 用自绘的主题化关闭按钮（默认红 X 在深色下太扎眼）
    m_bar->setExpanding(false);
    m_bar->setDrawBase(false);
    m_bar->setUsesScrollButtons(true);
    m_bar->setElideMode(Qt::ElideRight);
    m_bar->setFocusPolicy(Qt::NoFocus); // 别从终端抢走键盘焦点

    auto *plus = new QToolButton(this);
    plus->setText(QStringLiteral("+"));
    plus->setFocusPolicy(Qt::NoFocus);
    plus->setCursor(Qt::PointingHandCursor);
    plus->setToolTip(QStringLiteral("Ctrl+Shift+T"));

    m_strip    = new QWidget(this);
    m_stripLay = new QHBoxLayout(m_strip);
    m_stripLay->setContentsMargins(0, 0, 0, 0);
    m_stripLay->setSpacing(0);
    m_stripLay->addWidget(m_bar, 0);
    m_stripLay->addWidget(plus, 0);
    m_dragArea = new QWidget(m_strip); // 撑开的空白：窗口 chrome 模式下作拖动区
    m_dragArea->setFocusPolicy(Qt::NoFocus);
    m_stripLay->addWidget(m_dragArea, 1);
    m_strip->hide(); // 单标签不显示，多于一个才亮出来（窗口 chrome 模式下常显）

    m_stack = new QStackedWidget(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_strip, 0);
    root->addWidget(m_stack, 1);

    // 主题化：标签栏/选中态/关闭按钮/“+”按当前主题取色，与终端底色一致
    const QString bg = Theme::c("bg"), surface = Theme::c("surface"),
                  text = Theme::c("text"), accent = Theme::c("accent"),
                  hover = Theme::c("hoverBg");
    setStyleSheet(QString(R"(
        QWidget { background: %1; }
        QTabBar { background: %1; qproperty-drawBase: 0; }
        QTabBar::tab {
            background: %2; color: %3;
            padding: 3px 8px; margin: 4px 0 0 4px;
            border-radius: 6px; min-width: 44px;
            border: 1px solid transparent;   /* 占位，避免选中加边时尺寸跳动 */
        }
        QTabBar::tab:selected { background: %1; color: %4; border: 1px solid %4; }
        QTabBar::tab:hover:!selected { background: %5; }
        QToolButton {
            background: transparent; color: %3;
            border: none; font-size: 18px; padding: 0 10px;
        }
        QToolButton:hover { color: %4; }
    )")
                      .arg(bg, surface, text, accent, hover));

    connect(m_bar, &QTabBar::currentChanged, this, [this](int i) {
        if (i < 0) return;
        m_stack->setCurrentIndex(i);
        if (auto *p = currentPane()) p->focusTerminal();
    });
    connect(plus, &QToolButton::clicked, this, [this] { newTab(); });
}

TerminalPane *TerminalTabs::currentPane() const {
    return qobject_cast<TerminalPane *>(m_stack->currentWidget());
}

int TerminalTabs::count() const { return m_stack->count(); }

TerminalPane *TerminalTabs::newTab() {
    auto *pane = new TerminalPane(m_stack);
    // 快捷键须抢在 TerminalView 消费前拦截 → filter 装在 pane 的 view 上，统一由本容器处理
    pane->view()->installEventFilter(this);

    const int labelN = ++m_seq;

    // shell 退出 → 关掉该标签（deleteLater 延后，安全）
    connect(pane, &TerminalPane::sessionEnded, this, [this, pane] {
        const int i = m_stack->indexOf(pane);
        if (i >= 0) closeTab(i);
    });
    // OSC 标题（shell 常设为 cwd）→ 标签名（取最后一段）；空/无标题保留 "Terminal N" 回退
    if (auto *core = pane->view()->core()) {
        connect(core, &TerminalCore::titleChanged, this, [this, pane](const QString &t) {
            const int     i = m_stack->indexOf(pane);
            const QString s = shortTabLabel(t);
            if (i >= 0 && !s.isEmpty()) m_bar->setTabText(i, s);
        });
    }

    const int idx = m_stack->addWidget(pane);
    m_bar->insertTab(idx, QString(QStringLiteral("Terminal %1")).arg(labelN));

    // 自绘主题化关闭按钮（替代默认深色下扎眼的红 X）：平时随文本色，hover 变 danger
    auto *cb = new QToolButton(m_bar);
    cb->setText(QStringLiteral("×"));
    cb->setFocusPolicy(Qt::NoFocus);
    cb->setCursor(Qt::PointingHandCursor);
    cb->setToolTip(QStringLiteral("Ctrl+Shift+W"));
    cb->setStyleSheet(QString("QToolButton{border:none;background:transparent;color:%1;"
                              "font-size:15px;padding:0 2px;}"
                              "QToolButton:hover{color:%2;}")
                          .arg(Theme::c("text"), Theme::c("danger")));
    connect(cb, &QToolButton::clicked, this, [this, pane] {
        const int i = m_stack->indexOf(pane);
        if (i >= 0) closeTab(i);
    });
    m_bar->setTabButton(idx, QTabBar::RightSide, cb);

    pane->startSession();
    m_bar->setCurrentIndex(idx); // 触发 currentChanged → 切栈 + 聚焦
    updateStripVisibility();
    return pane;
}

void TerminalTabs::closeTab(int index) {
    if (index < 0 || index >= m_stack->count()) return;
    auto *pane = qobject_cast<TerminalPane *>(m_stack->widget(index));

    {
        QSignalBlocker block(m_bar); // 关时暂屏蔽 currentChanged 风暴，稍后显式设当前
        m_bar->removeTab(index);
    }
    if (pane) {
        m_stack->removeWidget(pane);
        pane->deleteLater(); // 析构里 teardown（停 reader/pty，顺序不可乱）
    }

    if (m_stack->count() == 0) {
        emit lastTabClosed(); // 关到 0 个 → 宿主退出终端模式 / 关窗
        return;
    }
    const int cur = qMin(index, m_stack->count() - 1);
    m_bar->setCurrentIndex(cur);
    m_stack->setCurrentIndex(cur);
    if (auto *p = currentPane()) p->focusTerminal();
    updateStripVisibility();
}

void TerminalTabs::updateStripVisibility() {
    // 窗口 chrome 模式常显（兼作标题栏）；否则单标签隐藏，回归干净单会话观感
    m_strip->setVisible(m_stripAlways || m_bar->count() > 1);
}

void TerminalTabs::setStripAlwaysVisible(bool on) {
    m_stripAlways = on;
    updateStripVisibility();
}

void TerminalTabs::addStripTrailing(QWidget *w) {
    w->setParent(m_strip);
    m_stripLay->addWidget(w, 0); // 落在拖拽区（stretch=1）之后 → 贴最右
}

bool TerminalTabs::pointInDragArea(const QPoint &globalPos) const {
    if (!m_strip->isVisible() || !m_dragArea) return false;
    return m_dragArea->rect().contains(m_dragArea->mapFromGlobal(globalPos));
}

void TerminalTabs::nextTab() {
    const int n = m_bar->count();
    if (n <= 1) return;
    m_bar->setCurrentIndex((m_bar->currentIndex() + 1) % n);
}

void TerminalTabs::prevTab() {
    const int n = m_bar->count();
    if (n <= 1) return;
    m_bar->setCurrentIndex((m_bar->currentIndex() - 1 + n) % n);
}

void TerminalTabs::jumpTab(int index) {
    if (index >= 0 && index < m_bar->count()) m_bar->setCurrentIndex(index);
}

void TerminalTabs::startSession() {
    if (m_stack->count() == 0) {
        newTab();
    } else if (auto *p = currentPane()) {
        p->startSession();
        p->focusTerminal();
    }
}

void TerminalTabs::runCommand(const QString &cmd) {
    if (m_stack->count() == 0) newTab();
    if (auto *p = currentPane()) {
        p->runCommand(cmd);
        p->focusTerminal();
    }
}

void TerminalTabs::focusTerminal() {
    if (auto *p = currentPane()) p->focusTerminal();
}

void TerminalTabs::teardown() {
    for (int i = 0; i < m_stack->count(); ++i)
        if (auto *p = qobject_cast<TerminalPane *>(m_stack->widget(i))) p->teardown();
}

bool TerminalTabs::eventFilter(QObject *obj, QEvent *e) {
    if (e->type() == QEvent::KeyPress) {
        auto      *key  = static_cast<QKeyEvent *>(e);
        const auto mods = key->modifiers();
        const int  k    = key->key();
        const bool ctrl = mods & Qt::ControlModifier;
        const bool shift = mods & Qt::ShiftModifier;
        const bool alt  = mods & Qt::AltModifier;

        if (ctrl && !shift && k == Qt::Key_QuoteLeft) { // Ctrl+` 退出终端模式
            emit exitRequested();
            return true;
        }
        if (ctrl && shift && k == Qt::Key_T) { // 新建标签
            newTab();
            return true;
        }
        if (ctrl && shift && k == Qt::Key_W) { // 关闭当前标签
            closeTab(m_bar->currentIndex());
            return true;
        }
        // 上一个：Ctrl+Shift+Tab（部分平台来的是 Backtab）
        if (ctrl && (k == Qt::Key_Backtab || (shift && k == Qt::Key_Tab))) {
            prevTab();
            return true;
        }
        if (ctrl && !shift && k == Qt::Key_Tab) { // 下一个
            nextTab();
            return true;
        }
        if (alt && !ctrl && k >= Qt::Key_1 && k <= Qt::Key_9) { // Alt+1..9 跳转
            jumpTab(k - Qt::Key_1);
            return true;
        }
    }
    return QWidget::eventFilter(obj, e);
}

// 不透明 TerminalView 会糊掉宿主卡片圆角；在容器层用圆角 QRegion 遮罩（含标签栏顶角 +
// 面板底角一次裁齐）。r==0 时清除遮罩（独立窗原生方框）。随尺寸重算。
void TerminalTabs::applyCornerMask() {
    if (m_cornerRadius <= 0 || width() <= 0 || height() <= 0) {
        clearMask();
        return;
    }
    const int   d = m_cornerRadius * 2;
    const QRect r = rect();
    QRegion     reg(r.adjusted(m_cornerRadius, 0, -m_cornerRadius, 0));
    reg += QRegion(r.adjusted(0, m_cornerRadius, 0, -m_cornerRadius));
    reg += QRegion(r.left(), r.top(), d, d, QRegion::Ellipse);
    reg += QRegion(r.right() - d + 1, r.top(), d, d, QRegion::Ellipse);
    reg += QRegion(r.left(), r.bottom() - d + 1, d, d, QRegion::Ellipse);
    reg += QRegion(r.right() - d + 1, r.bottom() - d + 1, d, d, QRegion::Ellipse);
    setMask(reg);
}

void TerminalTabs::setCornerRadius(int r) {
    m_cornerRadius = r;
    applyCornerMask();
}

void TerminalTabs::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    applyCornerMask();
}
