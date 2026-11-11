//go:build windows

package plugins

import (
	"os"
	"path/filepath"

	"golang.org/x/sys/windows/registry"
)

func searchRoots() []string {
	names := []string{
		"Desktop",
		"Personal",
		"{374DE290-123F-4565-9164-39C4925E467B}",
		"My Pictures",
	}
	var roots []string
	k, err := registry.OpenKey(registry.CURRENT_USER, `Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders`, registry.QUERY_VALUE)
	if err == nil {
		defer k.Close()
		for _, name := range names {
			s, _, err := k.GetStringValue(name)
			if err != nil || s == "" {
				continue
			}
			roots = appendUnique(roots, os.ExpandEnv(s))
		}
	}
	if len(roots) > 0 {
		return roots
	}
	home, err := os.UserHomeDir()
	if err != nil {
		return nil
	}
	for _, sub := range []string{"Desktop", "Documents", "Downloads", "Pictures"} {
		roots = appendUnique(roots, filepath.Join(home, sub))
	}
	return roots
}
