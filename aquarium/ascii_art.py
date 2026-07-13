"""
ascii_art.py — Library of detailed multi-line ASCII art for marine life + decor.

Art is authored facing RIGHT (head on the right, tail on the left). The renderer
mirrors it for left-swimming entities via `mirror()`. Sprites are built to read
as recognisable silhouettes — eye, body with scale/texture fill, fins and tail —
in the Nythos neon style, inspired by the reference splash art.
"""
from __future__ import annotations


def mirror(rows: list[str]) -> list[str]:
    """Flip ASCII horizontally, swapping directional glyphs so it still reads."""
    swap = str.maketrans("<>)(/\\[]{}dbpq╱╲", "><()\\/][}{bdqp╲╱")
    width = max((len(r) for r in rows), default=0)
    return [r.ljust(width).translate(swap)[::-1] for r in rows]


# =============================================================================
#  FISH  — three detail tiers picked by size in ascii_generator.py
# =============================================================================

# Tier 0: micro (1 row) — for the smallest, fastest shoaling fish.
FISH_MICRO = [
    "><(°>",
    "<º)><",
    ">·)))°>",
    "><>",
    "<*))><",
]

# Tier 1: small (2 rows) — a little body + tail.
FISH_SMALL = [
    [
        " ___",
        "<·:·°>",
    ],
    [
        " ,---.",
        "<:::::°>",
    ],
    [
        "·-._",
        "<====·°>",
    ],
]

# Hand-tuned showpiece fish (referenced by FISH_DETAILED below).
# Tall-bodied freshwater angelfish: triangular body, long trailing dorsal/anal
# fins and a streamer tail.
ANGELFISH = [
    "       /\\",
    "      /::\\___",
    "   __/:::::: \\",
    " <=<::::::::: O>",
    "   ‾‾\\:::::: /",
    "      \\::/‾‾‾",
    "       \\/",
]

# Koi carp: long flowing body, barbels at the mouth, spotted pattern and
# trailing fins fore and aft.
KOI = [
    "          _______",
    "    _.-~ /·°:·:°·\\ ~-._",
    "  <~~(::::°:·:°::::: O >~",
    "    '-~ \\·°:·:°·/ ~-'",
    "          ‾‾‾‾‾‾‾",
]

# Tier 2: detailed (3-5 rows) — recognisable fish with eye, fins, patterned body.
FISH_DETAILED = [
    # rounded reef fish
    [
        "    ______",
        "<==(::::::: \\",
        "<==(:::::: O >",
        "<==(::::::: /",
        "    ‾‾‾‾‾‾",
    ],
    # slim dart fish
    [
        "   .-------.",
        "<<<::·:·:·:· o>",
        "   '-------'",
    ],
    ANGELFISH,
    KOI,
    # puffer / round fish
    [
        "    _.-._",
        "  /'·:·:·'\\",
        " |::·:·:·:: O|>",
        "  \\.·:·:·./",
        "    '-·-'",
    ],
    # long pike fish
    [
        "  __________",
        "<=:::::::::::::·°>",
        "  ‾‾‾‾‾‾‾‾‾‾",
    ],
]


# =============================================================================
#  LARGER MARINE LIFE  (right-facing)
# =============================================================================

SHARK = [
    "                  ___",
    "             ,--''   `.",
    "        ____/  o        `--.__",
    "<======(_____   ><   _______/>",
    "             \\__         _.-'",
    "                `--..__.-'",
]

WHALE = [
    "         .---.",
    "      .-'     '-.____",
    "    .'   o          '--.___",
    "   (        ~~~            ><",
    "    '-.____________.--<__>",
    "          '''''''",
]

TURTLE = [
    "      _____",
    "    /#=#=#=\\",
    "  _( #=#=#= )_",
    " /  \\#=#=#/  \\>",
    " ‾  /‾‾‾‾‾\\  ‾",
    "   ‾       ‾",
]

JELLYFISH = [
    "   .-‾‾‾-.",
    "  /  o o  \\",
    " | ~~~~~~~ |",
    "  \\_______/",
    "   ) | | (",
    "  |  | |  |",
    "   ) | | (",
    "  |  ) (  |",
    "   '  '  '",
]

OCTOPUS = [
    "    .-‾‾‾-.",
    "  /  o  o  \\",
    " |   ~~~~   |",
    "  \\________/",
    "  /|/|/|\\|\\",
    " ( | | | | )",
    "  ' ' ' ' '",
]

STINGRAY = [
    "    ______",
    "  <(::::::: )>",
    "    \\::::::/",
    "     '-..-'~~~~~~",
]

SHRIMP = [
    "  __",
    "-=:·>vvv",
]

DIVER = [   # easter egg: 'fredo' — scuba diver carrying a glowing laptop
    "   _",
    "  (O)   .---.",
    " <|+|>--|[#]|",
    "  /|\\   '---'",
    "  / \\",
    " ~`~`~",
]

DRAGON = [  # easter egg: konami — fish become dragons
    "      __        __",
    "   /\\/  \\,~~~~,/  \\/\\",
    "  <  >°< (######) >°<  >",
    "   \\/\\__/`~~~~'\\__/\\/",
    "       VV      VV",
]

PHOENIX = [  # easter egg: 'mythos'
    "        /\\",
    "    /\\ /  \\ /\\",
    "   /  v    v  \\",
    " <{   PHOENIX   }>",
    "   \\  ^    ^  /",
    "    \\/ \\  / \\/",
    "        \\/",
]


# =============================================================================
#  DECOR  (anchored to the reef floor)
# =============================================================================

CORAL = [
    [
        "  \\|/",
        " \\\\|//",
        "  \\|/",
        "   |",
    ],
    [
        " (|)(|)",
        "  \\|/",
        " (|)|",
        "   |",
    ],
    [
        " .Y..Y.",
        "(|||||)",
        " \\|||/",
        "   |",
    ],
    [
        "  o o o",
        " (o(|)o)",
        "  \\\\|//",
        "    |",
    ],
]

SEAWEED = [
    [" )", "( ", " )", "( ", " )", "( ", " )", "|"],
    ["( ", " )", "( ", " )", "( ", " )", " |"],
    [" (,", "  )", " (,", "  )", " (,", "  |"],
    ["\\", " )", "/", " (", "\\", " |"],
]

TREASURE = [
    "    ._____________.",
    "   /::::::::::::::/|",
    "  /_____________/ |",
    "  |####[###]####| |",
    "  |##( N )######|/",
    "  '-------------'",
]

RUINS = [
    "   ||    ||    ||",
    "   ||    ||    ||",
    "  _||____||____||_",
    " |________________|",
    " #=#=#=#=#=#=#=#=#",
]

ROCK = [
    "    .-‾‾-.",
    "  .'      '.",
    " (          )",
    "  '-.____.-'",
]

CAVE = [
    "    _______",
    "   /       \\",
    "  /  .-‾-.  \\",
    " /  (     )  \\",
]

DEBRIS = ["·", "*", "•", "°", "˚", "+"]
BUBBLES = ["°", "o", "O", "·", "*", "0"]
