# aqt 安装的 MinGW 编译时报 `0xC0000135`（缺 DLL）

**现象：** 用 aqtinstall 装的 `tools_mingw1310` 配置 CMake 时，编译器自检失败；直接跑 `cc1plus.exe` 退出码 `0xC0000135`（`-1073741515`，STATUS_DLL_NOT_FOUND）。

**原因：** `cc1plus.exe` 位于 `libexec/` 下，运行时依赖 `bin/` 里的 `libisl/libgmp/libmpfr/libwinpthread` 等 DLL；这些目录不在 `PATH` 上时加载失败。

**正确做法：** 配置/编译/部署前，把 `<Qt>\Tools\mingw1310_64\bin` 加入 `PATH`（PowerShell：`$env:PATH = "$MINGW;$env:PATH"`）。
