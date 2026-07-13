"""
Entry point:  python -m aquarium [--mode windowed|fullscreen|screensaver|kiosk]

Examples:
    python -m aquarium                       # windowed (dev / dashboard)
    python -m aquarium --mode fullscreen     # splash + fullscreen experience
    python -m aquarium --mode screensaver    # exits on any input
    python -m aquarium --mode kiosk          # conference/demo machine
"""
from __future__ import annotations
import argparse
from .app import run


def main():
    ap = argparse.ArgumentParser(description="Nythos Aquarium Mode")
    ap.add_argument("--mode", default="windowed",
                    choices=["windowed", "fullscreen", "screensaver", "kiosk"])
    args = ap.parse_args()
    run(args.mode)


if __name__ == "__main__":
    main()
