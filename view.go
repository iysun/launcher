package main

import (
	"strings"

	"github.com/egoist/mygo"
	"github.com/egoist/mygo/ui"
)

func (l *launcher) view(c *ui.Context) {
	t := c.Theme()
	root := c.Root()
	if c.Vibrancy() {
		root.Background(ui.Transparent)
	} else {
		root.Background(t.Background)
	}
	if l.query != "" && c.Shortcut(0, ui.KeyEscape) {
		l.query = ""
		l.clearResults()
	}
	if c.Shortcut(0, ui.KeyDown) || c.Shortcut(ui.Ctrl, ui.KeyN) {
		l.choose(l.chosen + 1)
	}
	if c.Shortcut(0, ui.KeyUp) || c.Shortcut(ui.Ctrl, ui.KeyP) {
		l.choose(l.chosen - 1)
	}
	if c.Shortcut(ui.Ctrl, ui.KeyEnter) {
		l.flushAndOpen(true)
	} else if c.Shortcut(0, ui.KeyEnter) {
		l.flushAndOpen(false)
	}
	if c.Shortcut(ui.Ctrl, ui.KeyG) {
		l.panel.Hide()
	}

	ui.Column(c).Fill().Children(func() {
		ui.Row(c).Height(float32(searchH)).Padding(0, 18).Gap(12).AlignItems(ui.Center).Children(func() {
			ui.Text(c, "⌕").FontSize(20).TextColor(t.TextMuted)
			in := ui.TextInputBase(c, &l.query).Bind(&l.input).Grow(1).FontSize(22).
				Placeholder("输入关键词搜索, /help 获取帮助").Label("搜索")
			in.OnTextKey(func(k ui.TextKey) bool { return l.onTextKey(in, k) })
			if in.Submitted() {
				l.flushAndOpen(false)
			}
			if in.Changed() {
				l.scheduleQuery()
			}
		})
		if len(l.results) == 0 && (strings.TrimSpace(l.query) == "" || l.timer != nil) {
			return
		}
		ui.Divider(c)
		if len(l.results) == 0 {
			ui.Row(c).Height(float32(rowH)).Padding(0, 16).AlignItems(ui.Center).Children(func() {
				ui.Text(c, "没有匹配结果").TextColor(t.TextMuted)
			})
		} else {
			l.list.Selected = &l.chosen
			l.list.Key = func(i int) any { return l.results[i].Action }
			l.list.Label = func(i int) string { return l.results[i].Title }
			ui.List(c, &l.list, len(l.results), func(i int) { l.row(c, i) }).Height(float32(len(l.results) * rowH))
		}
		hint := "↑↓ 选择    ↵ 打开    Ctrl+↵ 次级    esc 关闭"
		if l.shortcut != "" {
			hint = l.shortcut + " 唤起    " + hint
		}
		ui.Row(c).Height(float32(footH)).Padding(0, 16).AlignItems(ui.Center).Children(func() {
			ui.Text(c, hint).FontSize(11).TextColor(t.TextMuted)
		})
	})
}

func (l *launcher) row(c *ui.Context, i int) {
	item := l.results[i]
	t := c.Theme()
	bg := ui.Transparent
	text, muted := t.Text, t.TextMuted
	if i == l.chosen {
		bg = t.Accent.Alpha(0.85)
		text, muted = t.AccentText, t.AccentText.Alpha(0.75)
	}
	row := ui.Row(c).Height(float32(rowH)).Padding(0, 10).Gap(12).AlignItems(ui.Center).Radius(8).Background(bg)
	if row.DoubleClicked() {
		l.chosen = i
		l.activate(false)
	} else if row.Clicked() {
		l.chosen = i
	}
	row.Children(func() {
		if icon := l.icon(item.IconPath); icon != nil {
			ui.Image(c, icon).Size(iconSize, iconSize)
		} else {
			ui.Avatar(c, item.Title, nil).Size(iconSize, iconSize)
		}
		ui.Column(c).Grow(1).Gap(1).Children(func() {
			l.title(c, item.Title, item.Highlights, text, t.Accent)
			if item.Subtitle != "" {
				ui.Text(c, item.Subtitle).FontSize(11).TextColor(muted).SingleLine().TruncateMiddle()
			}
		})
	})
}

func (l *launcher) title(c *ui.Context, title string, hits []int, base, hi ui.Color) {
	if len(hits) == 0 {
		ui.Text(c, title).TextColor(base).Bold().SingleLine()
		return
	}
	set := map[int]bool{}
	for _, h := range hits {
		set[h] = true
	}
	runes := []rune(title)
	var spans []ui.Span
	for i := 0; i < len(runes); {
		hit := set[i]
		j := i + 1
		for j < len(runes) && set[j] == hit {
			j++
		}
		sp := ui.Span{Text: string(runes[i:j]), Weight: 650, Color: base}
		if hit {
			sp.Color = hi
		}
		spans = append(spans, sp)
		i = j
	}
	ui.RichText(c, spans...).SingleLine()
}

func (l *launcher) icon(path string) *ui.Bitmap {
	if path == "" {
		return nil
	}
	if b, ok := l.icons[path]; ok {
		return b
	}
	scale := 1.0
	for _, d := range mygo.Screen.Displays() {
		if d.ScaleFactor > scale {
			scale = d.ScaleFactor
		}
	}
	var bitmap *ui.Bitmap
	if data, err := mygo.Shell.FileIcon(path, int(iconSize*scale)); err == nil {
		bitmap, _ = ui.DecodeBitmap(data)
	}
	l.icons[path] = bitmap
	return bitmap
}
