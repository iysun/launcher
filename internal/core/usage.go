package core

import (
	"encoding/json"
	"os"
	"path/filepath"
	"time"
)

const FrecencyCap = 60

type usageEntry struct {
	Count    int   `json:"count"`
	LastUsed int64 `json:"lastUsed"`
}

// Usage records how often a result is opened so frequent items rise
// within the same match tier. The bonus never exceeds FrecencyCap.
type Usage struct {
	path    string
	entries map[string]usageEntry
}

func LoadUsage(path string) *Usage {
	u := &Usage{path: path, entries: map[string]usageEntry{}}
	b, err := os.ReadFile(path)
	if err != nil {
		return u
	}
	_ = json.Unmarshal(b, &u.entries)
	if u.entries == nil {
		u.entries = map[string]usageEntry{}
	}
	return u
}

func (u *Usage) Record(key string) {
	if key == "" {
		return
	}
	e := u.entries[key]
	e.Count++
	e.LastUsed = time.Now().Unix()
	u.entries[key] = e
	u.save()
}

func (u *Usage) Bonus(key string) int {
	e, ok := u.entries[key]
	if !ok {
		return 0
	}
	age := time.Now().Unix() - e.LastUsed
	recency := 0
	switch {
	case age < 86400:
		recency = 30
	case age < 604800:
		recency = 20
	case age < 2592000:
		recency = 10
	}
	countW := e.Count * 3
	if countW > 30 {
		countW = 30
	}
	bonus := recency + countW
	if bonus > FrecencyCap {
		return FrecencyCap
	}
	return bonus
}

func (u *Usage) save() {
	if err := os.MkdirAll(filepath.Dir(u.path), 0o755); err != nil {
		return
	}
	b, err := json.Marshal(u.entries)
	if err != nil {
		return
	}
	_ = os.WriteFile(u.path, b, 0o644)
}
