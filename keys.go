package main

import (
	"unicode"
	"unicode/utf8"

	"github.com/egoist/mygo/ui"
)

func (l *launcher) onTextKey(in ui.Element, k ui.TextKey) bool {
	if k.Key == ui.KeyEnter && (k.Mods == 0 || k.Mods == ui.Ctrl) {
		if l.mode == modeWrap {
			l.rerunWrap(k.Mods == ui.Ctrl)
		} else if l.mode == modeSearch {
			l.flushAndOpen(k.Mods == ui.Ctrl)
		}
		return true
	}
	if k.Mods == ui.Ctrl {
		switch k.Key {
		case ui.KeyA:
			in.SetTextSelection(0, 0)
			return true
		case ui.KeyE:
			n := utf8.RuneCountInString(k.Text())
			in.SetTextSelection(n, n)
			return true
		case ui.KeyF:
			_, caret := in.TextAnchorCaret()
			in.SetTextSelection(caret+1, caret+1)
			return true
		case ui.KeyB:
			_, caret := in.TextAnchorCaret()
			if caret > 0 {
				caret--
			}
			in.SetTextSelection(caret, caret)
			return true
		case ui.KeyD:
			l.deleteForward(in, k, 1)
			return true
		case ui.KeyK:
			_, caret := in.TextAnchorCaret()
			in.ReplaceText(caret, utf8.RuneCountInString(k.Text()), "")
			return true
		}
	}
	if k.Mods == ui.Alt {
		switch k.Key {
		case ui.KeyF:
			_, caret := in.TextAnchorCaret()
			next := nextWord(k.Text(), caret)
			in.SetTextSelection(next, next)
			return true
		case ui.KeyB:
			_, caret := in.TextAnchorCaret()
			prev := prevWord(k.Text(), caret)
			in.SetTextSelection(prev, prev)
			return true
		case ui.KeyD:
			_, caret := in.TextAnchorCaret()
			in.ReplaceText(caret, nextWord(k.Text(), caret), "")
			return true
		}
	}
	return false
}

func (l *launcher) deleteForward(in ui.Element, k ui.TextKey, n int) {
	anchor, caret := in.TextAnchorCaret()
	if anchor != caret {
		lo, hi := anchor, caret
		if lo > hi {
			lo, hi = hi, lo
		}
		in.ReplaceText(lo, hi, "")
		return
	}
	in.ReplaceText(caret, caret+n, "")
}

func nextWord(s string, i int) int {
	r := []rune(s)
	if i < 0 {
		i = 0
	}
	if i >= len(r) {
		return len(r)
	}
	if isWordRune(r[i]) {
		for i < len(r) && isWordRune(r[i]) {
			i++
		}
		return i
	}
	for i < len(r) && !isWordRune(r[i]) {
		i++
	}
	for i < len(r) && isWordRune(r[i]) {
		i++
	}
	return i
}

func prevWord(s string, i int) int {
	r := []rune(s)
	if i > len(r) {
		i = len(r)
	}
	if i <= 0 {
		return 0
	}
	i--
	if isWordRune(r[i]) {
		for i > 0 && isWordRune(r[i-1]) {
			i--
		}
		return i
	}
	for i > 0 && !isWordRune(r[i-1]) {
		i--
	}
	for i > 0 && isWordRune(r[i-1]) {
		i--
	}
	return i
}

func isWordRune(r rune) bool {
	return unicode.IsLetter(r) || unicode.IsDigit(r)
}
