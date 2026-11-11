//go:build windows

package plugins

import (
	"log"
	"os"
	"path/filepath"
	"strings"
	"syscall"
	"unsafe"

	"github.com/egoist/mygo"
	"github.com/iysun/launcher/internal/core"
	"golang.org/x/sys/windows"
)

const (
	clsctxInprocServer = 1
	stgmRead           = 0
	slgpRawPath        = 0x4
)

var (
	ole32            = windows.NewLazySystemDLL("ole32.dll")
	procCoCreateInst = ole32.NewProc("CoCreateInstance")

	clsidShellLink = windows.GUID{Data1: 0x00021401, Data2: 0x0000, Data3: 0x0000, Data4: [8]byte{0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}}
	iidShellLinkW  = windows.GUID{Data1: 0x000214F9, Data2: 0x0000, Data3: 0x0000, Data4: [8]byte{0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}}
	iidPersistFile = windows.GUID{Data1: 0x0000010B, Data2: 0x0000, Data3: 0x0000, Data4: [8]byte{0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}}
)

type shellLink struct{ vtbl *shellLinkVtbl }
type shellLinkVtbl struct {
	QueryInterface uintptr
	AddRef         uintptr
	Release        uintptr
	GetPath        uintptr
}

type persistFile struct{ vtbl *persistFileVtbl }
type persistFileVtbl struct {
	QueryInterface uintptr
	AddRef         uintptr
	Release        uintptr
	GetClassID     uintptr
	IsDirty        uintptr
	Load           uintptr
}

func scanInstalled() []indexedApp {
	if err := windows.CoInitializeEx(0, windows.COINIT_APARTMENTTHREADED); err == nil {
		defer windows.CoUninitialize()
	}
	var items []indexedApp
	for _, root := range startMenuRoots() {
		_ = filepath.WalkDir(root, func(path string, d os.DirEntry, err error) error {
			if err != nil || d.IsDir() || !strings.EqualFold(filepath.Ext(path), ".lnk") {
				return nil
			}
			target := resolveLnk(path)
			if target == "" {
				target = path
			}
			if expanded := expandPath(target); expanded != "" && !strings.Contains(expanded, "%") {
				target = expanded
			}
			name := strings.TrimSuffix(d.Name(), filepath.Ext(d.Name()))
			items = append(items, indexedApp{
				name:     name,
				action:   target,
				subtitle: target,
				icon:     target,
				py:       core.ComputePinyin(name),
			})
			return nil
		})
	}
	if len(items) == 0 {
		log.Print("no start menu shortcuts")
	}
	return items
}

func startMenuRoots() []string {
	var roots []string
	if app := os.Getenv("APPDATA"); app != "" {
		roots = append(roots, filepath.Join(app, "Microsoft", "Windows", "Start Menu", "Programs"))
	}
	return append(roots, `C:\ProgramData\Microsoft\Windows\Start Menu\Programs`)
}

func resolveLnk(path string) string {
	var raw uintptr
	hr, _, _ := procCoCreateInst.Call(
		uintptr(unsafe.Pointer(&clsidShellLink)),
		0,
		clsctxInprocServer,
		uintptr(unsafe.Pointer(&iidShellLinkW)),
		uintptr(unsafe.Pointer(&raw)),
	)
	if hr != 0 || raw == 0 {
		return ""
	}
	link := (*shellLink)(unsafe.Pointer(raw))
	defer syscall.SyscallN(link.vtbl.Release, raw)

	var fileRaw uintptr
	qi, _, _ := syscall.SyscallN(link.vtbl.QueryInterface, raw, uintptr(unsafe.Pointer(&iidPersistFile)), uintptr(unsafe.Pointer(&fileRaw)))
	if qi != 0 || fileRaw == 0 {
		return ""
	}
	file := (*persistFile)(unsafe.Pointer(fileRaw))
	defer syscall.SyscallN(file.vtbl.Release, fileRaw)

	name, err := syscall.UTF16PtrFromString(path)
	if err != nil {
		return ""
	}
	load, _, _ := syscall.SyscallN(file.vtbl.Load, fileRaw, uintptr(unsafe.Pointer(name)), stgmRead)
	if load != 0 {
		return ""
	}
	buf := make([]uint16, 32768)
	got, _, _ := syscall.SyscallN(link.vtbl.GetPath, raw, uintptr(unsafe.Pointer(&buf[0])), uintptr(len(buf)), 0, slgpRawPath)
	if got != 0 || buf[0] == 0 {
		return ""
	}
	return syscall.UTF16ToString(buf)
}

func expandPath(path string) string {
	src, err := syscall.UTF16PtrFromString(path)
	if err != nil {
		return path
	}
	buf := make([]uint16, 32768)
	n, err := windows.ExpandEnvironmentStrings(src, &buf[0], uint32(len(buf)))
	if err != nil || n == 0 {
		return path
	}
	return syscall.UTF16ToString(buf)
}

func launchAction(action string, app mygo.InstalledApp, ok bool) {
	if ok {
		if err := mygo.Shell.LaunchApp(app); err != nil {
			log.Printf("launch %s: %v", app.Name, err)
		}
		return
	}
	if action != "" {
		_ = mygo.Shell.OpenPath(expandPath(action))
	}
}
