# Launcher

轻量启动器，参考 uTools 和 PowerToys Run。界面用 [mygo](https://github.com/egoist/mygo) 的原生 UI，逻辑用 Go。

## 功能

- 全局热键唤起 / 隐藏（默认 `Alt+Space`，可在设置里改）
- 无边框悬浮窗，失焦自动隐藏
- 键盘导航，含 Emacs 键位
- 应用搜索（系统已安装应用，支持拼音和首字母）
- `@` 文件搜索，`?` 网页搜索，`/` 命令
- 托盘、开机启动、设置

配置和用过记录仍在原来的目录：Windows 上是 `%APPDATA%\launcher`（`settings.json`、`usage.json`）。

## 环境

- Go 1.27.3 或更新

## 运行

```powershell
go run .
```

打一个不带控制台窗口的程序：

```powershell
go build -ldflags="-H windowsgui" -o launcher.exe .
```

## 测试

```powershell
go test ./internal/core/
```

## 结构

```
main.go                 窗口、热键、托盘
view.go                 搜索面板
query.go                前缀路由、防抖、排序
internal/core           匹配、拼音、frecency、设置
internal/plugins        应用、文件、网页、命令
```

新插件实现 `internal/plugin.Plugin`，在 `main.go` 里放进 `l.plugs`。打分用 `core.Score`，排序和截断由主窗口统一做。
