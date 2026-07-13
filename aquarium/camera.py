"""
camera.py — Subtle cinematic camera drift.

A slow Lissajous wander applied as a global offset to every rendered entity,
plus a faint breathing zoom. Gives the tank a hand-held, "alive" feel without
ever disorienting the viewer. Disable via Settings.camera_drift.
"""
from __future__ import annotations
import math


class Camera:
    def __init__(self, settings):
        self.s = settings
        self.t = 0.0
        self.ox = 0.0
        self.oy = 0.0
        self.zoom = 1.0

    def update(self, dt: float):
        if not self.s.camera_drift:
            self.ox = self.oy = 0.0
            self.zoom = 1.0
            return
        self.t += dt
        # incommensurate frequencies -> never repeats noticeably
        self.ox = math.sin(self.t * 0.13) * 14 + math.sin(self.t * 0.07) * 8
        self.oy = math.cos(self.t * 0.11) * 9 + math.sin(self.t * 0.053) * 5
        self.zoom = 1.0 + math.sin(self.t * 0.09) * 0.012

    def apply(self, x: float, y: float, cx: float, cy: float) -> tuple[float, float]:
        """Map world->screen with drift + breathing zoom about the centre."""
        zx = cx + (x - cx) * self.zoom
        zy = cy + (y - cy) * self.zoom
        return zx + self.ox, zy + self.oy
