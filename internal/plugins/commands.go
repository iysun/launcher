package plugins

import (
	"strings"

	"github.com/iysun/launcher/internal/core"
	"github.com/iysun/launcher/internal/plugin"
)

// Command is one launcher command, invoked as /id.
type Command struct {
	ID   string
	Desc string
	Run  func()
}

// Commands is the "/" command palette.
type Commands struct {
	cmds []Command
}

func (c *Commands) Name() string   { return "Commands" }
func (c *Commands) Prefix() string { return "/" }
func (c *Commands) Async() bool    { return false }

func (c *Commands) Add(id, desc string, run func()) {
	c.cmds = append(c.cmds, Command{ID: id, Desc: desc, Run: run})
}

func (c *Commands) List() []Command { return c.cmds }

func (c *Commands) Query(keyword string) []plugin.Result {
	keyword = strings.TrimSpace(keyword)
	var out []plugin.Result
	for _, cmd := range c.cmds {
		score := 0
		if keyword != "" {
			score = core.Score(cmd.ID, keyword)
			if d := core.Score(cmd.Desc, keyword); d > score {
				score = d
			}
			if score < 0 {
				continue
			}
		}
		out = append(out, plugin.Result{
			Title:    "/" + cmd.ID,
			Subtitle: cmd.Desc,
			Action:   cmd.ID,
			Score:    score,
		})
	}
	return out
}

func (c *Commands) Execute(item plugin.Result) {
	for _, cmd := range c.cmds {
		if cmd.ID == item.Action && cmd.Run != nil {
			cmd.Run()
			return
		}
	}
}

func (*Commands) ExecuteAlt(plugin.Result) {}
