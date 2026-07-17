#pragma once
#include <QWidget>

class TerminalTabs;

// 独立终端窗口：**无系统标题栏**（frameless）。标签栏与窗口控制键**二合一**——标签 + 拖拽空白
// + 最小化/最大化/关闭 同处一条（像 Windows Terminal）。边缘可拉伸（WM_NCHITTEST）。
// 内嵌 TerminalTabs（多标签），由 /termwin 复用同一单例。
class TerminalWindow : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWindow(QWidget *parent = nullptr);

    void startSession();                 // 转发给 tabs
    void runCommand(const QString &cmd); // 转发给 tabs

protected:
    void closeEvent(QCloseEvent *e) override;
#ifdef Q_OS_WIN
    // 无边框下用命中测试还原：拖拽空白区=可拖动(HTCAPTION)、四边=可缩放
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private:
    void toggleMaximize();

    TerminalTabs *m_tabs = nullptr;
};
