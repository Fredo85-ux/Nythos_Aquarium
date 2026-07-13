"""
renderer.py — Canvas renderer.

Two layers:
  1. Water layer  — a Pillow image (depth gradient + drifting volumetric light
     rays + soft violet glows + fog + threat tint), regenerated at a low cadence
     and blitted as a single Canvas image. Rendering it at reduced resolution +
     a Gaussian blur gives a soft, glowing, cinematic look very cheaply.
  2. Entity layer — pooled Canvas text items for fish/fauna/decor/particles plus
     vector effects (sonar rings, AI beam). Item pooling (reuse + reposition,
     never recreate) keeps it allocation-flat for 24/7 operation.

The renderer is mode-agnostic so future scenes (Space, Data Center, …) can reuse
it by swapping the water-layer generator and the entity set.
"""
from __future__ import annotations
import math
import random
from PIL import Image, ImageDraw, ImageTk, ImageFilter

from . import ascii_art as art
from .config import Palette, rgb, lerp_rgb, dim


class _Ray:
    __slots__ = ("x", "w", "spd", "amp", "alpha", "color")


class Renderer:
    def __init__(self, canvas, settings, rng: random.Random):
        self.canvas = canvas
        self.s = settings
        self.rng = rng
        self.w = settings.width
        self.h = settings.height

        self._bg_item = canvas.create_image(0, 0, anchor="nw")
        self._photo = None
        self._bg_timer = 0.0
        self._gradient = None
        self._rays: list[_Ray] = []
        self._glows = []
        self._build_water_static()

        # entity item pool: each slot = (glow_id, main_id)
        self._ent_pool: list[tuple[int, int]] = []
        self._part_pool: list[int] = []

        # per-item config cache so we only re-issue itemconfigure on change
        # (Tk re-parses fonts/colours every call — caching keeps draw cheap).
        self._cache: dict[int, dict] = {}

    # ---- resize -----------------------------------------------------------
    def resize(self, w, h):
        self.w, self.h = w, h
        self._build_water_static()

    # ---- cached item config ----------------------------------------------
    def _apply(self, item, x, y, **kw):
        self.canvas.coords(item, x, y)
        last = self._cache.get(item)
        if last is None:
            last = {}
            self._cache[item] = last
        changed = {k: v for k, v in kw.items() if last.get(k) != v}
        if changed:
            self.canvas.itemconfigure(item, **changed)
            last.update(changed)

    def _hide(self, item):
        last = self._cache.get(item)
        if last is None or last.get("state") != "hidden":
            self.canvas.itemconfigure(item, state="hidden")
            self._cache.setdefault(item, {})["state"] = "hidden"

    # ---- background internal size (capped: cost is resolution-independent) -
    def _bg_size(self):
        sc = self.s.bg_scale()
        sw, sh = max(1, self.w // sc), max(1, self.h // sc)
        cap_w = 560
        if sw > cap_w:
            sh = max(1, int(sh * cap_w / sw))
            sw = cap_w
        return sw, sh

    # ---- static water assets (gradient + ray/glow layout) -----------------
    def _build_water_static(self):
        sw, sh = self._bg_size()
        grad = Image.new("RGB", (sw, sh))
        d = ImageDraw.Draw(grad)
        for y in range(sh):
            t = y / max(1, sh - 1)
            if t < 0.5:
                c = lerp_rgb(Palette.BG_TOP, Palette.BG_MID, t * 2)
            else:
                c = lerp_rgb(Palette.BG_MID, Palette.BG_DEEP, (t - 0.5) * 2)
            d.line([(0, y), (sw, y)], fill=c)
        self._gradient = grad.convert("RGBA")

        self._rays = []
        for _ in range(6):
            r = _Ray()
            r.x = self.rng.uniform(-0.1, 1.0)
            r.w = self.rng.uniform(0.06, 0.16)
            r.spd = self.rng.uniform(0.04, 0.12)
            r.amp = self.rng.uniform(0.02, 0.06)
            r.alpha = self.rng.randint(14, 34)
            r.color = self.rng.choice([Palette.CYAN, Palette.VIOLET, Palette.BLUE])
            self._rays.append(r)

        self._glows = []
        for _ in range(4):
            self._glows.append((self.rng.uniform(0, 1), self.rng.uniform(0, 1),
                                self.rng.uniform(0.18, 0.34),
                                self.rng.choice([Palette.VIOLET, Palette.BLUE])))

    def _regen_water(self, t, threat: float):
        sw, sh = self._bg_size()
        img = self._gradient.copy()
        overlay = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
        d = ImageDraw.Draw(overlay)

        # drifting volumetric light rays from the surface
        for r in self._rays:
            x = (r.x + math.sin(t * r.spd) * r.amp) * sw
            wr = r.w * sw
            drift = math.sin(t * r.spd * 1.3) * sw * 0.08
            poly = [(x, 0), (x + wr, 0),
                    (x + wr * 0.5 + drift, sh), (x - wr * 0.5 + drift, sh)]
            d.polygon(poly, fill=(*r.color, r.alpha))

        # slow drifting violet glows (depth fog pools)
        for gx, gy, gr, gc in self._glows:
            cx = (gx + math.sin(t * 0.05 + gx * 6) * 0.05) * sw
            cy = (gy + math.cos(t * 0.04 + gy * 6) * 0.05) * sh
            rr = gr * sw
            d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], fill=(*gc, 22))

        overlay = overlay.filter(ImageFilter.GaussianBlur(6))
        img = Image.alpha_composite(img, overlay).convert("RGB")

        # threat tint — water darkens and shifts toward red
        if threat > 0.01:
            tint = Image.new("RGB", (sw, sh), (60, 6, 16))
            img = Image.blend(img, tint, min(0.5, threat * 0.5))

        img = img.resize((self.w, self.h), Image.BILINEAR)
        self._photo = ImageTk.PhotoImage(img)
        self.canvas.itemconfig(self._bg_item, image=self._photo)

    # ---- pooling helpers --------------------------------------------------
    def _ent_slot(self, i):
        while i >= len(self._ent_pool):
            g = self.canvas.create_text(0, 0, text="", anchor="c", state="hidden",
                                        justify="left", tags="ent")
            m = self.canvas.create_text(0, 0, text="", anchor="c", state="hidden",
                                        justify="left", tags="ent")
            self._ent_pool.append((g, m))
        return self._ent_pool[i]

    def _part_slot(self, i):
        while i >= len(self._part_pool):
            self._part_pool.append(
                self.canvas.create_text(0, 0, text="", anchor="c", state="hidden", tags="part"))
        return self._part_pool[i]

    # ---- main draw --------------------------------------------------------
    def draw(self, scene, camera, dt):
        # 1) water layer (low cadence)
        self._bg_timer += dt
        if self._photo is None or self._bg_timer >= 1.0 / max(1, self.s.bg_fps):
            self._bg_timer = 0.0
            self._regen_water(scene.time, scene.effects.threat_pulse)

        cx, cy = self.w / 2, self.h / 2
        glitch = scene.effects.glitch

        # 2) entities — decor (back) -> fauna -> fish -> predators, depth-sorted
        render_list = []
        for dco in scene.decor:
            render_list.append(("decor", dco))
        ents = scene.fauna + scene.fish + scene.predators
        ents.sort(key=lambda e: -e.depth)            # far first
        for e in ents:
            render_list.append(("ent", e))

        idx = 0
        for kind, obj in render_list:
            glow_id, main_id = self._ent_slot(idx)
            idx += 1
            if kind == "decor":
                self._draw_decor(obj, scene, glow_id, main_id)
            else:
                self._draw_entity(obj, scene, camera, cx, cy, glitch, glow_id, main_id)

        for j in range(idx, len(self._ent_pool)):       # hide unused
            g, m = self._ent_pool[j]
            self._hide(g)
            self._hide(m)

        # 3) particles
        pi = 0
        for p in scene.particles.alive():
            it = self._part_slot(pi); pi += 1
            px, py = camera.apply(p.x, p.y, cx, cy)
            c = dim(p.color, 0.8)
            self._apply(it, px, py, text=p.char, fill=rgb(c),
                        font=("Consolas", 11), state="normal")
        for j in range(pi, len(self._part_pool)):
            self._hide(self._part_pool[j])

        # 4) vector effects (cleared & redrawn each frame; few items)
        self._draw_effects(scene, camera, cx, cy)

    # ---- entity drawing ---------------------------------------------------
    def _entity_color(self, e, scene):
        # depth fog: fade far entities toward the deep water colour
        c = lerp_rgb(e.base_color if hasattr(e, "base_color") else e.color,
                     Palette.BG_DEEP, e.depth * 0.55)
        if getattr(e, "analyzing", 0) > 0:               # AI beam highlight
            pulse = 0.5 + 0.5 * math.sin(scene.time * 12)
            c = lerp_rgb(c, Palette.EMERALD, 0.4 + 0.4 * pulse)
        if scene.state == "threat" and e.kind != "shark":
            c = lerp_rgb(c, Palette.THREAT, 0.12)
        return c

    def _draw_entity(self, e, scene, camera, cx, cy, glitch, glow_id, main_id):
        rows = art.mirror(e.rows) if e.facing < 0 else e.rows
        text = "\n".join(rows)
        size = max(8, int(13 * e.scale * (1.25 - 0.55 * e.depth)))
        gx = self.rng.uniform(-3, 3) * glitch if glitch > 0.01 else 0
        x, y = camera.apply(e.x + gx, e.y, cx, cy)
        col = self._entity_color(e, scene)

        if self.s.glow and e.glow:
            self._apply(glow_id, x, y, text=text, fill=rgb(dim(col, 0.5)),
                        font=("Consolas", size + 2), state="normal", anchor="c")
        else:
            self._hide(glow_id)

        self._apply(main_id, x, y, text=text, fill=rgb(col),
                    font=("Consolas", size), state="normal", anchor="c")

    def _draw_decor(self, dco, scene, glow_id, main_id):
        self._hide(glow_id)
        sway = dco.offset(scene.time)
        text = "\n".join(dco.rows)
        col = dco.color
        if scene.state == "threat":
            col = lerp_rgb(col, Palette.THREAT, 0.1)
        self._apply(main_id, dco.x + sway, dco.y, text=text, fill=rgb(col),
                    anchor="nw", font=("Consolas", 13), state="normal")

    # ---- effects ----------------------------------------------------------
    def _draw_effects(self, scene, camera, cx, cy):
        self.canvas.delete("fx")
        for e in scene.effects.effects:
            x, y = camera.apply(e.x, e.y, cx, cy)
            if e.kind == "sonar":
                p = e.progress
                rad = p * max(self.w, self.h) * 0.6
                a = int(180 * (1 - p))
                col = lerp_rgb(Palette.BG_DEEP, Palette.CYAN, (1 - p))
                self.canvas.create_oval(x - rad, y - rad, x + rad, y + rad,
                                        outline=rgb(col), width=2, tags="fx")
            elif e.kind == "ai_scan":
                # scanning beam from top + reticle box around the fish
                col = Palette.EMERALD
                self.canvas.create_line(x, 0, x, y, fill=rgb(col), width=1,
                                        dash=(4, 4), tags="fx")
                r = 26
                self.canvas.create_rectangle(x - r, y - r, x + r, y + r,
                                             outline=rgb(col), width=1, tags="fx")
                self.canvas.create_text(x + r + 6, y, text="AI ANALYSIS", anchor="w",
                                        fill=rgb(col), font=("Consolas", 9), tags="fx")
