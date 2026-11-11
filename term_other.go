//go:build !windows

package main

import "github.com/egoist/mygo/ui"

type termState struct{}

func (l *launcher) runWrap(string)             {}
func (l *launcher) openPersistent(string)      {}
func (l *launcher) enterTerminal(string, bool) {}
func (l *launcher) leaveWrap(bool)             {}
func (l *launcher) exitWrap()                  {}
func (l *launcher) exitTerminal()              {}
func (l *launcher) closeTerms()                {}
func (l *launcher) termKeys(*ui.Context)       {}
func (l *launcher) wrapKeys(*ui.Context)       {}
func (l *launcher) termBody(*ui.Context)       {}
func (l *launcher) wrapOutput(*ui.Context)     {}
