#include "settingsdialog.h"
#include "core/appsettings.h"
#include "plugins/webplugin.h"
#include "ui/hotkeyedit.h"
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

// ── 颜色常量（Catppuccin Mocha） ──────────────────────────────
static const char *kBg       = "#1e1e2e";
static const char *kSurface0 = "#313244";
static const char *kOverlay0 = "#6c7086";
static const char *kText     = "#cdd6f4";
static const char *kBlue     = "#89b4fa";
static const char *kBorder   = "#45475a";

SettingsDialog::SettingsDialog(AppSettings *settings, const QList<IPlugin *> &plugins)
    : QWidget(nullptr), m_settings(settings) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(kWidth);
    setupUi(plugins);

    // 分区会随功能增长变多变高；给整个对话框设高度上限（留出边距），超出时内容区
    // 自动滚动（见 setupUi 里的 QScrollArea），保证标题栏和底部保存/取消按钮
    // 在任意屏幕高度下都可见、可达，不会被推出屏幕
    QScreen *scr = QGuiApplication::primaryScreen();
    if (scr) setMaximumHeight(scr->availableGeometry().height() - 24);
}

// ── UI 构建 ───────────────────────────────────────────────────

QLabel *SettingsDialog::makeSectionLabel(const QString &text) {
    auto *lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; "
                               "padding: 0; margin: 0;")
                           .arg(kOverlay0));
    return lbl;
}

