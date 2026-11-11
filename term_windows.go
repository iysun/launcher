//go:build windows

package main

import (
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/egoist/mygo/plugins/terminal"
	"github.com/egoist/mygo/ui"
	"github.com/iysun/launcher/internal/core"
)

type termTab struct {
	term *terminal.Terminal
	name string
}

type termState struct {
	wrap   *terminal.Terminal
	tabs   []*termTab
	index  int
	seq    int
	closed map[*terminal.Terminal]bool
}

func (l *launcher) runWrap(cmd string) {
	cmd = strings.TrimSpace(cmd)
	if cmd == "" {
		return
	}
	if l.mode == modeTerm {
		l.exitTerminal()
	}
	if l.terms.wrap == nil {
		t, err := terminal.New(terminal.Options{
			Command: wrapCommand(),
			OnKey:   func(ui.Modifiers, ui.Key) bool { return true },
			OnExit: func(int) {
				if l.panel == nil {
					return
				}
				l.panel.Update(func() { l.terms.wrap = nil })
			},
		})
		if err != nil {
			log.Print(err)
			return
		}
		l.terms.wrap = t
	}
	l.mode = modeWrap
	l.panel.SetContentSize(panelWidth, searchH+wrapOutH)
	l.place(false)
	l.query = ":"
	l.clearResults()
	l.terms.wrap.Feed([]byte("\x1b[3J\x1b[2J\x1b[H"))
	l.terms.wrap.Send([]byte(cmd + "\r"))
	l.input.Focus()
	l.shownAt = time.Now()
	if !l.panel.IsVisible() {
		l.panel.Show()
		l.panel.Focus()
	}
}

func (l *launcher) openPersistent(cmd string) {
	l.enterTerminal(cmd, true)
}

func (l *launcher) enterTerminal(cmd string, newTab bool) {
	if l.mode == modeWrap {
		l.leaveWrap(false)
	}
	l.mode = modeTerm
	l.panel.SetContentSize(termW, termH)
	l.place(true)
	if newTab || len(l.terms.tabs) == 0 {
		l.openTab(cmd)
	} else if strings.TrimSpace(cmd) != "" {
		if tab := l.currentTab(); tab != nil {
			tab.term.Send([]byte(strings.TrimSpace(cmd) + "\r"))
		}
	}
	l.shownAt = time.Now()
	if !l.panel.IsVisible() {
		l.panel.Show()
	}
	l.panel.Focus()
}

func (l *launcher) openTab(cmd string) {
	l.terms.seq++
	seq := l.terms.seq
	var t *terminal.Terminal
	t, err := terminal.New(terminal.Options{
		Command: interactiveCommand(),
		OnTitle: func(title string) {
			if l.panel == nil {
				return
			}
			l.panel.Update(func() {
				for _, tab := range l.terms.tabs {
					if tab.term == t {
						tab.name = tabLabel(title, seq)
					}
				}
			})
		},
		OnExit: func(int) {
			if l.panel == nil {
				return
			}
			l.panel.Update(func() { l.forget(t, false) })
		},
	})
	if err != nil {
		log.Print(err)
		return
	}
	l.terms.tabs = append(l.terms.tabs, &termTab{
		term: t,
		name: fmt.Sprintf("Terminal %d", seq),
	})
	l.terms.index = len(l.terms.tabs) - 1
	if strings.TrimSpace(cmd) != "" {
		t.Send([]byte(strings.TrimSpace(cmd) + "\r"))
	}
}

func (l *launcher) currentTab() *termTab {
	if l.terms.index < 0 || l.terms.index >= len(l.terms.tabs) {
		return nil
	}
	return l.terms.tabs[l.terms.index]
}

func (l *launcher) forget(t *terminal.Terminal, closeProc bool) {
	if l.terms.closed[t] {
		return
	}
	if l.terms.closed == nil {
		l.terms.closed = map[*terminal.Terminal]bool{}
	}
	l.terms.closed[t] = true
	for i, tab := range l.terms.tabs {
		if tab.term != t {
			continue
		}
		l.terms.tabs = append(l.terms.tabs[:i], l.terms.tabs[i+1:]...)
		break
	}
	if l.terms.index >= len(l.terms.tabs) {
		l.terms.index = len(l.terms.tabs) - 1
	}
	if closeProc {
		_ = t.Close()
	}
	if len(l.terms.tabs) == 0 && l.mode == modeTerm {
		l.exitTerminal()
	}
}

func (l *launcher) closeTab(i int) {
	if i < 0 || i >= len(l.terms.tabs) {
		return
	}
	l.forget(l.terms.tabs[i].term, true)
}

func (l *launcher) exitTerminal() {
	if l.mode != modeTerm {
		return
	}
	l.mode = modeSearch
	l.query = ""
	l.clearResults()
	l.panel.SetContentSize(panelWidth, searchH)
	l.place(false)
	l.input.Focus()
}

func (l *launcher) leaveWrap(keep bool) {
	if l.mode != modeWrap {
		return
	}
	l.mode = modeSearch
	if l.terms.wrap != nil {
		_ = l.terms.wrap.Close()
		l.terms.wrap = nil
	}
	if !keep {
		l.query = ""
	}
	l.clearResults()
	l.panel.SetContentSize(panelWidth, searchH)
	l.place(false)
	l.input.Focus()
	if keep && strings.TrimSpace(l.query) != "" {
		l.scheduleQuery()
	}
}

func (l *launcher) exitWrap() { l.leaveWrap(false) }

func (l *launcher) closeTerms() {
	if l.terms.wrap != nil {
		_ = l.terms.wrap.Close()
		l.terms.wrap = nil
	}
	for _, tab := range l.terms.tabs {
		_ = tab.term.Close()
	}
	l.terms.tabs = nil
}

