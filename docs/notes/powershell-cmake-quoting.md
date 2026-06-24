# PowerShell 下 cmake 的 `-D...=值` 必须整体加引号

**现象：** 在 PowerShell 里执行配置命令，cmake 报 `Invalid CMAKE_POLICY_VERSION_MINIMUM value "3". A numeric major.minor[.patch[.tweak]] must be given.`——传进去的 `3.5` 变成了 `3`。

**原因：** 形如 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` 不加引号时，PowerShell 在向原生命令（cmake.exe）传参的过程中会把含小数点的尾部 `.5` 截断，实际只传了 `3`。

**正确做法：** 把**整个** `-D...=值` 用引号包起来，让 PowerShell 原样透传：

```powershell
cmake -S . -B build -G "MinGW Makefiles" `
    "-DCMAKE_PREFIX_PATH=$QT_DIR" `
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
```

注意是引号包住 `-D键=值` 整体，而非只给值加引号（`-DKEY="$VAL"` 这种形式仍可能踩坑）。

更稳妥的写法是用**数组传参**，把每个参数作为独立元素整体加引号，再 splatting 给 cmake：

```powershell
$args = @(
    "-S", ".", "-B", "build", "-G", "MinGW Makefiles",
    "-DCMAKE_PREFIX_PATH=$QT_DIR",
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
)
cmake @args
```

