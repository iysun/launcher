#include "mainwindow.h"
#include "core/appsettings.h"
#include "core/i18n.h"
#include "core/theme.h"
#include "core/usagestore.h"
#include "ui/resultdelegate.h"
#include "ui/terminaltabs.h"
#include "ui/wrappane.h"
#include <algorithm>
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <QFrame>
#include <QGuiApplication>
#include <QHotkey>
#include <QLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>

static constexpr int kWidth           = 620;
static constexpr int kItemH           = 56; // 须与 ResultDelegate::sizeHint 同步
static constexpr int kSearchH         = 52;
static constexpr int kMaxItems        = 8;
static constexpr int kQueryDebounceMs = 100; // 停止输入后再查询，避免逐键查询
static constexpr int kTermW           = 900; // 终端模式卡片宽
static constexpr int kTermH           = 520; // 终端模式 pane 高
static constexpr int kWrapOutH        = 280; // Wrap 输出区高度

MainWindow::MainWindow(AppSettings *settings, QWidget *parent)
    : QWidget(parent), m_settings(settings) {
    setupUi();

    m_usage = new UsageStore();

    m_queryTimer = new QTimer(this);
    m_queryTimer->setSingleShot(true);
    m_queryTimer->setInterval(kQueryDebounceMs);
    connect(m_queryTimer, &QTimer::timeout, this, &MainWindow::runQuery);

    const QString hotkey = m_settings ? m_settings->hotkey() : "Alt+Space";
    m_hotkey             = new QHotkey(QKeySequence(hotkey), true, this);
    connect(m_hotkey, &QHotkey::activated, this, &MainWindow::toggle);

    if (m_settings) {
        connect(m_settings, &AppSettings::hotkeyChanged, this,
                [this](const QString &seq) {
                    m_hotkey->setRegistered(false);
                    m_hotkey->setShortcut(QKeySequence(seq), true);
                    notifyHotkeyStatus(); // 换热键后若新热键注册失败，同样提示
                });
    }

    // ── 系统托盘 ──────────────────────────────────────────────
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QIcon(":/icons/tray.png"));
    m_tray->setToolTip("Launcher");

    auto *trayMenu = new QMenu();
    trayMenu->addAction(I18n::t("tray.showHide"), this, &MainWindow::toggle);
    trayMenu->addSeparator();
    trayMenu->addAction(I18n::t("tray.quit"), qApp, &QApplication::quit);
    m_tray->setContextMenu(trayMenu);

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::DoubleClick) toggle();
            });

    m_tray->show();

    // 托盘就绪后再提示：热键在构造 QHotkey 时即尝试注册，失败（如被系统/其他程序占用）
    // 时用户只会遇到"按热键唤不起来"却无从得知原因，这里用托盘气泡把失败可见化
    notifyHotkeyStatus();
}

void MainWindow::notifyHotkeyStatus() {
    if (!m_hotkey || m_hotkey->isRegistered()) return;
    if (m_tray)
        m_tray->showMessage(I18n::t("hotkey.registerFailedTitle"),
                            I18n::t("hotkey.registerFailedBody"),
                            QSystemTrayIcon::Warning, 8000);
}

void MainWindow::addPlugin(IPlugin *plugin) {
    // 触发前缀冲突检测：两个插件注册同一非空前缀时，靠 runQuery 的"谁先匹配谁生效"会
    // 静默遮蔽后者。插件少时不易察觉，加装配期告警便于新增插件时第一时间发现。
    const QString prefix = plugin->triggerPrefix();
    if (!prefix.isEmpty()) {
        for (IPlugin *p : m_plugins) {
            if (p->triggerPrefix() == prefix) {
                qWarning().noquote()
                    << "触发前缀冲突：" << prefix << "已被" << p->name() << "占用，"
                    << plugin->name() << "将被遮蔽";
                break;
            }
        }
    }
    m_plugins.append(plugin);
}

// ── UI ────────────────────────────────────────────────────────

