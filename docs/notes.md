# 注意事项

开发过程中遇到的踩坑、限制与架构决策。
**每次遇到新问题，必须在提交代码前追加到本文件。**

记录格式：简述现象 → 原因 → 正确做法。

---

## 编译器版本必须与 Qt 预编译包配套

**现象：** 使用非配套 MinGW 版本编译后，程序在 `QHotkey` 构造函数内部崩溃，退出码 `0xC0000374`（堆损坏）。

**原因：** Qt 预编译包的 C++ ABI 与编译器版本强绑定。MinGW 16.1（Scoop）与 Qt 6.8.3 所用的 MinGW 13.1 ABI 不兼容，运行时破坏堆布局。

**正确做法：** 必须使用与 Qt 配套的编译器，Qt 6.8.3 对应 MinGW 13.1.0（可通过 `aqt install-tool ... tools_mingw1310` 安装）。

---

## 用 `qt_add_executable` 而非 `add_executable`

**现象：** 使用 `add_executable` + `WIN32_EXECUTABLE TRUE` 时链接报错：`undefined reference to '__imp___argc'`。

**原因：** `WIN32_EXECUTABLE TRUE` 会强制链接 `Qt6EntryPoint`，该库与非配套 MinGW 的 CRT（msvcrt vs ucrt）存在符号冲突。

**正确做法：** 使用 `qt_add_executable`，Qt 会自动处理入口点，无需手动设置 `WIN32_EXECUTABLE`。

---

## CMake 4.x 需要传 `CMAKE_POLICY_VERSION_MINIMUM`

**现象：** CMake 4.x 配置时报错，提示 QHotkey 子目录的 `cmake_minimum_required` 版本过低。

**原因：** CMake 4.0 移除了对 `cmake_minimum_required(VERSION < 3.5)` 的向后兼容支持，QHotkey 的 CMakeLists.txt 未及时更新。

**正确做法：** 配置时追加 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`。

---

## `Alt+Space` 是 Windows 系统保留键

**现象：** 将 `Alt+Space` 注册为全局热键不会崩溃（编译器正确时），但该键是 Windows 窗口菜单的系统快捷键。

**原因：** Windows 对 `Alt+Space` 有系统级拦截，某些情况下注册可能失败（非崩溃，仅注册无效）。

**正确做法：** 编译器版本正确时可正常使用；若注册失败（`QHotkey::isRegistered()` 返回 false），改为 `Ctrl+Space`。
