#include "settingsdialog.h"
#include "core/appsettings.h"
#include "ui/hotkeyedit.h"
#include <QApplication>
#include <QCheckBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

// ── 颜色常量（Catppuccin Mocha） ──────────────────────────────
static const char *kBg        = "#1e1e2e";
static const char *kSurface0  = "#313244";
static const char *kOverlay0  = "#6c7086";
static const char *kText      = "#cdd6f4";
static const char *kBlue      = "#89b4fa";
static const char *kBorder    = "#45475a";

SettingsDialog::SettingsDialog(AppSettings *settings, const QList<IPlugin *> &plugins)
    : QWidget(nullptr), m_settings(settings) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(kWidth);
    setupUi(plugins);

    // 屏幕居中
    QScreen *scr = QGuiApplication::primaryScreen();
    if (scr) {
        const QRect geo = scr->availableGeometry();
        move(geo.left() + (geo.width() - kWidth) / 2, geo.top() + geo.height() / 3);
    }
}

// ── UI 构建 ───────────────────────────────────────────────────

QLabel *SettingsDialog::makeSectionLabel(const QString &text) {
    auto *lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; "
                               "padding: 0; margin: 0;").arg(kOverlay0));
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
    )").arg(kBg, kBorder));
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
    titleLbl->setStyleSheet(QString("color: %1; font-size: 15px; font-weight: bold;").arg(kText));
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
    )").arg(kOverlay0, kSurface0));
    titleLayout->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, this, &SettingsDialog::hide);

    outer->addWidget(titleBar);

    // ── 分隔线 ───────────────────────────────────────────────
    auto *sep0 = new QFrame(card);
    sep0->setFrameShape(QFrame::HLine);
    sep0->setStyleSheet(QString("background: %1; border: none; max-height: 1px;").arg(kBorder));
    outer->addWidget(sep0);

    // ── 内容区 ───────────────────────────────────────────────
    auto *content = new QWidget(card);
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
    )").arg(kSurface0, kText, kBorder, kBlue);

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
    )").arg(kText, kBorder, kSurface0, kBlue);

    // 热键
    contentLayout->addWidget(makeSectionLabel("热键"));
    m_hotkeyEdit = new HotkeyEdit(content);
    m_hotkeyEdit->setKeySequence(QKeySequence(m_settings->hotkey()));
    m_hotkeyEdit->setStyleSheet(inputStyle);
    contentLayout->addWidget(m_hotkeyEdit);

    // 开机自启动
    auto *sep1 = new QFrame(content);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet(QString("background: %1; border: none; max-height: 1px; margin: 4px 0;").arg(kBorder));
    contentLayout->addWidget(sep1);

    m_autostartCheck = new QCheckBox("开机自启动", content);
    m_autostartCheck->setChecked(m_settings->autostart());
    m_autostartCheck->setStyleSheet(checkStyle);
    contentLayout->addWidget(m_autostartCheck);

    // 插件
    if (!plugins.isEmpty()) {
        auto *sep2 = new QFrame(content);
        sep2->setFrameShape(QFrame::HLine);
        sep2->setStyleSheet(QString("background: %1; border: none; max-height: 1px; margin: 4px 0;").arg(kBorder));
        contentLayout->addWidget(sep2);

        contentLayout->addWidget(makeSectionLabel("插件"));

        const QStringList disabled = m_settings->disabledPlugins();
        for (IPlugin *p : plugins) {
            const QString name = p->name();
            auto *cb = new QCheckBox(name, content);
            cb->setChecked(!disabled.contains(name));
            cb->setStyleSheet(checkStyle);
            contentLayout->addWidget(cb);
            m_pluginChecks.append(cb);
            m_pluginNames.append(name);
        }
    }

    outer->addWidget(content);

    // ── 底部按钮 ─────────────────────────────────────────────
    auto *sep3 = new QFrame(card);
    sep3->setFrameShape(QFrame::HLine);
    sep3->setStyleSheet(QString("background: %1; border: none; max-height: 1px;").arg(kBorder));
    outer->addWidget(sep3);

    auto *footer = new QWidget(card);
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
    )").arg(kSurface0, kText));
    footerLayout->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton("保存", footer);
    saveBtn->setStyleSheet(btnBase + QString(R"(
        QPushButton { background: %1; color: #1e1e2e; font-weight: bold; }
        QPushButton:hover { background: #7aa2f7; }
    )").arg(kBlue));
    footerLayout->addWidget(saveBtn);

    outer->addWidget(footer);

    connect(cancelBtn, &QPushButton::clicked, this, &SettingsDialog::hide);
    connect(saveBtn,   &QPushButton::clicked, this, &SettingsDialog::save);
}

// ── 保存 ──────────────────────────────────────────────────────

void SettingsDialog::save() {
    const QString newHotkey = m_hotkeyEdit->keySequence().toString();
    if (newHotkey != m_settings->hotkey() && !newHotkey.isEmpty())
        m_settings->setHotkey(newHotkey);

    m_settings->setAutostart(m_autostartCheck->isChecked());

    QStringList disabled;
    for (int i = 0; i < m_pluginChecks.size(); ++i) {
        if (!m_pluginChecks[i]->isChecked())
            disabled.append(m_pluginNames[i]);
    }
    m_settings->setDisabledPlugins(disabled);

    m_settings->save();
    hide();
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