void MainWindow::setupUi() {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->setSizeConstraint(QLayout::SetFixedSize); // 窗口高度始终精确贴合内容

    // 整体一张圆角卡片：背景 + 描边 + 圆角都由 card 绘制，
    // 搜索框与列表透明叠在上面，避免子控件圆角拼接产生缝隙
    m_card = new QFrame(this);
    m_card->setObjectName("card");
    m_card->setFixedWidth(kWidth);
    m_card->setStyleSheet(QString(R"(
        QFrame#card {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }
    )")
                             .arg(Theme::c("bg"), Theme::c("border")));
    root->addWidget(m_card);

    m_cardLayout = new QVBoxLayout(m_card);
    m_cardLayout->setContentsMargins(0, 0, 0, 0);
    m_cardLayout->setSpacing(0);

    m_search = new QLineEdit(m_card);
    m_search->setPlaceholderText(I18n::t("search.placeholder"));
    m_search->setFixedHeight(kSearchH);
    m_search->setStyleSheet(QString(R"(
        QLineEdit {
            background: transparent;
            color: %1;
            font-size: 18px;
            padding: 0 16px;
            border: none;
        }
    )")
                                 .arg(Theme::c("text")));

    m_list = new QListWidget(m_card);
    m_list->setFocusPolicy(Qt::StrongFocus);
    m_delegate = new ResultDelegate(m_list);
    m_list->setItemDelegate(m_delegate);
    m_list->setUniformItemSizes(true);
    m_list->setStyleSheet(QString(R"(
        QListWidget {
            background: transparent;
            color: %1;
            border: none;
            border-top: 1px solid %2;
            font-size: 14px;
            outline: 0;
        }
    )")
                               .arg(Theme::c("text"), Theme::c("surface")));
    m_list->viewport()->setAutoFillBackground(false); // 让卡片底色透出
    m_list->hide();

    m_cardLayout->addWidget(m_search);
    m_cardLayout->addWidget(m_list);

    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::onTextChanged);
    connect(m_list, &QListWidget::itemActivated, this, &MainWindow::onItemActivated);

    m_search->installEventFilter(this);
    m_list->installEventFilter(this);
}

// ── 显示逻辑 ──────────────────────────────────────────────────

void MainWindow::toggle() {
    if (isVisible()) {
        if (m_mode == Mode::Wrap)
            exitWrap();
        hide();
    } else {
        centerOnScreen();
        show();
        raise();
        activateWindow();
        if (m_mode == Mode::Terminal && m_term) {
            m_term->focusTerminal(); // 终端模式：唤起即回到终端，不动搜索框
        } else if (m_mode == Mode::Wrap) {
            m_search->setFocus();
        } else {
            m_search->clear();
            m_search->setFocus();
        }
    }
}

void MainWindow::centerOnScreen() {
    // 跟随光标所在屏（多屏场景），取不到再回退主屏
    QScreen *scr = QGuiApplication::screenAt(QCursor::pos());
    if (!scr) scr = QApplication::primaryScreen();
    const QRect geo = scr->availableGeometry();
    // 用 left()/top() 偏移，确保在副屏或带任务栏时定位正确；终端模式更宽更高，垂直居中
    if (m_mode == Mode::Terminal) {
        move(geo.left() + (geo.width() - kTermW) / 2,
             geo.top() + (geo.height() - kTermH) / 2);
    } else if (m_mode == Mode::Wrap) {
        const int h = kSearchH + kWrapOutH;
        move(geo.left() + (geo.width() - kWidth) / 2,
             geo.top() + (geo.height() - h) / 2);
    } else {
        move(geo.left() + (geo.width() - kWidth) / 2, geo.top() + geo.height() / 4);
    }
}

void MainWindow::changeEvent(QEvent *e) {
    // 失焦自动隐藏仅在搜索模式生效：终端模式下 alt-tab 走开不能让会话窗口消失
    if (m_mode == Mode::Launch && e->type() == QEvent::ActivationChange &&
        !isActiveWindow())
        hide();
    QWidget::changeEvent(e);
}

// ── 内联终端模式 ──────────────────────────────────────────────

