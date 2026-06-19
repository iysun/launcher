#include "mainwindow.h"
#include "ui/resultdelegate.h"
#include <QApplication>
#include <QFrame>
#include <QHotkey>
#include <QLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QVBoxLayout>

static constexpr int kWidth     = 620;
static constexpr int kItemH     = 56;  // 须与 ResultDelegate::sizeHint 同步
static constexpr int kSearchH   = 52;
static constexpr int kMaxItems  = 8;

MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
    setupUi();
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
    QScreen *scr = QApplication::primaryScreen();
    QRect geo    = scr->availableGeometry();
    move(geo.center().x() - kWidth / 2, geo.height() / 4);
}

void MainWindow::changeEvent(QEvent *e) {
    if (e->type() == QEvent::ActivationChange && !isActiveWindow())
        hide();
    QWidget::changeEvent(e);
}

// ── 搜索与结果 ────────────────────────────────────────────────

void MainWindow::onTextChanged(const QString &text) {
    const QString kw = text.trimmed();
    if (kw.isEmpty()) {
        m_list->hide();
        return;
    }

    m_delegate->setKeyword(kw);

    QList<ResultItem> results;
    for (auto *p : m_plugins)
        results += p->query(kw);

    showResults(results);
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
    for (auto *p : m_plugins)
        p->execute(result);
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
