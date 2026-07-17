#include "helpdialog.h"
#include "core/i18n.h"
#include "core/theme.h"
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

// ── 辅助：小节标题 ────────────────────────────────────────────────
static QLabel *sectionLabel(const QString &text) {
    auto *lbl = new QLabel(text);
    lbl->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold;").arg(Theme::c("overlay")));
    return lbl;
}

// 辅助：单行 key + desc（固定宽度左列，右列自然延伸）
static QWidget *row(const QString &key, const QString &desc, QWidget *parent) {
    auto *w  = new QWidget(parent);
    auto *hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(12);

    auto *keyLbl = new QLabel(key, w);
    keyLbl->setFixedWidth(130);
    keyLbl->setStyleSheet(QString("color: %1; font-size: 13px;").arg(Theme::c("accent")));

    auto *descLbl = new QLabel(desc, w);
    descLbl->setStyleSheet(QString("color: %1; font-size: 13px;").arg(Theme::c("text")));
    descLbl->setWordWrap(true);

    hl->addWidget(keyLbl);
    hl->addWidget(descLbl, 1);
    return w;
}

static QFrame *separator(QWidget *parent) {
    auto *sep = new QFrame(parent);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(
        QString("background: %1; border: none; max-height: 1px;").arg(Theme::c("border")));
    return sep;
}

// ── 构建界面 ──────────────────────────────────────────────────────

HelpDialog::HelpDialog(QWidget *parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(kWidth);
    buildLayout();

    QScreen *scr = QGuiApplication::primaryScreen();
    if (scr) {
        const QRect geo = scr->availableGeometry();
        move(geo.left() + (geo.width() - kWidth) / 2, geo.top() + geo.height() / 3);
    }
}

void HelpDialog::buildLayout() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *card = new QFrame(this);
    card->setObjectName("helpCard");
    card->setFixedWidth(kWidth);
    card->setStyleSheet(QString(R"(
        QFrame#helpCard {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }
    )")
                            .arg(Theme::c("bg"), Theme::c("border")));
    root->addWidget(card);

    auto *outer = new QVBoxLayout(card);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── 标题栏 ───────────────────────────────────────────────
    auto *titleBar = new QWidget(card);
    titleBar->setFixedHeight(kTitleH);
    titleBar->setStyleSheet("background: transparent;");
    auto *tl = new QHBoxLayout(titleBar);
    tl->setContentsMargins(16, 0, 12, 0);

    auto *titleLbl = new QLabel(I18n::t("help.title"), titleBar);
    titleLbl->setStyleSheet(
        QString("color: %1; font-size: 15px; font-weight: bold;").arg(Theme::c("text")));
    tl->addWidget(titleLbl);
    tl->addStretch();

    auto *closeBtn = new QPushButton("×", titleBar);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setStyleSheet(QString(R"(
        QPushButton { background: transparent; color: %1; font-size: 18px; border: none; border-radius: 4px; }
        QPushButton:hover { background: %2; }
    )")
                                .arg(Theme::c("overlay"), Theme::c("surface")));
    tl->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, this, &HelpDialog::hide);
    outer->addWidget(titleBar);
    outer->addWidget(separator(card));

    // ── 内容区 ───────────────────────────────────────────────
    auto *content = new QWidget(card);
    auto *cl      = new QVBoxLayout(content);
    cl->setContentsMargins(20, 14, 20, 14);
    cl->setSpacing(6);

    // 快捷键
    cl->addWidget(sectionLabel(I18n::t("help.shortcuts")));
    cl->addWidget(row("Alt+Space", I18n::t("help.toggle"), content));
    cl->addWidget(row("Enter", I18n::t("help.execute"), content));
    cl->addWidget(row("Ctrl+Enter", I18n::t("help.altAction"), content));
    cl->addWidget(row("Esc / Ctrl+G", I18n::t("help.close"), content));
    cl->addWidget(row("↑ ↓ / Ctrl+P/N", I18n::t("help.moveSelection"), content));

    cl->addWidget(separator(content));

    // 前缀
    cl->addWidget(sectionLabel(I18n::t("help.prefixesLabel")));
    cl->addWidget(row(I18n::t("help.noPrefix"), I18n::t("help.appSearch"), content));
    cl->addWidget(row("/", I18n::t("help.commandDesc"), content));
    cl->addWidget(row("@", I18n::t("help.fileSearch"), content));
    cl->addWidget(row("?", I18n::t("help.webSearch"), content));
    cl->addWidget(row(":", I18n::t("help.runCommand"), content));

    cl->addWidget(separator(content));

    // 命令列表（动态，由 setCommands 填入）
    cl->addWidget(sectionLabel(I18n::t("help.commandsLabel")));
    auto *cmdContainer = new QWidget(content);
    m_cmdLayout        = new QVBoxLayout(cmdContainer);
    m_cmdLayout->setContentsMargins(0, 0, 0, 0);
    m_cmdLayout->setSpacing(4);
    cl->addWidget(cmdContainer);

    outer->addWidget(content);
    outer->addWidget(separator(card));

    // ── 底部按钮 ─────────────────────────────────────────────
    auto *footer = new QWidget(card);
    auto *fl     = new QHBoxLayout(footer);
    fl->setContentsMargins(20, 10, 20, 10);
    fl->addStretch();

    auto *closeBtn2 = new QPushButton(I18n::t("help.close"), footer);
    closeBtn2->setFixedWidth(80);
    closeBtn2->setStyleSheet(QString(R"(
        QPushButton { background: %1; color: %2; font-size: 13px; border: none; border-radius: 6px; padding: 6px 0; }
        QPushButton:hover { background: %3; }
    )")
                                 .arg(Theme::c("surface"), Theme::c("text"), Theme::c("border")));
    fl->addWidget(closeBtn2);
    connect(closeBtn2, &QPushButton::clicked, this, &HelpDialog::hide);

    outer->addWidget(footer);
}

void HelpDialog::setCommands(const QList<QPair<QString, QString>> &cmds) {
    m_cmds = cmds;
    rebuildCommands();
}

void HelpDialog::rebuildCommands() {
    // 清空旧命令行
    while (m_cmdLayout->count()) {
        auto *item = m_cmdLayout->takeAt(0);
        delete item->widget();
        delete item;
    }
    for (const auto &c : m_cmds) {
        auto *w = row("/" + c.first, c.second, m_cmdLayout->parentWidget());
        m_cmdLayout->addWidget(w);
    }
    adjustSize();
}

// ── 键盘 / 拖拽 ───────────────────────────────────────────────────

void HelpDialog::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QWidget::keyPressEvent(e);
}

void HelpDialog::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton && e->pos().y() < kTitleH)
        m_dragPos = e->globalPosition().toPoint() - frameGeometry().topLeft();
    QWidget::mousePressEvent(e);
}

void HelpDialog::mouseMoveEvent(QMouseEvent *e) {
    if ((e->buttons() & Qt::LeftButton) && !m_dragPos.isNull())
        move(e->globalPosition().toPoint() - m_dragPos);
    QWidget::mouseMoveEvent(e);
}

void HelpDialog::mouseReleaseEvent(QMouseEvent *e) {
    m_dragPos = {};
    QWidget::mouseReleaseEvent(e);
}