void MainWindow::enterTerminal(const QString &cmd, bool newTab) {
    if (m_mode == Mode::Wrap)
        exitWrap();

    if (!m_term) { // 懒建：首次进入才创建标签容器并挂进卡片
        m_term = new TerminalTabs(m_card);
        m_term->setCornerRadius(10); // 与卡片 border-radius 对齐，避免不透明终端糊掉圆角
        m_cardLayout->addWidget(m_term);
        m_term->hide();
        // 用户在终端内按 Ctrl+` → 切回搜索；关掉最后一个标签 → 也切回搜索
        connect(m_term, &TerminalTabs::exitRequested, this, &MainWindow::exitTerminal);
        connect(m_term, &TerminalTabs::lastTabClosed, this, &MainWindow::exitTerminal);
    }

    m_mode = Mode::Terminal;
    m_search->hide();
    m_list->hide();
    m_card->setFixedWidth(kTermW); // 切到终端尺寸档
    m_term->setFixedHeight(kTermH);
    m_term->show();

    if (!isVisible()) show();
    centerOnScreen(); // 宽高都变了，重新居中
    raise();
    activateWindow();

    if (newTab)
        m_term->openNewTab();
    else
        m_term->startSession();
    if (!cmd.isEmpty())
        m_term->runCommand(cmd);
    m_term->focusTerminal();
}

void MainWindow::runWrap(const QString &cmd) {
    if (cmd.isEmpty())
        return;

    if (!m_wrap) {
        m_wrap = new WrapPane(m_card);
        m_cardLayout->addWidget(m_wrap);
        m_wrap->hide();
    }

    m_mode = Mode::Wrap;
    m_list->hide();
    m_card->setFixedWidth(kWidth);
    m_wrap->setFixedHeight(kWrapOutH);
    m_wrap->show();

    if (!isVisible())
        show();
    centerOnScreen();
    raise();
    activateWindow();

    m_wrap->run(cmd);
    m_search->setFocus();
}

void MainWindow::exitWrap() {
    if (m_mode != Mode::Wrap)
        return;
    m_mode = Mode::Launch;
    if (m_wrap) {
        m_wrap->cancel();
        m_wrap->hide();
    }
    m_search->clear();
    m_search->setFocus();
    centerOnScreen();
}

void MainWindow::exitTerminal() {
    if (m_mode != Mode::Launch) {
        m_mode = Mode::Launch;
        if (m_term) m_term->hide(); // 会话不 teardown，后台留存，再进即续
        m_card->setFixedWidth(kWidth);
        m_search->show();
        m_search->clear(); // 清空，回到干净搜索态（列表随空查询隐藏）
        m_search->setFocus();
        centerOnScreen();
    }
}

// ── 搜索与结果 ────────────────────────────────────────────────

void MainWindow::onTextChanged(const QString &text) {
    m_pendingKeyword = text.trimmed();
    if (m_pendingKeyword.isEmpty()) {
        m_queryTimer->stop();
        m_list->clear(); // 清空残留项，避免空框回车启动上一条结果
        m_list->hide();
        return;
    }
    m_queryTimer->start(); // 连续输入只在停顿 kQueryDebounceMs 后查询一次
}

// 防抖到期后执行。同步插件（应用/命令）即时收集并展示；异步插件（文件搜索）派发到
// 工作线程，结果回来再合并重排。每次查询自增代次，异步回调据此丢弃过期结果。
void MainWindow::runQuery() {
    if (!isVisible()) return; // 窗口已隐藏则无需查询

    const QString kw = m_pendingKeyword;
    if (kw.isEmpty()) {
        m_list->hide();
        return;
    }

    m_delegate->setKeyword(kw);

    const int gen = ++m_queryGen; // 本次查询代次
    m_asyncResults.clear();       // 上一代次的异步结果作废

    // 输入命中某个前缀插件时，抑制全局插件，避免应用结果污染命令/文件视图
    bool prefixActive = false;
    for (auto *p : m_plugins) {
        const QString pre = p->triggerPrefix();
        if (!pre.isEmpty() && kw.startsWith(pre)) {
            prefixActive = true;
            break;
        }
    }

    const QStringList disabled =
        m_settings ? m_settings->disabledPlugins() : QStringList{};

    QList<ResultItem> sync;
    for (auto *p : m_plugins) {
        if (disabled.contains(p->name())) continue;
        const QString prefix = p->triggerPrefix();
        QString       sub;
        if (prefix.isEmpty()) {
            if (prefixActive) continue; // 已有前缀插件接管，全局插件让位
            sub = kw;                   // 全局插件：对任意输入生效
        } else if (kw.startsWith(prefix)) {
            // 裸前缀（只键入前缀本身）也照常调用插件，由插件决定空查询如何处理：
            // 命令类插件可列出全部命令（命令面板），文件类插件可返回空避免全盘列举。
            sub = kw.mid(prefix.length()).trimmed();
        } else {
            continue; // 前缀不匹配，跳过该插件
        }

        if (p->runsAsync()) {
            // 派发到线程池；finished 回调在主线程执行，先校验代次再合并
            auto *w = new QFutureWatcher<QList<ResultItem>>(this);
            connect(w, &QFutureWatcher<QList<ResultItem>>::finished, this,
                    [this, w, p, gen] {
                        const QList<ResultItem> r = w->result();
                        w->deleteLater();
                        if (gen != m_queryGen || !isVisible()) return; // 过期/已隐藏则弃
                        onAsyncResults(p, r);
                    });
            const QString subCopy = sub; // 值捕获，跨线程安全
            w->setFuture(QtConcurrent::run([p, subCopy] { return p->query(subCopy); }));
        } else {
            QList<ResultItem> r = p->query(sub);
            for (ResultItem &item : r) {
                item.owner = p; // 标记产出插件，execute 时只路由给它
                item.score += m_usage->frecencyBonus(item.action); // 常用项同档内上浮
            }
            sync += r;
        }
    }

    m_baseResults = sync;
    mergeAndShow(); // 先展示同步结果（应用/命令即时可见），异步结果稍后流入
}

