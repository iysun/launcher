# 功能规划（索引）

「已完成」直接列在下方；「待实现」每项独立成文，本节只是**索引**，按需点开。

## 已完成

| 功能 | 说明 |
|------|------|
| 全局热键唤起 | `Alt+Space` 显示/隐藏窗口 |
| 无边框悬浮窗 | 居中显示，失焦自动隐藏，always-on-top |
| 键盘导航 | ↑ ↓ 移动焦点，Enter 执行，Esc 关闭 |
| 应用搜索插件 | Windows 扫描 Start Menu `.lnk`，Linux 解析 `.desktop` |
| 结果项展示 | 自定义 delegate：真实应用图标 + 两行（标题 / 真实路径，长路径中间省略）。Windows 用 COM 解析 `.lnk` 目标 exe |
| 搜索排序与高亮 | 按匹配质量排序（完全相等 > 前缀 > 词首 > 子串，同档优先短标题）；命中关键字高亮；默认选中首项；搜索框与列表拼为一体卡片 |
| 插件接口 | `IPlugin` 抽象接口，支持任意扩展 |

## 待实现

- [计算器插件（CalcPlugin）](features/calc-plugin.md) — `=` 开头触发，实时计算，回车复制结果
- [文件搜索插件（FilePlugin）](features/file-plugin.md) — `/` 或盘符开头触发，搜索文件系统
- [网页搜索插件（WebPlugin）](features/web-plugin.md) — `?` 开头触发，默认浏览器打开搜索引擎
- [系统托盘](features/system-tray.md) — 最小化到托盘，右键退出，双击唤起
- [设置界面](features/settings-ui.md) — 自定义热键、开机自启、插件启停

> 实现某项后：把它从「待实现」移到「已完成」表，并相应处理 `features/` 下的文件。新增功能需求则加一篇 + 补一行索引。
