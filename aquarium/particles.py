"""
particles.py — Bubbles, drifting motes and debris (the foreground particle layer).

Light rays and volumetric fog are painted into the PIL water layer by the
renderer; this module owns the discrete, character-based particles that float in
front of it. Stochastic emission keeps the field from ever visibly looping.
Object pooling keeps allocations flat for long-running operation.
"""
from __future__ import annotations
import math
import random
from . import ascii_art as art
from .config import Palette


class Particle:
    __slots__ = ("x", "y", "vx", "vy", "char", "color", "life", "max_life",
                 "kind", "size", "seed", "active")

    def __init__(self):
        self.active = False


class ParticleSystem:
    def __init__(self, settings, rng: random.Random):
        self.s = settings
        self.rng = rng
        self.w = settings.width
        self.h = settings.height
        self.pool = [Particle() for _ in range(900)]
        self._bub_acc = 0.0
        self._mote_acc = 0.0
        # seed a calm field of suspended motes
        for _ in range(140):
            self._spawn("mote")

    def resize(self, w, h):
        self.w, self.h = w, h

    def _free(self) -> Particle | None:
        for p in self.pool:
            if not p.active:
                return p
        return None

    def _spawn(self, kind: str):
        p = self._free()
        if p is None:
            return
        p.active = True
        p.kind = kind
        p.seed = self.rng.uniform(0, 6.28)
        if kind == "bubble":
            p.x = self.rng.uniform(0, self.w)
            p.y = self.h + 5
            p.vx = 0.0
            p.vy = -self.rng.uniform(28, 70)
            p.char = self.rng.choice(art.BUBBLES)
            p.color = Palette.CYAN if self.rng.random() < 0.5 else Palette.BLUE
            p.max_life = self.rng.uniform(5, 9)
        else:  # mote
            p.x = self.rng.uniform(0, self.w)
            p.y = self.rng.uniform(0, self.h)
            p.vx = self.rng.uniform(-6, 6)
            p.vy = self.rng.uniform(-3, 6)
            p.char = self.rng.choice(art.DEBRIS)
            p.color = self.rng.choice([Palette.VIOLET, Palette.MUTED, Palette.CYAN])
            p.max_life = self.rng.uniform(10, 22)
        p.life = p.max_life
        p.size = self.rng.uniform(0.8, 1.4)

    def update(self, dt: float, t: float, current: float):
        d = self.s.particle_density
        self._bub_acc += dt * 16 * d
        self._mote_acc += dt * 6 * d
        while self._bub_acc >= 1:
            self._spawn("bubble")
            self._bub_acc -= 1
        while self._mote_acc >= 1:
            self._spawn("mote")
            self._mote_acc -= 1

        for p in self.pool:
            if not p.active:
                continue
            p.life -= dt
            if p.life <= 0:
                p.active = False
                continue
            if p.kind == "bubble":
                p.y += p.vy * dt
                p.x += math.sin(t * 2 + p.seed) * 14 * dt + current * 8 * dt
                if p.y < -10:
                    p.active = False
            else:
                p.x += (p.vx + math.sin(t * 0.3 + p.seed) * 4 + current * 10) * dt
                p.y += (p.vy + math.cos(t * 0.21 + p.seed) * 3) * dt
                if p.x < -10:
                    p.x = self.w + 10
                elif p.x > self.w + 10:
                    p.x = -10

    def alive(self):
        return (p for p in self.pool if p.active)
