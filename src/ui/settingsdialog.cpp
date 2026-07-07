#include "settingsdialog.h"
#include "core/appsettings.h"
#include "plugins/webplugin.h"
#include "ui/hotkeyedit.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QUuid>
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
    setupUi(plugins);       // 只建结构：控件/按钮/信号连接，不填当前值
    syncFormFromSettings(); // 首次按当前设置填值

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

// 供 setupUi() 里的 HotkeyEdit 和 promptForWebEngine() 里的 QLineEdit 共用
static QString lineEditStyle() {
    return QString(R"(
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
            image: url(:/icons/check.png);
        }
    )")
                                   .arg(kText, kBorder, kSurface0, kBlue);

    // 热键
    contentLayout->addWidget(makeSectionLabel("热键"));
    m_hotkeyEdit = new HotkeyEdit(content);
    m_hotkeyEdit->setStyleSheet(lineEditStyle());
    contentLayout->addWidget(m_hotkeyEdit);

    // 开机自启动
    auto *sep1 = new QFrame(content);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet(
        QString("background: %1; border: none; max-height: 1px; margin: 4px 0;")
            .arg(kBorder));
    contentLayout->addWidget(sep1);

    m_autostartCheck = new QCheckBox("开机自启动", content);
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

        for (IPlugin *p : plugins) {
            const QString name = p->name();
            auto         *cb   = new QCheckBox(name, content);
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

        rowLayout->addWidget(m_engineList, 1);

        // ↑ ↓ 排序，添加/移除自定义引擎；四个按钮统一样式，避免列宽参差
        const QString engineBtnStyle = QString(R"(
            QPushButton {
                background: %1; color: %2; font-size: 13px;
                border: 1px solid %3; border-radius: 6px;
                min-width: 44px; min-height: 32px;
            }
            QPushButton:hover:!disabled { background: #45475a; }
            QPushButton:disabled { color: %3; }
        )")
                                          .arg(kSurface0, kText, kBorder);

        auto *btnCol    = new QWidget(row);
        auto *btnLayout = new QVBoxLayout(btnCol);
        btnLayout->setContentsMargins(0, 0, 0, 0);
        btnLayout->setSpacing(6);

        auto *upBtn           = new QPushButton("↑", btnCol);
        auto *downBtn         = new QPushButton("↓", btnCol);
        auto *addEngineBtn    = new QPushButton("添加", btnCol);
        auto *removeEngineBtn = new QPushButton("移除", btnCol);
        upBtn->setStyleSheet(engineBtnStyle);
        downBtn->setStyleSheet(engineBtnStyle);
        addEngineBtn->setStyleSheet(engineBtnStyle);
        removeEngineBtn->setStyleSheet(engineBtnStyle);
        btnLayout->addWidget(upBtn);
        btnLayout->addWidget(downBtn);
        btnLayout->addWidget(addEngineBtn);
        btnLayout->addWidget(removeEngineBtn);
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
        connect(addEngineBtn, &QPushButton::clicked, this, [this] {
            if (const std::optional<WebEngine> e = promptForWebEngine()) {
                auto *it = new QListWidgetItem(e->name, m_engineList);
                it->setData(Qt::UserRole, e->id);
                it->setData(Qt::UserRole + 1, e->urlTemplate);
                m_engineList->setCurrentItem(it);
            }
        });
        connect(removeEngineBtn, &QPushButton::clicked, this, [this] {
            const int row = m_engineList->currentRow();
            if (row < 0) return;
            const QString id = m_engineList->item(row)->data(Qt::UserRole).toString();
            if (!id.startsWith("custom:")) return; // 内置引擎不可删除
            delete m_engineList->takeItem(row);
        });
        // "移除"只对自定义引擎生效，选中内置引擎时禁用，避免点了没反应却不知道why
        auto updateRemoveEngineEnabled = [this, removeEngineBtn] {
            const int  row = m_engineList->currentRow();
            const bool removable =
                row >= 0 &&
                m_engineList->item(row)->data(Qt::UserRole).toString().startsWith("custom:");
            removeEngineBtn->setEnabled(removable);
        };
        connect(m_engineList, &QListWidget::currentRowChanged, this, updateRemoveEngineEnabled);
        updateRemoveEngineEnabled();

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

    const QString btnBase = QString(R"(
        QPushButton {
            font-size: 13px;
            border-radius: 6px;
            padding: 6px 20px;
            border: none;
        }
    )");

    auto *resetBtn = new QPushButton("重置", footer);
    resetBtn->setStyleSheet(btnBase + QString(R"(
        QPushButton { background: transparent; color: %1; border: 1px solid %2; }
        QPushButton:hover { background: %3; }
    )")
                                          .arg(kOverlay0, kBorder, kSurface0));
    footerLayout->addWidget(resetBtn);

    footerLayout->addStretch();

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

    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::resetToDefaults);
    connect(cancelBtn, &QPushButton::clicked, this, &SettingsDialog::hide);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::save);
}

// ── 填值 / 重置 ───────────────────────────────────────────────

// 把表单所有字段从 m_settings 灌回控件；构造时（首次）和 showEvent（每次重新显示前）都会调用，
// 确保「取消」在任何时候都是真正无副作用的操作，不会把上一次未保存的编辑残留到下次打开
void SettingsDialog::syncFormFromSettings() {
    m_hotkeyEdit->setKeySequence(QKeySequence(m_settings->hotkey()));
    m_autostartCheck->setChecked(m_settings->autostart());

    const QStringList disabled = m_settings->disabledPlugins();
    for (int i = 0; i < m_pluginChecks.size(); ++i)
        m_pluginChecks[i]->setChecked(!disabled.contains(m_pluginNames[i]));

    if (m_engineList)
        populateEngineList(m_settings->webEngineOrder(), WebPlugin::allEngines(m_settings));
    if (m_pathList) populatePathList(m_settings->fileSearchPaths());
}

