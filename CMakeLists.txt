cmake_minimum_required(VERSION 3.20)
project(Launcher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 导出 compile_commands.json，供 clangd / eglot / vscode 提供跳转、补全、诊断
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test Concurrent)

add_subdirectory(third_party/QHotkey)

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
    src/plugins/webplugin.cpp
    src/ui/resultdelegate.cpp
    src/ui/settingsdialog.cpp
    src/ui/hotkeyedit.cpp
    src/ui/helpdialog.cpp
    resources/resources.qrc
)

target_include_directories(launcher PRIVATE src)
target_link_libraries(launcher PRIVATE Qt6::Widgets Qt6::Concurrent qhotkey)

# MinGW 不像 MSVC 自动链接 COM 库，需显式链接（IShellLink / CLSID_ShellLink）
if(WIN32)
    target_link_libraries(launcher PRIVATE ole32 oleaut32 uuid)
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
