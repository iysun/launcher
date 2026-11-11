package core

import "testing"

func TestScoreTiers(t *testing.T) {
	if Score("Code", "Code") != 1000 || Score("Code", "code") != 1000 {
		t.Fatalf("exact = %d / %d", Score("Code", "Code"), Score("Code", "code"))
	}
	prefix := Score("Firefox", "fire")
	boundary := Score("Visual Studio Code", "studio")
	substring := Score("Foobar", "oob")
	subsequence := Score("Firefox", "ff")
	if !(prefix > boundary && boundary > substring && substring > subsequence && subsequence > 0) {
		t.Fatalf("tiers %d %d %d %d", prefix, boundary, substring, subsequence)
	}
	if Score("Code", "co") <= Score("Codex", "co") {
		t.Fatal("shorter title should win inside a tier")
	}
	if Score("Code", "xyz") != -1 || Score("", "a") != -1 {
		t.Fatal("expected no match")
	}
	if Score("FireFox", "FIREFOX") != 1000 || Score("FireFox", "ff") <= 0 {
		t.Fatal("case folding")
	}
	if Score("Visual Studio Code", "vsc") <= 0 || Score("Firefox", "xf") != -1 {
		t.Fatal("subsequence")
	}
}

func TestMatchedPositions(t *testing.T) {
	got := MatchedPositions("Firefox", "fire")
	want := []int{0, 1, 2, 3}
	if !intsEqual(got, want) {
		t.Fatalf("substring %#v", got)
	}
	if !intsEqual(MatchedPositions("Firefox", "ff"), []int{0, 4}) {
		t.Fatalf("subsequence %#v", MatchedPositions("Firefox", "ff"))
	}
	if MatchedPositions("Code", "zzz") != nil || MatchedPositions("Code", "") != nil {
		t.Fatal("empty positions")
	}
}

func TestPinyin(t *testing.T) {
	r := ComputePinyin("微信")
	if r.Full != "weixin" || r.Initials != "wx" || len(r.Offsets) != 2 || r.Offsets[0] != 0 || r.Offsets[1] != 3 {
		t.Fatalf("weixin %#v", r)
	}
	ascii := ComputePinyin("WeChat")
	if ascii.Full != "WeChat" || ascii.Initials != "WeChat" || len(ascii.Offsets) != 6 || ascii.Offsets[0] != 0 {
		t.Fatalf("ascii %#v", ascii)
	}
	dot := ComputePinyin("微·信")
	if dot.Full != "weixin" || dot.Initials != "wx" || len(dot.Offsets) != 3 || dot.Offsets[1] != -1 {
		t.Fatalf("punct %#v", dot)
	}
	py := ComputePinyin("微信")
	if Score(py.Full, "wei") <= 0 || Score(py.Full, "weixin") != 1000 || Score("微信", "wei") != -1 {
		t.Fatal("full pinyin score")
	}
	if Score(py.Initials, "wx") != 1000 || Score(py.Initials, "w") <= 0 {
		t.Fatal("initials score")
	}
	direct := Score("WeightX", "wei")
	pinyin := Score(ComputePinyin("微信").Full, "wei") - 50
	if direct <= pinyin {
		t.Fatalf("direct %d should beat pinyin %d", direct, pinyin)
	}
	if !intsEqual(PinyinTitlePositions(py, "wei", false), []int{0}) {
		t.Fatalf("full highlight %#v", PinyinTitlePositions(py, "wei", false))
	}
	if !intsEqual(PinyinTitlePositions(py, "wx", true), []int{0, 1}) {
		t.Fatalf("initial highlight %#v", PinyinTitlePositions(py, "wx", true))
	}
}

func intsEqual(a, b []int) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}
