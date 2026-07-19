# Resolution & UI scaling — retail's two-value model and the SCP virtual-canvas blueprint

A design-mining note on **how to run the game above 1024×768**. It separates the
question into the two independent problems it actually is, documents the
resolution-independence machinery already worked out in the `reference/2023`
fork (SCP's virtual-canvas system), and sketches a **retail-shaped adaptation**
that is markedly less invasive than the fork's own.

No code has been written for this yet — this is the delineation doc, the
resolution analogue of [sexp-vm.md](sexp-vm.md) / [pof-model.md](pof-model.md).

**Anchor convention.** Symbols are the anchor; line hints are hints. Two source
lines coexist here:
- **retail / working tree** — plain `code/graphics/2d.cpp:NNN`, against `master`.
- **the reference blueprint** — `reference/2023:src/graphics/2d.cc:NNN`. That
  tree is a `.cc/.hh`, `src/`-reorganised fork and is **not in the working
  tree**; read an anchor with `git show reference/2023:src/graphics/2d.cc` (or
  `git --no-pager cat-file -p reference/2023:<path>`). Mine the **design**, not
  the diff — the fork is the modern SCP engine and nothing there is cherry-pickable
  as code ([[openspace-state]] reference-branch note; ignore Bobboau's graphics
  rework specifically).

---

## 1. Resolution is two problems, not one

Retail conflates two things under one word. Keeping them apart is the whole
insight.

1. **The framebuffer** — the pixel dimensions of the surface the engine draws
   into (`gr_screen.max_w/max_h`) and the SDL window that presents it. This is a
   *size*.
2. **`gr_screen.res`** — the enum `GR_640` / `GR_1024` (`code/graphics/2d.h`,
   `GR_640 = 0`, `GR_1024 = 1`). This is **not a size — it is a content
   selector**. ~46 files branch on it to choose between two hand-authored UI
   layouts (integer coordinate tables + art bitmaps).

Getting a bigger framebuffer is a few lines. Making the UI *look right* in it is
the real work, and it lives entirely in problem (2).

---

## 2. Retail's model — a two-value switch

**Framebuffer.** `gr_init(res, mode, depth, fred_x, fred_y)`
(`code/graphics/2d.h:299`) → `gr_init_res()` (`code/graphics/2d.cpp`) forces the
surface size from a two-case switch on `res`:

- `code/graphics/2d.cpp:308-321` — `case GR_640: 640×480` / `case GR_1024:
  1024×768` / `default: Int3()`. **But the `else` branch (FRED/pofview) already
  feeds arbitrary `fred_x/fred_y`** — so the rasterizer was never resolution-bound;
  only this switch is.
- Everything downstream keys off `max_w/max_h`, not the enum: clip rects
  (`2d.cpp:343-348`), `save_screen`/`gr_reset_clip`, and the window itself —
  `os_create_window(gr_screen.max_w, gr_screen.max_h)` (`code/graphics/grsoft.cpp:789`).
- Selection at boot: `has_sparky_hi` file-probe → `gr_init(has_sparky_hi ?
  GR_1024 : GR_640, …)` (`code/freespace2/freespace.cpp:1577-1591`).

So a full 1440-tall **framebuffer + window** is reachable by replacing the
switch with explicit dimensions (a `-res W×H` flag, or a new case). The 3-D
world renders at that size immediately.

**Content.** `gr_screen.res` drives two authored layouts. Coordinates are
literal integer pairs chosen by the enum, e.g. `code/freespace2/freespace.cpp`:
`63, 316  // GR_640` vs `101, 505  // GR_1024`, and loading-screen art
`"LoadingBG"` / `"2_LoadingBG"`. Art for the hi-res layout ships in
`sparky_hi_fs2.vp`. **There is no 1440 layout and no 1440 art.** Leave `res =
GR_1024` under a 2560×1440 framebuffer and every UI element draws at its literal
1024-space coordinate → the UI clusters into the top-left ~40 % of the screen;
1024×768 backgrounds leave the rest black. The 3-D viewport, though, is full-size.

Retail's `gr_screen` already carries the seed of a fix: `int offset_x, offset_y`
(`code/graphics/2d.h:95`) — a draw origin the blit layer already honours.

---

## 3. The reference blueprint — SCP's virtual canvas

`reference/2023` carries the full SCP resolution-independence system. It is
precisely the "native-3-D + virtual-canvas UI" design, generalised to **arbitrary
resolution and arbitrary aspect ratio**. The core is `src/graphics/2d.cc`.

### 3.1 Virtual canvas + scale factors

The UI is authored once in a fixed **virtual canvas** (still retail's 1024×768,
selected by the surviving `gr_screen.res`) and multiplied up to the real
framebuffer.

- `reference/2023:src/graphics/2d.cc:100` — **`gr_set_screen_scale(w, h, zoom_w,
  zoom_h, max_w, max_h, center_w, center_h, force_stretch)`**. The heart of it:
  `Gr_full_resize_X = (float)max_w / (float)w` (real ÷ virtual), likewise Y. All
  scale state is module-static (`Gr_full_resize_X/Y`, `Gr_resize_X/Y`,
  `Gr_menu_offset_X/Y`, `2d.cc:78-88`), with a `Gr_save_*` shadow set for
  push/pop.
- `reference/2023:src/graphics/2d.cc:225-227` — the virtual canvas dimensions are
  `(gr_screen.res == GR_1024) ? 1024 : 640` × `768 : 480`. `res` survives, demoted
  from "the resolution" to "which authored canvas."
- `reference/2023:src/graphics/2d.cc:205` — **`gr_reset_screen_scale()`** restores
  the `Gr_save_*` shadows (the pop).
- Convenience entry + auto-pick: `reference/2023:src/graphics/2d.cc:742` classifies
  a real size into `GR_1024`/`GR_640` by a width/height threshold; the 2-arg
  `gr_set_screen_scale(width, height)` call site is
  `reference/2023:src/freespace2/freespace.cc:6203`.

### 3.2 The per-call resize primitives — the invasive part

Every 2-D draw call carries a **resize mode** saying whether its coordinates are
virtual (scale them) or already real (leave them).

- `reference/2023:src/graphics/2d.cc:232` — **`gr_resize_screen_pos(x, y, w, h,
  resize_mode)`** (virtual → real). The switch multiplies by the scale factor per
  mode: `case GR_RESIZE_FULL: xy_tmp = (*x) * Gr_full_resize_X` at `2d.cc:253`
  (and `:275/:297/:312` for y/w/h).
- `reference/2023:src/graphics/2d.cc:423` — **`gr_unsize_screen_pos(...)`** (real →
  virtual), the inverse: `case GR_RESIZE_FULL: xy_tmp = (*x) / Gr_full_resize_X`
  at `2d.cc:347/:369/:391/:406`. Used to map mouse/hit-test coordinates back into
  canvas space.
- The **`GR_RESIZE_*` enum** (`reference/2023:src/graphics/2d.hh`) — `GR_RESIZE_NONE`
  (real pixels, no scale), `GR_RESIZE_FULL` (full framebuffer scale),
  `GR_RESIZE_MENU` / center / zoomed variants (canvas scale with the letterbox
  offset applied).

This is why porting it is invasive: the mode argument is threaded through *every*
UI/HUD draw site — the fork touches dozens of files (animplay, controlsconfig,
cutscenes, contexthelp, gameplayhelp, hud, console, missionscreencommon, …) for
exactly this reason.

### 3.3 Aspect handling is built in

`gr_set_screen_scale` (`reference/2023:src/graphics/2d.cc:100-200`) solves the
4:3-content-on-16:9-screen problem directly:

- `aspect_quotient = (center_w/center_h) / (w/h)` — how far the real display
  departs from the canvas aspect.
- It then either **letterboxes** (centre the canvas via `Gr_menu_offset_X/Y`,
  leaving pillar/letter bars) or **stretches** (`force_stretch ||
  Cmdline_stretch_menu` → `Gr_resize_* = Gr_full_center_resize_*`, offsets zeroed).
- A `zoom_w/zoom_h` path supports a separately-scaled sub-region (the 3-D cockpit
  view vs the surrounding menu).

### 3.4 The `gr_screen` struct grows

Retail's `screen` gains a family of real-vs-virtual fields
(`reference/2023:src/graphics/2d.hh:598-644`):

- `max_w_unscaled, max_h_unscaled` — the virtual canvas size (what UI code thinks
  in).
- `max_w_unscaled_zoomed, max_h_unscaled_zoomed` — the zoomed sub-region.
- `center_w, center_h`, `center_offset_x, center_offset_y` — the active display
  rectangle inside a possibly-larger surface (multi-monitor; the letterbox origin).
- `bool custom_size` (`2d.hh:644`) — set when real ≠ canvas; gates the scaling
  fast-path (`2d.cc:245` early-outs when `!custom_size`).
- `save_*` shadows for each — the push/pop discipline again.

---

## 4. The retail-shaped adaptation — a shortcut the fork can't take

The fork needs *per-call* resize modes because it renders 3-D and 2-D into one
buffer at different scales, with render-to-texture and multi-monitor centring in
play. **Retail is far simpler:** it draws its entire UI in the 1024 canvas and
essentially never needs real-pixel 2-D except the 3-D viewport. That asymmetry
buys a cheaper design.

**Proposal: one implicit virtual→real scale in the blit layer, not a per-call
enum.**

- Keep `res = GR_1024` as the virtual canvas (unchanged — all 46 `.res` sites and
  all hi-res art keep working verbatim).
- Split the framebuffer size from the canvas: `gr_init` takes real `W×H`; set
  `max_w_unscaled/​max_h_unscaled = 1024/768` and compute one global
  `Gr_full_resize_X/Y = W/1024, H/768` — the `2d.cc:100/105` math, minus the zoom
  and multi-monitor arms.
- Apply that scale **once, low in the primitives** — `gr_bitmap`, `gr_string`,
  `gr_rect`, `gr_line`, and the clip setter — reusing the existing
  `gr_screen.offset_x/offset_y` (`code/graphics/2d.h:95`) for the letterbox origin.
  UI code keeps passing 1024-space integers; it never learns it's been scaled.
- Provide **one opt-out** for the 3-D scene render (draw at real size, no canvas
  scale) — the single place that genuinely wants full-res pixels. This is the
  fork's `GR_RESIZE_NONE`, collapsed from an every-call argument to a single
  render-path flag.
- Invert the same global factor for mouse coordinates (the `gr_unsize` role,
  `2d.cc:423`) so hit-testing stays in canvas space — one function, one call in
  the input path.

**Net:** the fork's dozens-of-files change becomes a handful of primitive-layer
edits plus one input-path inversion, because retail's UI is single-canvas. We
inherit the fork's proven scale math and its letterbox/stretch aspect handling
(§3.3) while paying almost none of its threading cost.

**Trade-off to accept up front:** software-renderer scaling is a per-pixel blit
cost (the software path has no GPU sampler); under `-opengl` the scale is free
(texture sampling). At 4:3 (e.g. 1920×1440) the scale is a uniform integer-ish
factor and the software cost is tolerable; 16:9 adds pillarbox math but no new
per-pixel cost. Recommend proving the path at **4:3 first** (uniform scale, no
aspect arm), then adding the 16:9 letterbox as a deliberate second step.

---

## 5. Plan

0. **This doc.** *(done)*
1. **Framebuffer + window** — `gr_init` takes explicit `W×H`; replace the
   `2d.cpp:308-321` switch; verify the 3-D world renders at 1920×1440 with the UI
   *knowingly* mispositioned. A pure size change, visible immediately, commits
   nothing to the UI design.
2. **Implicit canvas scale** (§4) — global `Gr_full_resize_*` from `W/1024`,
   applied in the blit/clip primitives via `offset_x/offset_y`; 3-D-viewport
   opt-out flag; mouse un-scale. UI correct at 4:3.
3. **Aspect pass** — port the `aspect_quotient` letterbox/stretch arm
   (`2d.cc:100-200`) for 16:9; `-stretch` cmdline mirroring `Cmdline_stretch_menu`.

**Open decisions:**
- Canvas = fixed 1024×768 always, or keep the `GR_640`/`GR_1024` pick for low-end?
  (Recommend fixed 1024 — the port assumes `sparky_hi`.)
- Software scaling: point-sample (retail-faithful, blocky) vs bilinear (needs a
  new scaler). Recommend point-sample first; the GL path is already sampled.
- No verification oracle exists yet for UI layout the way `pof_dump`/`sexp_dump`
  do for geometry — layout correctness is visual. A frame-dump diff at two
  resolutions (same canvas coords → same relative layout) is the closest
  automatable check.

---
*Status: design-mining note, no code yet. The framebuffer path (stage 1) is a
few lines against `code/graphics/2d.cpp:308-321`; the UI path (stage 2) is the
real work, and §4 argues it is much smaller for retail than the
`reference/2023` fork's per-call system because retail's UI is single-canvas.
Blueprint anchors are git-ref form against `reference/2023` — read with `git
show`. Gated on nothing; pick up when a larger resolution is wanted.*
