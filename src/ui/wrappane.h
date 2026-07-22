#pragma once
#include <QWidget>
#include <memory>

class TerminalView;
class ConPty;
class PtyReader;
class QVBoxLayout;

// launcher 形态内的命令输出（REPL）：复用一个常驻后台 shell（ConPTY），命令经 stdin
// 逐条送入，TerminalView（libvterm）渲染其输出——带完整 ANSI 颜色/VT 格式，观感与内联
// 终端一致；但只读、不可交互（输入由外层搜索框驱动），每条命令前清屏只显示当条输出。
// 常驻 shell 避免了"每条命令都新起重进程"的 spawn storm（曾致 pwsh 0xC0000142）。
class WrapPane : public QWidget {
    Q_OBJECT
public:
    explicit WrapPane(QWidget *parent = nullptr);
    ~WrapPane() override;

    void run(const QString &cmd); // 清屏 + 把 cmd 送进常驻 shell（未起则先起）
    void cancel();                // teardown 常驻 shell（退出 Wrap 模式时调）
    bool isRunning() const;

signals:
    void finished();

private slots:
    void onEof(); // 常驻 shell 退出（如用户 exit）：emit finished

private:
    void startSession();    // 懒启动常驻 shell，幂等
    void teardownSession(); // 停 reader + stop pty，复位

    QVBoxLayout            *m_layout = nullptr;
    TerminalView           *m_view   = nullptr;
    std::unique_ptr<ConPty> m_pty;
    PtyReader              *m_reader  = nullptr;
    bool                    m_started = false;
};
