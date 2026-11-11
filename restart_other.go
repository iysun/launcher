//go:build !windows

package main

import "os/exec"

func startDetached(exe string) error {
	return exec.Command("sh", "-c", "sleep 0.5; exec \"$0\"", exe).Start()
}
