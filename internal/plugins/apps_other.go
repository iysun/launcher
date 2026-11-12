//go:build !windows

package plugins

import (
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/iysun/launcher/internal/core"
)

func scanInstalled() []indexedApp {
	home, _ := os.UserHomeDir()
	dirs := []string{"/usr/share/applications"}
	if home != "" {
		dirs = append(dirs, filepath.Join(home, ".local", "share", "applications"))
	}
	var items []indexedApp
	for _, dir := range dirs {
		entries, err := os.ReadDir(dir)
		if err != nil {
			continue
		}
		for _, e := range entries {
			if e.IsDir() || !strings.HasSuffix(strings.ToLower(e.Name()), ".desktop") {
				continue
			}
			b, err := os.ReadFile(filepath.Join(dir, e.Name()))
			if err != nil {
				continue
			}
			name, execLine, icon, ok := parseDesktop(string(b))
			if !ok {
				continue
			}
			iconPath := ""
			if icon != "" {
				if st, err := os.Stat(icon); err == nil && !st.IsDir() {
					iconPath = icon
				}
			}
			items = append(items, indexedApp{
				name:     name,
				action:   execLine,
				subtitle: execLine,
				icon:     iconPath,
				py:       core.ComputePinyin(name),
			})
		}
	}
	return items
}

func expandPath(path string) string { return path }

func launchAction(action, _ string) {
	cmd := stripFieldCodes(action)
	if cmd == "" {
		return
	}
	c := exec.Command("/bin/sh", "-c", cmd)
	if err := c.Start(); err != nil {
		log.Printf("launch %s: %v", cmd, err)
	}
}
