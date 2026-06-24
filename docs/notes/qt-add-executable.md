# 用 `qt_add_executable` 而非 `add_executable`

**现象：** 使用 `add_executable` + `WIN32_EXECUTABLE TRUE` 时链接报错：`undefined reference to '__imp___argc'`。

**原因：** `WIN32_EXECUTABLE TRUE` 会强制链接 `Qt6EntryPoint`，该库与非配套 MinGW 的 CRT（msvcrt vs ucrt）存在符号冲突。

**正确做法：** 使用 `qt_add_executable`，Qt 会自动处理入口点，无需手动设置 `WIN32_EXECUTABLE`。
