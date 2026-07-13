"""
fauna.py — Ambient large marine life: jellyfish, turtle, octopus, stingray,
whale, shrimp.

These are lower-density "characters" that drift through to add life and variety.
They use fixed ASCII templates (ascii_art.py) and simple, distinctive motion so
each species reads differently from the schooling fish.
"""
from __future__ import annotations
import math
import random
from .entities import Entity
from . import ascii_art as art
from .config import Palette


class Jellyfish(Entity):
    def __init__(self, x, y):
        super().__init__(x, y, art.JELLYFISH, Palette.VIOLET_HI, scale=1.0,
                         depth=random.uniform(0.3, 0.8))
        self.kind = "jellyfish"
        self.glow = True
        self.phase = random.uniform(0, 6.28)
        self.drift = random.uniform(-8, 8)

    def update(self, dt, t, scene):
        # pulse upward then sink — hypnotic bob
        self.vy = math.sin(t * 1.2 + self.phase) * 18 - 4
        self.vx = self.drift + math.sin(t * 0.3 + self.phase) * 6
        if self.y < scene.h * 0.1:
            self.vy = abs(self.vy)
        if self.y > scene.h * 0.85:
            self.vy = -abs(self.vy)
        super().update(dt, t, scene)
        self._wrap_x(scene)

    def _wrap_x(self, scene):
        if self.x < -60: self.x = scene.w + 60
        if self.x > scene.w + 60: self.x = -60


class Drifter(Entity):
    """Slow horizontal cruisers: turtle, whale, octopus, stingray."""
    def __init__(self, rows, color, x, y, speed, scale=1.0, glow=False):
        super().__init__(x, y, rows, color, scale=scale, depth=random.uniform(0.4, 0.9))
        self.kind = "drifter"
        self.glow = glow
        self.vx = speed
        self.phase = random.uniform(0, 6.28)

    def update(self, dt, t, scene):
        self.vy = math.sin(t * 0.5 + self.phase) * 8
        super().update(dt, t, scene)
        if self.x < -120: self.dead = True
        if self.x > scene.w + 120: self.dead = True


class Shrimp(Entity):
    def __init__(self, x, y):
        super().__init__(x, y, art.SHRIMP, Palette.EMERALD, scale=0.7, depth=0.95)
        self.kind = "shrimp"
        self.phase = random.uniform(0, 6.28)

    def update(self, dt, t, scene):
        # darting little hops near the reef
        self.vx = math.sin(t * 3 + self.phase) * 30
        self.vy = math.cos(t * 5 + self.phase) * 8
        super().update(dt, t, scene)


def make_turtle(w, h, rng):
    s = rng.choice([-1, 1])
    x = -100 if s > 0 else w + 100
    return Drifter(art.TURTLE, Palette.EMERALD, x, rng.uniform(h * 0.3, h * 0.7),
                   s * rng.uniform(22, 34), scale=1.1)


def make_whale(w, h, rng):
    s = rng.choice([-1, 1])
    x = -160 if s > 0 else w + 160
    return Drifter(art.WHALE, Palette.BLUE, x, rng.uniform(h * 0.2, h * 0.55),
                   s * rng.uniform(14, 22), scale=1.6, glow=True)


def make_octopus(w, h, rng):
    s = rng.choice([-1, 1])
    x = -100 if s > 0 else w + 100
    return Drifter(art.OCTOPUS, Palette.VIOLET, x, rng.uniform(h * 0.5, h * 0.8),
                   s * rng.uniform(16, 26), scale=1.1, glow=True)


def make_stingray(w, h, rng):
    s = rng.choice([-1, 1])
    x = -100 if s > 0 else w + 100
    return Drifter(art.STINGRAY, Palette.CYAN, x, rng.uniform(h * 0.55, h * 0.82),
                   s * rng.uniform(26, 40), scale=1.0)
