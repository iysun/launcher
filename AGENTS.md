# Launcher — AI Agent 指引

跨平台快速启动器，参考 uTools / PowerToys Run，基于 Qt6 + C++17 实现，优先支持 Windows 和 Linux。

## AI 工具（harness 入口）

本仓库已配好 Claude Code 的项目级工具，优先用它们而非临场拼命令：

| 入口 | 用途 |
|------|------|
| `/build` | 构建-修复循环：配置 → 编译 → 读错误对照已知坑修复 → 重编，直到通过 |
| `/runapp` | 运行-验证循环：启动应用并按清单核对 Alt+Space / 搜索 / 导航等核心交互 |
| `add-plugin` 技能 | 新插件流水线：生成 IPlugin 骨架 → 注册 CMake/main.cpp → 构建验证 → 文档判断 |

> 构建依赖的本机 Qt/MinGW 路径由 `.claude/settings.local.json` 注入（`QT_DIR` / `MINGW`，不入库，需各自配置）。详见下方「环境变量」小节。

## 维护约定（文档）

文档拆成「索引 + 按需读取的细粒度文件」：`docs/notes.md`、`docs/features.md` 是索引，正文在 `docs/notes/`、`docs/features/` 下，一条一文件。

- 改动若涉及**新踩坑 / 限制 / 架构决策** → 判断是否值得沉淀，值得就在 `docs/notes/` 加一篇并在 `docs/notes.md` 索引补一行
- 改动若**改变了功能边界**（新增 / 调整 / 完成功能）→ 更新 `docs/features.md` 及 `docs/features/` 对应文件
- **是否更新文档由你按改动性质自行判断**：纯重构、小修小补、不影响行为的改动无需更新；文档与代码可在同一次提交内完成
- 其他文档也可以自己判断是否需要改动

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

### 环境变量

| 变量 | 说明 | 示例（Windows） |
|------|------|------|
| `QT_DIR` | Qt 安装目录，精确到架构/工具链那一层 | `D:\Qt\6.8.3\mingw_64` |
| `MINGW` | 与 Qt 二进制配套的 MinGW `bin` 目录（**仅 Windows** 需要，版本必须与 Qt 官方发布的编译器一致，见 [docs/notes/mingw-abi-mismatch.md](docs/notes/mingw-abi-mismatch.md)） | `D:\Qt\Tools\mingw1310_64\bin` |

这两个变量被 `CMakePresets.json`（`$env{QT_DIR}` / `$env{MINGW}`）、根目录 `Makefile`、`.claude/settings.local.json`（AI harness）共同依赖。缺失时的典型报错是 `CMAKE_CXX_COMPILER: /g++.exe is not a full path...`（`MINGW` 被解析成空字符串），容易误以为是编译器没装。

- **AI harness（Claude Code）**：由 `.claude/settings.local.json` 的 `env` 字段注入，该文件不入库，需各自配置；改动后需重启会话才生效。
- **人工终端**：每次新开终端都要重新设置，否则会报上面那个报错；建议写进 shell 启动脚本持久化一次：
  - PowerShell：加到 `$PROFILE`（`$env:QT_DIR = "..."` / `$env:MINGW = "..."`），新开的终端自动生效
  - Bash/Zsh（Linux）：加到 `~/.bashrc` / `~/.zshrc`；Linux 一般不需要 `MINGW`，仅当 Qt 装在非标准路径时设 `CMAKE_PREFIX_PATH`

---

## 构建步骤

### 快捷方式：Makefile

根目录 `Makefile` 把下面的 `cmake --preset` / `ctest --preset` 命令收敛成短别名，跨平台通过 `$(OS)` 自动选 `windows`/`linux` preset（可用 `PRESET=xxx` 覆盖），执行前仍需设置好上面的 `QT_DIR`/`MINGW`（Windows），否则 `configure` 会被内置的 `check-env` 目标提前拦截并给出提示：

| 命令 | 作用 |
|------|------|
| `make` / `make build` | 配置 + 编译（默认目标） |
| `make configure` | 仅配置 CMake（首次或 `CMakeLists.txt` 变更后） |
| `make run` | 构建后运行 launcher |
| `make test` | 跑 ctest（等价于 `ctest --preset windows/linux`） |
| `make deploy` | 部署 Qt 运行库（`windeployqt`，仅 Windows） |
| `make clean` | 删除 `build/` 目录 |
| `make rebuild` | `clean` + `build` |
| `make submodules` | 初始化 git 子模块（QHotkey） |
| `make help` | 列出所有目标 |

Windows 上用 `& "$env:MINGW\mingw32-make.exe" <target>`（与实际编译器版本配套）或 PATH 上任意 GNU Make 均可——Makefile 本身只编排 cmake 命令，不直接参与编译，所以不要求是特定的 make 实现。

