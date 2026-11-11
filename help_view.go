package main

import "github.com/egoist/mygo/ui"

func (l *launcher) helpView(c *ui.Context) {
	t := c.Theme()
	if c.Shortcut(0, ui.KeyEscape) {
		l.helpWin.Hide()
	}
	ui.Column(c).Fill().Background(t.Background).Children(func() {
		ui.Text(c, "帮助").FontSize(18).Bold().Padding(16, 20, 8, 20)
		ui.Divider(c)
		ui.Scroll(c).Grow(1).Padding(8, 20).Children(func() {
			ui.Column(c).Gap(8).Children(func() {
				section(c, "快捷键")
				helpRow(c, "Alt+Space", "唤起 / 隐藏（可在设置里改）")
				helpRow(c, "Enter", "执行选中项")
				helpRow(c, "Ctrl+Enter", "次级动作（复制路径、打开所在目录）")
				helpRow(c, "Esc / Ctrl+G", "关闭；有输入时 Esc 先清空")
				helpRow(c, "↑ ↓ / Ctrl+P N", "移动选中项")
				helpRow(c, "Ctrl+A E F B", "行首、行尾、左右移一字")
				helpRow(c, "Ctrl+D K", "删除后一字符、删到行尾")
				helpRow(c, "Alt+F B D", "后一词、前一词、删除后一词")
				section(c, "搜索前缀")
				helpRow(c, "（无前缀）", "搜索已安装应用，支持拼音和首字母")
				helpRow(c, "/", "命令（只输入 / 列出全部）")
				helpRow(c, "@", "文件搜索")
				helpRow(c, "?", "网页搜索")
				section(c, "命令")
				for _, cmd := range l.helpCmds {
					helpRow(c, "/"+cmd.ID, cmd.Desc)
				}
			})
		})
		ui.Divider(c)
		ui.Row(c).Padding(12, 20).Justify(ui.End).Children(func() {
			if ui.PrimaryButton(c, "关闭").Clicked() {
				l.helpWin.Hide()
			}
		})
	})
}

func section(c *ui.Context, title string) {
	ui.Text(c, title).FontSize(12).Bold().TextColor(c.Theme().TextMuted).Padding(8, 0, 0, 0)
}

func helpRow(c *ui.Context, key, desc string) {
	t := c.Theme()
	ui.Row(c).Gap(12).Children(func() {
		ui.Text(c, key).TextColor(t.Accent).Width(160)
		ui.Text(c, desc).TextColor(t.Text).Grow(1)
	})
}
