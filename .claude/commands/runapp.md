---
description: 启动 launcher 并按清单验证核心交互（运行-验证循环）
allowed-tools: Bash, PowerShell, Read
---

# /runapp — 运行-验证 Loop

构建产物后启动应用，对照已完成功能逐项验证核心交互是否正常。GUI 行为无法纯程序断言，本命令负责"把应用跑起来 + 给出可核对的清单"，由人工/截图确认。

## 步骤

1. **确保已构建**：若 `build/launcher.exe` 不存在，先走 `/build` 构建-修复循环。
2. **启动**：
   ```powershell
   & "D:\Projects\launcher\build\launcher.exe"
   ```
   （后台启动；启动后应用常驻、`setQuitOnLastWindowClosed(false)`，窗口默认隐藏。）

## 验证清单（对照 docs/features.md 已完成项）

逐项人工确认：

- [ ] **全局热键唤起**：按 `Alt+Space` 弹出居中悬浮窗，再按隐藏。
- [ ] **失焦自动隐藏**：点窗口外区域，窗口自动隐藏。
- [ ] **键盘导航**：↑ ↓ 移动选中项，`Esc` 关闭。
- [ ] **应用搜索插件**：输入应用名（如 `note`/`chrome`），结果列表出现匹配项并显示图标与路径。
- [ ] **执行**：选中项按 `Enter`，应用启动且窗口随即隐藏。

## 注意

- `Alt+Space` 是 Windows 系统保留键，**某些环境注册会失败**（不崩溃，仅热键无效）。若唤不起，参见 [docs/notes/altspace-reserved-key.md](../../docs/notes/altspace-reserved-key.md)，临时改用 `Ctrl+Space` 验证。
- 验证中发现的偏差：若属新坑记入 `docs/notes/`，若属功能缺口更新 `docs/features.md`（判断式，见维护约定）。
