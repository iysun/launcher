package plugins

import (
	"net/url"

	"github.com/egoist/mygo"
	"github.com/iysun/launcher/internal/plugin"
)

// Engine is one web search destination.
type Engine struct {
	ID   string
	Name string
	URL  string
}

// Engines is the built-in search catalog. Order in settings.json selects among these ids.
func Engines() []Engine {
	return []Engine{
		{ID: "google", Name: "Google", URL: "https://www.google.com/search?q="},
		{ID: "bing", Name: "Bing", URL: "https://www.bing.com/search?q="},
		{ID: "baidu", Name: "Baidu", URL: "https://www.baidu.com/s?wd="},
		{ID: "github", Name: "GitHub", URL: "https://github.com/search?q="},
	}
}

func EngineName(id string) string {
	for _, e := range Engines() {
		if e.ID == id {
			return e.Name
		}
	}
	return id
}

// Web offers one result per search engine. Score steps of 100 keep the
// user's order ahead of frecency.
type Web struct {
	Order func() []string
}

func (w Web) Name() string   { return "Web" }
func (w Web) Prefix() string { return "?" }
func (w Web) Async() bool    { return false }

func (w Web) Query(keyword string) []plugin.Result {
	if keyword == "" {
		return nil
	}
	order := w.Order()
	if len(order) == 0 {
		for _, e := range Engines() {
			order = append(order, e.ID)
		}
	}
	score := len(order) * 100
	var out []plugin.Result
	for _, id := range order {
		if EngineName(id) == id && !knownEngine(id) {
			continue
		}
		out = append(out, plugin.Result{
			Title:    "在 " + EngineName(id) + " 中搜索",
			Subtitle: keyword,
			Action:   "web:" + id,
			Score:    score,
		})
		score -= 100
	}
	return out
}

func (w Web) Execute(item plugin.Result) {
	id := item.Action
	if len(id) > 4 {
		id = id[4:]
	}
	for _, e := range Engines() {
		if e.ID == id {
			_ = mygo.Shell.OpenExternal(e.URL + url.QueryEscape(item.Subtitle))
			return
		}
	}
}

func (Web) ExecuteAlt(plugin.Result) {}

func knownEngine(id string) bool {
	for _, e := range Engines() {
		if e.ID == id {
			return true
		}
	}
	return false
}
