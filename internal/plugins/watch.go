package plugins

import (
	"context"
	"log"
	"os"
	"path/filepath"
	"runtime"

	"github.com/egoist/mygo"
)

// WatchAppDirs reports when installed-application directories change.
// The callback runs on the watcher's goroutine.
func WatchAppDirs(ctx context.Context, onChange func()) {
	for _, dir := range appWatchDirs() {
		info, err := os.Stat(dir)
		if err != nil || !info.IsDir() {
			continue
		}
		go func(dir string) {
			w, err := mygo.WatchFiles(ctx, dir, mygo.FileWatchOptions{Recursive: true, HintsOnly: true})
			if err != nil {
				log.Printf("watch %s: %v", dir, err)
				return
			}
			defer w.Close()
			for {
				if _, err := w.Next(ctx); err != nil {
					return
				}
				onChange()
			}
		}(dir)
	}
}

func appWatchDirs() []string {
	switch runtime.GOOS {
	case "windows":
		var dirs []string
		if app := os.Getenv("APPDATA"); app != "" {
			dirs = append(dirs, filepath.Join(app, "Microsoft", "Windows", "Start Menu", "Programs"))
		}
		return append(dirs, `C:\ProgramData\Microsoft\Windows\Start Menu\Programs`)
	case "linux":
		home, _ := os.UserHomeDir()
		return []string{
			"/usr/share/applications",
			filepath.Join(home, ".local", "share", "applications"),
		}
	default:
		return nil
	}
}
