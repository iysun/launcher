#!/usr/bin/env python3
"""Generate resources/icons/check.png — the checkmark glyph drawn on top of
QCheckBox::indicator:checked in src/ui/settingsdialog.cpp.

Qt Style Sheets don't support ::after/content (see
docs/notes/qss-checkbox-checkmark.md), so the checkmark must be a real image
asset referenced via `image: url(:/icons/check.png)`.

Usage:
    pip install Pillow
    python tools/gen_check_icon.py
"""
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 16
STROKE = 2
COLOR = (30, 30, 46, 255)  # kBg (#1e1e2e) — dark mark on the light-blue checked square

OUT = Path(__file__).resolve().parent.parent / "resources" / "icons" / "check.png"


def main() -> None:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    # 简单三点折线对勾，留白避免贴边
    draw.line([(3, 8), (6.5, 11.5), (13, 4)], fill=COLOR, width=STROKE, joint="curve")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    img.save(OUT)
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