### Windows（底层命令）

不想用 Makefile，或需要理解 Makefile 具体做了什么时，可参考以下等价的原始命令。推荐用 **CMakePresets**（`CMakePresets.json` 已入库，需 CMake ≥ 3.21；仅装 3.20 的用下方 `<details>` 手写命令），把机器相关路径留在环境变量里、preset 只引用，免去手敲一长串 `-D`：

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
ctest --preset windows   # 或 linux；等价于 make test
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
│   ├── appsettings.h / .cpp    # 用户配置持久化（settings.json）：热键、开机自启、插件启停、文件搜索目录
│   ├── i18n.h / .cpp           # 极简 i18n：datadir/i18n/<code>.json，见下方「自定义语言」
│   └── theme.h / .cpp          # 极简主题：datadir/themes/<code>.json，见下方「自定义主题」
├── plugin/
│   ├── iplugin.h               # IPlugin 接口（含 triggerPrefix 前缀路由）
│   └── resultitem.h            # ResultItem 数据结构（含 owner / score，已注册 QMetaType）
├── plugins/
│   ├── appplugin.h / .cpp      # 应用搜索（Start Menu .lnk / .desktop，拼音匹配）
│   ├── commandplugin.h / .cpp  # "/" 前缀命令插件（通用注册表，main.cpp 装配动作）
│   ├── fileplugin.h / .cpp     # "@" 前缀文件搜索（异步，QtConcurrent）
│   ├── runplugin.h / .cpp      # ":" 前缀命令执行（runner 由 main.cpp 装配，写入内置终端）
│   └── webplugin.h / .cpp      # "?" 前缀网页搜索（Google/Bing/Baidu/GitHub）
├── pty/
│   ├── conpty.h / .cpp         # Windows ConPTY 封装（起 shell、读写管道、resize、拆解）
│   └── ptyreader.h / .cpp      # worker 线程：阻塞 ReadFile ConPTY 输出，QueuedConnection 投给 GUI 线程
├── vt/
│   └── terminalcore.h / .cpp   # libvterm 封装（唯一 include vterm.h）：喂字节/读单元格/键盘编码，回调转 Qt 信号
└── ui/
    ├── resultdelegate.h / .cpp  # 结果项绘制（图标 + 两行 + 命中高亮）
    ├── settingsdialog.h / .cpp  # 设置对话框（热键/自启/插件启停，Catppuccin 风格）
    ├── hotkeyedit.h / .cpp      # 热键录制控件（WH_KEYBOARD_LL 低级钩子，支持系统保留键）
    ├── helpdialog.h / .cpp      # 帮助对话框（快捷键/前缀/命令三区，替代 QMessageBox）
    └── terminalview.h / .cpp    # 终端渲染 + 输入（QPainter 逐格自绘，Qt 按键→libvterm 编码）
tests/
└── test_matcher.cpp            # Matcher 单测（Qt Test，ctest 运行）
third_party/
├── QHotkey/                    # Git submodule，跨平台全局热键库
└── libvterm/                   # vendored 纯 C99 终端内核（VT 解析/屏幕状态），MinGW 直接编（CMake 需启用 C 语言）
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

---

## 自定义语言

`I18n`（`src/core/i18n.h` / `.cpp`）是极简 i18n：语言包是**用户数据目录**（`QStandardPaths::AppDataLocation`）下的 `i18n/<code>.json` 文件，不是编译期资源，运行时可直接编辑/新增。

- 内置 `zh`（简体中文）/ `en`（English）两个语言包源文件在 `resources/i18n/zh.json`、`resources/i18n/en.json`，通过 `resources/resources.qrc` 的 `/i18n` prefix 打进 Qt 资源；首次运行时 `I18n::ensureDefaultFiles()` 会把它们落盘到 `<datadir>/i18n/`（`/settings` 命令的 "打开配置/数据目录" 可以直接定位到这里）。落盘只在目标文件不存在时写入，不会覆盖用户已编辑的内容。
- 语言文件格式：
  ```json
  {
      "meta": { "code": "ja", "name": "日本語" },
      "strings": { "settings.title": "設定", "...": "..." }
  }
  ```
  `meta.code` 是语言标识（存进 `settings.json` 的 `language` 字段），`meta.name` 是设置页语言下拉框里显示的名字。
