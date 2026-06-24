# 注意事项（索引）

开发过程中遇到的踩坑、限制与架构决策。每条独立成文，本文件只是**索引**——按需点开对应文件，不必整份读完。

> 遇到值得沉淀的新坑：在 `docs/notes/` 加一篇（格式 现象→原因→正确做法），并在下面补一行索引。是否值得沉淀由改动性质判断，纯小修无需记录。

- [编译器版本必须与 Qt 预编译包配套](notes/mingw-abi-mismatch.md) — 用非配套 MinGW 编译会导致 QHotkey 构造函数堆损坏崩溃（`0xC0000374`）
- [用 qt_add_executable 而非 add_executable](notes/qt-add-executable.md) — 链接报 `undefined reference to '__imp___argc'` 时看这篇
- [CMake 4.x 需要传 CMAKE_POLICY_VERSION_MINIMUM](notes/cmake-policy-version.md) — 配置时报 QHotkey `cmake_minimum_required` 过低
- [MinGW 用 COM（IShellLink）需显式链接 + 初始化](notes/mingw-com-ishelllink.md) — 解析 `.lnk` 报 `undefined reference to CoCreateInstance` 或运行时全失败
- [aqt 安装的 MinGW 编译报 0xC0000135（缺 DLL）](notes/aqt-mingw-missing-dll.md) — 编译器自检失败 / `cc1plus.exe` 退出码 `0xC0000135` 时看这篇
- [delegate 行高必须与列表 kItemH 同步](notes/delegate-itemheight-sync.md) — 改了 sizeHint 后弹窗高度被裁切或留白
- [PowerShell 下 cmake 的 -D 参数必须整体加引号](notes/powershell-cmake-quoting.md) — 报 `Invalid ... value "3"`（3.5 被吞成 3）时看这篇
- [Alt+Space 是 Windows 系统保留键](notes/altspace-reserved-key.md) — 全局热键注册失败（不崩溃仅无效）时回退 Ctrl+Space
