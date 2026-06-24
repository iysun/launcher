# `Alt+Space` 是 Windows 系统保留键

**现象：** 将 `Alt+Space` 注册为全局热键不会崩溃（编译器正确时），但该键是 Windows 窗口菜单的系统快捷键。

**原因：** Windows 对 `Alt+Space` 有系统级拦截，某些情况下注册可能失败（非崩溃，仅注册无效）。

**正确做法：** 编译器版本正确时可正常使用；若注册失败（`QHotkey::isRegistered()` 返回 false），改为 `Ctrl+Space`。
