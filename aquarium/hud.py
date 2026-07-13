"""
hud.py — Cyber HUD overlay drawn on top of the aquarium.

Two layers:
  * Brand/status HUD — the Nythos mark, endpoint health, risk score, live state.
  * Debug overlay (toggled by typing "debug") — FPS, entity/particle counts,
    telemetry and process memory.

Drawn with Canvas text items tagged "hud"/"dbg" and cleared each frame; the HUD
is tiny so the per-frame rebuild is cheap and keeps the code simple.
"""
from __future__ import annotations
import os
from .config import Palette, rgb

try:
    import psutil
    _PROC = psutil.Process(os.getpid())
except Exception:
    _PROC = None


class Hud:
    FONT = ("Consolas", 13)
    FONT_BIG = ("Consolas", 26, "bold")
    FONT_SMALL = ("Consolas", 10)

    def __init__(self, settings):
        self.s = settings
        self.show_debug = False

    def draw(self, canvas, scene, fps: float):
        canvas.delete("hud")
        canvas.delete("dbg")
        if self.s.show_hud:
            self._brand(canvas, scene)
        if self.show_debug:
            self._debug(canvas, scene, fps)

    def _text(self, canvas, x, y, s, color, font=None, anchor="nw", tag="hud"):
        canvas.create_text(x, y, text=s, fill=rgb(color), font=font or self.FONT,
                           anchor=anchor, tags=tag)

    def _brand(self, canvas, scene):
        w = self.s.width
        snap = scene.snapshot
        accent = Palette.THREAT if scene.state == "threat" else Palette.CYAN

        # brand
        self._text(canvas, 28, 24, "NYTHOS", Palette.WHITE, self.FONT_BIG)
        self._text(canvas, 30, 58, "AQUARIUM MODE", accent, self.FONT_SMALL)

        # status block (top-right)
        state_label = {"healthy": "● SECURE", "busy": "● ACTIVE",
                       "threat": "▲ THREAT DETECTED"}.get(scene.state, "● SECURE")
        state_col = {"healthy": Palette.EMERALD, "busy": Palette.CYAN,
                     "threat": Palette.THREAT}.get(scene.state, Palette.EMERALD)
        self._text(canvas, w - 28, 24, state_label, state_col, self.FONT, anchor="ne")
        self._text(canvas, w - 28, 46,
                   f"Health {snap.health:4.0f}   Risk {snap.risk:4.0f}",
                   Palette.MUTED, self.FONT, anchor="ne")
        self._text(canvas, w - 28, 64,
                   f"CPU {snap.cpu:3.0f}%   MEM {snap.mem:3.0f}%   NET {snap.net_kbps:5.0f} KB/s",
                   Palette.MUTED, self.FONT_SMALL, anchor="ne")

    def _debug(self, canvas, scene, fps: float):
        mem = (_PROC.memory_info().rss / (1024 * 1024)) if _PROC else 0.0
        snap = scene.snapshot
        lines = [
            "── DEBUG ──",
            f"FPS        {fps:5.1f}",
            f"fish       {len(scene.fish)}",
            f"fauna      {len(scene.fauna)}",
            f"predators  {len(scene.predators)}",
            f"particles  {sum(1 for _ in scene.particles.alive())}",
            f"effects    {len(scene.effects.effects)}",
            f"decor      {len(scene.decor)}",
            f"state      {scene.state}",
            f"mem RSS    {mem:5.1f} MB",
            f"cpu/mem    {snap.cpu:.0f}% / {snap.mem:.0f}%",
            f"procs      {snap.processes}",
        ]
        y = self.s.height - 18 * len(lines) - 16
        for i, ln in enumerate(lines):
            self._text(canvas, 28, y + i * 18, ln, Palette.EMERALD, self.FONT_SMALL, tag="dbg")
