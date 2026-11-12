package main

import (
	"bytes"
	"context"
	"image"
	"image/color"
	"image/draw"
	"image/png"
	"log"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	"github.com/egoist/mygo"
	"github.com/egoist/mygo/ui"
	"github.com/iysun/launcher/internal/core"
	"github.com/iysun/launcher/internal/plugin"
	"github.com/iysun/launcher/internal/plugins"
)

const (
	panelWidth  = 620
	searchH     = 56
	rowH        = 56
	footH       = 28
	iconSize    = 32
	maxResults  = 8
	panelRadius = 16
)

type plugDraft struct {
	Name  string
	Label string
	On    bool
}

type launcher struct {
	panel       *mygo.Window
	settingsWin *mygo.Window
	helpWin     *mygo.Window
	tray        *mygo.Tray
	shortcut    string

	cfg      *core.Settings
	usage    *core.Usage
	apps     *plugins.Apps
	commands *plugins.Commands
	plugs    []plugin.Plugin

	query   string
	results []plugin.Result
	chosen  int
	list    ui.ListState
	input   ui.Handle
	icons   map[string]*ui.Bitmap
	gen     int
	base    []plugin.Result
	async   []plugin.Result
	timer   *time.Timer

	draftHotkey  string
	draftAuto    bool
	draftPlugs   []plugDraft
	draftEngines []string
	settingsErr  string
	helpCmds     []plugins.Command

	watchCancel context.CancelFunc
	reloadTimer *time.Timer
	shownAt     time.Time
	trayClick   time.Time
	recording   bool
}

func main() {
	if !mygo.App.RequestSingleInstanceLock() {
		return
	}
	dir := core.DataDir()
	l := &launcher{
		cfg:   core.LoadSettings(filepath.Join(dir, "settings.json")),
		usage: core.LoadUsage(filepath.Join(dir, "usage.json")),
		apps:  &plugins.Apps{},
		icons: map[string]*ui.Bitmap{},
	}
	l.commands = &plugins.Commands{}
	l.commands.Add("help", "显示帮助", l.openHelp)
	l.commands.Add("quit", "退出 launcher", mygo.App.Quit)
	l.commands.Add("reload", "重新加载应用列表", l.reloadApps)
	l.commands.Add("datadir", "打开配置 / 数据目录", l.openData)
	l.commands.Add("restart", "重启 launcher", l.restart)
	l.commands.Add("settings", "打开设置", l.openSettings)
	web := plugins.Web{Order: func() []string { return l.cfg.WebEngineOrder }}
	l.plugs = []plugin.Plugin{
		l.apps,
		plugins.Files{},
		web,
		l.commands,
	}

	mygo.App.SetName("Launcher")
	mygo.App.SetVersion("0.1.0")
	mygo.App.SetActivationPolicy(mygo.ActivationPolicyAccessory)
	mygo.App.OnWindowAllClosed(func() {})
	mygo.App.OnSecondInstance(func([]string, string) { l.show() })
	mygo.App.WhenReady(l.ready)
	if err := mygo.App.Run(); err != nil {
		log.Fatal(err)
	}
}

func (l *launcher) ready() {
	core.RemoveLegacyAutostart()
	if l.cfg.Autostart {
		if err := mygo.App.SetOpenAtLogin(true); err != nil {
			log.Print(err)
		}
	}

	vibrancy := mygo.VibrancyGlass
	if plainPanel() {
		vibrancy = mygo.VibrancyNone
	}
	l.panel = mygo.NewWindow(mygo.WindowOptions{
		Title:         "Launcher",
		Panel:         true,
		Hidden:        true,
		Frameless:     true,
		Transparent:   true,
		Vibrancy:      vibrancy,
		CornerRadius:  panelRadius,
		Width:         panelWidth,
		Height:        searchH,
		DisableResize: true,
		DisableShadow: plainPanel(),
		Content:       ui.View(l.view),
	})
	l.panel.OnClose(func(e *mygo.CloseEvent) {
		e.PreventDefault()
		l.panel.Hide()
	})
	// 刚 Show 时 Windows 常常不把前台交给新进程，失焦会立刻到来。
	// 这段时间里藏起来，看起来就像窗口没了。
	l.panel.OnBlur(func() {
		if time.Since(l.shownAt) < 600*time.Millisecond {
			return
		}
		l.panel.Hide()
	})

	l.settingsWin = mygo.NewWindow(mygo.WindowOptions{
		Title:         "设置",
		Width:         480,
		Height:        640,
		Hidden:        true,
		Frameless:     true,
		AlwaysOnTop:   true,
		DisableResize: true,
		Content:       ui.View(l.settingsView),
	})
	l.settingsWin.OnBlur(func() { l.stopRecording() })
	l.helpWin = mygo.NewWindow(mygo.WindowOptions{
		Title:         "帮助",
		Width:         520,
		Height:        560,
		Hidden:        true,
		Frameless:     true,
		AlwaysOnTop:   true,
		DisableResize: true,
		Content:       ui.View(l.helpView),
	})

	l.registerInitialHotkey()
	l.syncTray()
	l.apps.Reload()
	l.watchApps()

	mygo.App.OnWillQuit(func(*mygo.QuitEvent) {
		if l.watchCancel != nil {
			l.watchCancel()
		}
		if l.tray != nil {
			l.tray.Destroy()
		}
	})

	if !mygo.App.WasOpenedAtLogin() {
		l.show()
	}
}

