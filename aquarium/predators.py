"""
predators.py — The threat predator (shark).

Spawned by the scene when a threat is detected. Hunts the nearest fish, sweeps
across the tank, then leaves. Its presence makes schools scatter (see Fish.fear)
and is what tips the water into the red-accented threat state.
"""
from __future__ import annotations
import math
from .entities import Entity
from . import ascii_art as art
from .config import Palette


class Shark(Entity):
    def __init__(self, x, y, w, h):
        super().__init__(x, y, art.SHARK, Palette.THREAT, scale=1.6, depth=0.35)
        self.kind = "shark"
        self.glow = True
        self.speed = 150
        self.life = 16.0           # seconds before it leaves
        self.w, self.h = w, h
        self.vx = self.speed if x < w / 2 else -self.speed

    def update(self, dt, t, scene):
        self.life -= dt
        # hunt nearest fish
        target = None
        best = 1e9
        for f in scene.fish:
            d = (f.x - self.x) ** 2 + (f.y - self.y) ** 2
            if d < best:
                best = d
                target = f
        ax = ay = 0.0
        if target:
            dx = target.x - self.x
            dy = target.y - self.y
            d = math.hypot(dx, dy) + 1
            ax += (dx / d) * 260
            ay += (dy / d) * 160
        self.vx += ax * dt
        self.vy += ay * dt
        sp = math.hypot(self.vx, self.vy)
        if sp > self.speed:
            self.vx *= self.speed / sp
            self.vy *= self.speed / sp
        # vertical bound
        self.y = max(self.h * 0.15, min(self.h * 0.85, self.y))
        super().update(dt, t, scene)

        if self.life <= 0:
            self.dead = True
