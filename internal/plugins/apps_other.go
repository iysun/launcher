//go:build !windows

package plugins

import (
	"log"

	"github.com/egoist/mygo"
	"github.com/iysun/launcher/internal/core"
)

func scanInstalled() []indexedApp {
	apps, err := mygo.Shell.InstalledApps()
	if err != nil {
		log.Print(err)
		return nil
	}
	items := make([]indexedApp, 0, len(apps))
	for _, app := range apps {
		action := app.Path
		if action == "" {
			action = app.ID
		}
		items = append(items, indexedApp{
			name:     app.Name,
			action:   action,
			subtitle: action,
			icon:     action,
			py:       core.ComputePinyin(app.Name),
			sys:      app,
			useSys:   true,
		})
	}
	return items
}

func expandPath(path string) string { return path }
