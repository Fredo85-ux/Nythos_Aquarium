"""
tips.py — Rotating "Tip of the Tank" cybersecurity facts with fade in/out.

Mirrors the native .scr HUD's EducationalTips module: a built-in offline pool
of aquatic-framed security facts, cycling on a relaxed cadence with a soft
fade in -> hold -> fade out (never an abrupt swap). No Ollama integration
here — the Python build stays fully offline.
"""
from __future__ import annotations
import random

POOL = [
    "CVE is a public catalog of known software weaknesses — like charting reefs so ships avoid the rocks.",
    "DNS is the ocean's current map: it turns names into addresses. Poison the current and ships drift astray.",
    "A Root CA is the lighthouse of trust — every certificate's chain leads back to one you already trust.",
    "TLS encrypts data in transit, like a sealed capsule that only the right diver can open.",
    "Least privilege: give each fish only the depth it needs. Fewer doors, fewer ways in.",
    "MFA is a second current to swim against — a stolen password alone can't reach the surface.",
    "Patch early. An unpatched system is a cracked tank: small leaks sink the whole reef.",
    "Phishing lures glitter like bait. Hover before you bite — check the link's true destination.",
    "Backups are your spawning grounds: the 3-2-1 rule keeps the ecosystem alive after a storm.",
    "Zero Trust assumes every fish could be a predator — verify each, every time, everywhere.",
    "Ransomware freezes your reef and demands a ransom. Offline backups thaw it without paying.",
    "Network segmentation builds reef walls: a breach in one cove can't flood the whole tank.",
    "Lateral movement is a predator slipping reef to reef. Segment and monitor to cut it off.",
    "A password manager is your shell: one strong guard for many delicate, unique keys.",
]


class Tips:
    """Cycles POOL with an ~18s show time: 2s fade in, 14s hold, 2s fade out."""

    CYCLE = 18.0
    FADE = 2.0

    def __init__(self, rng: random.Random | None = None):
        self._rng = rng or random.Random()
        self._order = self._rng.sample(range(len(POOL)), len(POOL))
        self._cursor = 0
        self.current = POOL[self._order[0]]
        self.alpha = 0.0
        self._t = 0.0

    def update(self, dt: float, enabled: bool = True):
        if not enabled:
            self.alpha += (0.0 - self.alpha) * min(1.0, dt * 3)
            return

        self._t += dt
        if self._t >= self.CYCLE:
            self._t -= self.CYCLE
            self._cursor = (self._cursor + 1) % len(self._order)
            if self._cursor == 0:
                self._order = self._rng.sample(range(len(POOL)), len(POOL))
            self.current = POOL[self._order[self._cursor]]

        if self._t < self.FADE:
            target = self._t / self.FADE
        elif self._t > self.CYCLE - self.FADE:
            target = (self.CYCLE - self._t) / self.FADE
        else:
            target = 1.0
        self.alpha += (target - self.alpha) * min(1.0, dt * 5)
