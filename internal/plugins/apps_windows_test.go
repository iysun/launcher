//go:build windows

package plugins

import (
	"os"
	"strings"
	"testing"
)

func TestScanInstalledResolvesTargets(t *testing.T) {
	items := scanInstalled()
	if len(items) == 0 {
		t.Fatal("expected start menu shortcuts")
	}
	var resolved int
	for _, it := range items {
		if strings.Contains(strings.ToLower(it.action), "shell:appsfolder") {
			t.Fatalf("action still a shell path: %s", it.action)
		}
		if !strings.EqualFold(ext(it.action), ".lnk") {
			resolved++
		}
		if it.subtitle == "" {
			t.Fatalf("empty subtitle for %s", it.name)
		}
	}
	if resolved == 0 {
		t.Fatal("no shortcut resolved to a target path")
	}
	opened := 0
	for _, it := range items {
		if _, err := os.Stat(it.subtitle); err == nil {
			opened++
			break
		}
	}
	if opened == 0 {
		t.Fatal("no resolved target exists on disk")
	}
}

func ext(path string) string {
	if i := strings.LastIndex(path, "."); i >= 0 {
		return path[i:]
	}
	return ""
}
