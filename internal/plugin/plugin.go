package plugin

// Result is one row a plugin offers for the current query.
type Result struct {
	Title        string
	Subtitle     string
	Action       string
	Score        int
	Highlights   []int
	HighlightSet bool
	IconPath     string
	Owner        Plugin
}

// Plugin searches and runs one kind of result.
// Prefix is empty for plugins that match every query.
// A non-empty prefix is stripped before Query, and while the query starts
// with any prefix the global plugins are skipped.
type Plugin interface {
	Name() string
	Prefix() string
	Async() bool
	Query(keyword string) []Result
	Execute(Result)
	ExecuteAlt(Result)
}
