# 功能规划（索引）

「已完成」直接列在下方；「待实现」每项独立成文，本节只是**索引**，按需点开。

## 已完成

| 功能 | 说明 |
|------|------|
| 全局热键唤起 | `Alt+Space` 显示/隐藏窗口 |
| 无边框悬浮窗 | 居中显示，失焦自动隐藏，always-on-top |
| 键盘导航 | ↑ ↓ 移动焦点，Enter 执行，Esc 关闭；Emacs 键位：`C-n`/`C-p` 上下选项（焦点留搜索框）、`C-g` 取消、`C-a`/`C-e` 行首尾、`C-f`/`C-b` 左右移、`C-d` 删后字符、`C-k` 删到行尾、`M-f`/`M-b` 按词左右移、`M-d` 删后一词 |
| 应用搜索插件 | Windows 扫描 Start Menu `.lnk`，Linux 解析 `.desktop`；`QFileSystemWatcher` 监听扫描目录，装/卸应用后去抖重扫，免重启刷新 |
| 结果项展示 | 自定义 delegate：真实应用图标 + 两行（标题 / 真实路径，长路径中间省略）。Windows 用 COM 解析 `.lnk` 目标 exe |
| 搜索排序与高亮 | 按匹配质量排序（完全相等 > 前缀 > 词首 > 子串 > 子序列模糊，同档优先短标题）；支持子序列模糊匹配（如 `ff`→Firefox）；命中字符高亮（连续与非连续均支持）；默认选中首项；搜索框与列表拼为一体卡片 |
| frecency 排序 | 记录使用频率与最近使用时间（持久化到用户数据目录 `usage.json`），常用项在同档匹配内上浮；加分有界（≤60），不跨匹配档 |
| 插件接口 | `IPlugin` 抽象接口，支持任意扩展（`triggerPrefix` 前缀路由、`executeAlt` 次级动作均为可选）；输入命中某前缀插件时自动抑制全局插件，避免应用结果污染命令/文件视图 |
| 次级操作 | `Ctrl+Enter` 触发结果的次级动作；应用插件用于把路径/命令复制到剪贴板（不计入 frecency） |
| 命令插件 | `/` 前缀触发 launcher 自身命令：`help`（自定义帮助页，分快捷键/前缀/命令三区）、`settings`（打开设置）、退出、重载应用列表、打开数据目录、重启；裸 `/` 列出全部命令（命令面板式），可按中文标题或英文 id 过滤；通用命令注册表（动作用 `std::function`，由 `main.cpp` 装配，插件不耦合具体能力） |
| 文件搜索插件 | `@` 前缀触发，递归扫描用户可在设置页配置的目录列表（默认桌面/文档/下载/图片）做文件名模糊匹配；回车用默认程序打开，`Ctrl+Enter` 打开所在目录；**异步查询**（`QtConcurrent` 工作线程遍历 + 失效代次丢弃过期结果，不阻塞 UI），带访问数上限兜底，图标在主线程对展示项补齐 |
| 网页搜索插件 | `?` 前缀触发，每个搜索引擎作为一条结果（内置 Google / Bing / Baidu / GitHub + 用户在设置页自定义添加的引擎）；回车默认浏览器打开；frecency 按引擎聚合，用多哪个自动排前 |
| 系统托盘 | 启动后常驻托盘放大镜图标（QPainter 绘制，Catppuccin blue）；双击托盘图标等同全局热键（toggle 显示/隐藏）；右键菜单：显示/隐藏、退出 |
| 设置界面 | `/settings` 命令打开；Catppuccin 风格自定义对话框（可拖拽标题栏，Esc 关闭，内容区超出屏幕高度时可滚动）；每次显示都会用当前设置刷新表单，保证"取消"不残留未保存的编辑。配置持久化到 `settings.json`：① 全局热键（`HotkeyEdit` 低级钩子录制，支持 Alt+Space 等系统保留键）；② 开机自启动（Windows 写入 `HKCU\...\Run`）；③ 插件启用/禁用；④ 网页搜索引擎优先级（列表 + 上下移动排序 + 添加/移除自定义引擎，内置引擎不可删除）；⑤ 文件搜索目录列表（添加/移除，默认桌面/文档/下载/图片）；⑥ 一键重置为出厂默认（仅重置表单，需再点保存才落盘） |
| 多语言 (i18n) | 内置简体中文/English，`/settings` 页语言下拉框切换，需重启生效（不支持热切换）。语言包是 datadir `i18n/<code>.json` 文件，内置语言首次运行时从 Qt 资源落盘，用户可直接编辑或复制出自定义语言；详见 [AGENTS.md「自定义语言」](../AGENTS.md#自定义语言) |
| 内嵌终端 | `/terminal` 命令打开独立终端窗口：基于 **libvterm**（VT 解析/屏幕状态内核，vendored 纯 C）+ **ConPTY** 起 shell（pwsh > powershell > cmd）+ QPainter 逐格自绘 + 键盘编码。支持 ANSI 彩色、光标、窗口 resize 往返（reflow）、可交互（vim/htop 等）；**ANSI 16 色跟主题**（调色板注入 vterm，`ls --color`/vim 配色与当前主题一致，主题 JSON 可选 `ansi` 数组，缺失回退内置）；**鼠标上报**（应用开启鼠标模式时 vim/htop/tmux/lazygit 可点击/拖动/滚轮，按 Shift 强制本地选区）；**滚动回看**（5000 行历史，滚轮回看/`Shift+PageUp/PageDown` 键盘翻页/`Shift+Home/End` 跳顶贴底/按键贴底/备用屏滚轮转方向键/右侧位置指示条）；**鼠标选择与复制粘贴**（拖选跨历史行、双击选词按分隔符集合分词/路径域名整体选中、Ctrl+Shift+C/V、右键粘贴、bracketed paste）；**控制键**（`Ctrl+[`=Esc、`Ctrl+\ ] ^ _`、`Ctrl+Space`=NUL）；**中文 IME**（预编辑串绘制、候选框跟随光标）；关闭后可重开新会话。**终端栈已抽为可嵌入的 `TerminalPane`**（TerminalView+ConPty+PtyReader+会话接线），两个宿主共用：`/terminal` 在 launcher 内**内联展开终端模式**（卡片морф为终端，`Ctrl+\`` 进/出、Esc 保留给终端、失焦不隐藏、会话跨模式后台留存），`/termwin` 打开**独立可缩放终端窗口**（各持独立会话）。**未做**：Linux 端（ConPTY→forkpty）、脏区局部重绘、字体/shell/scrollback 等配置化。工程坑见 [notes/embedded-terminal-libvterm.md](notes/embedded-terminal-libvterm.md)、[notes/terminal-inline-mode.md](notes/terminal-inline-mode.md) |
| 命令执行插件 | `:` 前缀触发（如 `: ipconfig`），回车进入 launcher 内联终端模式并执行该命令（写 shell stdin，会话复用）；裸 `:` 显示提示项（回车只进终端）。runner 由 `main.cpp` 装配，插件与终端解耦（`win.enterTerminal(c)`）；执行过的命令按 frecency 上浮 |
| 多主题 (theme) | 内置 4 套主题（Dark/Catppuccin Mocha、Light/Catppuccin Latte、Dracula、Nord）。`/settings` 页三项联动的外观设置（类 Zed 编辑器设计）：「外观」深色/浅色/跟随系统 + 分别为深色/浅色模式各配一个默认主题；跟随系统时按启动那一刻的系统深浅色偏好自动选用对应默认主题。三者均需重启生效（与语言切换共用同一重启提示，不支持运行时热切换/热跟随）。主题包是 datadir `themes/<code>.json` 文件，内置主题首次运行时从 Qt 资源落盘，用户可直接编辑或新增自定义主题；详见 [AGENTS.md「自定义主题」](../AGENTS.md#自定义主题) |

## 待实现

（暂无）

> 实现某项后：把它从「待实现」移到「已完成」表，并相应处理 `features/` 下的文件。新增功能需求则加一篇 + 补一行索引。
