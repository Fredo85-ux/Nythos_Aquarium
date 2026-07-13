"""
entities.py — Entity base class and the procedural Fish agent.

Fish are autonomous: wander + schooling (cohesion/alignment) + separation +
edge avoidance + predator flee, modulated by per-fish personality and the global
security state. Depth (z, 0=near .. 1=far) drives parallax size and fog dimming.
Larger fauna live in fauna.py / predators.py and subclass Entity.
"""
from __future__ import annotations
import math
import random


def _len(x, y):
    return math.hypot(x, y)


class Entity:
    """Common state + helpers for everything that swims."""
    def __init__(self, x, y, rows, color, scale=1.0, depth=0.5):
        self.x = x
        self.y = y
        self.vx = 0.0
        self.vy = 0.0
        self.rows = rows                # right-facing ASCII
        self.color = color
        self.scale = scale
        self.depth = depth
        self.facing = 1                 # 1 right, -1 left
        self.dead = False
        self.alpha = 1.0                # used for fade in/out
        self.kind = "entity"
        self.glow = False

    def _clamp_facing(self):
        if self.vx > 6:
            self.facing = 1
        elif self.vx < -6:
            self.facing = -1

    def update(self, dt, t, scene):
        self.x += self.vx * dt
        self.y += self.vy * dt
        self._clamp_facing()


class Fish(Entity):
    def __init__(self, spec: dict, x: float, y: float):
        super().__init__(x, y, spec["rows"], spec["color"], spec["scale"], spec["depth"])
        self.kind = "fish"
        self.base_color = spec["color"]
        self.speed = spec["speed"]
        self.wander_amt = spec["wander"]
        self.schooling = spec["schooling"]
        self.curiosity = spec["curiosity"]
        self.bob = spec["bob"]
        self.phase = spec["phase"]
        self.fear = 0.0
        self.heading = random.uniform(0, 6.28)
        self.vx = math.cos(self.heading) * self.speed * 0.4
        self.vy = math.sin(self.heading) * self.speed * 0.4
        self.analyzing = 0.0            # >0 while the AI beam inspects this fish

    def update(self, dt, t, scene):
        w, h = scene.w, scene.h
        ax = ay = 0.0

        # --- wander (smooth heading drift) ---
        self.heading += (random.uniform(-1, 1) * self.wander_amt) * dt * 2.0
        ax += math.cos(self.heading) * self.speed * 0.6
        ay += math.sin(self.heading) * self.speed * 0.6

        # --- schooling: cohesion + alignment + separation with neighbours ---
        cx = cy = vx = vy = 0.0
        sep_x = sep_y = 0.0
        n = 0
        for o in scene.fish:
            if o is self:
                continue
            dx = o.x - self.x
            dy = o.y - self.y
            d2 = dx * dx + dy * dy
            if d2 < 120 * 120:
                n += 1
                cx += o.x; cy += o.y
                vx += o.vx; vy += o.vy
                if d2 < 42 * 42 and d2 > 1:
                    inv = 1.0 / math.sqrt(d2)
                    sep_x -= dx * inv
                    sep_y -= dy * inv
        if n:
            cx = cx / n - self.x
            cy = cy / n - self.y
            ax += cx * 0.012 * self.schooling + vx / n * 0.04 * self.schooling
            ay += cy * 0.012 * self.schooling + vy / n * 0.04 * self.schooling
            ax += sep_x * 26
            ay += sep_y * 26

        # --- flee predators (water darkens, fish scatter) ---
        self.fear = max(0.0, self.fear - dt * 0.6)
        for p in scene.predators:
            dx = self.x - p.x
            dy = self.y - p.y
            d = _len(dx, dy)
            if d < 240 and d > 1:
                f = (240 - d) / 240
                self.fear = min(1.0, self.fear + f * dt * 5)
                ax += (dx / d) * f * 420
                ay += (dy / d) * f * 420

        # --- curiosity toward an active sonar / scan centre ---
        if scene.sonar_active and self.curiosity > 0.4 and self.fear < 0.3:
            dx = scene.sonar_x - self.x
            dy = scene.sonar_y - self.y
            d = _len(dx, dy) + 1
            ax += (dx / d) * 40 * self.curiosity
            ay += (dy / d) * 40 * self.curiosity

        # --- soft edge avoidance (stay in frame, depth-banded vertically) ---
        m = 60
        if self.x < m:        ax += (m - self.x) * 3
        if self.x > w - m:    ax -= (self.x - (w - m)) * 3
        top = h * 0.08 + self.depth * h * 0.15
        bot = h * 0.92
        if self.y < top:      ay += (top - self.y) * 3
        if self.y > bot:      ay -= (self.y - bot) * 3

        # --- integrate with momentum, clamp speed (fear -> faster) ---
        max_spd = self.speed * (1.0 + self.fear * 1.8) * (1.4 if scene.state == "threat" else 1.0)
        self.vx += ax * dt
        self.vy += ay * dt
        sp = _len(self.vx, self.vy)
        if sp > max_spd:
            self.vx *= max_spd / sp
            self.vy *= max_spd / sp

        # gentle vertical bob
        self.y += math.sin(t * self.bob + self.phase) * 6 * dt

        super().update(dt, t, scene)
        if self.analyzing > 0:
            self.analyzing -= dt
