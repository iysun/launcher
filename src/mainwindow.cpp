#include "mainwindow.h"
#include "core/usagestore.h"
#include "ui/resultdelegate.h"
#include <algorithm>
#include <QApplication>
#include <QCursor>
#include <QFrame>
#include <QGuiApplication>
#include <QHotkey>
#include <QLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

static constexpr int kWidth          = 620;
static constexpr int kItemH          = 56;  // 须与 ResultDelegate::sizeHint 同步
static constexpr int kSearchH        = 52;
static constexpr int kMaxItems       = 8;
static constexpr int kQueryDebounceMs = 100;  // 停止输入后再查询，避免逐键查询

MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
    setupUi();

    m_usage = new UsageStore();

    m_queryTimer = new QTimer(this);
    m_queryTimer->setSingleShot(true);
    m_queryTimer->setInterval(kQueryDebounceMs);
    connect(m_queryTimer, &QTimer::timeout, this, &MainWindow::runQuery);

    m_hotkey = new QHotkey(QKeySequence("Alt+Space"), true, this);
    connect(m_hotkey, &QHotkey::activated, this, &MainWindow::toggle);
}

void MainWindow::addPlugin(IPlugin *plugin) {
    m_plugins.append(plugin);
}

// ── UI ────────────────────────────────────────────────────────

void MainWindow::setupUi() {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->setSizeConstraint(QLayout::SetFixedSize);  // 窗口高度始终精确贴合内容

    // 整体一张圆角卡片：背景 + 描边 + 圆角都由 card 绘制，
    // 搜索框与列表透明叠在上面，避免子控件圆角拼接产生缝隙
    auto *card = new QFrame(this);
    card->setObjectName("card");
    card->setFixedWidth(kWidth);
    card->setStyleSheet(R"(
        QFrame#card {
            background: #1e1e2e;
            border: 1px solid #45475a;
            border-radius: 10px;
        }
    )");
    root->addWidget(card);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    m_search = new QLineEdit(card);
    m_search->setPlaceholderText("搜索应用、文件、命令…");
    m_search->setFixedHeight(kSearchH);
    m_search->setStyleSheet(R"(
        QLineEdit {
            background: transparent;
            color: #cdd6f4;
            font-size: 18px;
            padding: 0 16px;
            border: none;
        }
    )");

    m_list = new QListWidget(card);
    m_list->setFocusPolicy(Qt::StrongFocus);
    m_delegate = new ResultDelegate(m_list);
    m_list->setItemDelegate(m_delegate);
    m_list->setUniformItemSizes(true);
    m_list->setStyleSheet(R"(
        QListWidget {
            background: transparent;
            color: #cdd6f4;
            border: none;
            border-top: 1px solid #313244;
            font-size: 14px;
            outline: 0;
        }
    )");
    m_list->viewport()->setAutoFillBackground(false);  // 让卡片底色透出
    m_list->hide();

    cardLayout->addWidget(m_search);
    cardLayout->addWidget(m_list);

    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::onTextChanged);
    connect(m_list, &QListWidget::itemActivated, this, &MainWindow::onItemActivated);

    m_search->installEventFilter(this);
    m_list->installEventFilter(this);
}

// ── 显示逻辑 ──────────────────────────────────────────────────

void MainWindow::toggle() {
    if (isVisible()) {
        hide();
    } else {
        centerOnScreen();
        show();
        raise();
        activateWindow();
        m_search->clear();
        m_search->setFocus();
    }
}

void MainWindow::centerOnScreen() {
    // 跟随光标所在屏（多屏场景），取不到再回退主屏
    QScreen *scr = QGuiApplication::screenAt(QCursor::pos());
    if (!scr)
        scr = QApplication::primaryScreen();
    const QRect geo = scr->availableGeometry();
    // 用 left()/top() 偏移，确保在副屏或带任务栏时定位正确
    move(geo.left() + (geo.width() - kWidth) / 2, geo.top() + geo.height() / 4);
}

