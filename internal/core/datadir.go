package core

import (
	"os"
	"path/filepath"
	"runtime"
)

// DataDir is the directory of settings.json and usage.json.
// It matches the Qt build's AppDataLocation for the application name "launcher".
func DataDir() string {
	switch runtime.GOOS {
	case "windows":
		if app := os.Getenv("APPDATA"); app != "" {
			return filepath.Join(app, "launcher")
		}
	case "darwin":
		if home, err := os.UserHomeDir(); err == nil {
			return filepath.Join(home, "Library", "Application Support", "launcher")
		}
	default:
		if home, err := os.UserHomeDir(); err == nil {
			return filepath.Join(home, ".local", "share", "launcher")
		}
	}
	return "launcher"
}
