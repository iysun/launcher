cmake_minimum_required(VERSION 3.20)
project(Launcher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

include(FetchContent)
FetchContent_Declare(QHotkey
    GIT_REPOSITORY https://github.com/Skycoder42/QHotkey.git
    GIT_TAG 1.5.0
)
FetchContent_MakeAvailable(QHotkey)

add_executable(launcher
    src/main.cpp
    src/mainwindow.cpp
    src/plugins/appplugin.cpp
)

target_include_directories(launcher PRIVATE src)
target_link_libraries(launcher PRIVATE Qt6::Widgets qhotkey)

if(WIN32)
    set_target_properties(launcher PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
