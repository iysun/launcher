# 内联终端模式：两个易踩坑

launcher 主窗口把终端融合为「同一卡片内的终端模式」（`Ctrl+\`` 进/出），终端不再是独立顶层窗口。
实现中有两处不显眼但会直接翻车的点，记录如下。

## 1. 退出模式键必须是 `Ctrl+\``，不能用 Esc

**现象/动机**：直觉上「Esc 退出终端模式回搜索」很自然，但 **Esc 是 vim/htop/less 等 TUI 的命门**——一旦被主窗口截去退模式，终端里的 vim 就永远收不到 Esc，等于打残。

**正确做法**：进/出终端模式统一用 `Ctrl+\``（`Qt::Key_QuoteLeft` + `ControlModifier`）。**Esc 及其余所有按键一律放行给终端**。可选的精细化（默认不做，易误触）：仅当 `TerminalCore::altScreen()` 为假且行首空白时才让 Esc 退出。

## 2. 退出键要在 `TerminalView` 消费之前拦截

**现象**：`TerminalView::keyPressEvent` 把按键 raw 编码送 PTY——若在 `MainWindow` 层等按键冒泡上来再判断，`Ctrl+\`` 早已被 TerminalView 吞掉送进 shell，退不出来。

**正确做法**：容器 `TerminalTabs` 在**每个 pane 的 `view()` 上装 event filter**（`pane->view()->installEventFilter(this)`），在 filter 里抢先捕获 `Ctrl+\`` → `emit exitRequested()` 并 `return true` 吞掉，不下送 PTY；宿主 `MainWindow` 连 `exitRequested`。进入方向则相反：搜索框获焦时由 `MainWindow::eventFilter` 捕获 `Ctrl+\`` → `enterTerminal()`。

> 引入多标签后，**所有终端模式管理键统一在 `TerminalTabs` 一处拦截**（`Ctrl+\`` 退出、`Ctrl+Shift+T/W`、`Ctrl+Tab`/`Ctrl+Shift+Tab`（后者部分平台来的是 `Key_Backtab`）、`Alt+1..9`）。`TerminalPane` 退化为纯会话 widget，不再自管任何快捷键/圆角遮罩——遮罩也上收到容器层（标签栏顶角 + 面板底角一次裁齐）。键位选 `Ctrl+Shift`/`Alt+数字` 而非 `Ctrl+T/W`，是为了不撞 shell 行编辑（transpose/delete-word）。

## 3. 标签名要显示 cwd：得让 shell 主动上报（注入 prompt 钩子）

**现象/坑**：终端拿不到子进程的 cwd。尤其 **PowerShell 的 `Set-Location` 不改进程工作目录**（.NET 名坑），`NtQueryInformationProcess` 读 PEB 也读不到；pwsh 默认只把 OSC 标题设成 exe 全路径，不随 `cd` 变。

**正确做法**：`terminalpane.cpp` 落盘一个 `pwsh-cwd-title.ps1`，启动 pwsh/powershell 时 `-NoExit -File` 加载它——脚本**包住已有 `prompt`**（先于 `-File` 加载的 profile/oh-my-posh 的 prompt 被捕获再包一层），每次刷新提示符 `$Host.UI.RawUI.WindowTitle = ...ProviderPath`。这样标题=cwd 全路径 → 现有 `titleChanged` → `TerminalTabs::shortTabLabel` 取最后一段即目录名。cmd.exe 无此机制，回退 `Terminal N`。

## 附带约束（沿用既有终端栈）

- **`outputToPty`/`resized` 连接只连一次**（放 `TerminalPane` 构造函数）：放 `startSession` 会在重开会话时重复连接 → 每键写多次。
- **teardown 顺序不可乱**：`requestStop → pty->stop → wait → deleteLater`，否则 worker 线程解不开阻塞的 `ReadFile`。
- **失焦自动隐藏仅搜索模式生效**：`changeEvent` 里加 `m_mode == Launch` 守卫，否则终端模式下 alt-tab 走开会话窗口就没了。
- **尺寸双档**：主窗口 `QLayout::SetFixedSize` 是内容驱动，终端模式改 `m_card` 固定宽（620→900）+ 给 pane 固定高，切回时还原。
