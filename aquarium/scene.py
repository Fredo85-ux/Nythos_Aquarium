"""
scene.py — The world: all entities, the security-state machine and the mapping
from telemetry to behaviour.

States:
    healthy  — calm purple water, relaxed schooling
    busy     — higher CPU -> livelier fish, more current
    threat   — risk crosses threshold (or demo trigger): predator spawns, water
               darkens/reddens, schools scatter, glitch burst

Events:
    network spike  -> sonar sweep (fish investigate)
    periodic       -> AI analysis beam scans a suspicious fish, then it returns
The scene is the single source of truth the renderer/HUD read from.
"""
from __future__ import annotations
import math
import random

from .config import Settings
from .telemetry import Telemetry, Snapshot
from .particles import ParticleSystem
from .effects import EffectManager
from .entities import Fish
from .predators import Shark
from . import fauna as fa
from . import decor as dec
from .ascii_generator import generate_fish


class Scene:
    def __init__(self, settings: Settings, telemetry: Telemetry, audio, rng: random.Random):
        self.s = settings
        self.telemetry = telemetry
        self.audio = audio
        self.rng = rng
        self.w = settings.width
        self.h = settings.height
        self.time = 0.0

        self.fish: list[Fish] = []
        self.fauna: list = []
        self.predators: list[Shark] = []
        self.decor = dec.build_reef(self.w, self.h, rng)
        self.particles = ParticleSystem(settings, rng)
        self.effects = EffectManager()

        self.snapshot = Snapshot()
        self.state = "healthy"
        self.current = 0.0
        self.sonar_active = False
        self.sonar_x = self.sonar_y = 0.0

        self._next_scan = rng.uniform(8, 16)
        self._next_fauna = rng.uniform(6, 12)
        self._force_threat = 0.0

        self._spawn_initial()

    # ---- spawning ---------------------------------------------------------
    def _spawn_initial(self):
        for _ in range(self.s.fish_count):
            self.add_fish()
        for _ in range(3):
            self._spawn_fauna()

    def add_fish(self):
        spec = generate_fish(self.rng)
        x = self.rng.uniform(0, self.w)
        y = self.rng.uniform(self.h * 0.15, self.h * 0.85)
        self.fish.append(Fish(spec, x, y))

    def _spawn_fauna(self):
        maker = self.rng.choice([
            lambda: fa.Jellyfish(self.rng.uniform(0, self.w), self.rng.uniform(self.h * 0.2, self.h * 0.7)),
            lambda: fa.make_turtle(self.w, self.h, self.rng),
            lambda: fa.make_octopus(self.w, self.h, self.rng),
            lambda: fa.make_stingray(self.w, self.h, self.rng),
            lambda: fa.Shrimp(self.rng.uniform(0, self.w), self.h * 0.9),
        ])
        self.fauna.append(maker())
        if len(self.fauna) > 8:
            self.fauna.pop(0)

    def spawn_whale(self):
        self.fauna.append(fa.make_whale(self.w, self.h, self.rng))

    # ---- demo / easter-egg hooks -----------------------------------------
    def trigger_threat_demo(self):
        self._force_threat = 12.0

    def konami(self):
        from . import ascii_art as art
        for f in self.fish:
            f.rows = art.DRAGON
            f.glow = True

    def spawn_diver(self):
        from . import ascii_art as art
        from .fauna import Drifter
        from .config import Palette
        s = self.rng.choice([-1, 1])
        x = -120 if s > 0 else self.w + 120
        d = Drifter(art.DIVER, Palette.CYAN, x, self.h * 0.4, s * 26, scale=1.2, glow=True)
        self.fauna.append(d)

    def spawn_phoenix(self):
        from . import ascii_art as art
        from .fauna import Drifter
        from .config import Palette
        phoenix = [
            "      /\\",
            "  /\\ /  \\ /\\",
            " <  PHOENIX  >",
            "  \\/ \\  / \\/",
            "      \\/",
        ]
        d = Drifter(phoenix, Palette.VIOLET_HI, -200, self.h * 0.5, 30, scale=2.4, glow=True)
        self.fauna.append(d)

    # ---- state machine ----------------------------------------------------
    def _update_state(self, dt):
        self.snapshot = self.telemetry.snapshot
        risk = self.snapshot.risk
        self._force_threat = max(0.0, self._force_threat - dt)

        if risk > 60 or self._force_threat > 0:
            new = "threat"
        elif self.snapshot.cpu > 55:
            new = "busy"
        else:
            new = "healthy"

        if new == "threat" and self.state != "threat":
            self._enter_threat()
        self.state = new

        # current strength tracks CPU (busier machine -> stronger drift)
        target_cur = math.sin(self.time * 0.1) * (0.3 + self.snapshot.cpu / 100)
        self.current += (target_cur - self.current) * min(1.0, dt)

    def _enter_threat(self):
        if not self.predators:
            x = self.rng.choice([-60, self.w + 60])
            self.predators.append(Shark(x, self.h * 0.5, self.w, self.h))
            self.effects.glitch_burst()
            self.audio.ping()

    # ---- main update ------------------------------------------------------
    def update(self, dt):
        self.time += dt
        self._update_state(dt)

        # network spike -> sonar sweep
        if self.snapshot.net_spike:
            self.trigger_sonar(self.rng.uniform(self.w * 0.3, self.w * 0.7),
                               self.rng.uniform(self.h * 0.3, self.h * 0.7))

        # periodic AI analysis of a "suspicious" fish
        self._next_scan -= dt
        if self._next_scan <= 0 and self.fish:
            self._next_scan = self.rng.uniform(10, 20)
            self.effects.ai_scan(self.rng.choice(self.fish))
            self.audio.ping()

        # occasional new ambient fauna
        self._next_fauna -= dt
        if self._next_fauna <= 0:
            self._next_fauna = self.rng.uniform(8, 16)
            self._spawn_fauna()

        # sonar lifetime flag (drives fish curiosity)
        self.sonar_active = any(e.kind == "sonar" for e in self.effects.effects)
        if self.sonar_active:
            son = next(e for e in self.effects.effects if e.kind == "sonar")
            self.sonar_x, self.sonar_y = son.x, son.y

        for f in self.fish:
            f.update(dt, self.time, self)
        for fa_ in self.fauna:
            fa_.update(dt, self.time, self)
        for p in self.predators:
            p.update(dt, self.time, self)

        self.fauna = [e for e in self.fauna if not e.dead]
        self.predators = [p for p in self.predators if not p.dead]

        self.particles.update(dt, self.time, self.current)
        self.effects.update(dt, self.state)

    def trigger_sonar(self, x, y):
        self.effects.sonar(x, y)
        self.audio.ping()

    def resize(self, w, h):
        self.w, self.h = w, h
        self.particles.resize(w, h)
        self.decor = dec.build_reef(w, h, self.rng)
