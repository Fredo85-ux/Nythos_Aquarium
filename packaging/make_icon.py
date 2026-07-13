"""
make_icon.py — Generate the Nythos Aquarium app icon (icon.ico).

Draws a neon-violet rounded badge with a stylised "N" on a deep background,
exported at all the standard Windows icon sizes. No external assets needed.
"""
from __future__ import annotations
import os
from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(__file__), "..", "build", "icon.ico")
BG_DEEP = (8, 4, 20)
VIOLET = (155, 93, 229)
VIOLET_HI = (190, 130, 255)
CYAN = (0, 229, 255)


def render(size: int) -> Image.Image:
    s = size * 4  # supersample for smooth edges, downscale at the end
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    pad = s * 0.08
    radius = s * 0.22
    # badge background
    d.rounded_rectangle([pad, pad, s - pad, s - pad], radius=radius,
                        fill=BG_DEEP, outline=VIOLET, width=max(2, s // 64))

    # neon "N" — two verticals + a diagonal
    m = s * 0.30
    top, bot = s * 0.30, s * 0.70
    lw = max(3, int(s * 0.07))
    left, right = m, s - m
    d.line([(left, bot), (left, top)], fill=VIOLET_HI, width=lw)
    d.line([(left, top), (right, bot)], fill=CYAN, width=lw)
    d.line([(right, bot), (right, top)], fill=VIOLET_HI, width=lw)
    # rounded caps
    for (x, y) in [(left, top), (left, bot), (right, top), (right, bot)]:
        d.ellipse([x - lw / 2, y - lw / 2, x + lw / 2, y + lw / 2], fill=VIOLET_HI)

    return img.resize((size, size), Image.LANCZOS)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    sizes = [16, 24, 32, 48, 64, 128, 256]
    base = render(256)
    base.save(OUT, format="ICO", sizes=[(x, x) for x in sizes])
    print("wrote", os.path.abspath(OUT))


if __name__ == "__main__":
    main()