void MainWindow::changeEvent(QEvent *e) {
    if (e->type() == QEvent::ActivationChange && !isActiveWindow())
        hide();
    QWidget::changeEvent(e);
}

// ── 搜索与结果 ────────────────────────────────────────────────

void MainWindow::onTextChanged(const QString &text) {
    m_pendingKeyword = text.trimmed();
    if (m_pendingKeyword.isEmpty()) {
        m_queryTimer->stop();
        m_list->clear();  // 清空残留项，避免空框回车启动上一条结果
        m_list->hide();
        return;
    }
    m_queryTimer->start();  // 连续输入只在停顿 kQueryDebounceMs 后查询一次
}

// 防抖到期后执行。此处是将来把耗时插件查询挪到工作线程的接缝点：
// 现阶段插件均为内存级即时查询，故同步执行；待 FilePlugin 等慢插件落地，
// 再在这里改为异步回调 + 失效代次丢弃旧结果。
void MainWindow::runQuery() {
    if (!isVisible()) return;  // 窗口已隐藏则无需查询

    const QString kw = m_pendingKeyword;
    if (kw.isEmpty()) {
        m_list->hide();
        return;
    }

    m_delegate->setKeyword(kw);

    QList<ResultItem> results;
    for (auto *p : m_plugins) {
        const QString prefix = p->triggerPrefix();
        QString sub;
        if (prefix.isEmpty()) {
            sub = kw;  // 全局插件：对任意输入生效
        } else if (kw.startsWith(prefix)) {
            sub = kw.mid(prefix.length()).trimmed();
            if (sub.isEmpty()) continue;  // 只键入了前缀本身，尚无查询词
        } else {
            continue;  // 前缀不匹配，跳过该插件
        }
        QList<ResultItem> r = p->query(sub);
        for (ResultItem &item : r) {
            item.owner  = p;  // 标记产出插件，execute 时只路由给它
            item.score += m_usage->frecencyBonus(item.action);  // 常用项同档内上浮
        }
        results += r;
    }

    // 跨插件统一排序：分数高者优先，同分按标题字母序稳定排序
    std::stable_sort(results.begin(), results.end(),
                     [](const ResultItem &a, const ResultItem &b) {
                         if (a.score != b.score) return a.score > b.score;
                         return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
                     });
    if (results.size() > kMaxItems)
        results = results.mid(0, kMaxItems);

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
        auto *li = new QListWidgetItem();  // 内容由 ResultDelegate 绘制
        li->setData(Qt::UserRole, QVariant::fromValue(item));
        li->setToolTip(item.subtitle);
        m_list->addItem(li);
    }

    m_list->setCurrentRow(0);  // 默认选中首项，与回车行为一致

    int shown = qMin(items.size(), kMaxItems);
    m_list->setFixedHeight(shown * kItemH + 1);  // +1 给顶部分隔线，避免裁掉一行像素
    m_list->show();
}

void MainWindow::onItemActivated(QListWidgetItem *item) {
    if (!item) return;
    auto result = item->data(Qt::UserRole).value<ResultItem>();
    if (result.owner) {  // 只让产出该结果的插件执行，互不串扰
        result.owner->execute(result);
        m_usage->recordUse(result.action);  // 记录使用，供 frecency 排序
    }
    hide();
}

// ── 键盘导航 ──────────────────────────────────────────────────

bool MainWindow::eventFilter(QObject *obj, QEvent *e) {
    if (e->type() != QEvent::KeyPress)
        return QWidget::eventFilter(obj, e);

    auto *key = static_cast<QKeyEvent *>(e);

    if (obj == m_search) {
        switch (key->key()) {
        case Qt::Key_Escape:
            hide();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            flushPendingQuery();  // 确保作用于最新关键词的结果
            onItemActivated(m_list->count() ? m_list->item(0) : nullptr);
            return true;
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
            flushPendingQuery();  // 确保作用于最新关键词的结果
            onItemActivated(m_list->currentItem());
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
