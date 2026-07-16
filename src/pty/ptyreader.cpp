#include "ptyreader.h"

#ifdef Q_OS_WIN

void PtyReader::run() {
    if (m_read == INVALID_HANDLE_VALUE)
        return;
    char  buf[8192];
    DWORD n = 0;
    while (!m_stop.load()) {
        BOOL ok = ReadFile(m_read, buf, sizeof(buf), &n, nullptr);
        if (!ok || n == 0)
            break; // 管道关闭（ConPTY 被 stop）或子进程退出
        emit bytesRead(QByteArray(buf, static_cast<int>(n)));
    }
    emit eof();
}

#else

void PtyReader::run() {}

#endif
