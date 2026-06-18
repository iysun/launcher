# Launcher — Agent Guide

Cross-platform application launcher built with Qt6/C++17, targeting Windows and Linux.
Inspired by uTools / PowerToys Run.

## Architecture

| Component | Role |
|-----------|------|
| `MainWindow` | Frameless, always-on-top widget. Hides on focus loss. |
| `IPlugin` | Interface every plugin must implement (`query` + `execute`). |
| `ResultItem` | POD struct passed between plugins and the UI. |
| `QHotkey` | Third-party library (submodule) for cross-platform global hotkeys. |

**Data flow:** `SearchBar::textChanged` → `IPlugin::query` (all plugins) → `showResults` → user picks → `IPlugin::execute` → `hide()`

## Extending

### New plugin checklist

- [ ] Implement `IPlugin` in `src/plugins/`
- [ ] Add source file to `CMakeLists.txt` `qt_add_executable` block
- [ ] Instantiate and register in `main.cpp`
- [ ] `query()` returns at most ~8 items; filter by `keyword` case-insensitively
- [ ] `execute()` must not block the UI thread — use `QProcess::startDetached` or similar

### Platform guards

```cpp
#ifdef Q_OS_WIN
    // Windows-specific code
#else
    // Linux-specific code
#endif
```

## Toolchain (Windows)

| Item | Path |
|------|------|
| Qt 6.8.3 | `D:\Qt\6.8.3\mingw_64` |
| MinGW 13.1 (required) | `D:\Qt\Tools\mingw1310_64\bin` |
| windeployqt | `D:\Qt\6.8.3\mingw_64\bin\windeployqt.exe` |

> Using any MinGW version other than 13.1 causes a runtime crash (ABI mismatch with Qt 6.8.3 binaries).

## Submodules

```bash
git submodule update --init   # after fresh clone
```

## Planned plugins

| Plugin | Trigger | Status |
|--------|---------|--------|
| AppPlugin | any text | ✅ done |
| CalcPlugin | `= <expr>` | planned |
| FilePlugin | `/` or `~` | planned |
| WebPlugin | `? <query>` | planned |