- **新增一个语言**：在 `<datadir>/i18n/` 下新建一个 `<code>.json`（照抄 `zh.json`/`en.json` 的 key 结构，把 value 换成目标语言），`I18n::availableLanguages()` 会在设置页打开时自动扫描该目录下所有 `*.json`，无需改代码、无需重新编译。
- **翻译不全时的回退链**：当前语言缺 key → 回退到内置英文（`m_englishFallback`，永远从 `:/i18n/en.json` 加载，不受用户是否改过 datadir 里的 `en.json` 影响）→ 仍缺则直接显示 key 本身（如 `settings.title`），便于一眼发现遗漏而不是空白/崩溃。
- **语言切换不做运行时热切换**：`I18n::init()` 只在启动时调用一次；`/settings` 保存时若语言变更，会弹窗询问是否立即重启进程（`SettingsDialog::save()`）。
- 新增字符串 key：先在 `resources/i18n/zh.json`、`resources/i18n/en.json` 两个内置文件里都补上（缺了会走英文/key 回退，不会崩溃，但用户自定义语言文件不会自动同步，需要使用者自行补齐）。

---

## 自定义主题

`Theme`（`src/core/theme.h` / `.cpp`）形状与 `I18n` 完全一致：主题包是**用户数据目录**下的 `themes/<code>.json` 文件，不是编译期资源，运行时可直接编辑/新增。

- 内置 `mocha`（深色，Catppuccin Mocha）/ `latte`（浅色，Catppuccin Latte）/ `dracula`（深色）/ `nord`（深色）四套主题源文件在 `resources/themes/*.json`，通过 `resources/resources.qrc` 的 `/themes` prefix 打进 Qt 资源；首次运行时 `Theme::ensureDefaultFiles()` 会把它们落盘到 `<datadir>/themes/`。落盘只在目标文件不存在时写入，不会覆盖用户已编辑的内容。
- 主题文件格式：
  ```json
  {
      "meta": { "code": "solarized", "name": "Solarized", "appearance": "dark" },
      "colors": {
          "bg": "#002b36", "surface": "#073642", "border": "#586e75",
          "text": "#839496", "overlay": "#657b83", "accent": "#268bd2",
          "hoverBg": "#00212b", "highlight": "#b58900",
          "accentHover": "#2aa1f2", "danger": "#dc322f"
      }
  }
  ```
  `meta.code` 是主题标识，`meta.name` 是设置页主题下拉框里显示的名字，`meta.appearance`（`"dark"` / `"light"`）决定这套主题出现在设置页的"深色主题"下拉框还是"浅色主题"下拉框里——**缺失或非法值一律按 `"dark"` 归类**，不会导致主题从下拉框里消失，只是可能出现在不符合预期的那一个分类下，新增自定义主题时应显式填对。`colors` 是一张扁平的颜色角色表，10 个角色缺一不可（缺的角色会退回内置 mocha 的对应色，仍缺则显示醒目的洋红 `#ff00ff`，便于一眼发现遗漏而不是隐形崩溃）。
- **新增一个主题**：在 `<datadir>/themes/` 下新建一个 `<code>.json`（照抄内置文件的 `colors` 结构，把颜色值换成目标配色，别忘了填 `meta.appearance`），`Theme::availableThemes()` 会在设置页打开时自动扫描该目录下所有 `*.json`，无需改代码、无需重新编译。
- **颜色角色缺失时的回退链**：当前主题缺角色 → 回退到内置 mocha 的对应角色 → 仍缺则用醒目的洋红 `#ff00ff`。
- **外观模式（类 Zed 设计）**：设置页不是直接选一个具体主题，而是三项联动——「外观」下拉框选深色 / 浅色 / 跟随系统（`settings.json` 的 `appearanceMode` 字段），以及分别为深色、浅色各配一个默认主题（`darkTheme` / `lightTheme` 字段，出厂默认 `mocha` / `latte`）。`Theme::init(appearanceMode, darkCode, lightCode)` 在启动时解析出最终生效的主题：`appearanceMode` 为 `"dark"`/`"light"` 时直接用对应字段；为 `"system"` 时用 `QGuiApplication::styleHints()->colorScheme()`（Qt 6.5+ 跨平台 API）查一次当前系统深浅色偏好，查不到（部分不支持系统级查询的 Linux 桌面环境返回 `Unknown`）时按深色兜底。
- **不做运行时热切换**：`Theme::init()` 只在启动时调用一次——包括"跟随系统"，也只在这一刻解析，运行期间系统深浅色再变化不会让已打开的 launcher 跟着变，需要重新启动才会按最新系统状态重新解析。三项外观设置（外观模式/深色主题/浅色主题）与语言切换共用同一条重启确认（`SettingsDialog::save()`：任一变更都会弹窗询问是否立即重启进程，只弹一次）。
- 勾选框对勾、下拉箭头这两个 `QSS image: url(...)` 引用的小图标不是编译期烘焙的 PNG（主题数据驱动、用户可任意新增后没法为每个主题预生成），而是 `Theme::init()` 里按解析出的最终主题色用 `QPainter` 现画，落盘到 `<datadir>/theme-cache/`，通过磁盘绝对路径引用（详见 [docs/notes/qss-checkbox-checkmark.md](docs/notes/qss-checkbox-checkmark.md)）。
