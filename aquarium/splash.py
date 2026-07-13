"""
splash.py — Startup splash sequence drawn over the live aquarium.

Sequence: fade in -> Nythos logo with rotating "Loading <module>" messages and
an animated progress bar -> fade out, handing off to the running aquarium. The
aquarium is already simulating behind the splash, so the transition is seamless.
"""
from __future__ import annotations
import time
from .config import Palette, rgb, lerp_rgb

LOADING_STEPS = [
    "Initializing Core",
    "Loading Fredo AI",
    "Loading Endpoint Engine",
    "Loading Compliance Database",
    "Loading Telemetry",
    "Loading Application Security",
    "Loading Threat Intelligence",
    "Initializing Dashboard",
]


class Splash:
    DURATION = 4.6

    def __init__(self, settings):
        self.s = settings
        self.t = 0.0
        self.done = False
        self._start = None

    def update(self, dt):
        # Wall-clock based so the loading sequence always lasts ~DURATION
        # regardless of frame rate (the sim's dt is clamped under load).
        now = time.perf_counter()
        if self._start is None:
            self._start = now
        self.t = now - self._start
        if self.t >= self.DURATION:
            self.done = True

    @property
    def fade(self) -> float:
        """Overall splash opacity proxy 0..1 (fade in then out)."""
        if self.t < 0.6:
            return self.t / 0.6
        if self.t > self.DURATION - 0.8:
            return max(0.0, (self.DURATION - self.t) / 0.8)
        return 1.0

    def draw(self, canvas):
        canvas.delete("splash")
        f = self.fade
        if f <= 0.01:
            return
        w, h = self.s.width, self.s.height
        cx, cy = w / 2, h / 2

        # dim veil over the aquarium (simulated with a translucent-looking fill
        # via a stipple rectangle for that "behind glass" feel)
        canvas.create_rectangle(0, 0, w, h, fill="#05010e", outline="",
                                stipple="gray50", tags="splash")

        logo = lerp_rgb(Palette.BG_DEEP, Palette.WHITE, f)
        accent = lerp_rgb(Palette.BG_DEEP, Palette.CYAN, f)
        muted = lerp_rgb(Palette.BG_DEEP, Palette.MUTED, f)

        canvas.create_text(cx, cy - 70, text="N Y T H O S", fill=rgb(logo),
                           font=("Consolas", 52, "bold"), tags="splash")
        canvas.create_text(cx, cy - 20, text="C Y B E R   A Q U A R I U M",
                           fill=rgb(accent), font=("Consolas", 16), tags="splash")

        # progress
        prog = min(1.0, self.t / (self.DURATION - 0.8))
        step = min(len(LOADING_STEPS) - 1, int(prog * len(LOADING_STEPS)))
        bw, bh = 420, 8
        bx, by = cx - bw / 2, cy + 36
        canvas.create_rectangle(bx, by, bx + bw, by + bh, outline=rgb(muted),
                                width=1, tags="splash")
        canvas.create_rectangle(bx, by, bx + bw * prog, by + bh, fill=rgb(accent),
                                outline="", tags="splash")
        canvas.create_text(cx, by + 28, text=LOADING_STEPS[step] + " …",
                           fill=rgb(muted), font=("Consolas", 12), tags="splash")
        canvas.create_text(cx, by + 50, text=f"{int(prog * 100):3d}%",
                           fill=rgb(accent), font=("Consolas", 11), tags="splash")
