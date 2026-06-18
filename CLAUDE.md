# Launcher — Claude Code Guide

## Build

```powershell
# Configure (one-time)
cmake -S . -B build -G "MinGW Makefiles" `
    -DCMAKE_PREFIX_PATH="D:\Qt\6.8.3\mingw_64" `
    -DCMAKE_CXX_COMPILER="D:\Qt\Tools\mingw1310_64\bin\g++.exe" `
    -DCMAKE_MAKE_PROGRAM="D:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5

# Build
D:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe -C build -j8

# Deploy Qt DLLs (first run only)
D:\Qt\6.8.3\mingw_64\bin\windeployqt.exe build\launcher.exe
```

## Critical: Compiler Version

**Always use MinGW 13.1.0 at `D:\Qt\Tools\mingw1310_64\`.**

Qt 6.8.3 was compiled with MinGW 13.1. Using any other MinGW version (e.g. Scoop's 16.1) causes a C++ ABI mismatch that crashes at runtime with heap corruption (0xC0000374) inside `QHotkey` construction.

## Project Structure

```
src/
├── main.cpp
├── mainwindow.h / .cpp     # Frameless floating window, Alt+Space toggle
├── plugin/
│   ├── iplugin.h           # IPlugin interface
│   └── resultitem.h        # ResultItem struct (Q_DECLARE_METATYPE)
└── plugins/
    └── appplugin.h / .cpp  # Scans Start Menu (.lnk) on Windows
third_party/
└── QHotkey/                # Git submodule — global hotkey library
```

## Adding a Plugin

1. Create `src/plugins/myplugin.h` / `.cpp` implementing `IPlugin`
2. Add the `.cpp` to `add_executable` sources in `CMakeLists.txt`
3. Register in `main.cpp`: `win.addPlugin(new MyPlugin)`

## Known Issues / Notes

- `Alt+Space` is a Windows reserved key (system window menu) but QHotkey handles it correctly when compiled with the right MinGW — do not change the hotkey to work around a build issue.
- `qt_add_executable` is used instead of `add_executable + WIN32_EXECUTABLE` to avoid `Qt6EntryPoint` ABI issues with mismatched MinGW.
- QHotkey is a git submodule at `third_party/QHotkey`. Run `git submodule update --init` after a fresh clone.
