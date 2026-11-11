package main

import (
	"cmp"
	"slices"
	"strings"
	"time"

	"github.com/iysun/launcher/internal/core"
	"github.com/iysun/launcher/internal/plugin"
)

func (l *launcher) clearResults() {
	if l.timer != nil {
		l.timer.Stop()
		l.timer = nil
	}
	l.gen++
	l.base = nil
	l.async = nil
	l.results = nil
	l.chosen = 0
	l.fit()
}

func (l *launcher) scheduleQuery() {
	if strings.TrimSpace(l.query) == "" {
		l.clearResults()
		return
	}
	if l.timer != nil {
		l.timer.Stop()
	}
	l.timer = time.AfterFunc(100*time.Millisecond, func() {
		l.panel.Update(func() {
			l.timer = nil
			l.runQuery()
		})
	})
}

func (l *launcher) flushAndOpen(alt bool) {
	if l.timer != nil {
		l.timer.Stop()
		l.timer = nil
		l.runQuery()
	}
	l.activate(alt)
}

func (l *launcher) runQuery() {
	if l.mode != modeSearch {
		return
	}
	if l.panel != nil && !l.panel.IsVisible() {
		return
	}
	kw := strings.TrimSpace(l.query)
	l.gen++
	gen := l.gen
	l.async = nil
	if kw == "" {
		l.base = nil
		l.results = nil
		l.chosen = 0
		l.fit()
		return
	}

	prefixActive := false
	for _, p := range l.plugs {
		if pre := p.Prefix(); pre != "" && strings.HasPrefix(kw, pre) {
			prefixActive = true
			break
		}
	}

	var sync []plugin.Result
	for _, p := range l.plugs {
		if !l.cfg.PluginEnabled(p.Name()) {
			continue
		}
		pre := p.Prefix()
		var sub string
		switch {
		case pre == "":
			if prefixActive {
				continue
			}
			sub = kw
		case strings.HasPrefix(kw, pre):
			sub = strings.TrimSpace(kw[len(pre):])
		default:
			continue
		}
		if p.Async() {
			go func(p plugin.Plugin, sub string, gen int) {
				items := p.Query(sub)
				l.panel.Update(func() {
					if gen != l.gen || !l.panel.IsVisible() {
						return
					}
					l.async = append(l.async, l.tag(p, sub, items)...)
					l.merge()
				})
			}(p, sub, gen)
			continue
		}
		sync = append(sync, l.tag(p, sub, p.Query(sub))...)
	}
	l.base = sync
	l.merge()
}

func (l *launcher) tag(p plugin.Plugin, keyword string, items []plugin.Result) []plugin.Result {
	for i := range items {
		items[i].Owner = p
		items[i].Score += l.usage.Bonus(items[i].Action)
		if !items[i].HighlightSet {
			items[i].Highlights = core.MatchedPositions(items[i].Title, keyword)
		}
	}
	return items
}

func (l *launcher) merge() {
	results := append([]plugin.Result{}, l.base...)
	results = append(results, l.async...)
	slices.SortStableFunc(results, func(a, b plugin.Result) int {
		if a.Score != b.Score {
			return cmp.Compare(b.Score, a.Score)
		}
		return strings.Compare(strings.ToLower(a.Title), strings.ToLower(b.Title))
	})
	if len(results) > maxResults {
		results = results[:maxResults]
	}
	l.results = results
	l.choose(0)
	l.fit()
}

func (l *launcher) choose(i int) {
	if len(l.results) == 0 {
		l.chosen = 0
		return
	}
	if i < 0 {
		i = 0
	}
	if i >= len(l.results) {
		i = len(l.results) - 1
	}
	l.chosen = i
	l.list.ScrollIntoView(l.chosen)
}

func (l *launcher) activate(alt bool) {
	if l.chosen < 0 || l.chosen >= len(l.results) {
		return
	}
	item := l.results[l.chosen]
	l.panel.Hide()
	if item.Owner == nil {
		return
	}
	if alt {
		item.Owner.ExecuteAlt(item)
		return
	}
	item.Owner.Execute(item)
	l.usage.Record(item.Action)
}
