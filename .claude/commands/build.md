---
description: 构建 launcher（构建-修复循环：配置 → 编译 → 读错误修复 → 重编，直到通过）
allowed-tools: Bash, PowerShell, Read, Edit, Grep, Glob
---

# /build — 构建-修复 Loop

把本项目复杂的 Qt6 + MinGW 构建固化成单一入口，并内嵌"编译失败就读错误、定位、修复、重编"的迭代循环。

## 前置：环境注入

构建依赖两个**机器相关**路径，由 `.claude/settings.local.json` 注入为环境变量：

- `$env:QT_DIR` —— 例如 `D:\Qt\6.8.3\mingw_64`
- `$env:MINGW` —— 例如 `D:\Qt\Tools\mingw1310_64\bin`

**第一步先校验**：若 `$env:QT_DIR` / `$env:MINGW` 为空或路径不存在（`Test-Path` 为假），**停下**，明确告诉用户去 `.claude/settings.local.json` 配置本机 Qt/MinGW 安装路径，不要瞎猜路径硬跑。

> 注意：`settings.local.json` 的 `env` 在**会话启动时**注入。若刚创建/修改过它，当前会话仍读不到，需重启 Claude Code 会话才生效。

## 构建步骤（Windows / MinGW）

```powershell
# 1) 子模块（首次克隆后执行一次；已存在则跳过）
git submodule update --init

# 2) 配置（注意：每个 -D 参数整体加引号，否则 PowerShell 会把 3.5 吞成 3）
cmake -S . -B build -G "MinGW Makefiles" `
    "-DCMAKE_PREFIX_PATH=$env:QT_DIR" `
    "-DCMAKE_CXX_COMPILER=$env:MINGW\g++.exe" `
    "-DCMAKE_MAKE_PROGRAM=$env:MINGW\mingw32-make.exe" `
    "-DCMAKE_BUILD_TYPE=Release" `
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"

# 3) 编译
& "$env:MINGW\mingw32-make.exe" -C build -j8
```

> `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` 必须带，否则 CMake 4.x 会因 QHotkey 报错 —— 见 [docs/notes/cmake-policy-version.md](../../docs/notes/cmake-policy-version.md)。
> **PowerShell 坑**：`-D...=3.5` 不整体加引号时，`.5` 会被吞掉、传给 cmake 的值变成 `3` 并报 `Invalid CMAKE_POLICY_VERSION_MINIMUM value "3"`。务必按上面写法给整个 `-D...` 加引号。

### Linux 分支

```bash
git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build -j$(nproc)
```

## 修复循环（核心）

编译报错时**不要直接放弃**，按以下循环处理：

1. 完整读取编译/链接错误输出。
2. 先比对已知坑（命中就按对应文档处置，不要重新折腾）：
   - 运行期 `QHotkey` 构造崩溃（堆损坏 `0xC0000374`）→ [docs/notes/mingw-abi-mismatch.md](../../docs/notes/mingw-abi-mismatch.md)
   - 链接 `undefined reference to '__imp___argc'` → [docs/notes/qt-add-executable.md](../../docs/notes/qt-add-executable.md)
   - CMake 4.x 配置报 `cmake_minimum_required` 过低 → [docs/notes/cmake-policy-version.md](../../docs/notes/cmake-policy-version.md)
3. 若是普通源码错误 → 用 Read/Edit 定位修复。
4. 回到「编译」步重跑，直到通过。
5. **判定为环境问题**（编译器/Qt 路径不配套、ABI 不兼容等）时，停止空转，给出清晰的人工处置指引，而不是反复重编。

## 收尾

- 首次构建或更新过 Qt 版本：提示运行 `& "$env:QT_DIR\bin\windeployqt.exe" build\launcher.exe` 部署 DLL。
- 若编译过程中踩到**新坑**：判断是否值得沉淀，值得就在 `docs/notes/` 加一篇并在 `docs/notes.md` 索引补一行（见维护约定）。
