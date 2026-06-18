# Launcher — AI Agent 指引

跨平台快速启动器，参考 uTools / PowerToys Run，基于 Qt6 + C++17 实现，优先支持 Windows 和 Linux。

## 维护约定

- 遇到踩坑、限制或架构决策 → 记录到 [docs/notes.md](docs/notes.md)
- 新增或调整功能需求 → 更新 [docs/features.md](docs/features.md)
- **先更新文档，再提交代码。**

---

## 环境要求

| 工具 | 版本要求 | 说明 |
|------|---------|------|
| CMake | ≥ 3.20 | 推荐通过 Scoop / 官网安装 |
| Qt | 6.x（Widgets 模块）| 见下方安装指引 |
| 编译器 | **必须与 Qt 二进制配套** | 见 [docs/notes.md](docs/notes.md) |
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

---

## 构建步骤

### Windows

```powershell
# 设置路径变量（根据你的实际安装目录修改）
$QT_DIR = "<Qt安装目录>\6.8.3\mingw_64"
$MINGW  = "<Qt安装目录>\Tools\mingw1310_64\bin"

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
docs/
├── notes.md                # 踩坑与注意事项
└── features.md             # 功能规划
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
