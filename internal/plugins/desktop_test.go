package plugins

import "testing"

func TestParseDesktop(t *testing.T) {
	name, execLine, icon, ok := parseDesktop(`
[Desktop Entry]
Name=Files
Exec=nautilus %U
Icon=/usr/share/icons/files.png
NoDisplay=false

[Desktop Action Extra]
Name=Other
`)
	if !ok || name != "Files" || execLine != "nautilus %U" || icon != "/usr/share/icons/files.png" {
		t.Fatalf("got %q %q %q %v", name, execLine, icon, ok)
	}
	if _, _, _, ok := parseDesktop("[Desktop Entry]\nName=Hidden\nExec=hidden\nNoDisplay=true\n"); ok {
		t.Fatal("NoDisplay should drop the entry")
	}
	if _, _, _, ok := parseDesktop("[Desktop Entry]\nName=Only\n"); ok {
		t.Fatal("missing Exec should drop the entry")
	}
}

func TestStripFieldCodes(t *testing.T) {
	got := stripFieldCodes("firefox %u --name %c")
	if got != "firefox  --name" {
		t.Fatalf("got %q", got)
	}
}
