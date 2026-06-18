# Launcher

A lightweight cross-platform application launcher (Windows & Linux), inspired by uTools and PowerToys Run.

Built with **Qt6 + C++17**.

## Features

- `Alt+Space` global hotkey to show/hide
- Frameless floating window, auto-hides on focus loss
- Keyboard navigation (↑ ↓ Enter Esc)
- Plugin architecture for easy extension
- App search built-in (Start Menu on Windows, `.desktop` files on Linux)

## Requirements

- CMake 3.20+
- Qt 6.x (`Widgets` module)
- C++17 compiler
- Git (for FetchContent to fetch [QHotkey](https://github.com/Skycoder42/QHotkey))

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Project Structure

```
src/
├── main.cpp
├── mainwindow.h / .cpp     # Core window
├── plugin/
│   ├── iplugin.h           # Plugin interface
│   └── resultitem.h        # Result data struct
└── plugins/
    └── appplugin.h / .cpp  # Built-in app launcher plugin
```

## Adding a Plugin

Implement `IPlugin` and register it in `main.cpp`:

```cpp
class MyPlugin : public IPlugin {
public:
    QString           name()  const override { return "MyPlugin"; }
    QList<ResultItem> query(const QString &keyword) override { /* ... */ }
    void              execute(const ResultItem &item) override { /* ... */ }
};

// main.cpp
win.addPlugin(new MyPlugin);
```

## Roadmap

- [ ] Calculator plugin (`= 1+2*3`)
- [ ] File search plugin
- [ ] Web search plugin
- [ ] System tray icon
- [ ] Settings UI
