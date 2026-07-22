# 跨平台方案与待办（Windows / Linux / macOS）

本文件是 launcher 的**跨平台移植路线图 + 待办清单**，供后续（含 AI 辅助）在 Linux/macOS 上启用本项目时按图施工。现状结论：**Windows 完整；Linux 能编能跑但功能残缺；macOS 当前无构建入口**。项目定位见 `README.md` / `AGENTS.md`（「优先支持 Windows 和 Linux」）。

> 用法：让 AI 补某平台代码时，把「目标平台 + 下面对应小节」一起给它；每完成一项勾掉 checkbox，并按 `AGENTS.md` 维护约定同步 `docs/features.md` / `docs/notes.md`。

## 0. 现状矩阵

| 子系统 | Windows | Linux | macOS | 代码位置 |
|--------|---------|-------|-------|----------|
| 核心搜索/匹配/拼音/frecency/主题/i18n | ✅ | ✅ | ✅ | `src/core/*`（纯 Qt，无需改） |
| 文件路径 | ✅ | ✅ | ✅ | 全走 `QStandardPaths`（无需改） |
| 文件/网页/命令插件 | ✅ | ✅ | ✅ | `src/plugins/{file,web,command}plugin.cpp`（无需改） |
| 应用发现 | ✅ `.lnk`+COM | ✅ `.desktop` | ❌ 无 `.app` | `src/plugins/appplugin.cpp` |
| 全局热键（唤起） | ✅ | ⚠️ 仅 X11 | ⚠️ Carbon 后端未验证 | QHotkey 子模块 |
| 热键录制控件 | ✅ 低级钩子 | ⚠️ 退化 | ⚠️ 退化 | `src/ui/hotkeyedit.cpp` |
| 开机自启 | ✅ 注册表 | ❌ 空操作 | ❌ 空操作 | `src/core/appsettings.cpp` |
| 内嵌终端 / PTY | ✅ ConPTY | ❌ stub | ❌ stub | `src/pty/conpty.cpp`、`ptyreader.cpp` |
| 终端 shell | ✅ pwsh/cmd | ❌ 无分支 | ❌ 无分支 | `src/ui/{wrappane,terminalpane}.cpp` |
| 构建 preset | ✅ windows | ✅ linux | ❌ 无 | `CMakePresets.json`、`Makefile` |
| 打包 | ✅ windeployqt | ❌ 无 | ❌ 无 | — |
| CI | ❌ 无 | ❌ 无 | ❌ 无 | 无 `.github/` |

## 1. 移植约定（先读）

- **平台隔离统一用预处理分支**：`#ifdef Q_OS_WIN` / `#elif defined(Q_OS_MAC)` / `#else`(Linux/其它 Unix)。现状代码只有 `Q_OS_WIN` vs `#else`，把 macOS 和 Linux 混为一谈——**新增 macOS 逻辑时务必用 `Q_OS_MAC` 单列**，否则 mac 会误落进 Linux 分支（应用发现就是这个坑）。
- **优先 Qt 抽象 / `QStandardPaths`**，不要硬编码路径与分隔符。参考 `src/plugins/fileplugin.cpp`。
- **CMake 平台条件**：`if(WIN32)` / `if(APPLE)` / `if(UNIX AND NOT APPLE)`。pty/vt 源码在所有平台都参与编译，靠 `.cpp` 内 `#ifdef` 兜底 stub（见 `CMakeLists.txt`）。
- **测试**：现有 `ctest`（`matcher_test`/`core_test`/`vt_test`）跨平台可跑，是回归底座；新增 PTY 后建议加一个 echo→读回的 smoke test。

## 2. 待办（按子系统，含实现指引）

### 2.1 PTY 终端后端（Linux/macOS）—— 优先级最高，解锁终端全家桶
非 Windows 下 `/terminal`、`:cmd`（wrap 输出）、`::`（交互终端）全部失效：`ConPty::start()` 在 `#else` 直接 `return false`，`PtyReader` 接线又整段包在 `#ifdef Q_OS_WIN` 内。

- [ ] `src/pty/conpty.h`：在 `#else` 分支加 POSIX 成员（`int m_master = -1;`、`pid_t m_child = -1;`）与访问器（`int masterFd() const`）。类名 ConPty 对 POSIX 有误导，可保留类名只换实现，或抽 `IPty` 接口 + 平台实现文件（推荐后者，更清晰）。
- [ ] `src/pty/conpty.cpp` 的 `#else`：用 `forkpty()`（Linux `<pty.h>` / macOS `<util.h>`）或 `openpty()`+`fork()` 实现：
  - 子进程：`setsid()` → `execvp(shell, argv)`；父进程留 master fd。
  - 初始窗口大小 `struct winsize`（rows/cols）→ `openpty` 第 4 参或 `ioctl(TIOCSWINSZ)`。
  - `write()` → 写 master fd；`resize()` → `ioctl(m_master, TIOCSWINSZ, &ws)`（内核会自动发 `SIGWINCH`）；`stop()` → `kill` 子进程 + `close(m_master)`。
- [ ] `src/pty/ptyreader.{h,cpp}`：`#else` 用阻塞 `read(m_master, buf, n)` 循环，替换 `ReadFile`；`eof` 判定为 `read<=0`。头文件加非 Windows 的 `int m_fd` + `setReadFd()`。
- [ ] `src/ui/terminalpane.cpp` / `src/ui/wrappane.cpp`：把 `#ifdef Q_OS_WIN` 包裹的 `PtyReader` 接线加上 `#else`（或去掉平台门控，改用统一的 fd/handle 抽象）。
- [ ] `CMakeLists.txt`：`if(UNIX AND NOT APPLE) target_link_libraries(launcher PRIVATE util) endif()`（glibc `forkpty` 在 `libutil`；macOS 在 libSystem，无需 `-lutil`）。

