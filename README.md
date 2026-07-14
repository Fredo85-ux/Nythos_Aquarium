# Nythos Cyber Aquarium — Python Aquarium Mode

A **Python / CustomTkinter** desktop aquarium: autonomous fish, drifting kelp
and coral, rising bubbles and light rays, rendered on a Tkinter canvas — that
doubles as a calm cybersecurity dashboard. A semi-transparent HUD is fed by
**real** system telemetry (CPU, memory, processes, uptime, network), a
bottom-right clock, and rotating aquatic-themed security tips. Runs windowed,
fullscreen, as a screensaver, or in kiosk mode, and packages into a standalone
`NythosAquarium.exe` with Start Menu / Desktop shortcuts.

![preview](docs/aquarium.png)

---

## Run

Requirements: Python 3.10+, plus:

```powershell
pip install customtkinter pillow psutil
```

```powershell
python -m aquarium                       # windowed (dev / dashboard)
python -m aquarium --mode fullscreen     # splash + fullscreen experience
python -m aquarium --mode screensaver    # exits on any input
python -m aquarium --mode kiosk          # conference/demo machine
```

`nythos_aquarium.py` is the same entry point packaged for PyInstaller
(defaults to `--mode fullscreen`).

### Modes
| Mode | Behaviour |
|------|-----------|
| `windowed` | Resizable window — the dashboard, good for dev. |
| `fullscreen` | Borderless fullscreen with the splash sequence. |
| `screensaver` | Fullscreen; exits on any mouse move / click / key. |
| `kiosk` | Fullscreen; ignores input except the secret exit (Esc-hold) — for conference/demo machines. |

---

## Build a standalone .exe

```powershell
pwsh -ExecutionPolicy Bypass -File build_exe.ps1
# -> dist\NythosAquarium.exe   (PyInstaller, one-file, windowed)

pwsh -ExecutionPolicy Bypass -File install_aquarium.ps1
# per-user install: copies to %LOCALAPPDATA%\Programs\NythosAquarium,
# creates Start Menu + Desktop shortcuts, registers an uninstall entry
# (no admin needed)
```

---

## Keyboard controls
| Key | Action |
|-----|--------|
| `Esc` | Exit (windowed / fullscreen / kiosk*) |
| `F11` / `F` | Toggle fullscreen |
| `F2` | Drop to the windowed dashboard from any mode |
| `H` | Toggle HUD |
| `M` | Toggle audio |
| `T` | Demo threat event |
| `S` | Manual sonar sweep |
| `P` | Cycle theme (Reef / Abyss / Cyberpunk) |
| `W` | Spawn a whale |
| `+` / `-` | Add / remove fish |
| type `"debug"` | Toggle the debug/stats overlay |

Plus hidden easter-egg sequences: the Konami code turns the fish into dragons,
typing `"fredo"` spawns a scuba diver with a glowing laptop, and typing
`"mythos"` reveals a giant phoenix silhouette beneath the water.

---

## Architecture

| Module | Responsibility |
|--------|----------------|
| `app.py` | CustomTkinter application shell — window/fullscreen/kiosk lifecycle, input, easter eggs. |
| `engine.py` | Frame loop orchestrator; drives the scene and renderer each tick. |
| `renderer.py` | Canvas renderer — a Pillow-generated water layer (gradient, light rays, glows, fog) plus a pooled entity layer (fish/fauna/decor/particles). |
| `scene.py` | Owns all live entities; wires telemetry into behaviour and triggers threat/sonar events. |
| `entities.py` / `ascii_art.py` / `ascii_generator.py` | Base entity model and the ASCII/vector art used to draw fish and creatures. |
| `fauna.py` / `predators.py` / `decor.py` | Species definitions (jellyfish, turtles, whales, octopus, stingrays), the threat shark, and reef decor (coral, kelp, treasure, ruins). |
| `particles.py` | Bubbles and suspended motes. |
| `camera.py` | Gentle camera drift. |
| `hud.py` | The dashboard overlay: brand, system health, threat monitor, clock, tip of the tank — mirrors the native `.scr` HUD layout. |
| `telemetry.py` | Background-thread `psutil` polling (CPU/mem/processes/uptime/network) → a threat/risk score that drives HUD and fish behaviour. |
| `tips.py` | Offline rotating "Tip of the Tank" security facts with fade in/out. |
| `audio.py` | Optional lightweight sonar-ping audio via `winsound` (no third-party dependency). |
| `easter_eggs.py` | Hidden keyboard sequences. |
| `splash.py` | Fullscreen intro splash sequence. |
| `config.py` | `Palette` (incl. theme presets) and the `Settings` dataclass — all tunables live here. |
| `settings.py` | Loads/saves `Settings` as JSON under `%APPDATA%\Nythos\Aquarium\settings.json`. |

---

## Configuration

Settings persist automatically to `%APPDATA%\Nythos\Aquarium\settings.json`
(see `config.py`'s `Settings` dataclass):

- **Ecosystem:** `fish_count`, `particle_density`
- **Presentation:** `quality` (`low` / `med` / `high`), `theme`
  (`reef` / `abyss` / `cyberpunk`), `glow`, `camera_drift`, `show_hud`, `show_tips`
- **Telemetry:** `use_telemetry` — real CPU/memory/process/uptime/network data
  drives fish liveliness, sonar sweeps and the threat state instead of random motion
- **Audio:** `audio` (ambient sonar pings)

### Themes

`P` cycles between three water-gradient looks, matching the native `.scr`'s
theme system: **Reef** (teal-green shallows), **Abyss** (darker blue-black
depths), and **Cyberpunk** (violet/black — the default Nythos identity).

---

## Related: the native `.scr` screensaver

This repo also contains a separate, native **C++ / DirectX 11** Windows
screensaver build (`src/`, `shaders/`, builds to `NythosCyberAquarium.scr`)
with a full HDR shader pipeline — god rays, caustics, bloom, GPU-instanced
boids fish, and optional local-Ollama-generated tips. It shares the same
concept and HUD layout as this Python version but is a fully independent
renderer. See `src/` and `shaders/` if you're looking for that build; this
README now documents the Python program as the primary experience.

---

## Notes / gotchas

- The Python build is fully offline — no Ollama dependency; `tips.py` ships
  its own rotating fact pool.
- `renderer.py`'s water layer regenerates at a low cadence (`bg_fps`, default
  12 Hz) since the Pillow pass is the most expensive part of a frame; the
  60 FPS entity layer (fish/particles) doesn't notice.
- `quality` controls the background's internal downscale factor — perf vs.
  crispness tradeoff on lower-end machines.
