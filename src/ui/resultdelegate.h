#pragma once
#include <QStyledItemDelegate>

// 结果项绘制：左侧图标 + 右侧两行（标题 / 路径）
class ResultDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void  paint(QPainter *p, const QStyleOptionViewItem &opt,
                const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &opt,
                   const QModelIndex &index) const override;
};
