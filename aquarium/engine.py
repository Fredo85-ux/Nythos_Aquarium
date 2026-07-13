"""
engine.py — Timing loop, frame pacing and orchestration.

Tkinter requires all canvas work on the main thread, so the render/update loop
runs via `after()` with a measured delta-time (telemetry polling lives on its own
thread in telemetry.py — that's the heavy/blocking work moved off the UI). The
loop self-paces to the target FPS and exposes a smoothed FPS for the debug HUD.
"""
from __future__ import annotations
import time

from .camera import Camera
from .renderer import Renderer
from .hud import Hud
from .splash import Splash


class Engine:
    def __init__(self, root, canvas, scene, settings, rng, show_splash=True):
        self.root = root
        self.canvas = canvas
        self.scene = scene
        self.s = settings
        self.camera = Camera(settings)
        self.renderer = Renderer(canvas, settings, rng)
        self.hud = Hud(settings)
        self.splash = Splash(settings) if show_splash else None

        self._running = False
        self._last = None
        self._fps = float(settings.target_fps)
        self._frame_budget = 1.0 / settings.target_fps

    def start(self):
        self._running = True
        self._last = time.perf_counter()
        self._schedule()

    def stop(self):
        self._running = False

    def _schedule(self):
        if self._running:
            self.root.after(max(1, int(self._frame_budget * 1000)), self._tick)

    def _tick(self):
        if not self._running:
            return
        now = time.perf_counter()
        dt = now - self._last
        self._last = now
        if dt > 0.1:
            dt = 0.1                      # clamp after a stall (alt-tab etc.)
        if dt > 0:
            self._fps = self._fps * 0.9 + (1.0 / dt) * 0.1

        # simulate + render
        self.camera.update(dt)
        self.scene.update(dt)
        self.renderer.draw(self.scene, self.camera, dt)
        self.hud.draw(self.canvas, self.scene, self._fps)
        if self.splash and not self.splash.done:
            self.splash.update(dt)
            self.splash.draw(self.canvas)
        elif self.splash and self.splash.done:
            self.canvas.delete("splash")
            self.splash = None

        self._schedule()

    # convenience for the app/easter eggs
    def toggle_debug(self):
        self.hud.show_debug = not self.hud.show_debug
