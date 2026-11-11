---
description: 构建 launcher（编译，失败则按编译错误修到通过）
allowed-tools: Bash, PowerShell, Read, Edit, Grep, Glob
---

# /build — 构建

```powershell
go test ./internal/core/
go build -ldflags="-H windowsgui" -o launcher.exe .
```

需要 Go 1.27.3 或更新。编译失败时读错误、改代码、再跑这两条，直到通过。

不要再走 Qt / CMake / MinGW。那些记录在 `docs/notes/` 里，只属于旧实现。