func (l *launcher) registerInitialHotkey() {
	seen := map[string]bool{}
	for _, seq := range []string{l.cfg.Hotkey, "Ctrl+Space", "Alt+Shift+Space"} {
		if seq == "" || seen[seq] {
			continue
		}
		seen[seq] = true
		if err := mygo.GlobalShortcut.Register(seq, l.toggle); err != nil {
			log.Printf("shortcut %s: %v", seq, err)
			continue
		}
		l.shortcut = seq
		return
	}
}

func (l *launcher) applyHotkey(seq string) error {
	seq = strings.TrimSpace(seq)
	if seq == "" {
		return errEmptyHotkey
	}
	if seq == l.shortcut {
		l.cfg.Hotkey = seq
		return nil
	}
	if err := mygo.GlobalShortcut.Register(seq, l.toggle); err != nil {
		return err
	}
	if l.shortcut != "" {
		mygo.GlobalShortcut.Unregister(l.shortcut)
	}
	l.shortcut = seq
	l.cfg.Hotkey = seq
	l.syncTray()
	return nil
}

func (l *launcher) watchApps() {
	ctx, cancel := context.WithCancel(context.Background())
	l.watchCancel = cancel
	plugins.WatchAppDirs(ctx, func() {
		l.panel.Update(func() {
			if l.reloadTimer != nil {
				l.reloadTimer.Stop()
			}
			l.reloadTimer = time.AfterFunc(500*time.Millisecond, func() {
				l.apps.Reload()
				l.panel.Update(func() {
					if l.panel.IsVisible() && strings.TrimSpace(l.query) != "" {
						l.runQuery()
					}
				})
			})
		})
	})
}

func (l *launcher) reloadApps() {
	go func() {
		l.apps.Reload()
		l.panel.Update(func() {
			if l.panel.IsVisible() && strings.TrimSpace(l.query) != "" {
				l.runQuery()
			}
		})
	}()
}

func (l *launcher) show() {
	l.query = ""
	l.clearResults()
	l.place()
	l.shownAt = time.Now()
	l.input.Focus()
	l.panel.Show()
	l.panel.Focus()
}

func (l *launcher) place() {
	d := mygo.Screen.DisplayNearestPoint(mygo.Screen.CursorScreenPoint())
	w, _ := l.panel.Size()
	if w == 0 {
		w = panelWidth
	}
	x := d.WorkArea.X + (d.WorkArea.Width-w)/2
	y := d.WorkArea.Y + d.WorkArea.Height/4
	l.panel.SetPosition(x, y)
}

func (l *launcher) fit() {
	if l.panel == nil {
		return
	}
	n := len(l.results)
	h := searchH
	if n > 0 || (strings.TrimSpace(l.query) != "" && l.timer == nil) {
		if n == 0 {
			n = 1
		}
		h = searchH + 1 + n*rowH + footH
	}
	w, cur := l.panel.ContentSize()
	if w == panelWidth && cur == h {
		return
	}
	l.panel.SetContentSize(panelWidth, h)
}

func (l *launcher) stopRecording() {
	l.recording = false
	stopHotkeyCapture()
}

func (l *launcher) toggle() {
	if l.panel.IsVisible() {
		l.panel.Hide()
		return
	}
	l.show()
}

func (l *launcher) openData() {
	dir := core.DataDir()
	_ = os.MkdirAll(dir, 0o755)
	_ = mygo.Shell.OpenPath(dir)
}

func (l *launcher) restart() {
	exe, err := os.Executable()
	if err != nil {
		log.Print(err)
		return
	}
	if err := startDetached(exe); err != nil {
		log.Print(err)
		return
	}
	mygo.App.Quit()
}

