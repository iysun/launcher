//go:build windows

package core

import "golang.org/x/sys/windows/registry"

// RemoveLegacyAutostart deletes the Qt build's Run entry ("launcher"),
// which is a different value from the one MyGo writes.
func RemoveLegacyAutostart() {
	k, err := registry.OpenKey(registry.CURRENT_USER, `Software\Microsoft\Windows\CurrentVersion\Run`, registry.SET_VALUE)
	if err != nil {
		return
	}
	defer k.Close()
	_ = k.DeleteValue("launcher")
}
