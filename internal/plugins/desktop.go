package plugins

import (
	"regexp"
	"strings"
)

var fieldCode = regexp.MustCompile(`%[uUfFdDnNickvm]`)

// parseDesktop reads a Desktop Entry the way the Qt app plugin did:
// the first Name, Exec and Icon, and NoDisplay=true drops the file.
func parseDesktop(text string) (name, execLine, icon string, ok bool) {
	in := false
	for _, line := range strings.Split(text, "\n") {
		line = strings.TrimSpace(line)
		if line == "[Desktop Entry]" {
			in = true
			continue
		}
		if strings.HasPrefix(line, "[") {
			in = false
			continue
		}
		if !in {
			continue
		}
		eq := strings.IndexByte(line, '=')
		if eq < 0 {
			continue
		}
		key := strings.TrimSpace(line[:eq])
		val := strings.TrimSpace(line[eq+1:])
		switch key {
		case "Name":
			if name == "" {
				name = val
			}
		case "Exec":
			if execLine == "" {
				execLine = val
			}
		case "Icon":
			if icon == "" {
				icon = val
			}
		case "NoDisplay":
			if val == "true" {
				return "", "", "", false
			}
		}
	}
	if name == "" || execLine == "" {
		return "", "", "", false
	}
	return name, execLine, icon, true
}

func stripFieldCodes(execLine string) string {
	return strings.TrimSpace(fieldCode.ReplaceAllString(execLine, ""))
}
