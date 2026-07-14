"""
config.py — Palette, tunables and the global Settings object for Aquarium Mode.

Everything visual or behavioural that a designer might want to tweak lives here
so the rest of the engine stays free of magic numbers. Colours follow the Nythos
cyberpunk-deep-ocean identity: deep black-purple water, electric violet, neon
cyan/blue, with restrained emerald accents. Bright red is reserved exclusively
for an active threat state.
"""
from __future__ import annotations
from dataclasses import dataclass, field


# --- Theme --------------------------------------------------------------
# Mirrors the .scr's `enum class Theme { Reef, Abyss, Cyberpunk }`
# (src/Settings.h) — only the water depth gradient changes per theme there
# (see ThemeShallow/ThemeDeep in shaders/scene.hlsl), so that's all a theme
# swap touches here too. Values are the shader's linear 0..1 colours scaled
# to 0..255; MID is interpolated the same way the .scr's depth curve settles
# roughly 60% of the way toward the deep colour.
THEME_NAMES = ("reef", "abyss", "cyberpunk")

_THEMES = {
    "reef":      {"BG_TOP": (15, 56, 87), "BG_MID": (8, 30, 52),  "BG_DEEP": (4, 13, 28)},
    "abyss":     {"BG_TOP": (10, 26, 46), "BG_MID": (6, 13, 26),  "BG_DEEP": (3, 5, 13)},
    "cyberpunk": {"BG_TOP": (26, 10, 58), "BG_MID": (11, 4, 32),  "BG_DEEP": (5, 1, 14)},
}


# --- Palette ----------------------------------------------------------------
class Palette:
    # Water / background depth gradient (top -> bottom). Default theme is
    # "cyberpunk" — the Nythos identity these were originally tuned for.
    BG_TOP    = (26, 10, 58)     # electric-violet tinted surface
    BG_MID    = (11, 4, 32)      # deep purple
    BG_DEEP   = (5, 1, 14)       # near-black abyss

    VIOLET    = (155, 93, 229)   # #9B5DE5
    VIOLET_HI = (190, 130, 255)
    CYAN      = (0, 229, 255)     # #00E5FF
    BLUE      = (45, 125, 255)    # #2D7DFF
    EMERALD   = (43, 245, 160)    # #2BF5A0 — accents only
    WHITE     = (224, 232, 255)
    MUTED     = (120, 130, 180)

    THREAT    = (255, 45, 85)     # ONLY when a threat is detected
    THREAT_HI = (255, 120, 140)

    # Fish palettes for procedural colouring (healthy ocean)
    FISH = [CYAN, BLUE, VIOLET, VIOLET_HI, EMERALD, (90, 200, 255), (180, 120, 255)]

    @classmethod
    def apply_theme(cls, name: str) -> None:
        """Swap the water gradient to match a .scr theme. Accent/HUD/fish
        colours are intentionally left alone — the .scr doesn't vary those
        by theme either (see shaders/scene.hlsl: uTheme only feeds
        PSBackground)."""
        t = _THEMES.get(name, _THEMES["cyberpunk"])
        cls.BG_TOP, cls.BG_MID, cls.BG_DEEP = t["BG_TOP"], t["BG_MID"], t["BG_DEEP"]


def rgb(c: tuple[int, int, int]) -> str:
    """RGB tuple -> Tk hex colour string."""
    return "#%02x%02x%02x" % (max(0, min(255, c[0])), max(0, min(255, c[1])), max(0, min(255, c[2])))


def lerp_rgb(a, b, t: float):
    t = 0.0 if t < 0 else 1.0 if t > 1 else t
    return (int(a[0] + (b[0] - a[0]) * t),
            int(a[1] + (b[1] - a[1]) * t),
            int(a[2] + (b[2] - a[2]) * t))


def dim(c, f: float):
    return (int(c[0] * f), int(c[1] * f), int(c[2] * f))


# --- Settings ---------------------------------------------------------------
@dataclass
class Settings:
    """User/runtime configuration. Persisted by settings.py."""
    width: int = 1280
    height: int = 720
    target_fps: int = 60
    fish_count: int = 28
    particle_density: float = 1.0
    glow: bool = True
    camera_drift: bool = True
    audio: bool = False
    show_hud: bool = True
    show_tips: bool = True
    use_telemetry: bool = True
    quality: str = "high"          # "low" | "med" | "high"
    theme: str = "cyberpunk"       # "reef" | "abyss" | "cyberpunk" (see THEME_NAMES)

    # Background regeneration cadence (Hz). The PIL water layer is expensive,
    # so it animates slower than the 60 FPS entity layer — the eye doesn't
    # notice on slow drifting light/fog.
    bg_fps: int = 12

    def bg_scale(self) -> int:
        """Downscale factor for the PIL background (perf vs. crispness)."""
        return {"low": 4, "med": 3, "high": 2}.get(self.quality, 2)
