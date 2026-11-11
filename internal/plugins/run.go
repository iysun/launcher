package plugins

import (
	"strings"

	"github.com/iysun/launcher/internal/plugin"
)

// Run is the ":" plugin. Enter shows the command's output in the panel.
// Ctrl+Enter opens it in a new terminal tab.
type Run struct {
	Wrap func(string)
	Term func(string)
}

func (Run) Name() string   { return "Run" }
func (Run) Prefix() string { return ":" }
func (Run) Async() bool    { return false }

func (Run) Query(keyword string) []plugin.Result {
	kw := strings.TrimSpace(keyword)
	if strings.HasPrefix(kw, ":") {
		return nil
	}
	if kw == "" {
		return []plugin.Result{{
			Title:    "输入命令，Enter 在 launcher 内展示输出",
			Subtitle: "Ctrl+Enter 或 :: 前缀进持久终端",
			Action:   "run:",
		}}
	}
	return []plugin.Result{{
		Title:    "执行：" + kw,
		Subtitle: "Enter 展示输出 · Ctrl+Enter 进终端",
		Action:   "run:" + kw,
		Score:    1000,
	}}
}

func (r Run) Execute(item plugin.Result) {
	cmd := strings.TrimPrefix(item.Action, "run:")
	if cmd != "" && r.Wrap != nil {
		r.Wrap(cmd)
	}
}

func (r Run) ExecuteAlt(item plugin.Result) {
	cmd := strings.TrimPrefix(item.Action, "run:")
	if cmd != "" && r.Term != nil {
		r.Term(cmd)
	}
}

// RunPersistent is the "::" plugin. Enter opens a new terminal tab.
type RunPersistent struct {
	Open func(string)
}

func (RunPersistent) Name() string   { return "RunPersistent" }
func (RunPersistent) Prefix() string { return "::" }
func (RunPersistent) Async() bool    { return false }

func (RunPersistent) Query(keyword string) []plugin.Result {
	kw := strings.TrimSpace(keyword)
	if kw == "" {
		return []plugin.Result{{
			Title:    "输入命令，回车在新标签页打开终端",
			Subtitle: "回车新开标签并进入终端模式",
			Action:   "runp:",
		}}
	}
	return []plugin.Result{{
		Title:    "终端：" + kw,
		Subtitle: "回车新开标签并进入终端模式",
		Action:   "runp:" + kw,
		Score:    1000,
	}}
}

func (r RunPersistent) Execute(item plugin.Result) {
	if r.Open == nil {
		return
	}
	r.Open(strings.TrimPrefix(item.Action, "runp:"))
}

func (RunPersistent) ExecuteAlt(plugin.Result) {}
