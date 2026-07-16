#include "conpty.h"

#ifdef Q_OS_WIN

#include <vector>

// 某些较老的 mingw-w64 即便设了 NTDDI_VERSION 也可能不声明这个属性常量，兜底一下。
// 注意正确宏名是 ..._PSEUDOCONSOLE（无 _LIST），值为 0x00020016。
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

ConPty::~ConPty() { stop(); }

bool ConPty::start(const QString &cmdline, short cols, short rows) {
    if (m_started)
        return false;

    // 输入管道：m_inRead 交给 ConPTY，m_inWrite 我们写
    // 输出管道：m_outWrite 交给 ConPTY，m_outRead 我们读
    if (!CreatePipe(&m_inRead, &m_inWrite, nullptr, 0))
        return false;
    if (!CreatePipe(&m_outRead, &m_outWrite, nullptr, 0)) {
        closeHandles();
        return false;
    }

    COORD size;
    size.X = cols > 0 ? cols : 80;
    size.Y = rows > 0 ? rows : 24;
    HRESULT hr = CreatePseudoConsole(size, m_inRead, m_outWrite, 0, &m_hpc);
    if (FAILED(hr)) {
        closeHandles();
        return false;
    }

    // 构造带伪控制台属性的 STARTUPINFOEX
    STARTUPINFOEXW si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);

    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    std::vector<char> attrBuf(attrSize);
    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrBuf.data());
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrSize)) {
        closeHandles();
        return false;
    }
    if (!UpdateProcThreadAttribute(si.lpAttributeList, 0,
                                   PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, m_hpc,
                                   sizeof(m_hpc), nullptr, nullptr)) {
        DeleteProcThreadAttributeList(si.lpAttributeList);
        closeHandles();
        return false;
    }

    std::wstring cmd = cmdline.toStdWString();
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE,
                             EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                             &si.StartupInfo, &m_pi);
    DeleteProcThreadAttributeList(si.lpAttributeList);

    if (!ok) {
        closeHandles();
        return false;
    }

    // 父进程不再需要 ConPTY 侧的两个句柄；关掉它们，子进程退出时 m_outRead 才能读到 EOF
    CloseHandle(m_inRead);
    m_inRead = INVALID_HANDLE_VALUE;
    CloseHandle(m_outWrite);
    m_outWrite = INVALID_HANDLE_VALUE;

    m_started = true;
    return true;
}

bool ConPty::write(const char *data, int len) {
    if (!m_started || m_inWrite == INVALID_HANDLE_VALUE || len <= 0)
        return false;
    DWORD written = 0;
    return WriteFile(m_inWrite, data, static_cast<DWORD>(len), &written, nullptr) != 0;
}

void ConPty::resize(short cols, short rows) {
    if (!m_started || !m_hpc)
        return;
    COORD size;
    size.X = cols > 0 ? cols : 1;
    size.Y = rows > 0 ? rows : 1;
    ResizePseudoConsole(m_hpc, size);
}

bool ConPty::isRunning() const {
    if (!m_started || m_pi.hProcess == nullptr)
        return false;
    return WaitForSingleObject(m_pi.hProcess, 0) == WAIT_TIMEOUT;
}

void ConPty::stop() {
    if (m_hpc) {
        // 关闭伪控制台会让阻塞在 m_outRead 上的 ReadFile 返回，从而解锁 reader 线程
        ClosePseudoConsole(m_hpc);
        m_hpc = nullptr;
    }
    closeHandles();
    if (m_pi.hProcess) {
        // 若子进程还没退，给它一个了断
        if (WaitForSingleObject(m_pi.hProcess, 0) == WAIT_TIMEOUT)
            TerminateProcess(m_pi.hProcess, 0);
        CloseHandle(m_pi.hProcess);
        m_pi.hProcess = nullptr;
    }
    if (m_pi.hThread) {
        CloseHandle(m_pi.hThread);
        m_pi.hThread = nullptr;
    }
    m_started = false;
}

void ConPty::closeHandles() {
    auto close = [](HANDLE &h) {
        if (h != INVALID_HANDLE_VALUE && h != nullptr) {
            CloseHandle(h);
            h = INVALID_HANDLE_VALUE;
        }
    };
    close(m_inWrite);
    close(m_inRead);
    close(m_outRead);
    close(m_outWrite);
}

#else // !Q_OS_WIN — 非 Windows 暂未实现（Linux forkpty 后续里程碑）

ConPty::~ConPty() {}
bool ConPty::start(const QString &, short, short) { return false; }
bool ConPty::write(const char *, int) { return false; }
void ConPty::resize(short, short) {}
bool ConPty::isRunning() const { return false; }
void ConPty::stop() {}

#endif
