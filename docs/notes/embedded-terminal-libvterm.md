# 内嵌终端：为何用 libvterm，以及集成的几个坑

`/terminal` 打开的内嵌终端 = **libvterm（VT 解析/屏幕状态内核）+ ConPTY（起 shell）+ QPainter 自绘 + 键盘编码**。相关文件：`src/vt/terminalcore.*`、`src/pty/conpty.*`、`src/pty/ptyreader.*`、`src/ui/terminalview.*`、`src/ui/terminalwindow.*`、`third_party/libvterm/`。

## 为何不用 libghostty（最初的方案）

- libghostty 的**完整嵌入 API**（GPU 渲染/窗口/输入）尚未稳定，且**不支持 Windows**。
- 唯一今天可用的是 libghostty-vt 内核，但它**必须用 Zig 0.15.2 构建**，而 ghostty 的 `build.zig` 在 **Windows 主机上有构建系统 bug**（`std.Build.Step.Run.convertPathArg` 断言 panic），被迫改走 Linux 交叉编译——性价比过低。
- **改用 libvterm**（LeoNerd 的 C99 库，Neovim `:terminal` 同款）：纯 C、约 10 个 `.c` 文件、发布 tarball 已含预生成表，**用现有 MinGW 直接编**，无 Zig/无交叉编译。它同时覆盖渲染端（喂字节→读单元格）和输入端（`vterm_keyboard_*` 编码→output 回调）。

> vendored 源码务必用**发布 tarball**（`libvterm-0.3.3.tar.gz`，含 `src/encoding/*.inc` 等预生成表），不要用 raw git checkout（需 Perl 生成表）。

## 坑 1：CMake 必须启用 C 语言，否则 `.c` 被静默丢弃

- **现象**：`add_library(vterm STATIC <一堆.c>)` 编出的 `libvterm.a` 里**只有 `mocs_compilation.cpp.obj`**，链接期一片 `undefined reference to vterm_*`。`file(GLOB)` 明明匹配到了 9 个 `.c`。
- **原因**：`project(... LANGUAGES CXX)` 只启用了 CXX。CMake 不知道怎么编 `.c`，就把它们从目标里**静默丢弃**（不报错）。
- **正确做法**：`project(... LANGUAGES CXX C)`。

## 坑 2：mingw-w64 的 ConPTY 声明被 NTDDI 门控

- **现象**：`HPCON`/`CreatePseudoConsole` 报未声明，即使已 `-D_WIN32_WINNT=0x0A00`。
- **原因**：mingw-w64 把 ConPTY API 门控在 `NTDDI_VERSION >= NTDDI_WIN10_RS5`（`0x0A000006`，1809），只设 `_WIN32_WINNT` 不够。
- **正确做法**：CMake 里同时 `-DNTDDI_VERSION=0x0A000006`。另外进程属性常量的**正确宏名是 `PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE`（无 `_LIST`）**，值 `0x00020016`；`conpty.cpp` 里有兜底 `#define`。

## 坑 3：重复的信号连接 → 输入被写两次（每个键变两个）

- **现象**：终端里每敲一个字符出现两个（`x`→`xx`），shell 实际收到双倍。首次开窗正常，**关闭后重开才复现**。
- **原因**：`startSession()` 每次都把**长存的** `m_view` 的 `outputToPty` 连到写 PTY 的 lambda，`teardown()` 不断开——重开一次多一条连接，一次 `keyChar` 触发多次 `WriteFile`。
- **正确做法**：`m_view`/`m_pty` 随窗口生命周期长存的连接**只在构造函数连一次**；`startSession()` 里只连每会话**新建**的 `m_reader`（它每次 `deleteLater` 重建，天然无重复）。

## 线程模型

libvterm 的 `VTerm` 对象**非线程安全，只在 GUI 线程访问**。`PtyReader`（worker 线程）仅阻塞 `ReadFile` 并把裸字节经 `QueuedConnection` 投给 GUI 线程；output 回调（键盘编码/查询响应）在 GUI 线程内触发直接 `WriteFile`，故 VTerm 无需加锁。关窗拆解顺序：`ConPty::stop()` 关句柄解锁阻塞的 `ReadFile` → `reader->wait()` join → 再析构，否则关窗卡死。

## 现状与后续

MVP 已验证：起 pwsh、彩色渲染、可交互、resize、关闭重开无残留。后续已补齐：

- **滚动回看**：接入 `sb_pushline/popline/clear` 回调，历史行**原样存 `VTermScreenCell`**（deque 上限 5000 行，驱逐计数做全局行号基准），开启 `vterm_screen_enable_reflow`。存储结构定义在 terminalcore.cpp 内（不透明指针），维持 vterm 类型不出 .cpp 的约束。备用屏（`VTERM_PROP_ALTSCREEN`）下滚轮转发为方向键。注意：reflow 下 resize 会触发 push/pop 往返，回调蹦床里只动 deque，槽里严禁写回 vterm（重入）。
- **鼠标选择/复制粘贴**：选区端点用全局行号（驱逐计数 + 序号），滚动/驱逐下天然稳定；Ctrl+Shift+C/V 拦截必须放在 Ctrl+字母编码分支**之前**；粘贴走 `vterm_keyboard_start/end_paste`（bracketed paste 自动包裹）。
- **中文 IME**：`WA_InputMethodEnabled` + `inputMethodEvent`；候选框跟随靠 `cursorMoved` 时 `updateMicroFocus()`。

**仍未做**：Linux 端（ConPTY→forkpty）、脏区局部重绘（damage 带参但全量 update）。