// 异步结果到达（已过代次/可见性校验）：打 owner+frecency，累积后重新合并展示
void MainWindow::onAsyncResults(IPlugin *p, const QList<ResultItem> &r) {
    QList<ResultItem> tagged = r;
    for (ResultItem &item : tagged) {
        item.owner = p;
        item.score += m_usage->frecencyBonus(item.action);
    }
    m_asyncResults += tagged;
    mergeAndShow();
}

// 合并同步+异步结果，统一排序、截断，再对将展示的项做主线程装饰（补图标等）
void MainWindow::mergeAndShow() {
    QList<ResultItem> results = m_baseResults + m_asyncResults;

    // 跨插件统一排序：分数高者优先，同分按标题字母序稳定排序
    std::stable_sort(results.begin(), results.end(),
                     [](const ResultItem &a, const ResultItem &b) {
                         if (a.score != b.score) return a.score > b.score;
                         return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
                     });
    if (results.size() > kMaxItems) results = results.mid(0, kMaxItems);

    // 仅对最终展示的 ≤kMaxItems 条装饰：异步插件借此在主线程补图标（不可跨线程）
    for (ResultItem &item : results)
        if (item.owner) item.owner->decorate(item);

    showResults(results);
}

// 防抖窗口期内若用户立即回车，先把待执行查询冲刷掉，
// 避免回车作用于上一个关键词的过期列表（启动错项）
void MainWindow::flushPendingQuery() {
    if (m_queryTimer->isActive()) {
        m_queryTimer->stop();
        runQuery();
    }
}

void MainWindow::showResults(const QList<ResultItem> &items) {
    m_list->clear();

    if (items.isEmpty()) {
        m_list->hide();
        return;
    }

    for (const auto &item : items) {
        auto *li = new QListWidgetItem(); // 内容由 ResultDelegate 绘制
        li->setData(Qt::UserRole, QVariant::fromValue(item));
        li->setToolTip(item.subtitle);
        m_list->addItem(li);
    }

    m_list->setCurrentRow(0); // 默认选中首项，与回车行为一致

    int shown = qMin(items.size(), kMaxItems);
    m_list->setFixedHeight(shown * kItemH + 1); // +1 给顶部分隔线，避免裁掉一行像素
    m_list->show();
}

void MainWindow::onItemActivated(QListWidgetItem *item) {
    activate(item, false); // 鼠标双击 / itemActivated 信号：主动作
}

void MainWindow::activate(QListWidgetItem *item, bool alt) {
    if (!item) return;
    auto result = item->data(Qt::UserRole).value<ResultItem>();
    if (result.owner) { // 只让产出该结果的插件执行，互不串扰
        if (alt) {
            result.owner->executeAlt(result);
        } else {
            result.owner->execute(result);
        }
        m_usage->recordUse(result.action);
    }
    // execute 可能已切入内联终端 / wrap 模式：此时保持窗口，不隐藏
    if (m_mode == Mode::Terminal || m_mode == Mode::Wrap)
        return;
    hide();
}

