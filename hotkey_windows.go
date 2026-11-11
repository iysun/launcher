//go:build windows

package main

import (
	"fmt"
	"strings"
	"sync"
	"syscall"
	"unsafe"

	"golang.org/x/sys/windows"
)

const (
	whKeyboardLL = 13
	wmKeydown    = 0x0100
	wmSyskeydown = 0x0104
	llkhfUp      = 0x80
)

type kbdLLHook struct {
	vkCode   uint32
	scanCode uint32
	flags    uint32
	time     uint32
	extra    uintptr
}

var (
	user32       = windows.NewLazySystemDLL("user32.dll")
	procSetHook  = user32.NewProc("SetWindowsHookExW")
	procUnhook   = user32.NewProc("UnhookWindowsHookEx")
	procCallNext = user32.NewProc("CallNextHookEx")
	procKeyState = user32.NewProc("GetAsyncKeyState")

	hookMu     sync.Mutex
	hookHandle uintptr
	hookFn     func(string)
	hookCB     uintptr
)

func startHotkeyCapture(fn func(string)) bool {
	stopHotkeyCapture()
	hookMu.Lock()
	defer hookMu.Unlock()
	hookFn = fn
	if hookCB == 0 {
		hookCB = syscall.NewCallback(lowLevelKey)
	}
	h, _, _ := procSetHook.Call(whKeyboardLL, hookCB, 0, 0)
	hookHandle = h
	if h == 0 {
		hookFn = nil
		return false
	}
	return true
}

func stopHotkeyCapture() {
	h, _ := takeHook()
	if h != 0 {
		procUnhook.Call(h)
	}
}

func takeHook() (uintptr, func(string)) {
	hookMu.Lock()
	defer hookMu.Unlock()
	h, fn := hookHandle, hookFn
	hookHandle, hookFn = 0, nil
	return h, fn
}

func lowLevelKey(nCode, wParam, lParam uintptr) uintptr {
	if int32(nCode) >= 0 && (wParam == wmKeydown || wParam == wmSyskeydown) {
		k := (*kbdLLHook)(unsafe.Pointer(lParam))
		if k.flags&llkhfUp == 0 {
			if seq, ok, swallow := chord(k.vkCode); ok {
				if swallow {
					go deliverHotkey(seq)
					return 1
				}
			}
		}
	}
	r, _, _ := procCallNext.Call(0, nCode, wParam, lParam)
	return r
}

func deliverHotkey(seq string) {
	h, fn := takeHook()
	if h != 0 {
		procUnhook.Call(h)
	}
	if fn != nil {
		fn(seq)
	}
}

func keyDown(vk uintptr) bool {
	r, _, _ := procKeyState.Call(vk)
	return r&0x8000 != 0
}

// chord reports the accelerator, whether the key is one we understand,
// and whether recording should swallow it. A bare Escape cancels.
func chord(vk uint32) (seq string, ok, swallow bool) {
	if isModifier(vk) {
		return "", false, false
	}
	name, known := keyName(vk)
	if !known {
		return "", false, false
	}
	var parts []string
	if keyDown(0x5B) || keyDown(0x5C) {
		parts = append(parts, "Super")
	}
	if keyDown(0x11) || keyDown(0xA2) || keyDown(0xA3) {
		parts = append(parts, "Ctrl")
	}
	if keyDown(0x12) || keyDown(0xA4) || keyDown(0xA5) {
		parts = append(parts, "Alt")
	}
	if keyDown(0x10) || keyDown(0xA0) || keyDown(0xA1) {
		parts = append(parts, "Shift")
	}
	if name == "Escape" && len(parts) == 0 {
		return "", true, true
	}
	parts = append(parts, name)
	return strings.Join(parts, "+"), true, true
}

func isModifier(vk uint32) bool {
	switch vk {
	case 0x10, 0x11, 0x12, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0x5B, 0x5C:
		return true
	default:
		return false
	}
}

func keyName(vk uint32) (string, bool) {
	switch vk {
	case 0x20:
		return "Space", true
	case 0x09:
		return "Tab", true
	case 0x0D:
		return "Enter", true
	case 0x1B:
		return "Escape", true
	case 0x08:
		return "Backspace", true
	case 0x2E:
		return "Delete", true
	case 0x2D:
		return "Insert", true
	case 0x24:
		return "Home", true
	case 0x23:
		return "End", true
	case 0x21:
		return "PageUp", true
	case 0x22:
		return "PageDown", true
	case 0x25:
		return "Left", true
	case 0x26:
		return "Up", true
	case 0x27:
		return "Right", true
	case 0x28:
		return "Down", true
	case 0xBD:
		return "-", true
	case 0xBB:
		return "=", true
	}
	if vk >= 0x70 && vk <= 0x87 {
		return fmt.Sprintf("F%d", vk-0x70+1), true
	}
	if vk >= '0' && vk <= '9' {
		return string(rune(vk)), true
	}
	if vk >= 'A' && vk <= 'Z' {
		return string(rune(vk)), true
	}
	return "", false
}