// 表单恢复出厂默认；不碰 m_settings/磁盘，用户仍需点"保存"才会落盘
void SettingsDialog::resetToDefaults() {
    m_hotkeyEdit->setKeySequence(QKeySequence(AppSettings::defaultHotkey()));
    m_autostartCheck->setChecked(AppSettings::defaultAutostart());

    for (QCheckBox *cb : m_pluginChecks) cb->setChecked(true); // 默认插件全部启用

    if (m_engineList)
        populateEngineList(AppSettings::defaultWebEngineOrder(), WebPlugin::allEngines());
    if (m_pathList) populatePathList(AppSettings::defaultFileSearchPaths());
}

// 按 order 排、追加 available 里 order 没有的（兼容日后新增内置引擎），
// 每项 UserRole 存 id、UserRole+1 存 urlTemplate（供 save() 识别/收集自定义引擎）
void SettingsDialog::populateEngineList(const QStringList      &order,
                                        const QList<WebEngine> &available) {
    m_engineList->clear();
    QList<WebEngine> ordered;
    for (const QString &id : order)
        for (const WebEngine &e : available)
            if (e.id == id) {
                ordered.append(e);
                break;
            }
    for (const WebEngine &e : available) {
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
        it->setData(Qt::UserRole + 1, e.urlTemplate);
    }
    if (m_engineList->count()) m_engineList->setCurrentRow(0);
}

void SettingsDialog::populatePathList(const QStringList &paths) {
    m_pathList->clear();
    for (const QString &p : paths)
        new QListWidgetItem(p, m_pathList);
}

// 添加自定义网页搜索引擎的小表单：用普通 QDialog（保留系统标题栏），只做深色配色，
// 不叠加 Frameless + 半透明——那套组合在模态子对话框上曾经导致窗口完全不可见，
// 不值得为一个低频小表单冒这个风险
std::optional<WebEngine> SettingsDialog::promptForWebEngine() {
    QDialog dlg(this);
    dlg.setWindowTitle("添加网页搜索引擎");
    dlg.setStyleSheet(QString("QDialog { background: %1; }").arg(kBg));
    dlg.setFixedWidth(320);

    auto *cardLayout = new QVBoxLayout(&dlg);
    cardLayout->setContentsMargins(20, 16, 20, 16);
    cardLayout->setSpacing(8);

    cardLayout->addWidget(makeSectionLabel("名称"));
    auto *nameEdit = new QLineEdit(&dlg);
    nameEdit->setStyleSheet(lineEditStyle());
    cardLayout->addWidget(nameEdit);

    cardLayout->addWidget(makeSectionLabel("URL 模板（用 %1 占位查询词）"));
    auto *urlEdit = new QLineEdit(&dlg);
    urlEdit->setPlaceholderText("https://example.com/search?q=%1");
    urlEdit->setStyleSheet(lineEditStyle());
    cardLayout->addWidget(urlEdit);

    auto *errorLbl = new QLabel(&dlg);
    errorLbl->setStyleSheet("color: #f38ba8; font-size: 12px;");
    errorLbl->hide();
    cardLayout->addWidget(errorLbl);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();

    const QString btnBase = QString(R"(
        QPushButton {
            font-size: 13px;
            border-radius: 6px;
            padding: 6px 20px;
            border: none;
        }
    )");
    auto         *cancelBtn = new QPushButton("取消", &dlg);
    cancelBtn->setStyleSheet(btnBase + QString(R"(
        QPushButton { background: %1; color: %2; }
        QPushButton:hover { background: #45475a; }
    )")
                                           .arg(kSurface0, kText));
    auto *okBtn = new QPushButton("确定", &dlg);
    okBtn->setStyleSheet(btnBase + QString(R"(
        QPushButton { background: %1; color: #1e1e2e; font-weight: bold; }
        QPushButton:hover { background: #7aa2f7; }
    )")
                                       .arg(kBlue));
    okBtn->setDefault(true);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    cardLayout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dlg, [&] {
        if (nameEdit->text().trimmed().isEmpty()) {
            errorLbl->setText("请输入名称");
            errorLbl->show();
            return;
        }
        if (!urlEdit->text().contains("%1")) {
            errorLbl->setText("URL 模板必须包含 %1 占位符");
            errorLbl->show();
            return;
        }
        dlg.accept();
    });

    if (dlg.exec() != QDialog::Accepted) return std::nullopt;

    WebEngine e;
    e.name        = nameEdit->text().trimmed();
    e.urlTemplate = urlEdit->text().trimmed();
    e.id          = "custom:" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    return e;
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
        QStringList      order;
        QList<WebEngine> customEngines;
        for (int i = 0; i < m_engineList->count(); ++i) {
            auto         *item = m_engineList->item(i);
            const QString id   = item->data(Qt::UserRole).toString();
            order.append(id);
            if (id.startsWith("custom:")) {
                customEngines.append(
                    {id, item->text(), item->data(Qt::UserRole + 1).toString()});
            }
        }
        m_settings->setWebEngineOrder(order);
        m_settings->setCustomWebEngines(customEngines);
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
    syncFormFromSettings(); // 每次显示前用真实设置刷新表单，避免"取消"过的编辑残留到下次打开
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