func (l *launcher) termKeys(c *ui.Context) {
	if c.Shortcut(ui.Ctrl, ui.KeyBackquote) {
		l.exitTerminal()
		return
	}
	if c.Shortcut(ui.Ctrl|ui.Shift, ui.KeyT) {
		l.openTab("")
		return
	}
	if c.Shortcut(ui.Ctrl|ui.Shift, ui.KeyW) {
		l.closeTab(l.terms.index)
		return
	}
	if c.Shortcut(ui.Ctrl|ui.Shift, ui.KeyTab) {
		l.cycleTab(-1)
		return
	}
	if c.Shortcut(ui.Ctrl, ui.KeyTab) {
		l.cycleTab(1)
		return
	}
	digits := []ui.Key{ui.Key1, ui.Key2, ui.Key3, ui.Key4, ui.Key5, ui.Key6, ui.Key7, ui.Key8, ui.Key9}
	for i, key := range digits {
		if c.Shortcut(ui.Alt, key) && i < len(l.terms.tabs) {
			l.terms.index = i
			return
		}
	}
}

func (l *launcher) cycleTab(delta int) {
	n := len(l.terms.tabs)
	if n == 0 {
		return
	}
	l.terms.index = (l.terms.index + delta + n) % n
}

func (l *launcher) wrapKeys(c *ui.Context) {
	if c.Shortcut(0, ui.KeyEscape) {
		l.exitWrap()
		return
	}
	if c.Shortcut(ui.Ctrl, ui.KeyEnter) {
		l.rerunWrap(true)
		return
	}
	if c.Shortcut(0, ui.KeyEnter) {
		l.rerunWrap(false)
	}
}

func (l *launcher) rerunWrap(toTerm bool) {
	cmd := strings.TrimSpace(l.query)
	cmd = strings.TrimPrefix(cmd, ":")
	cmd = strings.TrimSpace(cmd)
	if cmd == "" {
		return
	}
	if toTerm {
		l.openPersistent(cmd)
		return
	}
	l.runWrap(cmd)
}

func (l *launcher) termBody(c *ui.Context) {
	t := c.Theme()
	ui.Column(c).Fill().Children(func() {
		if len(l.terms.tabs) > 1 {
			ui.Row(c).Height(32).Padding(4, 8).Gap(4).AlignItems(ui.Center).Children(func() {
				for i := range l.terms.tabs {
					i := i
					tab := l.terms.tabs[i]
					label := tab.name
					bg := ui.Transparent
					if i == l.terms.index {
						bg = t.Accent.Alpha(0.85)
					}
					ui.Row(c).Padding(2, 8).Gap(6).Radius(6).Background(bg).AlignItems(ui.Center).Children(func() {
						if ui.Button(c, label).Clicked() {
							l.terms.index = i
						}
						if ui.Button(c, "×").Clicked() {
							l.closeTab(i)
						}
					})
				}
				if ui.Button(c, "+").Clicked() {
					l.openTab("")
				}
			})
		}
		if tab := l.currentTab(); tab != nil {
			terminal.View(c, tab.term).Grow(1).AutoFocus()
		}
	})
}

func (l *launcher) wrapOutput(c *ui.Context) {
	if l.terms.wrap == nil {
		return
	}
	terminal.View(c, l.terms.wrap).Height(float32(wrapOutH))
}

func tabLabel(title string, seq int) string {
	title = strings.TrimSpace(title)
	if title == "" {
		return fmt.Sprintf("Terminal %d", seq)
	}
	base := title
	if i := strings.LastIndexAny(title, `\/`); i >= 0 && i < len(title)-1 {
		base = title[i+1:]
	}
	if base == "" {
		return fmt.Sprintf("Terminal %d", seq)
	}
	return base
}

func interactiveCommand() []string {
	exe := findShell()
	base := strings.ToLower(filepath.Base(exe))
	if base == "cmd" || base == "cmd.exe" {
		return []string{exe}
	}
	hook := ensureCwdHook()
	if hook == "" {
		return []string{exe}
	}
	return []string{exe, "-NoExit", "-File", hook}
}

func wrapCommand() []string {
	exe := findShell()
	base := strings.ToLower(filepath.Base(exe))
	if base == "cmd" || base == "cmd.exe" {
		return []string{exe}
	}
	return []string{exe, "-NoLogo", "-NoProfile", "-NoExit", "-Command",
		"if ($PSStyle) { $PSStyle.OutputRendering = 'Ansi' }; function prompt { '' }"}
}

func findShell() string {
	for _, name := range []string{"pwsh", "powershell", "cmd"} {
		if p, err := exec.LookPath(name); err == nil {
			return p
		}
	}
	return "cmd.exe"
}

func ensureCwdHook() string {
	dir := filepath.Join(core.DataDir(), "shell")
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return ""
	}
	path := filepath.Join(dir, "pwsh-cwd-title.ps1")
	script := "if ($PSStyle) { $PSStyle.OutputRendering = 'Ansi' }\r\n" +
		"$global:__lnPrevPrompt = $function:prompt\r\n" +
		"function global:prompt {\r\n" +
		"    try { $Host.UI.RawUI.WindowTitle = $ExecutionContext.SessionState.Path.CurrentLocation.ProviderPath } catch {}\r\n" +
		"    if ($global:__lnPrevPrompt) { & $global:__lnPrevPrompt } else { \"PS $($ExecutionContext.SessionState.Path.CurrentLocation.Path)> \" }\r\n" +
		"}\r\n"
	if err := os.WriteFile(path, []byte(script), 0o644); err != nil {
		return ""
	}
	return path
}
