cmake_minimum_required(VERSION 3.20)
project(Launcher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_subdirectory(third_party/QHotkey)

qt_add_executable(launcher
    src/main.cpp
    src/mainwindow.cpp
    src/plugins/appplugin.cpp
    src/ui/resultdelegate.cpp
)

target_include_directories(launcher PRIVATE src)
target_link_libraries(launcher PRIVATE Qt6::Widgets qhotkey)

# MinGW 不像 MSVC 自动链接 COM 库，需显式链接（IShellLink / CLSID_ShellLink）
if(WIN32)
    target_link_libraries(launcher PRIVATE ole32 oleaut32 uuid)
endif()
