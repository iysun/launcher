package core

import (
	"sort"
	"unicode"
)

// Pinyin is the romanization of a title.
// Offsets[i] is where rune i starts in Full, or -1 when that rune is skipped.
type Pinyin struct {
	Full     string
	Initials string
	Offsets  []int
}

// ComputePinyin maps CJK unified ideographs to their most common syllable
// and passes ASCII letters and digits through. Other characters are skipped.
func ComputePinyin(text string) Pinyin {
	var full, initials []rune
	offsets := make([]int, 0, len(text))
	for _, ch := range text {
		idx := int(ch) - 0x4E00
		if idx >= 0 && idx < len(pinyinTable) && pinyinTable[idx] != "" {
			syl := pinyinTable[idx]
			offsets = append(offsets, len(full))
			for _, r := range syl {
				full = append(full, r)
			}
			initials = append(initials, []rune(syl)[0])
			continue
		}
		if unicode.IsLetter(ch) && ch < 0x80 || unicode.IsDigit(ch) {
			offsets = append(offsets, len(full))
			full = append(full, ch)
			initials = append(initials, ch)
			continue
		}
		offsets = append(offsets, -1)
	}
	return Pinyin{Full: string(full), Initials: string(initials), Offsets: offsets}
}

// PinyinTitlePositions maps a match in Full or Initials back to title rune indexes.
func PinyinTitlePositions(py Pinyin, keyword string, useInitials bool) []int {
	search := py.Full
	if useInitials {
		search = py.Initials
	}
	hits := MatchedPositions(search, keyword)
	if len(hits) == 0 {
		return nil
	}
	var contributing []int
	for i, off := range py.Offsets {
		if off != -1 {
			contributing = append(contributing, i)
		}
	}
	set := map[int]struct{}{}
	if useInitials {
		for _, hit := range hits {
			if hit < len(contributing) {
				set[contributing[hit]] = struct{}{}
			}
		}
	} else {
		for _, hit := range hits {
			lo, hi := 0, len(contributing)-1
			for lo < hi {
				mid := (lo + hi + 1) / 2
				if py.Offsets[contributing[mid]] <= hit {
					lo = mid
				} else {
					hi = mid - 1
				}
			}
			if lo < len(contributing) && py.Offsets[contributing[lo]] <= hit {
				set[contributing[lo]] = struct{}{}
			}
		}
	}
	out := make([]int, 0, len(set))
	for i := range set {
		out = append(out, i)
	}
	sort.Ints(out)
	return out
}
