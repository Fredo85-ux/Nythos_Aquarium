"""
ascii_generator.py — Procedural ASCII fish generation.

Produces an endless variety of fish so the tank never looks copy-pasted. Each
generated fish has art (right-facing rows), a size class, a base colour and a
"personality" seed used by the behaviour code. Mirroring for direction is done
at render time, not here.
"""
from __future__ import annotations
import random
from . import ascii_art as art
from .config import Palette


SIZE_SCALE = {"tiny": 0.8, "small": 1.0, "med": 1.15, "large": 1.45}


def _compose_fish(rng: random.Random, size: str) -> list[str]:
    """Pick a fish sprite from the detail tier matching its size."""
    if size == "tiny":
        return [rng.choice(art.FISH_MICRO)]
    if size == "small":
        return list(rng.choice(art.FISH_SMALL))
    # med / large -> detailed multi-row sprites
    return list(rng.choice(art.FISH_DETAILED))


def generate_fish(rng: random.Random | None = None) -> dict:
    """Return a fish spec dict consumed by entities.Fish."""
    rng = rng or random
    # bias toward the detailed tiers so the tank reads like the reference art
    size = rng.choices(
        ["tiny", "small", "med", "large"],
        weights=[0.18, 0.27, 0.40, 0.15],
    )[0]
    rows = _compose_fish(rng, size)
    color = rng.choice(Palette.FISH)

    return {
        "rows": rows,
        "size": size,
        "scale": SIZE_SCALE[size],
        "color": color,
        # personality (no two fish behave identically)
        "speed": rng.uniform(28, 75) * SIZE_SCALE[size] ** 0.3,
        "wander": rng.uniform(0.4, 1.4),
        "schooling": rng.uniform(0.0, 1.0),
        "curiosity": rng.uniform(0.0, 1.0),
        "depth": rng.uniform(0.15, 0.95),
        "bob": rng.uniform(0.3, 1.2),
        "phase": rng.uniform(0, 6.28),
    }
