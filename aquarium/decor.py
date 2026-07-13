"""
decor.py — Reef floor decorations: coral, seaweed, treasure, ruins, rocks.

Decor is anchored to the bottom of the tank. Seaweed and coral sway with a
per-item phase so the reef is never static; treasure/ruins/rocks are solid
silhouettes that frame the scene. Layout is generated once at scene build.
"""
from __future__ import annotations
import math
import random
from . import ascii_art as art
from .config import Palette


class Decor:
    __slots__ = ("rows", "x", "y", "color", "sway", "phase", "kind")

    def __init__(self, rows, x, y, color, sway=0.0, kind="decor"):
        self.rows = rows
        self.x = x          # left anchor
        self.y = y          # top of the art (baseline near bottom)
        self.color = color
        self.sway = sway
        self.phase = random.uniform(0, 6.28)
        self.kind = kind

    def offset(self, t: float) -> float:
        return math.sin(t * 0.8 + self.phase) * self.sway if self.sway else 0.0


def build_reef(w: int, h: int, rng: random.Random) -> list[Decor]:
    items: list[Decor] = []
    floor = h - 14

    # a treasure chest somewhere off-centre
    tx = rng.uniform(w * 0.55, w * 0.8)
    items.append(Decor(art.TREASURE, tx, floor - len(art.TREASURE) * 14,
                       Palette.EMERALD, kind="treasure"))

    # sunken ruins to one side
    items.append(Decor(art.RUINS, w * 0.06, floor - len(art.RUINS) * 14,
                       Palette.MUTED, kind="ruins"))

    # scattered rocks
    for _ in range(3):
        rx = rng.uniform(0, w * 0.95)
        items.append(Decor(art.ROCK, rx, floor - len(art.ROCK) * 14,
                           Palette.MUTED, kind="rock"))

    # coral clusters
    for _ in range(6):
        cx = rng.uniform(0, w * 0.95)
        c = rng.choice(art.CORAL)
        col = rng.choice([Palette.VIOLET, Palette.CYAN, Palette.EMERALD, Palette.VIOLET_HI])
        items.append(Decor(c, cx, floor - len(c) * 14, col, sway=2.0, kind="coral"))

    # seaweed forest
    for _ in range(10):
        sx = rng.uniform(0, w)
        sw = rng.choice(art.SEAWEED)
        items.append(Decor(sw, sx, floor - len(sw) * 14, Palette.EMERALD,
                           sway=5.0, kind="seaweed"))

    return items
