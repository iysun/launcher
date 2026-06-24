# MinGW 用 COM（IShellLink）需显式链接 + 初始化

**现象：** 用 `CoCreateInstance(CLSID_ShellLink, IID_IShellLinkW, ...)` 解析 `.lnk` 时，MinGW 链接报 `undefined reference to CoCreateInstance / CLSID_ShellLink`；或运行时解析全部失败。

**原因：**（1）MSVC 靠 `#pragma comment(lib, ...)` 自动链接 COM 库，MinGW 不会；（2）COM 必须在使用前于本线程 `CoInitializeEx`。

**正确做法：**（1）`CMakeLists.txt` 中 `if(WIN32) target_link_libraries(launcher PRIVATE ole32 oleaut32 uuid)`；（2）在 `loadApps()`（主线程）开头 `CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)`，结束 `CoUninitialize()`。

**附加坑：** `IShellLinkW::GetPath` 对 UWP/Store/`.url`/文件夹快捷方式返回空 → 回退到 `.lnk` 路径，`ShellExecuteW` 对二者均可启动。
