---
name: add-plugin
description: 为 launcher 新增一个搜索插件。当用户要添加新插件、新触发能力（如计算器，以某前缀触发）时使用——按 Plugin 接口生成骨架、在 main.go 注册、构建验证，并判断是否更新文档。
---

# add-plugin — 新插件

## 输入（先问清）

- 插件名：作为 `Name()` 的稳定 id，例如 `Calc`。设置里用这个字符串记住启用状态。
- 触发方式：前缀（如 `=`）或全局（空前缀）。
- query 返回什么、Execute 做什么、要不要 Ctrl+Enter。

## 流水线

1. 在 `internal/plugins/` 新建文件，实现 `plugin.Plugin`（见 `internal/plugin/plugin.go`）。参考 `internal/plugins/web.go`。
2. 约束：
   - 前缀由主窗口剥离后再传入 `Query`。
   - 用 `core.Score` 填写 `Score`。不要自己排序，也不要截到 8 条；主窗口会做。
   - `Async() == true` 时 `Query` 在别的 goroutine 上跑，不能调用 UI。
   - `Execute` 只会被自己产出的结果调用。
3. 在 `main.go` 里把插件放进 `l.plugs`。顺序靠前的前缀先被识别。
4. `go test ./internal/core/` 和 `go build -ldflags="-H windowsgui" -o launcher.exe .` 通过后，用 `/runapp` 的方式实际触发一次。
5. 功能边界变了就更新 `docs/features.md`。纯骨架不用改文档。
