"""
easter_eggs.py — Hidden keyboard sequences.

Watches a rolling buffer of key presses and fires named events the scene/app
react to:
    * Konami code          -> fish become dragons
    * type "fredo"         -> a scuba diver with a glowing laptop appears
    * type "mythos"        -> a giant phoenix silhouette beneath the water
    * type "debug"         -> toggles the debug/stats overlay
"""
from __future__ import annotations

KONAMI = ["Up", "Up", "Down", "Down", "Left", "Right", "Left", "Right", "b", "a"]


class EasterEggs:
    def __init__(self):
        self._keys: list[str] = []      # recent keysyms (for konami)
        self._typed = ""                # recent letters (for words)

    def feed(self, keysym: str) -> str | None:
        """Return an event name if a sequence completed, else None."""
        # konami uses arrow keysyms + b/a
        self._keys.append(keysym)
        if len(self._keys) > len(KONAMI):
            self._keys.pop(0)
        if self._keys == KONAMI:
            self._keys.clear()
            return "konami"

        # word triggers
        if len(keysym) == 1 and keysym.isalpha():
            self._typed = (self._typed + keysym.lower())[-12:]
            for word, event in (("fredo", "fredo"), ("mythos", "mythos"), ("debug", "debug")):
                if self._typed.endswith(word):
                    return event
        return None
