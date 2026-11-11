//go:build windows

package main

import (
	"os/exec"
	"syscall"
)

func startDetached(exe string) error {
	cmd := exec.Command("cmd.exe", "/C", "ping 127.0.0.1 -n 2 >NUL & start \"\" \""+exe+"\"")
	cmd.SysProcAttr = &syscall.SysProcAttr{
		CreationFlags: 0x00000008 | 0x01000000, // DETACHED_PROCESS | CREATE_BREAKAWAY_FROM_JOB
		HideWindow:    true,
	}
	return cmd.Start()
}
