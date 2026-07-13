"""
settings.py — Load/save the Settings dataclass as JSON under %APPDATA%/Nythos.
"""
from __future__ import annotations
import json
import os
from dataclasses import asdict
from .config import Settings


def _path() -> str:
    base = os.environ.get("APPDATA") or os.path.expanduser("~")
    d = os.path.join(base, "Nythos", "Aquarium")
    os.makedirs(d, exist_ok=True)
    return os.path.join(d, "settings.json")


def load() -> Settings:
    s = Settings()
    try:
        with open(_path(), "r", encoding="utf-8") as f:
            data = json.load(f)
        for k, v in data.items():
            if hasattr(s, k):
                setattr(s, k, v)
    except Exception:
        pass
    return s


def save(s: Settings) -> None:
    try:
        with open(_path(), "w", encoding="utf-8") as f:
            json.dump(asdict(s), f, indent=2)
    except Exception:
        pass
