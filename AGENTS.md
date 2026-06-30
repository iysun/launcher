# Launcher — AI Agent 指引

跨平台快速启动器，参考 uTools / PowerToys Run，基于 Qt6 + C++17 实现，优先支持 Windows 和 Linux。

## AI 工具（harness 入口）

本仓库已配好 Claude Code 的项目级工具，优先用它们而非临场拼命令：

| 入口 | 用途 |
|------|------|
| `/build` | 构建-修复循环：配置 → 编译 → 读错误对照已知坑修复 → 重编，直到通过 |
| `/runapp` | 运行-验证循环：启动应用并按清单核对 Alt+Space / 搜索 / 导航等核心交互 |
| `add-plugin` 技能 | 新插件流水线：生成 IPlugin 骨架 → 注册 CMake/main.cpp → 构建验证 → 文档判断 |

> 构建依赖的本机 Qt/MinGW 路径由 `.claude/settings.local.json` 注入（`QT_DIR` / `MINGW`，不入库，需各自配置）。

## 维护约定（文档）

文档拆成「索引 + 按需读取的细粒度文件」：`docs/notes.md`、`docs/features.md` 是索引，正文在 `docs/notes/`、`docs/features/` 下，一条一文件。

- 改动若涉及**新踩坑 / 限制 / 架构决策** → 判断是否值得沉淀，值得就在 `docs/notes/` 加一篇并在 `docs/notes.md` 索引补一行
- 改动若**改变了功能边界**（新增 / 调整 / 完成功能）→ 更新 `docs/features.md` 及 `docs/features/` 对应文件
- **是否更新文档由你按改动性质自行判断**：纯重构、小修小补、不影响行为的改动无需更新；文档与代码可在同一次提交内完成

---

## 环境要求

| 工具 | 版本要求 | 说明 |
|------|---------|------|
| CMake | ≥ 3.20 | 推荐通过 Scoop / 官网安装 |
| Qt | 6.x（Widgets 模块）| 见下方安装指引 |
| 编译器 | **必须与 Qt 二进制配套** | 见 [docs/notes/mingw-abi-mismatch.md](docs/notes/mingw-abi-mismatch.md) |
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

推荐用 **CMakePresets**（`CMakePresets.json` 已入库，需 CMake ≥ 3.21；仅装 3.20 的用下方 `<details>` 手写命令），把机器相关路径留在环境变量里、preset 只引用，免去手敲一长串 `-D`：

```powershell
# 设置路径环境变量（根据你的实际安装目录修改；AI harness 由 .claude/settings.local.json 注入）
$env:QT_DIR = "<Qt安装目录>\6.8.3\mingw_64"
$env:MINGW  = "<Qt安装目录>\Tools\mingw1310_64\bin"

# 初始化子模块（首次克隆后执行一次）
git submodule update --init

# 配置 + 编译（preset 读取上面的 QT_DIR / MINGW）
cmake --preset windows
cmake --build --preset windows

# 部署 Qt DLL（首次或更新 Qt 版本后执行）
& "$env:QT_DIR\bin\windeployqt.exe" build\launcher.exe
```

<details><summary>不用 preset 的等价手写命令（备查）</summary>

```powershell
$QT_DIR = "<Qt安装目录>\6.8.3\mingw_64"
$MINGW  = "<Qt安装目录>\Tools\mingw1310_64\bin"

# 配置（每个 -D 参数整体加引号，否则 PowerShell 会把 3.5 吞成 3，报 Invalid value "3"）
cmake -S . -B build -G "MinGW Makefiles" `
    "-DCMAKE_PREFIX_PATH=$QT_DIR" `
    "-DCMAKE_CXX_COMPILER=$MINGW\g++.exe" `
    "-DCMAKE_MAKE_PROGRAM=$MINGW\mingw32-make.exe" `
    "-DCMAKE_BUILD_TYPE=Release" `
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"

& "$MINGW\mingw32-make.exe" -C build -j8
```
</details>

### Linux

```bash
# 初始化子模块
git submodule update --init

# 配置 + 编译（preset）
cmake --preset linux
cmake --build --preset linux

