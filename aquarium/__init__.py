"""
Nythos Aquarium Mode — an immersive cyberpunk ASCII aquarium that doubles as a
live endpoint-health visualization.

Public entry point:
    from aquarium import run
    run("windowed")   # or "fullscreen" / "screensaver" / "kiosk"

Architecture (each module is single-responsibility and extensible):
    config / settings ........ palette, tunables, persistence
    ascii_art / ascii_generator  art library + procedural fish
    entities / fish .......... boids fish agents
    predators / fauna ........ shark + ambient large marine life
    decor .................... reef floor decorations
    particles ................ bubbles / motes
    camera / lighting ........ cinematic drift (lighting baked into renderer)
    effects .................. sonar / AI beam / glitch / alert pulse
    telemetry ................ live psutil-driven state (Nythos feed seam)
    renderer ................. Pillow water layer + pooled canvas entity layer
    scene .................... world + security state machine
    engine ................... timing loop / FPS
    hud / splash ............. overlays
    easter_eggs / audio ...... hidden sequences + optional ambience
    app ...................... CustomTkinter shell + run modes
"""
from .app import run

__all__ = ["run"]
__version__ = "1.0.0"