// ── 键盘导航 ──────────────────────────────────────────────────

// 移动列表选中项（带钳制），焦点保持在搜索框
void MainWindow::moveSelection(int delta) {
    const int n = m_list->count();
    if (n == 0) return;
    const int row = qBound(0, m_list->currentRow() + delta, n - 1);
    m_list->setCurrentRow(row);
}

// Emacs 风格键位（Ctrl/Alt 组合；Ctrl+Enter 不在此处理，仍交回 Enter 分支做次级动作）
bool MainWindow::handleEmacsKey(QKeyEvent *key) {
    const Qt::KeyboardModifiers mods = key->modifiers();

    // 导航/取消（纯 Ctrl）：搜索框/列表两种焦点都生效
    if (mods == Qt::ControlModifier) {
        switch (key->key()) {
        case Qt::Key_N:
            moveSelection(+1);
            return true; // 下一项
        case Qt::Key_P:
            moveSelection(-1);
            return true; // 上一项
        case Qt::Key_G:
            hide();
            return true; // 取消（等同 Esc）
        }
    }

    // 以下为搜索框行内编辑，仅当搜索框获焦时作用于它，避免列表获焦时误改文本
    if (!m_search->hasFocus()) return false;

    if (mods == Qt::ControlModifier) {
        switch (key->key()) {
        case Qt::Key_A:
            m_search->home(false);
            return true; // 行首
        case Qt::Key_E:
            m_search->end(false);
            return true; // 行尾
        case Qt::Key_F:
            m_search->cursorForward(false);
            return true; // 前移一字符
        case Qt::Key_B:
            m_search->cursorBackward(false);
            return true; // 后移一字符
        case Qt::Key_D:
            m_search->del();
            return true; // 删后一字符
        case Qt::Key_K:  // kill 到行尾
            m_search->end(true);
            m_search->del();
            return true;
        }
    } else if (mods == Qt::AltModifier) { // Meta：词移动 / 删词
        switch (key->key()) {
        case Qt::Key_F:
            m_search->cursorWordForward(false);
            return true; // 前移一词
        case Qt::Key_B:
            m_search->cursorWordBackward(false);
            return true;                       // 后移一词
        case Qt::Key_D:                        // 删后一词
            m_search->deselect();              // 清残留选区，确保只删一个词
            m_search->cursorWordForward(true); // 从光标选到下一词尾
            m_search->del();
            return true;
        }
    }
    return false;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e) {
    if (e->type() != QEvent::KeyPress) return QWidget::eventFilter(obj, e);

    auto *key = static_cast<QKeyEvent *>(e);

    if (handleEmacsKey(key)) // Emacs 键位对搜索框/列表两种焦点都生效
        return true;

    if (m_mode == Mode::Wrap && key->key() == Qt::Key_Escape) {
        exitWrap();
        return true;
    }

    if (obj == m_search) {
        // Ctrl+` 从搜索模式进入内联终端模式（退出键由 TerminalPane 内部拦截）
        if ((key->modifiers() & Qt::ControlModifier) &&
            key->key() == Qt::Key_QuoteLeft) {
            enterTerminal();
            return true;
        }
        switch (key->key()) {
        case Qt::Key_Escape:
            hide();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            flushPendingQuery(); // 确保作用于最新关键词的结果
            // 取当前选中项（C-n/C-p 在搜索框内移动后仍正确），无则回退首项
            QListWidgetItem *target = m_list->currentItem();
            if (!target && m_list->count()) target = m_list->item(0);
            activate(target, key->modifiers() & Qt::ControlModifier);
            return true;
        }
        case Qt::Key_Down:
            if (m_list->count()) {
                m_list->setFocus();
                // 首项已默认选中，从搜索框下移直接到下一项
                m_list->setCurrentRow(qMin(1, m_list->count() - 1));
            }
            return true;
        }
    }

    if (obj == m_list) {
        switch (key->key()) {
        case Qt::Key_Escape:
            hide();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            flushPendingQuery(); // 确保作用于最新关键词的结果
            activate(m_list->currentItem(), key->modifiers() & Qt::ControlModifier);
            return true;
        case Qt::Key_Up:
            if (m_list->currentRow() == 0) {
                m_search->setFocus();
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, e);
}
