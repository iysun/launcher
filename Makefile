.DEFAULT_GOAL := build

BUILD_DIR := build

ifeq ($(OS),Windows_NT)
PRESET ?= windows
EXE     := $(BUILD_DIR)/launcher.exe
else
PRESET ?= linux
EXE     := $(BUILD_DIR)/launcher
endif

.PHONY: all build configure run test deploy clean rebuild submodules check-env help

all: build

submodules:
	git submodule update --init

# QT_DIR/MINGW 由 .claude/settings.local.json 或手动 $env: 注入；未设置时 CMakePresets.json
# 里的 $env{MINGW}/g++.exe 会静默塌成 "/g++.exe"，报错难以定位，这里提前给出明确提示
check-env:
ifeq ($(OS),Windows_NT)
	$(if $(QT_DIR),,$(error QT_DIR is not set. Set it to your Qt install dir, e.g. $$env:QT_DIR = "D:\Qt\6.8.3\mingw_64" (see AGENTS.md)))
	$(if $(MINGW),,$(error MINGW is not set. Set it to your MinGW bin dir, e.g. $$env:MINGW = "D:\Qt\Tools\mingw1310_64\bin" (see AGENTS.md)))
endif

# 以 CMakeCache.txt 是否存在作为文件依赖，重复 make build 不会每次都重新 configure
$(BUILD_DIR)/CMakeCache.txt: | check-env submodules
	cmake --preset $(PRESET)

configure: $(BUILD_DIR)/CMakeCache.txt

build: configure
	cmake --build --preset $(PRESET)

run: build
	$(EXE)

test: build
	ctest --preset $(PRESET)

ifeq ($(OS),Windows_NT)
deploy: build
	$(QT_DIR)/bin/windeployqt.exe $(EXE)
endif

clean:
	cmake -E rm -rf $(BUILD_DIR)

rebuild: clean build

help:
	@echo "Launcher dev shortcuts (make <target>):"
	@echo "  build        configure + compile (default target, plain 'make' works)"
	@echo "  configure    configure CMake only (first run, or after CMakeLists changes)"
	@echo "  run          build then run launcher"
	@echo "  test         run ctest (matcher_test)"
	@echo "  deploy       deploy Qt runtime DLLs via windeployqt (Windows only)"
	@echo "  clean        remove the build/ directory"
	@echo "  rebuild      clean + build"
	@echo "  submodules   init git submodules (QHotkey)"
	@echo "  help         show this help"
	@echo ""
	@echo "Override the preset with PRESET=xxx (current default: $(PRESET))"
