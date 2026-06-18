# Launcher — AI Agent 指引

跨平台快速启动器，参考 uTools / PowerToys Run，基于 Qt6 + C++17 实现，优先支持 Windows 和 Linux。

---

## 环境要求

| 工具 | 版本要求 | 说明 |
|------|---------|------|
| CMake | ≥ 3.20 | 推荐通过 Scoop / 官网安装 |
| Qt | 6.x（Widgets 模块）| 见下方安装指引 |
| 编译器 | **必须与 Qt 二进制配套** | 见下方说明 |
| Git | 任意版本 | 用于拉取子模块 |

### Qt 安装（推荐用 aqtinstall）

```bash
pip install aqtinstall

# Windows — MinGW 版
python -m aqt install-qt windows desktop 6.8.3 win64_mingw --outputdir <Qt安装目录>
python -m aqt install-tool windows desktop tools_mingw1310 --outputdir <Qt安装目录>

# Linux
python -m aqt install-qt linux desktop 6.8.3 linux_gcc_64 --outputdir <Qt安装目录>
# 或直接用发行版包管理器：
# sudo apt install qt6-base-dev cmake
```

> `<Qt安装目录>` 由你自己决定，例如 Windows 上的 `D:\Qt` 或 Linux 上的 `~/Qt`。

### ⚠️ 编译器版本必须与 Qt 配套

Qt 预编译包使用特定编译器版本构建，**ABI 不兼容会导致运行时崩溃（堆损坏）**，症状是进程以 `0xC0000374` 退出，崩溃点在 `QHotkey` 构造函数内部。

| 平台 | Qt 6.8.3 配套编译器 | 获取方式 |
|------|-------------------|---------|
| Windows | MinGW 13.1.0（posix-seh） | `aqt install-tool ... tools_mingw1310` |
| Linux | 系统 GCC（通常 ≥ 11） | 发行版自带，无需特殊版本 |

---

## 构建步骤

### Windows

```powershell
# 设置路径变量（根据你的实际安装目录修改）
$QT_DIR   = "<Qt安装目录>\6.8.3\mingw_64"
$MINGW    = "<Qt安装目录>\Tools\mingw1310_64\bin"

# 初始化子模块（首次克隆后执行一次）
git submodule update --init

# 配置
cmake -S . -B build -G "MinGW Makefiles" `
    -DCMAKE_PREFIX_PATH="$QT_DIR" `
    -DCMAKE_CXX_COMPILER="$MINGW\g++.exe" `
    -DCMAKE_MAKE_PROGRAM="$MINGW\mingw32-make.exe" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5

# 编译
& "$MINGW\mingw32-make.exe" -C build -j8

# 部署 Qt DLL（首次或更新 Qt 版本后执行）
& "$QT_DIR\bin\windeployqt.exe" build\launcher.exe
```

### Linux

```bash
# 初始化子模块
git submodule update --init

# 配置
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    # 若 Qt 装在非标准路径，追加：
    # -DCMAKE_PREFIX_PATH="<Qt安装目录>/6.8.3/gcc_64"

# 编译
cmake --build build -j$(nproc)
```

---

## 项目结构

```
src/
├── main.cpp
├── mainwindow.h / .cpp     # 无边框悬浮窗，Alt+Space 唤起/隐藏，失焦自动隐藏
├── plugin/
│   ├── iplugin.h           # IPlugin 接口
│   └── resultitem.h        # ResultItem 数据结构（已注册 QMetaType）
└── plugins/
    └── appplugin.h / .cpp  # 内置应用搜索插件
third_party/
└── QHotkey/                # Git submodule，跨平台全局热键库
```

**数据流：** 搜索框输入 → `IPlugin::query`（所有插件）→ 结果列表 → 用户回车 → `IPlugin::execute` → 隐藏窗口

---

## 添加新插件

1. 在 `src/plugins/` 新建 `myplugin.h` / `myplugin.cpp`，实现 `IPlugin`：

```cpp
class MyPlugin : public IPlugin {
public:
    QString           name()  const override { return "MyPlugin"; }
    QList<ResultItem> query(const QString &keyword) override { /* 返回最多 8 条 */ }
    void              execute(const ResultItem &item) override { /* 不能阻塞 UI 线程 */ }
};
```

2. 将 `.cpp` 加入 `CMakeLists.txt` 的 `qt_add_executable` 源文件列表
3. 在 `main.cpp` 注册：`win.addPlugin(new MyPlugin)`

跨平台差异用 `#ifdef Q_OS_WIN` / `#else` 隔离。

---

## 已知问题 / 注意事项

- **`qt_add_executable` 而非 `add_executable`**：后者配合 `WIN32_EXECUTABLE TRUE` 会因 `Qt6EntryPoint` ABI 不匹配而链接失败。
- **`Alt+Space` 是 Windows 系统保留键**（窗口菜单），但 QHotkey 在正确的编译器版本下可以正常注册，不必换键。若遇到注册失败（非崩溃），可改为 `Ctrl+Space`。
- **CMake 4.x**：需要传 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`，否则 QHotkey 子目录的旧版 `cmake_minimum_required` 会报错。

---

## 待实现插件

| 插件 | 触发方式 | 状态 |
|------|---------|------|
| AppPlugin | 任意文本 | ✅ 已完成 |
| CalcPlugin | `= 1+2*3` | 待实现 |
| FilePlugin | `/` 或 `~` 开头 | 待实现 |
| WebPlugin | `? 搜索词` | 待实现 |
