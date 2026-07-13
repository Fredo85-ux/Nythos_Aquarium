"""
hud.py — Cyber HUD overlay drawn on top of the aquarium.

Mirrors the native .scr's Direct2D dashboard: a brand panel, a System Health
readout fed by real telemetry, a Threat Monitor with a risk bar, a bottom-right
clock, a fading "Tip of the Tank" panel, and a footer strip. Plus a hidden
debug overlay (toggled by typing "debug") with FPS/entity/telemetry detail.

Panels use a translucent stipple fill — the same "behind glass" trick as the
splash screen — since Tkinter canvas items have no true alpha. The tip panel
instead fades by lerping its colours toward the background as it fades in and
out, the same trick the splash sequence uses for its logo.

Drawn with Canvas items tagged "hud"/"dbg" and cleared each frame; the HUD is
small so the per-frame rebuild is cheap and keeps the code simple.
"""
from __future__ import annotations
import datetime
import os
from .config import Palette, rgb, lerp_rgb
from .tips import Tips

try:
    import psutil
    _PROC = psutil.Process(os.getpid())
except Exception:
    _PROC = None

_VERSION = "1.0.0"
PANEL_FILL = (8, 15, 26)
PANEL_STROKE = (51, 140, 191)


class Hud:
    def __init__(self, settings):
        self.s = settings
        self.show_debug = False
        self.tips = Tips()

    def draw(self, canvas, scene, fps: float, dt: float = 0.0):
        canvas.delete("hud")
        canvas.delete("dbg")
        self.tips.update(dt, enabled=getattr(self.s, "show_tips", True))

        if self.s.show_hud:
            w, h = self.s.width, self.s.height
            sc = max(0.6, min(w, h) / 1080.0)
            tiny = w < 480
            self._brand(canvas, sc, tiny)
            if not tiny:
                self._system_health(canvas, scene, sc)
                self._threat_monitor(canvas, scene, sc)
            self._clock(canvas, sc, tiny)
            if not tiny and getattr(self.s, "show_tips", True):
                self._tip(canvas, sc)
            if not tiny:
                self._footer(canvas, scene, sc)

        if self.show_debug:
            self._debug(canvas, scene, fps)

    # ---- drawing primitives -------------------------------------------
    def _panel(self, canvas, x0, y0, x1, y1, fill=PANEL_FILL, stroke=PANEL_STROKE):
        canvas.create_rectangle(x0, y0, x1, y1, fill=rgb(fill), outline=rgb(stroke),
                                width=1, stipple="gray50", tags="hud")

    def _text(self, canvas, x, y, s, color, font, anchor="nw", width=0, tag="hud"):
        canvas.create_text(x, y, text=s, fill=rgb(color), font=font,
                           anchor=anchor, tags=tag, width=width)

    def _f(self, sc, px, *style):
        return ("Consolas", max(8, int(round(px * sc))), *style)

    # ---- brand panel (top-left) ----------------------------------------
    def _brand(self, canvas, sc, tiny):
        m = 24 * sc
        w = self.s.width
        pw = w * 0.9 if tiny else 300 * sc
        x0, y0, x1, y1 = m, m, m + pw, m + 66 * sc
        self._panel(canvas, x0, y0, x1, y1)
        self._text(canvas, x0 + 16 * sc, y0 + 8 * sc, "NYTHOS", Palette.WHITE,
                   self._f(sc, 24, "bold"))
        self._text(canvas, x0 + 18 * sc, y0 + 40 * sc, "CYBER AQUARIUM", Palette.CYAN,
                   self._f(sc, 11))

    # ---- system health panel (left) -------------------------------------
    def _system_health(self, canvas, scene, sc):
        m = 24 * sc
        snap = scene.snapshot
        x0, y0 = m, m + 76 * sc
        x1, y1 = x0 + 300 * sc, y0 + 226 * sc
        self._panel(canvas, x0, y0, x1, y1)

        x, y, vx = x0 + 16 * sc, y0 + 12 * sc, x1 - 16 * sc
        self._text(canvas, x, y, "SYSTEM HEALTH", Palette.CYAN, self._f(sc, 12, "bold"))
        y += 26 * sc

        calm = snap.risk < 50 and scene.state != "threat"
        self._text(canvas, x, y, "●  All Systems Operational" if calm else "▲  Elevated Activity",
                   Palette.EMERALD if calm else Palette.THREAT, self._f(sc, 11))
        y += 30 * sc

        def row(label, val, color):
            nonlocal y
            self._text(canvas, x, y, label, Palette.MUTED, self._f(sc, 11))
            self._text(canvas, vx, y, val, color, self._f(sc, 11, "bold"), anchor="ne")
            y += 26 * sc

        row("CPU Load", f"{snap.cpu:.0f}%", Palette.THREAT if snap.cpu > 80 else Palette.EMERALD)
        row("Memory", f"{snap.mem_used_mb:,.0f} / {snap.mem_total_mb:,.0f} MB",
            Palette.THREAT if snap.mem > 85 else Palette.EMERALD)
        row("Network", f"{snap.net_kbps:,.0f} KB/s", Palette.THREAT if snap.net_spike else Palette.MUTED)
        row("Processes", f"{snap.processes}", Palette.MUTED)
        row("Uptime", f"{snap.uptime_hours:.1f} h", Palette.MUTED)

    # ---- threat monitor panel (top-right) --------------------------------
    def _threat_monitor(self, canvas, scene, sc):
        m = 24 * sc
        w = self.s.width
        snap = scene.snapshot
        pw = 280 * sc
        x0, y0 = w - m - pw, m
        x1, y1 = w - m, m + 132 * sc
        self._panel(canvas, x0, y0, x1, y1)

        hot = scene.state == "threat" or snap.risk > 60
        x, y, vx = x0 + 16 * sc, y0 + 12 * sc, x1 - 16 * sc
        self._text(canvas, x, y, "▲  THREAT MONITOR" if hot else "●  THREAT MONITOR",
                   Palette.THREAT if hot else Palette.CYAN, self._f(sc, 12, "bold"))
        y += 26 * sc

        state_label = {"healthy": "SECURE", "busy": "ACTIVE",
                       "threat": "THREAT DETECTED"}.get(scene.state, "SECURE")
        self._text(canvas, x, y, state_label, Palette.THREAT if hot else Palette.EMERALD,
                   self._f(sc, 11))
        y += 24 * sc

        self._text(canvas, x, y, f"Risk Score: {snap.risk:.0f}", Palette.MUTED, self._f(sc, 11))
        y += 22 * sc

        bar_h = 8 * sc
        canvas.create_rectangle(x, y, vx, y + bar_h, fill=rgb((30, 36, 54)),
                                outline="", tags="hud")
        frac = max(0.0, min(1.0, snap.risk / 100.0))
        if frac > 0.01:
            canvas.create_rectangle(x, y, x + (vx - x) * frac, y + bar_h,
                                    fill=rgb(Palette.THREAT if hot else Palette.EMERALD),
                                    outline="", tags="hud")

    # ---- clock panel (bottom-right) --------------------------------------
    def _clock(self, canvas, sc, tiny):
        m = 24 * sc
        w, h = self.s.width, self.s.height
        now = datetime.datetime.now()
        tstr = now.strftime("%I:%M %p").lstrip("0")

        pw = w * 0.5 if tiny else 280 * sc
        ph = 56 * sc if tiny else 96 * sc
        x0, y0 = w - m - pw, h - m - ph
        x1, y1 = w - m, h - m
        self._panel(canvas, x0, y0, x1, y1)

        if tiny:
            self._text(canvas, x1 - 12 * sc, y0 + 14 * sc, tstr, Palette.WHITE,
                       self._f(sc, 16), anchor="ne")
            return

        self._text(canvas, x1 - 16 * sc, y0 + 8 * sc, tstr, Palette.WHITE,
                   self._f(sc, 34), anchor="ne")
        dstr = now.strftime("%A, %B %d, %Y")
        self._text(canvas, x1 - 16 * sc, y1 - 22 * sc, dstr, Palette.MUTED,
                   self._f(sc, 11), anchor="ne")

    # ---- tip of the tank (bottom-right, above the clock) ------------------
    def _tip(self, canvas, sc):
        a = self.tips.alpha
        if a < 0.02:
            return
        m = 24 * sc
        w, h = self.s.width, self.s.height
        pw = 340 * sc
        ph = 108 * sc
        clock_h = 96 * sc

        x0, x1 = w - m - pw, w - m
        y1 = h - m - clock_h - 12 * sc
        y0 = y1 - ph

        fill = lerp_rgb(Palette.BG_DEEP, PANEL_FILL, a)
        stroke = lerp_rgb(Palette.BG_DEEP, PANEL_STROKE, a)
        self._panel(canvas, x0, y0, x1, y1, fill=fill, stroke=stroke)

        title_col = lerp_rgb(Palette.BG_DEEP, Palette.CYAN, a)
        text_col = lerp_rgb(Palette.BG_DEEP, Palette.WHITE, a)
        self._text(canvas, x0 + 16 * sc, y0 + 10 * sc, "●  TIP OF THE TANK", title_col,
                   self._f(sc, 12, "bold"))
        self._text(canvas, x0 + 16 * sc, y0 + 36 * sc, self.tips.current, text_col,
                   self._f(sc, 11), width=int(pw - 32 * sc))

    # ---- footer -------------------------------------------------------
    def _footer(self, canvas, scene, sc):
        m = 24 * sc
        w, h = self.s.width, self.s.height
        y = h - 20 * sc
        self._text(canvas, m, y, "Stay secure. Stay curious. Stay in the tank.",
                   Palette.MUTED, self._f(sc, 10))
        n = len(scene.fish)
        self._text(canvas, w - m, y,
                   f"NYTHOS CYBER AQUARIUM  ·  {n} fish swimming  ·  v{_VERSION}",
                   Palette.MUTED, self._f(sc, 10), anchor="ne")

    # ---- debug overlay ("debug" easter egg) --------------------------------
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
            f"uptime     {snap.uptime_hours:.1f} h",
        ]
        y = self.s.height - 18 * len(lines) - 16
        for i, ln in enumerate(lines):
            self._text(canvas, 28, y + i * 18, ln, Palette.EMERALD,
                       ("Consolas", 10), tag="dbg")
