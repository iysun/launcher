package plugins

import (
	"strings"
	"sync"

	"github.com/egoist/mygo"
	"github.com/iysun/launcher/internal/core"
	"github.com/iysun/launcher/internal/plugin"
)

const (
	pinyinFullPenalty = 50
	pinyinInitPenalty = 120
)

type indexedApp struct {
	name     string
	action   string
	subtitle string
	icon     string
	py       core.Pinyin
	launch   string
}

// Apps searches installed applications. On Windows that is the Start Menu
// shortcuts, with the shortcut resolved to the program path.
type Apps struct {
	mu     sync.RWMutex
	items  []indexedApp
	launch map[string]string
}

func (a *Apps) Name() string   { return "AppLauncher" }
func (a *Apps) Prefix() string { return "" }
func (a *Apps) Async() bool    { return false }

func (a *Apps) Reload() {
	items := scanInstalled()
	launch := make(map[string]string, len(items))
	for _, it := range items {
		if it.launch != "" {
			launch[it.action] = it.launch
		}
	}
	a.mu.Lock()
	a.items = items
	a.launch = launch
	a.mu.Unlock()
}

func (a *Apps) Query(keyword string) []plugin.Result {
	keyword = strings.TrimSpace(keyword)
	if keyword == "" {
		return nil
	}
	a.mu.RLock()
	defer a.mu.RUnlock()
	var out []plugin.Result
	for _, it := range a.items {
		best := core.Score(it.name, keyword)
		usePinyin, useInitials := false, false
		if it.py.Full != "" {
			if s := core.Score(it.py.Full, keyword) - pinyinFullPenalty; s > best {
				best, usePinyin, useInitials = s, true, false
			}
		}
		if it.py.Initials != "" {
			if s := core.Score(it.py.Initials, keyword) - pinyinInitPenalty; s > best {
				best, usePinyin, useInitials = s, true, true
			}
		}
		if best < 0 {
			continue
		}
		item := plugin.Result{
			Title:    it.name,
			Subtitle: it.subtitle,
			Action:   it.action,
			Score:    best,
			IconPath: it.icon,
		}
		if usePinyin {
			item.HighlightSet = true
			item.Highlights = core.PinyinTitlePositions(it.py, keyword, useInitials)
		}
		out = append(out, item)
	}
	return out
}

func (a *Apps) Execute(item plugin.Result) {
	a.mu.RLock()
	launch := a.launch[item.Action]
	a.mu.RUnlock()
	launchAction(item.Action, launch)
}

func (a *Apps) ExecuteAlt(item plugin.Result) {
	if item.Action != "" {
		mygo.Clipboard.WriteText(item.Action)
	}
}
