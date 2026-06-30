#pragma once
#include <QWidget>
#include <QPair>
#include <QStringList>

class QVBoxLayout;

// 帮助对话框，替代 QMessageBox，风格与设置页统一（Catppuccin Mocha）。
// CommandPlugin 持有单例，showHelp() 时更新命令列表再 show()。
class HelpDialog : public QWidget {
    Q_OBJECT
public:
    explicit HelpDialog(QWidget *parent = nullptr);

    // 每次展示前由 CommandPlugin 传入当前命令列表（id, desc）
    void setCommands(const QList<QPair<QString, QString>> &cmds);

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    void buildLayout();
    void rebuildCommands();

    QVBoxLayout *m_cmdLayout = nullptr;  // 命令列表区，setCommands 时重建
    QList<QPair<QString, QString>> m_cmds;

    QPoint m_dragPos;
    static constexpr int kTitleH = 44;
    static constexpr int kWidth  = 480;
};
