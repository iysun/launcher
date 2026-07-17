cmake_minimum_required(VERSION 3.20)
project(Launcher VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 导出 compile_commands.json，供 clangd / eglot / vscode 提供跳转、补全、诊断
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test Concurrent)

add_subdirectory(third_party/QHotkey)

# libvterm：纯 C99 终端内核（VT 解析 + 屏幕网格状态），vendored 源码，MinGW 直接编。
# 只负责终端语义；PTY(ConPTY)/渲染(QPainter)/输入编码由本项目实现。
file(GLOB VTERM_SRC ${CMAKE_SOURCE_DIR}/third_party/libvterm/src/*.c)
add_library(vterm STATIC ${VTERM_SRC})
target_include_directories(vterm
    PUBLIC  ${CMAKE_SOURCE_DIR}/third_party/libvterm/include
    PRIVATE ${CMAKE_SOURCE_DIR}/third_party/libvterm/src)
# libvterm 用了 GCC 符号可见性属性，在 PE/COFF 上不适用，静音其告警
if(WIN32)
    target_compile_options(vterm PRIVATE -w)
endif()

qt_add_executable(launcher WIN32
    src/main.cpp
    src/mainwindow.cpp
    src/core/matcher.cpp
    src/core/pinyin.cpp
    src/core/pinyin_data.cpp
    src/core/usagestore.cpp
    src/core/appsettings.cpp
    src/core/i18n.cpp
    src/core/theme.cpp
    src/plugins/appplugin.cpp
    src/plugins/commandplugin.cpp
    src/plugins/fileplugin.cpp
    src/plugins/runplugin.cpp
    src/plugins/webplugin.cpp
    src/ui/resultdelegate.cpp
    src/ui/settingsdialog.cpp
    src/ui/hotkeyedit.cpp
    src/ui/helpdialog.cpp
    src/pty/conpty.cpp
    src/pty/ptyreader.cpp
    src/vt/terminalcore.cpp
    src/ui/terminalview.cpp
    src/ui/terminalwindow.cpp
    resources/resources.qrc
)

target_include_directories(launcher PRIVATE src)
target_link_libraries(launcher PRIVATE Qt6::Widgets Qt6::Concurrent qhotkey vterm)

# MinGW 不像 MSVC 自动链接 COM 库，需显式链接（IShellLink / CLSID_ShellLink）
if(WIN32)
    target_link_libraries(launcher PRIVATE ole32 oleaut32 uuid)
    # 解锁 ConPTY 声明：mingw-w64 把 CreatePseudoConsole/HPCON 等门控在
    # NTDDI_VERSION >= NTDDI_WIN10_RS5 (0x0A000006, 1809)，光设 _WIN32_WINNT 不够
    target_compile_definitions(launcher PRIVATE _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A000006)
endif()

# ── 测试 ──────────────────────────────────────────────────────
# Matcher 为纯函数，单独编进测试可执行文件，仅依赖 Qt6::Test（含 Core）
enable_testing()

qt_add_executable(matcher_test
    tests/test_matcher.cpp
    src/core/matcher.cpp
    src/core/pinyin.cpp
    src/core/pinyin_data.cpp
)
target_include_directories(matcher_test PRIVATE src)
target_link_libraries(matcher_test PRIVATE Qt6::Test)
add_test(NAME matcher_test COMMAND matcher_test)

# UsageStore（frecency）+ Theme（外观模式/颜色兜底/分类）单测。Theme::init 走 QPainter/
# styleHints，须 QGuiApplication，故链 Qt6::Gui 并用 offscreen 平台；含 resources.qrc 以便
# :/themes/*.json 可用（AUTORCC 自动编译）。
qt_add_executable(core_test
    tests/test_core.cpp
    src/core/theme.cpp
    src/core/usagestore.cpp
    resources/resources.qrc
)
target_include_directories(core_test PRIVATE src)
target_link_libraries(core_test PRIVATE Qt6::Gui Qt6::Test)
add_test(NAME core_test COMMAND core_test)

# TerminalCore scrollback 单测：喂字节流断言历史缓冲，纯逻辑无窗口（guiless）。
# QColor 需要 Qt6::Gui 的头与符号，但不需要 QGuiApplication 实例。
qt_add_executable(vt_test
    tests/test_vt.cpp
    src/vt/terminalcore.cpp
)
target_include_directories(vt_test PRIVATE src)
target_link_libraries(vt_test PRIVATE Qt6::Gui Qt6::Test vterm)
add_test(NAME vt_test COMMAND vt_test)
