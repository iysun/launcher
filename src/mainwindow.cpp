#include "mainwindow.h"
#include <QApplication>
#include <QHotkey>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QVBoxLayout>

static constexpr int kWidth     = 620;
static constexpr int kItemH     = 44;
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
    setFixedWidth(kWidth);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(1);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("搜索应用、文件、命令…");
    m_search->setFixedHeight(kSearchH);
    m_search->setStyleSheet(R"(
        QLineEdit {
            background: #1e1e2e;
            color: #cdd6f4;
            font-size: 18px;
            padding: 0 16px;
            border: none;
            border-radius: 10px;
        }
    )");

    m_list = new QListWidget(this);
    m_list->setFocusPolicy(Qt::StrongFocus);
    m_list->setStyleSheet(R"(
        QListWidget {
            background: #1e1e2e;
            color: #cdd6f4;
            border: none;
            border-radius: 0 0 10px 10px;
            font-size: 14px;
            outline: 0;
        }
        QListWidget::item { height: 44px; padding: 0 16px; }
        QListWidget::item:selected { background: #313244; color: #89b4fa; }
        QListWidget::item:hover    { background: #181825; }
    )");
    m_list->hide();

    root->addWidget(m_search);
    root->addWidget(m_list);

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
    if (text.trimmed().isEmpty()) {
        m_list->hide();
        adjustSize();
        return;
    }

    QList<ResultItem> results;
    for (auto *p : m_plugins)
        results += p->query(text);

    showResults(results);
}

void MainWindow::showResults(const QList<ResultItem> &items) {
    m_list->clear();

    if (items.isEmpty()) {
        m_list->hide();
        adjustSize();
        return;
    }

    for (const auto &item : items) {
        auto *li = new QListWidgetItem(item.icon, item.title);
        li->setToolTip(item.subtitle);
        li->setData(Qt::UserRole, QVariant::fromValue(item));
        m_list->addItem(li);
    }

    int shown = qMin(items.size(), kMaxItems);
    m_list->setFixedHeight(shown * kItemH);
    m_list->show();
    adjustSize();
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
                m_list->setCurrentRow(0);
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