void SettingsDialog::setupUi(const QList<IPlugin *> &plugins) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *card = new QFrame(this);
    card->setObjectName("settingsCard");
    card->setFixedWidth(kWidth);
    card->setStyleSheet(QString(R"(
        QFrame#settingsCard {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }
    )")
                            .arg(kBg, kBorder));
    root->addWidget(card);

    auto *outer = new QVBoxLayout(card);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── 标题栏 ───────────────────────────────────────────────
    auto *titleBar = new QWidget(card);
    titleBar->setFixedHeight(kTitleH);
    titleBar->setStyleSheet(QString("background: transparent;"));

    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(16, 0, 12, 0);
    titleLayout->setSpacing(0);

    auto *titleLbl = new QLabel("设置", titleBar);
    titleLbl->setStyleSheet(
        QString("color: %1; font-size: 15px; font-weight: bold;").arg(kText));
    titleLayout->addWidget(titleLbl);
    titleLayout->addStretch();

    auto *closeBtn = new QPushButton("×", titleBar);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setStyleSheet(QString(R"(
        QPushButton {
            background: transparent;
            color: %1;
            font-size: 18px;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover { background: %2; }
    )")
                                .arg(kOverlay0, kSurface0));
    titleLayout->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, this, &SettingsDialog::hide);

    outer->addWidget(titleBar);

    // ── 分隔线 ───────────────────────────────────────────────
    auto *sep0 = new QFrame(card);
    sep0->setFrameShape(QFrame::HLine);
    sep0->setStyleSheet(
        QString("background: %1; border: none; max-height: 1px;").arg(kBorder));
    outer->addWidget(sep0);

    // ── 内容区 ───────────────────────────────────────────────
    auto *content       = new QWidget(card);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(20, 16, 20, 16);
    contentLayout->setSpacing(12);

    const QString inputStyle = QString(R"(
        QLineEdit {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 4px 8px;
            font-size: 13px;
        }
        QLineEdit:focus { border-color: %4; }
    )")
                                   .arg(kSurface0, kText, kBorder, kBlue);

    const QString checkStyle = QString(R"(
        QCheckBox {
            color: %1;
            font-size: 13px;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 16px; height: 16px;
            border: 1px solid %2;
            border-radius: 4px;
            background: %3;
        }
        QCheckBox::indicator:checked {
            background: %4;
            border-color: %4;
            image: none;
        }
        QCheckBox::indicator:checked:after {
            content: "✓";
        }
    )")
                                   .arg(kText, kBorder, kSurface0, kBlue);

    // 热键
    contentLayout->addWidget(makeSectionLabel("热键"));
    m_hotkeyEdit = new HotkeyEdit(content);
    m_hotkeyEdit->setKeySequence(QKeySequence(m_settings->hotkey()));
    m_hotkeyEdit->setStyleSheet(inputStyle);
    contentLayout->addWidget(m_hotkeyEdit);

    // 开机自启动
    auto *sep1 = new QFrame(content);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet(
        QString("background: %1; border: none; max-height: 1px; margin: 4px 0;")
            .arg(kBorder));
    contentLayout->addWidget(sep1);

    m_autostartCheck = new QCheckBox("开机自启动", content);
    m_autostartCheck->setChecked(m_settings->autostart());
    m_autostartCheck->setStyleSheet(checkStyle);
    contentLayout->addWidget(m_autostartCheck);

    // 插件
    if (!plugins.isEmpty()) {
        auto *sep2 = new QFrame(content);
        sep2->setFrameShape(QFrame::HLine);
        sep2->setStyleSheet(
            QString("background: %1; border: none; max-height: 1px; margin: 4px 0;")
                .arg(kBorder));
        contentLayout->addWidget(sep2);

        contentLayout->addWidget(makeSectionLabel("插件"));

        const QStringList disabled = m_settings->disabledPlugins();
        for (IPlugin *p : plugins) {
            const QString name = p->name();
            auto         *cb   = new QCheckBox(name, content);
            cb->setChecked(!disabled.contains(name));
            cb->setStyleSheet(checkStyle);
            contentLayout->addWidget(cb);
            m_pluginChecks.append(cb);
            m_pluginNames.append(name);
        }
    }

    // 网页搜索引擎优先级
    {
        auto *sep = new QFrame(content);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet(
            QString("background: %1; border: none; max-height: 1px; margin: 4px 0;")
                .arg(kBorder));
        contentLayout->addWidget(sep);

        contentLayout->addWidget(makeSectionLabel("网页搜索引擎优先级"));

        auto *row       = new QWidget(content);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        m_engineList = new QListWidget(row);
        m_engineList->setFixedHeight(112);
        m_engineList->setStyleSheet(QString(R"(
            QListWidget {
                background: %1; color: %2;
                border: 1px solid %3; border-radius: 6px;
                font-size: 13px; outline: 0;
            }
            QListWidget::item { padding: 4px 8px; }
            QListWidget::item:selected { background: %4; color: #1e1e2e; }
            QListWidget::item:hover:!selected { background: #45475a; }
        )")
                                        .arg(kSurface0, kText, kBorder, kBlue));

        // 按已保存顺序填充引擎名称，UserRole 存 id
        const QStringList      savedOrder = m_settings->webEngineOrder();
        const QList<WebEngine> allEngines = WebPlugin::allEngines();
        // 先按 savedOrder 排，再追加 savedOrder 里没有的（兼容日后新增引擎）
        QList<WebEngine> ordered;
        for (const QString &id : savedOrder)
            for (const WebEngine &e : allEngines)
                if (e.id == id) {
                    ordered.append(e);
                    break;
                }
        for (const WebEngine &e : allEngines) {
            bool found = false;
            for (const WebEngine &o : ordered)
                if (o.id == e.id) {
                    found = true;
                    break;
                }
            if (!found) ordered.append(e);
        }
        for (const WebEngine &e : ordered) {
            auto *it = new QListWidgetItem(e.name, m_engineList);
            it->setData(Qt::UserRole, e.id);
        }
        if (m_engineList->count()) m_engineList->setCurrentRow(0);
        rowLayout->addWidget(m_engineList, 1);

        const QString arrowBtn = QString(R"(
            QPushButton {
                background: %1; color: %2; font-size: 16px;
                border: 1px solid %3; border-radius: 6px;
                min-width: 32px; min-height: 32px;
            }
            QPushButton:hover { background: #45475a; }
            QPushButton:disabled { color: %3; }
        )")
                                     .arg(kSurface0, kText, kBorder);

        auto *btnCol    = new QWidget(row);
        auto *btnLayout = new QVBoxLayout(btnCol);
        btnLayout->setContentsMargins(0, 0, 0, 0);
        btnLayout->setSpacing(6);

        auto *upBtn   = new QPushButton("↑", btnCol);
        auto *downBtn = new QPushButton("↓", btnCol);
        upBtn->setStyleSheet(arrowBtn);
        downBtn->setStyleSheet(arrowBtn);
        btnLayout->addWidget(upBtn);
        btnLayout->addWidget(downBtn);
        btnLayout->addStretch();
        rowLayout->addWidget(btnCol);

        connect(upBtn, &QPushButton::clicked, this, [this] {
            const int row = m_engineList->currentRow();
            if (row <= 0) return;
            auto *item = m_engineList->takeItem(row);
            m_engineList->insertItem(row - 1, item);
            m_engineList->setCurrentRow(row - 1);
        });
        connect(downBtn, &QPushButton::clicked, this, [this] {
            const int row = m_engineList->currentRow();
            if (row < 0 || row >= m_engineList->count() - 1) return;
            auto *item = m_engineList->takeItem(row);
            m_engineList->insertItem(row + 1, item);
            m_engineList->setCurrentRow(row + 1);
        });

        contentLayout->addWidget(row);
    }

    // 文件搜索目录
    {
        auto *sep = new QFrame(content);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet(
            QString("background: %1; border: none; max-height: 1px; margin: 4px 0;")
                .arg(kBorder));
        contentLayout->addWidget(sep);

        contentLayout->addWidget(makeSectionLabel("文件搜索目录"));

        auto *row       = new QWidget(content);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        m_pathList = new QListWidget(row);
        m_pathList->setFixedHeight(112);
        m_pathList->setStyleSheet(QString(R"(
            QListWidget {
                background: %1; color: %2;
                border: 1px solid %3; border-radius: 6px;
                font-size: 13px; outline: 0;
            }
            QListWidget::item { padding: 4px 8px; }
            QListWidget::item:selected { background: %4; color: #1e1e2e; }
            QListWidget::item:hover:!selected { background: #45475a; }
        )")
                                       .arg(kSurface0, kText, kBorder, kBlue));

        for (const QString &dir : m_settings->fileSearchPaths())
            new QListWidgetItem(dir, m_pathList);
        rowLayout->addWidget(m_pathList, 1);

        const QString dirBtnStyle = QString(R"(
            QPushButton {
                background: %1; color: %2; font-size: 13px;
                border: 1px solid %3; border-radius: 6px;
                min-width: 44px; min-height: 32px;
            }
            QPushButton:hover { background: #45475a; }
        )")
                                        .arg(kSurface0, kText, kBorder);

        auto *btnCol    = new QWidget(row);
        auto *btnLayout = new QVBoxLayout(btnCol);
        btnLayout->setContentsMargins(0, 0, 0, 0);
        btnLayout->setSpacing(6);

        auto *addDirBtn    = new QPushButton("添加", btnCol);
        auto *removeDirBtn = new QPushButton("移除", btnCol);
        addDirBtn->setStyleSheet(dirBtnStyle);
        removeDirBtn->setStyleSheet(dirBtnStyle);
        btnLayout->addWidget(addDirBtn);
        btnLayout->addWidget(removeDirBtn);
        btnLayout->addStretch();
        rowLayout->addWidget(btnCol);

        connect(addDirBtn, &QPushButton::clicked, this, [this] {
            const QString dir = QFileDialog::getExistingDirectory(this, "选择目录",
                                                                   QDir::homePath());
            if (dir.isEmpty()) return;
            const QString cleaned = QDir::cleanPath(dir);
            for (int i = 0; i < m_pathList->count(); ++i)
                if (m_pathList->item(i)->text() == cleaned) return; // 已存在，不重复添加
            new QListWidgetItem(cleaned, m_pathList);
        });
        connect(removeDirBtn, &QPushButton::clicked, this, [this] {
            const int row = m_pathList->currentRow();
            if (row < 0) return;
            delete m_pathList->takeItem(row);
        });

        contentLayout->addWidget(row);
    }

    // 用 QScrollArea 包裹内容区：分区变多、总高度超过 setMaximumHeight 上限时，
    // 只让内容区内部滚动，标题栏和底部按钮始终固定可见
    auto *scrollArea = new QScrollArea(card);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QString(R"(
        QScrollArea { background: transparent; border: none; }
        QScrollBar:vertical { background: %1; width: 8px; margin: 0; }
        QScrollBar::handle:vertical { background: %2; border-radius: 4px; min-height: 24px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )")
                                    .arg(kBg, kBorder));
    scrollArea->viewport()->setStyleSheet("background: transparent;");
    scrollArea->setWidget(content);
    outer->addWidget(scrollArea, 1);

    // ── 底部按钮 ─────────────────────────────────────────────
    auto *sep3 = new QFrame(card);
    sep3->setFrameShape(QFrame::HLine);
    sep3->setStyleSheet(
        QString("background: %1; border: none; max-height: 1px;").arg(kBorder));
    outer->addWidget(sep3);

    auto *footer       = new QWidget(card);
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(20, 12, 20, 12);
    footerLayout->setSpacing(8);
    footerLayout->addStretch();

    const QString btnBase = QString(R"(
        QPushButton {
            font-size: 13px;
            border-radius: 6px;
            padding: 6px 20px;
            border: none;
        }
    )");

    auto *cancelBtn = new QPushButton("取消", footer);
    cancelBtn->setStyleSheet(btnBase + QString(R"(
        QPushButton { background: %1; color: %2; }
        QPushButton:hover { background: #45475a; }
    )")
                                           .arg(kSurface0, kText));
    footerLayout->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton("保存", footer);
    saveBtn->setStyleSheet(btnBase + QString(R"(
        QPushButton { background: %1; color: #1e1e2e; font-weight: bold; }
        QPushButton:hover { background: #7aa2f7; }
    )")
                                         .arg(kBlue));
    footerLayout->addWidget(saveBtn);

    outer->addWidget(footer);

    connect(cancelBtn, &QPushButton::clicked, this, &SettingsDialog::hide);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::save);
}

// ── 保存 ──────────────────────────────────────────────────────

void SettingsDialog::save() {
    const QString newHotkey = m_hotkeyEdit->keySequence().toString();
    if (newHotkey != m_settings->hotkey() && !newHotkey.isEmpty())
        m_settings->setHotkey(newHotkey);

    m_settings->setAutostart(m_autostartCheck->isChecked());

    QStringList disabled;
    for (int i = 0; i < m_pluginChecks.size(); ++i) {
        if (!m_pluginChecks[i]->isChecked()) disabled.append(m_pluginNames[i]);
    }
    m_settings->setDisabledPlugins(disabled);

    if (m_engineList) {
        QStringList order;
        for (int i = 0; i < m_engineList->count(); ++i)
            order.append(m_engineList->item(i)->data(Qt::UserRole).toString());
        m_settings->setWebEngineOrder(order);
    }

    if (m_pathList) {
        QStringList paths;
        for (int i = 0; i < m_pathList->count(); ++i)
            paths.append(m_pathList->item(i)->text());
        m_settings->setFileSearchPaths(paths);
    }

    m_settings->save();
    hide();
}

// ── 定位 ──────────────────────────────────────────────────────

// 每次显示时按当前实际尺寸重新定位：分区数量会随功能增长变化，构造期算出的 height()
// 在样式表/布局完全生效前并不可靠，故放到 showEvent（此时布局已定型）里做。
// 纵向默认放在屏幕 1/3 处，但钳制到可用区域内，避免小屏幕上内容被推出屏幕底部。
void SettingsDialog::showEvent(QShowEvent *e) {
    QWidget::showEvent(e);
    QScreen *scr = QGuiApplication::primaryScreen();
    if (scr) {
        const QRect geo    = scr->availableGeometry();
        const int   idealY = geo.top() + geo.height() / 3;
        const int   maxY   = geo.bottom() - height();
        const int   y      = qMax(geo.top(), qMin(idealY, maxY));
        move(geo.left() + (geo.width() - kWidth) / 2, y);
    }
}

// ── 键盘 / 拖拽 ───────────────────────────────────────────────

void SettingsDialog::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QWidget::keyPressEvent(e);
}

void SettingsDialog::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton && e->pos().y() < kTitleH)
        m_dragPos = e->globalPosition().toPoint() - frameGeometry().topLeft();
    QWidget::mousePressEvent(e);
}

void SettingsDialog::mouseMoveEvent(QMouseEvent *e) {
    if ((e->buttons() & Qt::LeftButton) && !m_dragPos.isNull())
        move(e->globalPosition().toPoint() - m_dragPos);
    QWidget::mouseMoveEvent(e);
}

void SettingsDialog::mouseReleaseEvent(QMouseEvent *e) {
    m_dragPos = {};
    QWidget::mouseReleaseEvent(e);
}
