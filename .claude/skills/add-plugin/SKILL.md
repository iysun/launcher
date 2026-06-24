---
name: add-plugin
description: 为 launcher 新增一个搜索插件。当用户要添加新插件、新触发能力（如计算器/文件搜索/网页搜索等以某前缀触发的功能）时使用——按 IPlugin 模式生成骨架、注册到 CMake 与 main.cpp、走构建验证、并判断是否更新文档，贯穿一条流水线。
---

# add-plugin — 新插件开发 Loop

把"加一个插件"的零散步骤固化成端到端流水线，避免漏掉注册或构建验证。

## 输入（先问清）

- **插件类名**：如 `CalcPlugin`（PascalCase）。文件名取小写：`calcplugin.h/.cpp`。
- **触发方式**：前缀触发（如 `=` 计算、`/` 文件、`?` 网页）还是无前缀（始终参与查询，如应用搜索）。
- **一句话功能**：query 返回什么、execute 做什么。

## 流水线

### 1) 生成骨架 `src/plugins/<name>.h` / `.cpp`

实现 `IPlugin` 三个纯虚函数（接口见 `src/plugin/iplugin.h`，数据结构见 `src/plugin/resultitem.h`）。可参考现有 `src/plugins/appplugin.h/.cpp`。

约束：
- `query()` **最多返回 8 条** `ResultItem`（`title` / `subtitle` / `action` / `icon`）。
- 前缀触发的插件：keyword 不匹配前缀时 `return {};`，把查询让给其它插件。
- `execute()` **不能阻塞 UI 线程**（耗时操作异步处理）。
- 跨平台差异用 `#ifdef Q_OS_WIN` / `#else` 隔离。

头文件骨架：

```cpp
#pragma once
#include "plugin/iplugin.h"

class MyPlugin : public IPlugin {
public:
    QString           name()  const override { return "MyPlugin"; }
    QList<ResultItem> query(const QString &keyword) override;
    void              execute(const ResultItem &item) override;
};
```

### 2) 注册到构建

把新的 `.cpp` 加入 `CMakeLists.txt` 的 `qt_add_executable(launcher ...)` 源文件列表（与 `src/plugins/appplugin.cpp` 并列）。

### 3) 注册到运行时

在 `src/main.cpp` 仿照 `win.addPlugin(new AppPlugin);` 增加 `win.addPlugin(new MyPlugin);`（记得 `#include` 新头文件）。

### 4) 构建验证

走 `/build` 构建-修复循环，编译通过后用 `/runapp` 实际触发该插件确认行为。

### 5) 文档同步判断（判断式，非强制）

- 该插件实现了 `docs/features.md` 里的某个**待实现**项 → 把它从 `docs/features/` 对应文件迁移到「已完成」，并更新索引。
- 过程中踩到**新坑** → 在 `docs/notes/` 加一篇并在 `docs/notes.md` 索引补一行。
- 纯骨架、未改变功能边界 → 无需动文档。
