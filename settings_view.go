package main

import "github.com/egoist/mygo/ui"

func (l *launcher) settingsView(c *ui.Context) {
	t := c.Theme()
	if c.Shortcut(0, ui.KeyEscape) {
		l.stopRecording()
		l.settingsWin.Hide()
	}
	ui.Column(c).Fill().Background(t.Background).Children(func() {
		ui.Text(c, "设置").FontSize(18).Bold().Padding(16, 20, 8, 20)
		ui.Divider(c)
		ui.Scroll(c).Grow(1).Padding(8, 20).Children(func() {
			ui.Column(c).Gap(14).Children(func() {
				ui.Text(c, "全局热键").FontSize(12).Bold().TextColor(t.TextMuted)
				ui.Row(c).Gap(8).AlignItems(ui.Center).Children(func() {
					ui.TextInput(c, &l.draftHotkey).Grow(1).Placeholder("Alt+Space").Label("全局热键")
					label := "录制"
					if l.recording {
						label = "请按键…"
					}
					if ui.Button(c, label).Clicked() {
						if l.recording {
							l.stopRecording()
							return
						}
						l.recording = true
						l.settingsErr = ""
						if !startHotkeyCapture(func(seq string) {
							l.settingsWin.Update(func() {
								l.recording = false
								if seq != "" {
									l.draftHotkey = seq
								}
							})
						}) {
							l.recording = false
							l.settingsErr = "无法开始录制热键"
						}
					}
				})
				ui.Text(c, "点「录制」后按下组合键，Esc 取消。也可以直接写成 Alt+Space。Alt+Space 在 Windows 上可能被系统占用，失败时改用 Ctrl+Space。").
					FontSize(11).TextColor(t.TextMuted)
				ui.Checkbox(c, &l.draftAuto, "开机启动")

				ui.Text(c, "插件").FontSize(12).Bold().TextColor(t.TextMuted)
				for i := range l.draftPlugs {
					ui.Checkbox(c, &l.draftPlugs[i].On, l.draftPlugs[i].Label)
				}

				ui.Text(c, "网页搜索顺序").FontSize(12).Bold().TextColor(t.TextMuted)
				for i := range l.draftEngines {
					i := i
					ui.Row(c).Gap(8).AlignItems(ui.Center).Children(func() {
						ui.Text(c, engineLabel(l.draftEngines[i])).Grow(1)
						if ui.Button(c, "上移").Clicked() && i > 0 {
							l.draftEngines[i], l.draftEngines[i-1] = l.draftEngines[i-1], l.draftEngines[i]
						}
						if ui.Button(c, "下移").Clicked() && i < len(l.draftEngines)-1 {
							l.draftEngines[i], l.draftEngines[i+1] = l.draftEngines[i+1], l.draftEngines[i]
						}
					})
				}
				if l.settingsErr != "" {
					ui.Text(c, l.settingsErr).TextColor(t.Danger)
				}
			})
		})
		ui.Divider(c)
		ui.Row(c).Padding(12, 20).Gap(8).Justify(ui.End).Children(func() {
			if ui.Button(c, "取消").Clicked() {
				l.stopRecording()
				l.settingsWin.Hide()
			}
			if ui.PrimaryButton(c, "保存").Clicked() {
				l.commitSettings()
			}
		})
	})
}

func engineLabel(id string) string {
	switch id {
	case "google":
		return "Google"
	case "bing":
		return "Bing"
	case "baidu":
		return "Baidu"
	case "github":
		return "GitHub"
	default:
		return id
	}
}
