#pragma once
#include <QString>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Windows ConPTY 封装（不依赖 Qt 事件循环）：创建伪控制台 + 输入/输出管道，
// 用 STARTUPINFOEX + PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE_LIST 启动 shell。
// 输出管道句柄交给 PtyReader 在 worker 线程阻塞读；write() 写 shell stdin；
// resize() 调 ResizePseudoConsole；stop()/析构关句柄并终止子进程。
// 非 Windows 平台暂为 stub（Linux forkpty 后续里程碑再补）。
class ConPty {
public:
    ConPty() = default;
    ~ConPty();

    ConPty(const ConPty &)            = delete;
    ConPty &operator=(const ConPty &) = delete;

    // cmdline: 要启动的 shell 命令行（如 "pwsh.exe"）；cols/rows: 初始尺寸
    bool start(const QString &cmdline, short cols, short rows);
    bool write(const char *data, int len);
    void resize(short cols, short rows);
    void stop();
    bool isRunning() const;

#ifdef Q_OS_WIN
    HANDLE readHandle() const { return m_outRead; } // PtyReader 读这个
    HANDLE processHandle() const { return m_pi.hProcess; }
#endif

private:
#ifdef Q_OS_WIN
    void closeHandles();

    HPCON  m_hpc      = nullptr;
    HANDLE m_inWrite  = INVALID_HANDLE_VALUE; // 我们写 → shell stdin
    HANDLE m_inRead   = INVALID_HANDLE_VALUE; // 交给 ConPTY
    HANDLE m_outRead  = INVALID_HANDLE_VALUE; // 我们读 ← shell stdout
    HANDLE m_outWrite = INVALID_HANDLE_VALUE; // 交给 ConPTY
    PROCESS_INFORMATION m_pi{};
    bool                m_started = false;
#endif
};
