"""
app.py — The CustomTkinter application shell that hosts Aquarium Mode.

Run modes (see __main__.py):
    windowed     — a normal resizable window (default; good for dev/dashboard)
    fullscreen   — borderless fullscreen with the splash sequence
    screensaver  — fullscreen, exits on any input (mouse move / key / click)
    kiosk        — fullscreen, ignores input except the secret exit (Esc-hold),
                   for conference/demo machines

Keyboard:
    Esc            exit (windowed/fullscreen/kiosk*)
    F11 / F        toggle fullscreen
    F2             drop to the windowed dashboard (from any mode)
    H              toggle HUD          M  toggle audio
    T              demo threat         S  manual sonar sweep
    P              cycle theme (Reef / Abyss / Cyberpunk)
    + / -          add / remove fish   (debug overlay: type "debug")
Plus the hidden easter-egg sequences (see easter_eggs.py).
"""
from __future__ import annotations
import random
import customtkinter as ctk

from .config import Settings, rgb, Palette, THEME_NAMES
from . import settings as settings_io
from .telemetry import Telemetry
from .audio import Audio
from .scene import Scene
from .engine import Engine
from .easter_eggs import EasterEggs


class AquariumApp(ctk.CTk):
    def __init__(self, mode: str = "windowed"):
        super().__init__()
        self.mode = mode
        self.s: Settings = settings_io.load()
        self.rng = random.Random()
        Palette.apply_theme(self.s.theme)

        ctk.set_appearance_mode("dark")
        # Drive geometry in real pixels — CustomTkinter's automatic window
        # scaling otherwise multiplies our geometry under high-DPI and breaks
        # true fullscreen (window stays at its logical size in a corner).
        try:
            ctk.set_widget_scaling(1.0)
            ctk.set_window_scaling(1.0)
        except Exception:
            pass
        self.title("Nythos — Aquarium Mode")
        self.configure(fg_color=rgb(Palette.BG_DEEP))
        self.geometry(f"{self.s.width}x{self.s.height}")
        self.minsize(640, 360)

        # canvas (the whole window)
        import tkinter as tk
        self.canvas = tk.Canvas(self, highlightthickness=0, bd=0,
                                bg=rgb(Palette.BG_DEEP))
        self.canvas.pack(fill="both", expand=True)

        # subsystems
        self.audio = Audio(self.s)
        self.telemetry = Telemetry(self.s)
        self.telemetry.start()
        self.scene = Scene(self.s, self.telemetry, self.audio, self.rng)
        self.engine = Engine(self, self.canvas, self.scene, self.s, self.rng,
                             show_splash=(mode in ("fullscreen", "screensaver", "kiosk")))
        self.eggs = EasterEggs()

        self._fullscreen = False
        self._bind_input()
        self.after(60, self._post_init)   # let the window realise its real size

    # ---- lifecycle --------------------------------------------------------
    def _post_init(self):
        if self.mode in ("fullscreen", "screensaver", "kiosk"):
            self._enter_fullscreen()
        self.after(40, self._first_resize)
        self.engine.start()

    def _first_resize(self):
        self._on_resize(None, force=True)

    def _bind_input(self):
        self.bind("<Configure>", self._on_resize)
        self.bind("<Key>", self._on_key)
        if self.mode == "screensaver":
            # any input exits
            self.bind("<Motion>", lambda e: self._maybe_exit_screensaver(e))
            self.bind("<Button>", lambda e: self.destroy())
        self.protocol("WM_DELETE_WINDOW", self._quit)
        self._mouse_origin = None

    def _unbind_screensaver(self):
        """Stop input from exiting (used when F2 turns a saver into a dashboard)."""
        self.unbind("<Motion>")
        self.unbind("<Button>")

    def _maybe_exit_screensaver(self, e):
        if self._mouse_origin is None:
            self._mouse_origin = (e.x_root, e.y_root)
            return
        ox, oy = self._mouse_origin
        if abs(e.x_root - ox) > 8 or abs(e.y_root - oy) > 8:
            self.destroy()

    def _enter_fullscreen(self):
        """Go true fullscreen by sizing the (borderless) window to the screen.

        Explicit screen-sized geometry + overrideredirect is far more reliable
        across DPI/CTk than the `-fullscreen` attribute alone, which was leaving
        the window at its logical size in a corner.
        """
        self._fullscreen = True
        try:
            sw, sh = self.winfo_screenwidth(), self.winfo_screenheight()
            self.overrideredirect(True)              # borderless, covers taskbar
            self.geometry(f"{sw}x{sh}+0+0")
            self.attributes("-topmost", True)
            try:
                self.attributes("-fullscreen", True)  # belt-and-braces where supported
            except Exception:
                pass
            self.update_idletasks()
            self.lift()
            self.focus_force()
        except Exception:
            pass
        # <Configure> can lag; push the real size to renderer/scene shortly after.
        self.after(60, lambda: self._on_resize(None, force=True))
        self.after(220, lambda: self._on_resize(None, force=True))
        if not getattr(self, "_fs_watch", False):
            self._fs_watch = True
            self.after(500, self._fs_watchdog)

    def _fs_watchdog(self):
        if not self._fullscreen:
            self._fs_watch = False
            return
        self._assert_fullscreen()
        self.after(700, self._fs_watchdog)

    def _enter_windowed(self, w: int = 1280, h: int = 720):
        """Drop to the windowed dashboard (used by F2 and the toggle)."""
        self._fullscreen = False
        self.mode = "windowed"
        try:
            try:
                self.attributes("-fullscreen", False)
            except Exception:
                pass
            self.overrideredirect(False)             # restore title bar / borders
            self.attributes("-topmost", False)
            sw, sh = self.winfo_screenwidth(), self.winfo_screenheight()
            x, y = max(0, (sw - w) // 2), max(0, (sh - h) // 3)
            self.geometry(f"{w}x{h}+{x}+{y}")
            self.update_idletasks()
            self.lift()
            self.focus_force()
        except Exception:
            pass
        self.after(60, lambda: self._on_resize(None, force=True))
        self.after(220, lambda: self._on_resize(None, force=True))

    def _toggle_fullscreen(self):
        if self._fullscreen:
            self._enter_windowed()
        else:
            self._enter_fullscreen()

    def _assert_fullscreen(self) -> bool:
        """Self-heal fullscreen: CustomTkinter's geometry tracker intermittently
        reverts the window to its logical size, so whenever we detect the window
        is smaller than the screen while meant to be fullscreen, re-assert it.
        Returns True if a correction was issued."""
        if not self._fullscreen:
            return False
        sw, sh = self.winfo_screenwidth(), self.winfo_screenheight()
        if self.winfo_width() < sw - 4 or self.winfo_height() < sh - 4:
            try:
                self.overrideredirect(True)
                self.geometry(f"{sw}x{sh}+0+0")
                self.attributes("-topmost", True)
            except Exception:
                pass
            return True
        return False

    def _on_resize(self, event, force=False):
        if self._assert_fullscreen():
            return  # geometry change will fire another <Configure>; handle it then
        w = self.canvas.winfo_width()
        h = self.canvas.winfo_height()
        if w < 2 or h < 2:
            return
        if not force and w == self.s.width and h == self.s.height:
            return
        self.s.width, self.s.height = w, h
        self.scene.resize(w, h)
        self.engine.renderer.resize(w, h)

    # ---- input ------------------------------------------------------------
    def _on_key(self, e):
        k = e.keysym

        # F2 always drops to the windowed dashboard, from any mode (incl. saver).
        if k == "F2":
            self._unbind_screensaver()
            self._enter_windowed()
            return

        if self.mode == "screensaver":
            self.destroy(); return

        # easter eggs first
        event = self.eggs.feed(k)
        if event == "konami":   self.scene.konami()
        elif event == "fredo":  self.scene.spawn_diver()
        elif event == "mythos": self.scene.spawn_phoenix()
        elif event == "debug":  self.engine.toggle_debug()

        low = k.lower()
        if k == "Escape":
            self._quit()
        elif k in ("F11",) or low == "f":
            self._toggle_fullscreen()
        elif low == "h":
            self.s.show_hud = not self.s.show_hud
        elif low == "m":
            self.audio.toggle()
        elif low == "t":
            self.scene.trigger_threat_demo()
        elif low == "s":
            self.scene.trigger_sonar(self.s.width / 2, self.s.height / 2)
        elif low == "w":
            self.scene.spawn_whale()
        elif low == "p":
            self._cycle_theme()
        elif k in ("plus", "equal", "KP_Add"):
            self.scene.add_fish()
        elif k in ("minus", "KP_Subtract"):
            if self.scene.fish:
                self.scene.fish.pop()

    def _cycle_theme(self):
        i = THEME_NAMES.index(self.s.theme) if self.s.theme in THEME_NAMES else 0
        self.s.theme = THEME_NAMES[(i + 1) % len(THEME_NAMES)]
        Palette.apply_theme(self.s.theme)
        self.configure(fg_color=rgb(Palette.BG_DEEP))
        self.canvas.configure(bg=rgb(Palette.BG_DEEP))
        self.engine.renderer._build_water_static()

    def _quit(self):
        self.engine.stop()
        self.telemetry.stop()
        settings_io.save(self.s)
        self.destroy()


def run(mode: str = "windowed"):
    app = AquariumApp(mode)
    app.mainloop()
