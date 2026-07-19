"""
nythos_aquarium.py — Standalone launcher / PyInstaller entry point.

Bundled into NythosAquarium.exe. Accepts the same modes as `python -m aquarium`:
    NythosAquarium.exe                       (windowed dashboard)
    NythosAquarium.exe --mode fullscreen
    NythosAquarium.exe --mode screensaver
    NythosAquarium.exe --mode kiosk
"""
from __future__ import annotations
import argparse
from aquarium.app import run


def main():
    ap = argparse.ArgumentParser(description="Nythos Aquarium Mode")
    ap.add_argument("--mode", default="fullscreen",
                    choices=["windowed", "fullscreen", "screensaver", "kiosk"])
    args, _ = ap.parse_known_args()
    run(args.mode)


if __name__ == "__main__":
    main()
