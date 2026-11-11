# Launcher — AI Agent 指引

跨平台快速启动器，参考 uTools / PowerToys Run。界面是 mygo 原生 UI，语言是 Go，优先支持 Windows 和 Linux。

## 环境要求

| 工具 | 版本要求 | 说明 |
|------|---------|------|
| Go | ≥ 1.27.3 | 本机更旧时，Go 会按 `go.mod` 自动下载工具链 |

## 构建与测试

```powershell
go test ./internal/core/
go build -ldflags="-H windowsgui" -o launcher.exe .
.\launcher.exe
```

`go run .` 会带一个控制台窗口。发给自己用的程序加上 `-H windowsgui`。

`Alt+Space` 在 Windows 上可能被系统占用。注册失败时程序会改试 `Ctrl+Space`，再失败则试 `Alt+Shift+Space`。当前生效的热键写在面板底部。

## 维护约定（文档）

文档拆成「索引 + 按需读取的细粒度文件」：`docs/notes.md`、`docs/features.md` 是索引，正文在 `docs/notes/`、`docs/features/` 下，一条一文件。

- 改动若涉及**新踩坑 / 限制 / 架构决策** → 判断是否值得沉淀，值得就在 `docs/notes/` 加一篇并在 `docs/notes.md` 索引补一行
- 改动若**改变了功能边界**（新增 / 调整 / 完成功能）→ 更新 `docs/features.md` 及 `docs/features/` 对应文件
- **是否更新文档由你按改动性质自行判断**：纯重构、小修小补、不影响行为的改动无需更新；文档与代码可在同一次提交内完成
- 其他文档也可以自己判断是否需要改动

`docs/notes/` 里 Qt / MinGW 的记录是旧实现的历史，不要再按那些步骤构建。

## 项目结构

```
main.go                         # 生命周期、热键、托盘、设置和帮助窗口
view.go                         # 搜索面板
query.go                        # 前缀路由、100ms 防抖、统一排序、执行
keys.go                         # Emacs 行编辑
internal/core                   # Score、拼音、usage.json、settings.json
internal/plugin                 # Plugin 接口
internal/plugins                # 应用 / 文件 / 网页 / 命令
mygo.json                       # 打包标识
```

**数据流：** 搜索框输入 → 防抖 → 按前缀路由（前缀已剥离；一旦有前缀，全局插件让位）→ `Query` → 加上 frecency 后统一排序、截到 8 条 → 回车只交给产出该结果的插件 `Execute`（`Ctrl+Enter` 走 `ExecuteAlt`）→ 隐藏面板。

配置目录与 Qt 版本相同：Windows `%APPDATA%\launcher`。

## 添加新插件

1. 在 `internal/plugins/` 实现 `plugin.Plugin`。
2. `Name()` 是设置里的稳定 id。`Prefix()` 为空表示对任意输入生效。
3. `Query` 只筛选并填写 `Score`（用 `core.Score`）。不要自己排序或截断。
4. 耗时扫描把 `Async()` 设为 true。`Query` 不能碰 UI。
5. 在 `main.go` 的 `l.plugs` 里注册。插件顺序决定前缀检测顺序。