func (l *launcher) openSettings() {
	l.loadDraft()
	l.panel.Hide()
	l.settingsWin.Show()
}

func (l *launcher) openHelp() {
	l.helpCmds = append([]plugins.Command(nil), l.commands.List()...)
	l.panel.Hide()
	l.helpWin.Show()
}

func (l *launcher) loadDraft() {
	l.draftHotkey = l.cfg.Hotkey
	l.draftAuto = l.cfg.Autostart
	l.settingsErr = ""
	l.draftEngines = append([]string(nil), l.cfg.WebEngineOrder...)
	l.draftPlugs = []plugDraft{
		{"AppLauncher", "应用搜索", l.cfg.PluginEnabled("AppLauncher")},
		{"Files", "文件搜索", l.cfg.PluginEnabled("Files")},
		{"Web", "网页搜索", l.cfg.PluginEnabled("Web")},
		{"Commands", "命令", l.cfg.PluginEnabled("Commands")},
	}
}

func (l *launcher) commitSettings() {
	l.stopRecording()
	if err := l.applyHotkey(l.draftHotkey); err != nil {
		l.settingsErr = "无法注册热键：" + err.Error()
		return
	}
	if err := mygo.App.SetOpenAtLogin(l.draftAuto); err != nil {
		l.settingsErr = "无法设置开机启动：" + err.Error()
		return
	}
	l.cfg.Autostart = l.draftAuto
	disabled := []string{}
	for _, p := range l.draftPlugs {
		if !p.On {
			disabled = append(disabled, p.Name)
		}
	}
	l.cfg.DisabledPlugins = disabled
	l.cfg.WebEngineOrder = append([]string(nil), l.draftEngines...)
	if err := l.cfg.Save(); err != nil {
		l.settingsErr = err.Error()
		return
	}
	l.settingsErr = ""
	l.syncTray()
	l.settingsWin.Hide()
}

func (l *launcher) syncTray() {
	if l.tray == nil {
		tray, err := mygo.NewTray(mygo.TrayOptions{
			Icon:           trayIcon(),
			IconIsTemplate: true,
			ToolTip:        "Launcher",
		})
		if err != nil {
			log.Print("no tray icon: ", err)
			return
		}
		l.tray = tray
		l.tray.OnClick(l.onTrayClick)
	}
	accel := l.shortcut
	l.tray.SetMenu(mygo.NewMenu([]*mygo.MenuItem{
		{Label: "显示 / 隐藏", Accelerator: accel, Click: func(*mygo.MenuItem, *mygo.Window) { l.toggle() }},
		{Label: "设置", Click: func(*mygo.MenuItem, *mygo.Window) { l.openSettings() }},
		mygo.Separator(),
		{
			Label: "开机启动", Type: mygo.MenuItemCheckbox, Checked: l.cfg.Autostart,
			Click: func(it *mygo.MenuItem, _ *mygo.Window) {
				l.cfg.Autostart = it.Checked
				if err := mygo.App.SetOpenAtLogin(it.Checked); err != nil {
					log.Print(err)
				}
				if err := l.cfg.Save(); err != nil {
					log.Print(err)
				}
			},
		},
		mygo.Separator(),
		{Role: mygo.RoleQuit},
	}))
}

func (l *launcher) onTrayClick() {
	now := time.Now()
	if !l.trayClick.IsZero() && now.Sub(l.trayClick) < 500*time.Millisecond {
		l.trayClick = time.Time{}
		l.toggle()
		return
	}
	l.trayClick = now
}

func trayIcon() []byte {
	img := image.NewNRGBA(image.Rect(0, 0, 32, 32))
	ink := color.NRGBA{R: 79, G: 124, B: 255, A: 255}
	if runtime.GOOS == "darwin" {
		ink = color.NRGBA{A: 255}
	}
	for y := 0; y < 32; y++ {
		for x := 0; x < 32; x++ {
			dx, dy := float64(x)-13, float64(y)-13
			if r := dx*dx + dy*dy; r >= 7*7 && r <= 10*10 {
				img.Set(x, y, ink)
			}
		}
	}
	for i := 0; i < 8; i++ {
		draw.Draw(img, image.Rect(20+i, 20+i, 23+i, 23+i), image.NewUniform(ink), image.Point{}, draw.Src)
	}
	var b bytes.Buffer
	_ = png.Encode(&b, img)
	return b.Bytes()
}
