package core

import "unicode"

// Score ranks how well keyword matches text.
// Exact > prefix > word boundary > substring > subsequence.
// Within a tier, shorter text scores higher. No match returns -1.
func Score(text, keyword string) int {
	idx := indexFold(text, keyword)
	if idx >= 0 {
		if equalFold(text, keyword) {
			return 1000
		}
		n := runeLen(text)
		if idx == 0 {
			return 800 - n
		}
		tr := []rune(text)
		prev, cur := tr[idx-1], tr[idx]
		boundary := prev == ' ' || prev == '-' || prev == '_' ||
			(unicode.IsLower(prev) && unicode.IsUpper(cur))
		if boundary {
			return 600 - n
		}
		return 300 - n
	}
	bonus := subsequenceBonus(text, keyword)
	if bonus < 0 {
		return -1
	}
	if bonus > 120 {
		bonus = 120
	}
	return 100 + bonus
}

// MatchedPositions returns character indexes of keyword in text.
// A contiguous match is that span; otherwise the subsequence positions.
func MatchedPositions(text, keyword string) []int {
	if keyword == "" {
		return nil
	}
	tr := []rune(text)
	kr := []rune(keyword)
	if idx := indexFold(text, keyword); idx >= 0 {
		pos := make([]int, len(kr))
		for i := range kr {
			pos[i] = idx + i
		}
		return pos
	}
	var pos []int
	ti, ki := 0, 0
	for ti < len(tr) && ki < len(kr) {
		if unicode.ToLower(tr[ti]) == unicode.ToLower(kr[ki]) {
			pos = append(pos, ti)
			ki++
		}
		ti++
	}
	if ki < len(kr) {
		return nil
	}
	return pos
}

func indexFold(text, keyword string) int {
	tr := []rune(text)
	kr := []rune(keyword)
	if len(kr) == 0 {
		return 0
	}
	if len(kr) > len(tr) {
		return -1
	}
	for i := 0; i <= len(tr)-len(kr); i++ {
		if hasPrefixFold(tr[i:], kr) {
			return i
		}
	}
	return -1
}

func equalFold(a, b string) bool {
	ar, br := []rune(a), []rune(b)
	if len(ar) != len(br) {
		return false
	}
	return hasPrefixFold(ar, br)
}

func hasPrefixFold(text, prefix []rune) bool {
	if len(prefix) > len(text) {
		return false
	}
	for i, r := range prefix {
		if unicode.ToLower(text[i]) != unicode.ToLower(r) {
			return false
		}
	}
	return true
}

func subsequenceBonus(text, keyword string) int {
	tr := []rune(text)
	kr := []rune(keyword)
	ti, ki, bonus, prev := 0, 0, 0, -2
	for ti < len(tr) && ki < len(kr) {
		if unicode.ToLower(tr[ti]) == unicode.ToLower(kr[ki]) {
			if ti == prev+1 {
				bonus += 5
			}
			if ti == 0 {
				bonus += 10
			} else {
				pc := tr[ti-1]
				switch {
				case pc == ' ' || pc == '-' || pc == '_':
					bonus += 10
				case unicode.IsLower(pc) && unicode.IsUpper(tr[ti]):
					bonus += 8
				}
			}
			prev = ti
			ki++
		}
		ti++
	}
	if ki != len(kr) {
		return -1
	}
	return bonus
}

func runeLen(s string) int { return len([]rune(s)) }
