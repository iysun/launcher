package core

import (
	"encoding/json"
	"os"
	"path/filepath"
)

var defaultEngines = []string{"google", "bing", "baidu", "github"}

// Settings is the on-disk user preferences, compatible with the Qt build's settings.json.
type Settings struct {
	path            string
	Hotkey          string   `json:"hotkey"`
	Autostart       bool     `json:"autostart"`
	DisabledPlugins []string `json:"disabledPlugins"`
	WebEngineOrder  []string `json:"webEngineOrder"`
}

func LoadSettings(path string) *Settings {
	s := &Settings{
		path:           path,
		Hotkey:         "Alt+Space",
		WebEngineOrder: append([]string(nil), defaultEngines...),
	}
	b, err := os.ReadFile(path)
	if err != nil {
		return s
	}
	_ = json.Unmarshal(b, s)
	if s.Hotkey == "" {
		s.Hotkey = "Alt+Space"
	}
	if len(s.WebEngineOrder) == 0 {
		s.WebEngineOrder = append([]string(nil), defaultEngines...)
	}
	s.path = path
	return s
}

func (s *Settings) PluginEnabled(name string) bool {
	for _, n := range s.DisabledPlugins {
		if n == name {
			return false
		}
	}
	return true
}

func (s *Settings) Save() error {
	if err := os.MkdirAll(filepath.Dir(s.path), 0o755); err != nil {
		return err
	}
	b, err := json.MarshalIndent(s, "", "  ")
	if err != nil {
		return err
	}
	b = append(b, '\n')
	return os.WriteFile(s.path, b, 0o644)
}
