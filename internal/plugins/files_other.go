//go:build !windows

package plugins

import (
	"os"
	"path/filepath"
)

func searchRoots() []string {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil
	}
	var roots []string
	for _, sub := range []string{"Desktop", "Documents", "Downloads", "Pictures"} {
		roots = appendUnique(roots, filepath.Join(home, sub))
	}
	return roots
}
