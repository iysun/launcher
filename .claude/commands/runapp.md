---
description: 启动 launcher 并按清单验证核心交互
allowed-tools: Bash, PowerShell, Read
---

# /runapp — 运行-验证

1. 若还没有 `launcher.exe`，先走 `/build`。
2. 启动 `.\launcher.exe`（无控制台窗口，关闭面板后进程仍在托盘里）。

## 验证清单

- [ ] 全局热键唤起后再次按下会隐藏。面板底部写着实际注册到的热键。
- [ ] 点到窗口外，面板隐藏。
- [ ] ↑ ↓ 或 Ctrl+P / Ctrl+N 移动选中项，Esc 关闭。有输入时 Esc 先清空。
- [ ] 输入应用名能看到图标和路径，回车启动并隐藏。
- [ ] `@` 搜文件，`?` 出搜索引擎，`/` 列出命令。

`Alt+Space` 在 Windows 上可能注册失败。失败时程序会改用 `Ctrl+Space` 或 `Alt+Shift+Space`。