# 若 Qt 装在非标准路径，配置前设置环境变量：
#   export CMAKE_PREFIX_PATH="<Qt安装目录>/6.8.3/gcc_64"
```

### 运行测试

```bash
ctest --preset windows   # 或 linux
```

> Windows 上 test preset 已把 `QT_DIR\bin` 与 `MINGW` 注入测试进程的 PATH，
> 否则 `matcher_test.exe` 找不到 Qt/MinGW 运行库会以 `0xC0000135` 退出。

---

## 项目结构

```
src/
├── main.cpp
├── mainwindow.h / .cpp         # 无边框悬浮窗，全局热键唤起/隐藏，失焦自动隐藏，系统托盘
├── core/
│   ├── matcher.h / .cpp        # 共享匹配/打分（子串四档 + 子序列模糊），跨插件统一排序的依据
│   ├── pinyin.h / .cpp         # CJK → 拼音转换，支持全拼/首字母搜索
│   ├── usagestore.h / .cpp     # frecency 持久化（usage.json）
│   └── appsettings.h / .cpp    # 用户配置持久化（settings.json）：热键、开机自启、插件启停
├── plugin/
│   ├── iplugin.h               # IPlugin 接口（含 triggerPrefix 前缀路由）
│   └── resultitem.h            # ResultItem 数据结构（含 owner / score，已注册 QMetaType）
├── plugins/
│   ├── appplugin.h / .cpp      # 应用搜索（Start Menu .lnk / .desktop，拼音匹配）
│   ├── commandplugin.h / .cpp  # "/" 前缀命令插件（通用注册表，main.cpp 装配动作）
│   └── fileplugin.h / .cpp     # "@" 前缀文件搜索（异步，QtConcurrent）
└── ui/
    ├── resultdelegate.h / .cpp  # 结果项绘制（图标 + 两行 + 命中高亮）
    ├── settingsdialog.h / .cpp  # 设置对话框（热键/自启/插件启停，Catppuccin 风格）
    ├── hotkeyedit.h / .cpp      # 热键录制控件（WH_KEYBOARD_LL 低级钩子，支持系统保留键）
    └── helpdialog.h / .cpp      # 帮助对话框（快捷键/前缀/命令三区，替代 QMessageBox）
tests/
└── test_matcher.cpp            # Matcher 单测（Qt Test，ctest 运行）
third_party/
└── QHotkey/                    # Git submodule，跨平台全局热键库
docs/
├── notes.md                    # 踩坑与注意事项（索引）
└── features.md                 # 功能规划（索引）
```

**数据流：** 搜索框输入 → 防抖（停顿 ~100ms）→ 按 `triggerPrefix` 路由到匹配的插件（前缀已剥离）→ `IPlugin::query` → 跨插件统一打分排序（含 frecency）、截断 → 结果列表 → 用户回车 → 仅**产出该结果的插件** `IPlugin::execute`（`Ctrl+Enter` 则 `executeAlt`）→ 隐藏窗口

---

## 添加新插件

1. 在 `src/plugins/` 新建 `myplugin.h` / `myplugin.cpp`，实现 `IPlugin`：

```cpp
class MyPlugin : public IPlugin {
public:
    QString           name()  const override { return "MyPlugin"; }
    // 前缀触发：返回非空前缀（如 "="），MainWindow 仅在输入以此开头时调用本插件，
    // 并已剥离前缀；全局插件不重写此函数（默认返回空串）。
    QString           triggerPrefix() const override { return "="; }
    QList<ResultItem> query(const QString &keyword) override { /* 打分排序由共享 ranker/MainWindow 统一处理，这里只筛选+填 score */ }
    void              execute(const ResultItem &item) override { /* 不能阻塞 UI 线程 */ }
};
```

- 打分用 `Matcher::score`（`src/core/matcher.h`）填入 `ResultItem::score`，排序/截断由 MainWindow 跨插件统一处理，插件**不必**自己排序或限制条数。
- `execute` 只会对**产出该结果的插件**触发（`ResultItem::owner` 由 MainWindow 标记），插件间互不串扰。

2. 将 `.cpp` 加入 `CMakeLists.txt` 的 `qt_add_executable` 源文件列表
3. 在 `main.cpp` 注册：`win.addPlugin(new MyPlugin)`

跨平台差异用 `#ifdef Q_OS_WIN` / `#else` 隔离。
