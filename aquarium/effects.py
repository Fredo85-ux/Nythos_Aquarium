"""
effects.py — Transient visual effects: sonar sweep, AI analysis beam, threat
alert pulse and glitch bursts.

The manager keeps a small list of active effects, each a lightweight record with
a normalized progress 0..1. The renderer reads them and draws; this module only
owns timing/state so effects stay decoupled from how they're painted.
"""
from __future__ import annotations
import random


class Effect:
    __slots__ = ("kind", "x", "y", "t", "dur", "data")

    def __init__(self, kind, x=0, y=0, dur=1.0, data=None):
        self.kind = kind
        self.x = x
        self.y = y
        self.t = 0.0
        self.dur = dur
        self.data = data or {}

    @property
    def progress(self) -> float:
        return min(1.0, self.t / self.dur)

    @property
    def done(self) -> bool:
        return self.t >= self.dur


class EffectManager:
    def __init__(self):
        self.effects: list[Effect] = []
        self.threat_pulse = 0.0      # 0..1 smooth alert intensity
        self.glitch = 0.0            # 0..1 decays quickly

    def sonar(self, x, y):
        self.effects.append(Effect("sonar", x, y, dur=2.4))

    def ai_scan(self, fish):
        fish.analyzing = 2.0
        self.effects.append(Effect("ai_scan", fish.x, fish.y, dur=2.0, data={"fish": fish}))

    def glitch_burst(self):
        self.glitch = 1.0

    def update(self, dt, state: str):
        target = 1.0 if state == "threat" else 0.0
        self.threat_pulse += (target - self.threat_pulse) * min(1.0, dt * 3)
        self.glitch = max(0.0, self.glitch - dt * 2.5)
        for e in self.effects:
            e.t += dt
            if e.kind == "ai_scan":
                f = e.data.get("fish")
                if f:           # follow the fish being analysed
                    e.x, e.y = f.x, f.y
        self.effects = [e for e in self.effects if not e.done]
