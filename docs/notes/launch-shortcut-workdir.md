# 启动应用要打开 .lnk，而不是解析出的 exe

**现象**：从 launcher 启动 Emacs，`default-directory` 变成 launcher 自己的当前目录。

**原因**：`ShellExecuteW` 的 `lpDirectory` 传 NULL 时子进程继承调用者 cwd；快捷方式里记录的「起始位置」和参数只有在打开 `.lnk` 本身时才会被应用。

**做法**：展示与图标仍用解析后的目标 exe，执行时把 `.lnk` 路径交给 `Shell.OpenPath`。
