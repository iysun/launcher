//go:build !windows

package main

func startHotkeyCapture(func(string)) bool { return false }

func stopHotkeyCapture() {}
