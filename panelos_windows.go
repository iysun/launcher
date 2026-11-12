//go:build windows

package main

import "golang.org/x/sys/windows"

// Win10 没有 DWM 圆角，透明窗口还会沿矩形边画 1px 边框，这两样都要绕开。
func plainPanel() bool {
	return windows.RtlGetVersion().BuildNumber < 22000
}