### 2.2 终端 shell 选择（POSIX）
两个几乎相同的 `shellCommandLine()` 目前只产出 pwsh/powershell/cmd。

- [ ] `src/ui/wrappane.cpp` + `src/ui/terminalpane.cpp`：加 `#else` 分支——用 `qEnvironmentVariable("SHELL")`，回退 `/bin/bash` → `/bin/sh`。
- [ ] pwsh 专属的 `$PSStyle.OutputRendering = 'Ansi'`（强制上色）**仅 Windows 需要**：POSIX 下真实 pty 会让 `isatty()` 为真，`ls --color`/程序自动上色，无需强制。
- [ ] cwd→标签名：Windows 靠注入 pwsh prompt 钩子把窗口标题设成 cwd；POSIX 上改用 `PROMPT_COMMAND`(bash)/`precmd`(zsh) 发 OSC 7 或 OSC 0 标题，或直接依赖 shell/终端既有标题机制。

### 2.3 开机自启（Linux/macOS）
`src/core/appsettings.cpp::applyAutostart()` 目前 `#else` 是 `Q_UNUSED(on)` 空操作，但设置页勾选框三端都显示 → 非 Windows 点了没反应（静默 UX bug）。

- [ ] Linux：写/删 `~/.config/autostart/launcher.desktop`（XDG Autostart），`Exec=` 用 `QCoreApplication::applicationFilePath()`。
- [ ] macOS：写/删 `~/Library/LaunchAgents/<bundle-id>.plist` 并 `launchctl load/unload`（或用 `osascript` 加 Login Items）。
- [ ] `#ifdef Q_OS_WIN` / `#elif defined(Q_OS_MAC)` / `#else` 三分支；去掉现有 `.replace("/", "\\")`（Windows 分支专用）。

### 2.4 应用发现 macOS
`src/plugins/appplugin.cpp` 现在只有 `Q_OS_WIN`(.lnk) vs `#else`(Linux .desktop)；mac 会落进 Linux 分支扫不存在的 `/usr/share/applications`。

- [ ] 加 `#elif defined(Q_OS_MAC)` 分支：扫 `/Applications`、`/System/Applications`、`~/Applications` 下 `*.app` bundle；标题取 `CFBundleDisplayName`/`CFBundleName`（读 `Info.plist`）；启动用 `QProcess::startDetached("open", {"-a", appPath})`；图标从 bundle 取。

### 2.5 热键
- [ ] Linux Wayland：QHotkey 仅 X11，Wayland 下全局热键不可用。方案：接 `org.freedesktop.portal.GlobalShortcuts` 门户；或至少在文档/设置页提示限制。
- [ ] macOS：QHotkey Carbon 后端需「辅助功能」权限，首次运行要引导授权；本项目未验证，需实机跑通。
- [ ] 热键录制控件 `hotkeyedit.cpp` 非 Windows 已有退化实现（录不了系统保留键），macOS 如需对等可用 `CGEventTap`（可选，非阻塞项）。

### 2.6 构建 / 打包 / 图标
- [ ] `CMakePresets.json`：加 `macos` preset（`condition hostSystemName == "Darwin"`，generator Unix Makefiles 或 Ninja）。
- [ ] `Makefile`：OS 检测目前 `Windows_NT` 否则 linux，无 Darwin 分支；加 `uname -s == Darwin` → macos preset。
- [ ] `CMakeLists.txt`：macOS 加 `MACOSX_BUNDLE`（可条件化）+ `Info.plist` + bundle identifier + `.icns`；`qt_add_executable(... WIN32 ...)` 的 `WIN32` 标志非 Windows 被忽略（无害，可保留或条件化）。
- [ ] 图标：现仅 `resources/icons/tray.png`。补应用图标——Windows `.ico`、macOS `.icns`、Linux 多尺寸 PNG。
- [ ] 打包：Linux → linuxdeploy/AppImage + 随包安装 `launcher.desktop` 与 PNG 图标；macOS → `macdeployqt` + 打 `.dmg`；Windows → `windeployqt`（已有）。

### 2.7 CI（越早越好）
- [ ] 加 `.github/workflows/ci.yml` 三 OS 矩阵：`windows-latest`、`ubuntu-latest`（`apt` 装 `qt6-base-dev` + X11 开发库）、`macos-latest`（`brew install qt`）；各自 configure + build + `ctest`。把非 Windows preset 纳入自动验证，避免它们悄悄腐坏。

## 3. 建议路线（分阶段）

1. **阶段一 · Linux 功能对等**：2.1 PTY + 2.2 shell + 2.3 自启（Linux 段）→ Linux 变成真正可用。
2. **阶段二 · macOS 可构建**：2.6 preset/bundle + 2.4 应用发现 + 2.1/2.2 的 macOS 段 → mac 先能编能跑。
3. **阶段三 · 打包与图标**：2.6 AppImage/dmg + 图标集。
4. **阶段四 · CI 矩阵**：2.7，回填自动验证。
5. **阶段五 · 打磨**：2.5 Wayland / macOS 权限引导 / 自启 macOS 段。

## 4. 参考（已沉淀的移植相关踩坑）

- [`docs/notes/mingw-abi-mismatch.md`](notes/mingw-abi-mismatch.md) —— Windows：MinGW 版本须与 Qt 官方二进制精确配套。
- [`docs/notes/embedded-terminal-libvterm.md`](notes/embedded-terminal-libvterm.md) —— 终端集成坑；文末自述「仍未做：Linux 端（ConPTY→forkpty）」。
