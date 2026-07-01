#include "ui/resultdelegate.h"
#include "core/matcher.h"
#include "plugin/resultitem.h"
#include <QPainter>
#include <QSet>

static constexpr int kPad  = 16; // 左右边距
static constexpr int kIcon = 32; // 图标边长
static constexpr int kGap  = 12; // 图标与文字间距
static constexpr int kRowH = 56; // 行高（须与 mainwindow.cpp kItemH 同步）

void ResultDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt,
                           const QModelIndex &index) const {
    const auto item = index.data(Qt::UserRole).value<ResultItem>();

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = opt.state & QStyle::State_Selected;

    // 背景（delegate 自行绘制，CSS 不再处理 item 状态）
    if (selected) p->fillRect(opt.rect, QColor("#313244"));
    else if (opt.state & QStyle::State_MouseOver)
        p->fillRect(opt.rect, QColor("#181825"));

    const QRect r = opt.rect.adjusted(kPad, 0, -kPad, 0);

    // 图标：垂直居中
    const QRect iconR(r.left(), r.top() + (r.height() - kIcon) / 2, kIcon, kIcon);
    item.icon.paint(p, iconR, Qt::AlignCenter);

    // 文字区
    const QRect textR = r.adjusted(kIcon + kGap, 0, 0, 0);
    const int   half  = textR.height() / 2;

    // 上行：标题（粗体），命中关键字高亮
    QFont tf = opt.font;
    tf.setPointSize(11);
    tf.setBold(true);
    p->setFont(tf);
    const QRect   titleR(textR.left(), textR.top() + 6, textR.width(), half);
    const QColor  base = selected ? QColor("#89b4fa") : QColor("#cdd6f4");
    const QColor  hi   = selected ? QColor("#f5e0dc") : QColor("#89b4fa");
    const QString disp =
        p->fontMetrics().elidedText(item.title, Qt::ElideRight, titleR.width());
    // 拼音命中时使用预计算的 title 字符下标（过滤超出省略范围的位置）；
    // 否则在实际绘制的 disp 上动态计算，兼容子串与子序列两种命中。
    QList<int> hits;
    if (!item.matchHighlight.isEmpty()) {
        for (int idx : item.matchHighlight)
            if (idx < disp.length()) hits.append(idx);
    } else {
        hits = Matcher::matchedPositions(disp, m_kw);
    }

    if (hits.isEmpty()) {
        p->setPen(base);
        p->drawText(titleR, Qt::AlignLeft | Qt::AlignVCenter, disp);
    } else {
        const QSet<int> hitSet(hits.begin(), hits.end());
        int             x       = titleR.left();
        auto            drawSeg = [&](const QString &s, const QColor &col) {
            if (s.isEmpty()) return;
            p->setPen(col);
            const QRect seg(x, titleR.top(), titleR.right() - x, titleR.height());
            p->drawText(seg, Qt::AlignLeft | Qt::AlignVCenter, s);
            x += p->fontMetrics().horizontalAdvance(s);
        };
        // 把 disp 切成「命中 / 非命中」交替的连续段，逐段着色
        int i = 0;
        while (i < disp.length()) {
            const bool hit = hitSet.contains(i);
            int        j   = i;
            while (j < disp.length() && hitSet.contains(j) == hit)
                ++j;
            drawSeg(disp.mid(i, j - i), hit ? hi : base);
            i = j;
        }
    }

    // 下行：路径（暗色小字，中间省略以保留盘符与文件名）
    QFont sf = opt.font;
    sf.setPointSize(8);
    p->setFont(sf);
    p->setPen(QColor("#6c7086"));
    const QRect subR(textR.left(), textR.top() + half, textR.width(), half - 6);
    p->drawText(
        subR, Qt::AlignLeft | Qt::AlignVCenter,
        p->fontMetrics().elidedText(item.subtitle, Qt::ElideMiddle, subR.width()));

    p->restore();
}

QSize ResultDelegate::sizeHint(const QStyleOptionViewItem &opt,
                               const QModelIndex          &index) const {
    Q_UNUSED(opt);
    Q_UNUSED(index);
    return QSize(0, kRowH);
}
