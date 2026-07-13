"""
audio.py — Optional ambient audio (toggleable, fully safe to disable).

Kept intentionally lightweight: uses winsound for occasional soft sonar pings so
there is zero third-party audio dependency. Looping underwater ambience / cyber
synth would slot in here (e.g. via an optional `simpleaudio`/`pygame` backend)
behind the same interface. All calls no-op when audio is off or unavailable.
"""
from __future__ import annotations
import threading

try:
    import winsound
except Exception:                  # pragma: no cover (non-Windows)
    winsound = None


class Audio:
    def __init__(self, settings):
        self.s = settings

    @property
    def enabled(self) -> bool:
        return self.s.audio and winsound is not None

    def toggle(self) -> bool:
        self.s.audio = not self.s.audio
        return self.s.audio

    def _beep(self, freq: int, ms: int):
        if not self.enabled:
            return
        try:
            winsound.Beep(freq, ms)
        except Exception:
            pass

    def ping(self):
        """Soft sonar ping — runs off-thread so it never stalls rendering."""
        if self.enabled:
            threading.Thread(target=self._beep, args=(880, 60), daemon=True).start()

    def bubble(self):
        if self.enabled:
            threading.Thread(target=self._beep, args=(440, 30), daemon=True).start()
