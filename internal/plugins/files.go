package plugins

import (
	"io/fs"
	"os"
	"path/filepath"
	"strings"

	"github.com/egoist/mygo"
	"github.com/iysun/launcher/internal/core"
	"github.com/iysun/launcher/internal/plugin"
)

const (
	maxFileVisits  = 8000
	maxFileResults = 200
)

// Files searches names under the user's desktop, documents, downloads and pictures.
type Files struct{}

func (Files) Name() string   { return "Files" }
func (Files) Prefix() string { return "@" }
func (Files) Async() bool    { return true }

func (Files) Query(keyword string) []plugin.Result {
	keyword = strings.TrimSpace(keyword)
	if keyword == "" {
		return nil
	}
	var out []plugin.Result
	visited := 0
	for _, root := range searchRoots() {
		info, err := os.Stat(root)
		if err != nil || !info.IsDir() {
			continue
		}
		_ = filepath.WalkDir(root, func(path string, d fs.DirEntry, err error) error {
			if err != nil {
				if d != nil && d.IsDir() {
					return fs.SkipDir
				}
				return nil
			}
			if path == root {
				return nil
			}
			visited++
			if visited > maxFileVisits || len(out) >= maxFileResults {
				return fs.SkipAll
			}
			name := d.Name()
			score := core.Score(name, keyword)
			if score < 0 {
				return nil
			}
			out = append(out, plugin.Result{
				Title:    name,
				Subtitle: path,
				Action:   path,
				Score:    score,
				IconPath: path,
			})
			return nil
		})
		if visited > maxFileVisits || len(out) >= maxFileResults {
			break
		}
	}
	return out
}

func (Files) Execute(item plugin.Result) {
	if item.Action != "" {
		_ = mygo.Shell.OpenPath(item.Action)
	}
}

func (Files) ExecuteAlt(item plugin.Result) {
	if item.Action != "" {
		mygo.Shell.ShowItemInFolder(item.Action)
	}
}

func appendUnique(roots []string, dir string) []string {
	if dir == "" {
		return roots
	}
	for _, r := range roots {
		if strings.EqualFold(r, dir) {
			return roots
		}
	}
	return append(roots, dir)
}
