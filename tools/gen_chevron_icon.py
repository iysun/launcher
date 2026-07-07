#!/usr/bin/env python3
"""Generate resources/icons/chevron-down.png — the drop-down arrow glyph used by
QComboBox::down-arrow in src/ui/settingsdialog.cpp.

Qt renders QComboBox's drop-down arrow with the native platform style once the
combo box's own background/border are customized via QSS, which clashes with
the dark Catppuccin theme (see docs/notes/qss-checkbox-checkmark.md for the
same class of "QSS can't paint this, needs a real image asset" issue). The
arrow must be a real image asset referenced via
`image: url(:/icons/chevron-down.png)`.

Usage:
    pip install Pillow
    python tools/gen_chevron_icon.py
"""
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 12
STROKE = 2
COLOR = (108, 112, 134, 255)  # kOverlay0 (#6c7086) — muted, matches section labels

OUT = Path(__file__).resolve().parent.parent / "resources" / "icons" / "chevron-down.png"


def main() -> None:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    # 简单向下 chevron，三点折线，留白避免贴边
    draw.line([(2, 4), (6, 8.5), (10, 4)], fill=COLOR, width=STROKE, joint="curve")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    img.save(OUT)
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
