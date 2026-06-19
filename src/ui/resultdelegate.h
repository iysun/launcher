#pragma once
#include <QString>
#include <QStyledItemDelegate>

// 结果项绘制：左侧图标 + 右侧两行（标题 / 路径），命中关键字高亮
class ResultDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void  paint(QPainter *p, const QStyleOptionViewItem &opt,
                const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &opt,
                   const QModelIndex &index) const override;

    void setKeyword(const QString &kw) { m_kw = kw; }

private:
    QString m_kw;  // 当前搜索关键字，用于标题高亮
};
