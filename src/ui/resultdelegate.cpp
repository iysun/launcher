#include "ui/resultdelegate.h"
#include "plugin/resultitem.h"
#include <QPainter>

static constexpr int kPad  = 16;  // 左右边距
static constexpr int kIcon = 32;  // 图标边长
static constexpr int kGap  = 12;  // 图标与文字间距
static constexpr int kRowH = 56;  // 行高（须与 mainwindow.cpp kItemH 同步）

void ResultDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt,
                           const QModelIndex &index) const {
    const auto item = index.data(Qt::UserRole).value<ResultItem>();

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = opt.state & QStyle::State_Selected;

    // 背景（delegate 自行绘制，CSS 不再处理 item 状态）
    if (selected)
        p->fillRect(opt.rect, QColor("#313244"));
    else if (opt.state & QStyle::State_MouseOver)
        p->fillRect(opt.rect, QColor("#181825"));

    const QRect r = opt.rect.adjusted(kPad, 0, -kPad, 0);

    // 图标：垂直居中
    const QRect iconR(r.left(), r.top() + (r.height() - kIcon) / 2, kIcon, kIcon);
    item.icon.paint(p, iconR, Qt::AlignCenter);

    // 文字区
    const QRect textR = r.adjusted(kIcon + kGap, 0, 0, 0);
    const int   half  = textR.height() / 2;

    // 上行：标题（粗体）
    QFont tf = opt.font;
    tf.setPointSize(11);
    tf.setBold(true);
    p->setFont(tf);
    p->setPen(selected ? QColor("#89b4fa") : QColor("#cdd6f4"));
    const QRect titleR(textR.left(), textR.top() + 6, textR.width(), half);
    p->drawText(titleR, Qt::AlignLeft | Qt::AlignVCenter,
                p->fontMetrics().elidedText(item.title, Qt::ElideRight, titleR.width()));

    // 下行：路径（暗色小字，中间省略以保留盘符与文件名）
    QFont sf = opt.font;
    sf.setPointSize(8);
    p->setFont(sf);
    p->setPen(QColor("#6c7086"));
    const QRect subR(textR.left(), textR.top() + half, textR.width(), half - 6);
    p->drawText(subR, Qt::AlignLeft | Qt::AlignVCenter,
                p->fontMetrics().elidedText(item.subtitle, Qt::ElideMiddle, subR.width()));

    p->restore();
}

QSize ResultDelegate::sizeHint(const QStyleOptionViewItem &opt,
                               const QModelIndex &index) const {
    Q_UNUSED(opt);
    Q_UNUSED(index);
    return QSize(0, kRowH);
}
