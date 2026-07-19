# FreeSpace 2 engine survey

A subsystem-level map of the 1998 Volition FreeSpace 2 engine as it stands on
`master` (the retail-forward, single-player, Linux port). The goal is not to
make the game run — it already does — but to **record the knowledge** in this
codebase: what each subsystem does, how it works, and what in it is worth
mining, extracting, or documenting.

This survey is the *board*: shallow and complete. Deep dives (the SEXP VM, the
POF model parser) live in their own documents and link back here.

## Why this is tracked, and how it resists going stale

Analysis that anchors `file:line:` into `master` rots as the code moves. This
document avoids that by convention:

- **Symbol-first anchors.** Every reference names a symbol — `parse/sexp.cpp:
  eval_sexp()` — not a bare line number. Symbols survive edits; line numbers
  don't. A line may appear as a *hint*, never as the sole anchor.
- **Archaeology anchors to `retail`.** Where a claim is about the *original*
  1998 engine rather than the current port, it cites the immutable `retail`
  branch (`663b3471b`, never moves), marked `[retail]`. A line into `retail`
  is eternal by construction.
- **Git is the obsolescence manager.** Because this lives in the repo, a
  refactor updates the relevant survey file in the *same commit*, and
  `git blame` tells you exactly how stale any claim is relative to the code.

The untracked compilation-mode analysis files (Emacs, `public/notes.txt`)
remain the ephemeral *working / port-log* layer. This survey is the durable one.

## The board

`retail` ships **57** subsystem directories under `code/`. The port excised
**8** (multiplayer, non-Linux backends, dead codecs), leaving **49** live
subsystems on `master`. All 49 are surveyed here.

**Excised from retail (not surveyed, listed for honesty):**
`demo` `directx` `exceptionhandler` `glide` `inetfile` `network` `scramble`
`vcodec` — see `[retail]` if you need their original shape.

### Cluster map

| # | Cluster | Subsystems | File |
|---|---------|-----------|------|
| 01 | Combat entities | ship, weapon, cmeasure | [01-combat-entities.md](01-combat-entities.md) |
| 02 | World & physics | object, physics, debris, asteroid, fireball, jumpnode, observer | [02-world-physics.md](02-world-physics.md) |
| 03 | Mission & SEXP VM | parse (**SEXP**), mission, gamesequence | [03-mission-sexp.md](03-mission-sexp.md) |
| 04 | Render & models | graphics, render, model (**POF**), lighting, bmpman, palman, tgautils, pcxutils, starfield, nebula, particle | [04-render-models.md](04-render-models.md) |
| 05 | UI & HUD | hud, missionui, menuui, ui, popup, radar, controlconfig, stats, gamehelp | [05-ui-hud.md](05-ui-hud.md) |
| 06 | Platform / infra / audio | freespace2, io, cfile, cfilearchiver, sound, gamesnd, osapi, math, globalincs, localization, cmdline, playerman, anim, cutscene, debugconsole, cryptstring | [06-platform-infra.md](06-platform-infra.md) |

### Flagged for extraction

Two subsystems are marked for carving out into standalone, reusable parsers.
The survey identifies their seams; the deep dives will cut along them.

- **SEXP VM parser** (`parse/sexp.cpp`) — separate the text→node-tree parser
  from the evaluator and the game. **Seam found:** the reader path
  (`get_sexp_main`/`get_sexp`/`alloc_sexp`/`identify_operator`) touches *no*
  game state; all game coupling lives in `check_sexp_syntax()`, gated on
  `Fred_running`, which the runtime parser never calls. See cluster 03.
- **POF model parser** (`model/modelread.cpp`) — separate the file→polymodel
  reader from the rendering/collision runtime, to be oracled against the
  sibling **PCS2** POF editor. **Seam found:** the parser produces a fully-
  populated `polymodel` (including the opaque `submodel[i].bsp_data` blobs) and
  nothing else; the BSP bytecode is copied verbatim, never rebuilt, so the cut
  runs exactly along "copy `bsp_data`" vs "walk `bsp_data`". See cluster 04.

### Known gaps and corrections

- **AI now surveyed** in cluster 01 (it lives in `code/ship/` alongside the
  ship entity). Correction: there is **no `aiturret.cpp`** — turret AI is inside
  `aicode.cpp` (14,786L). Inventory: `ai.cpp`, `aicode.cpp`, `aibig.cpp`,
  `aigoals.cpp` (~18k). The SEXP↔AI bridge is `Sexp_ai_goal_links[]` →
  `ai_add_ship_goal_sexp` → `ai_add_goal_sub_sexp` (relevant to the SEXP dive).
- **Cutscenes are stubbed, not ported.** `movie_play()` is called but undefined
  project-wide; there is no ffmpeg/avcodec in the tree. `cutscene/` is a menu
  shell around a dead call — ffmpeg MVE decode remains future work.
- **Two unrelated "nebula" systems:** `starfield/nebula.cpp` (legacy FS1
  rotating background bitmap) vs `nebula/neb.cpp` (FS2 volumetric fog).

## Per-subsystem entry schema

Each subsystem entry carries: **Purpose**, **Entry points**, **Core state**,
**Mechanism**, **Rare knowledge** (the mining value), **Deps**,
**Extraction seams**, **Port notes**.

---
*Status: first pass complete — all 49 live subsystems mapped across clusters
01–06, plus the AI (cluster 01). Open follow-ups: the two flagged extraction
deep dives (SEXP parser — see [../sexp-vm.md](../sexp-vm.md); POF parser). Board
and conventions final.*
