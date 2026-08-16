# openspace-fs2

This is <u>**NOT**</u> a port or fork of fs2open, and deliberately so.

A retail-forward Linux port of the 1998 Volition FreeSpace 2 source — the 2002
CVS drop, ported as-is: non-networked, non-multiplayer, single-player, the
retail campaign with the retail SEXP vocabulary and nothing more.

The port starts from the pristine Volition import and moves forward one
subsystem at a time, keeping retail's own code — including its complete 8bpp
software rasterizer, which now runs at 1024x768 on an SDL2 surface and presents
magnified up to 2048x1536, something retail itself never shipped. An OpenGL
backend (revived from Volition's own abandoned `gropengl.cpp` skeleton) sits
beside it. Playable: the campaign boots, briefs, flies, and talks, on OpenAL
sound.

Mining the knowledge in the 1998 engine is a first-class goal, not just a
means: the asm-to-C conversions are brute-force-verified against emulation,
retail bugs are catalogued rather than silently fixed, and behavior decisions
are recorded (shipped behavior wins over authorial intent). The port's running
log lives in [docs/notes.md](docs/notes.md); the rest of what the work has
turned up is mapped under [Docs](#docs) below.

## Not fs2open

fs2open is the other descendant of the same 2002 release, and it is a reference
here, never a base: the `fs2open` branch is read, mined for ideas, and never
built on. Twenty-four years of divergence make it a different engine, and the
differences below were not assumed — each was found by checking a specific
fs2open change against the `retail` import and against this tree.

**Taken.** Twenty retail bugs, mined from fs2open's 2002-07..2005-12 history
(~2,525 commits, three parallel sweeps) and landed one bug per commit, each
citing its fs2open provenance hash: multi-ship SEXPs aborting on the first
departed ship, `never-warp` setting the wrong flag, the `#token#` inverted test
that smashed the stack, `Personas[-1]`, briefing-stage compaction, the
player-entry-delay leak across missions, `physics_apply_whack` ignoring its mass
parameter, dying ships re-entering attack mode, and the rest. These are
cherry-pick-the-*idea*, not literal cherry-picks — the fs2open commits carry SCP
noise around the load-bearing lines. The annotated harvest, with anchors and the
tier reasoning, is [docs/fixmine.txt](docs/fixmine.txt). The OpenGL backend was
likewise filled in from the 2002 fs2open implementation, over Volition's own
abandoned skeleton.

**Refused, deliberately.** Where fs2open changed behavior rather than fixing a
defect, retail wins:

- The `rand` SEXP re-roll. Retail *caches* the result and the stock missions are
  tuned to that; SCP later added `rand-multiple` as a separate operator for
  exactly this reason.
- `vm_matrix_interpolate`'s numeric-margin asserts, which gcc/SSE 32-bit floats
  erode where MSVC's x87 80-bit intermediates did not. fs2open replaced the
  whole function (PR 2668); here the asserts are demoted to diagnostics, because
  the residual they measure is discarded two lines later and retail shipped with
  asserts compiled out.
- SCP's virtual-canvas resolution system, whose per-call resize is a disease the
  8bpp path cannot afford. Presentation scaling gets the same playable window
  with byte-identical frames — see [docs/resolution-scaling.md](docs/resolution-scaling.md).
- The AI-profiles system, an fs2open invention. Retail's flat `ai.tbl` plus the
  hard-coded per-skill tables in `aicode.cpp` is the tuning this campaign was
  balanced against.
- The Lua/libRocket `scpui` layer. Retail's native `menuui`/`missionui` is the
  UI, and dropping scripting drops that whole stack with it.

**Rejected as not-Volition's-bugs.** Six famous early-SCP "fixes" were checked
and turned down because the code they repair does not exist in the 1998 source:
the `hud_config` uninitialised pointer, the "insidious beam bug" (Bobboau's
`shield_factor`), warp-always-knossos, initial-status hull/shield,
`ai_select_primary`'s null deref, and pilot-select cancel corruption. A large
share of early-SCP bugfixes repair SCP's own additions — the working rule that
came out of it is to confirm the touched code is Volition's before crediting the
fix. Separately, some genuine fs2open fixes are simply no-ops on retail data:
stock `weapons.tbl` never sets `$Weapon Range`.

**Agreed independently.** Several defects the survey found on its own turned out
to have fs2open concurring, which is the useful kind of corroboration:
`is_subsys_destroyed`'s bare `false;`, `popup.cc`'s bare-macro
`PF_ALLOW_DEAD_KEYS` test ("unused even in retail," both verdicts),
`hudsquadmsg`'s comma-for-`&` typo, the auto-match-target-speed toggle that had
been dead since retail, and `vm_matrix_to_rot_axis_and_angle`'s inverted
reciprocal — found here by a hands-off campaign flight sweep that aborted in
SM2-02, and fixed the same way upstream.

## Two projects, one lineage

The Linux port is complete, and it is not the end of the line — it is the end
of *a* line. It was mothballed 2026-07-30 at its survey-complete milestone:
the campaign plays, the codebase survey is closed (warnings 4,756 → 15,
every survivor a catalogued decision in [docs/notes.md](docs/notes.md)), and
further chipping would mean modernizing subsystems — the renderer above all
— that the next stage replaces outright.

**This branch is live again as of 2026-08-05**, for one specific reason:
the campaign playtest that was always this port's own closing gate has never
been run to the end. The Godot migration on `godot` reached the point where
its remaining work is *campaign-completion* work — player death, red-alert
missions, debrief promotion stages, the briefing chain — and the sane way to
learn which of those matter, and in what shape, is to finish the retail
campaign here first. So `godot` is mothballed at 31/31 with its restart
brief in its own README, and the flight deck is open here.

The lineage runs retail 2002 → Linux port 2026 → Godot host, in one
continuous history, and neither branch is abandoned: fixes land on whichever
one they belong to, and this branch remains the migration's authoritative
retail reference — the code the GDExtension compiles and the behavior its
oracles diff against.

## Branches

- `retail` — `663b3471b`, the 2002 Volition warpcore CVS import. Pristine,
  immutable.
- `master` — the retail-forward port, and the working branch again as of
  2026-08-05: the campaign playthrough happens here.
- `godot` — the Godot migration, GDExtension-first; mothballed 2026-08-05 at
  31/31 with the campaign simulating end to end. Its README carries the map
  and the four moves that restart it.
- `fs2open` — fs2open `release_26_0_0`, the reference implementation and
  fix-mine source; `git show <hash>` here resolves the provenance hashes cited
  in the fix commits and in [docs/fixmine.txt](docs/fixmine.txt). See
  [Not fs2open](#not-fs2open).
- `reference/*` — abandoned 2018/2023 fork lines; style reference only.

## Layout

- `src/` — the ported corpus: 49 subsystem directories, ~237k lines, compiled
  into a single `foundation` library. Subdirectory-qualified angle includes
  (`<parse/parselo.hh>`), so every subsystem is addressable by its name.
- `bin/` — the shippable executables: the game (`fs2`) and `cfilearchiver`.
- `tests/` — unit tests, oracles, and the stub files the not-yet-ported
  gameplay still needs. The game binary links two of those stubs; that wart is
  documented where it lives, and shrinks as subsystems land.
- `docs/` — the port log, the engine survey, and the design-mining notes.

## Building

Meson + ninja, C++17, Linux only:

```
meson setup build
ninja -C build
```

Dependencies: SDL2, OpenAL, OpenGL. The build deploys the game binary to a
sibling `../rundir` if one exists.

## Game data

You need the retail FreeSpace 2 data from your own copy of the game — the GOG
release works. No game data is included here or ever will be.

Set up a **run directory** holding the retail `.vp` archives at its top level.
An unpacked `data/` tree works too, and the two can coexist: retail searches
loose files first and falls back to the archives, so an unpacked file shadows
its packed twin — which is how a single asset gets swapped for testing.

The run directory is **the directory the `fs2` binary sits in**, not your shell's
working directory. Retail resolves `/proc/self/exe` and `chdir`s there before it
reads anything, and that resolution follows symlinks — so the binary has to be a
real copy in the run directory, which is why `ninja` *copies* it into a sibling
`../rundir` instead of linking it.

## Running

```
cd ../rundir && ./fs2 -window -res 2048x1536
```

- `-window` — windowed instead of fullscreen.
- `-res WxH` — present the authored 1024x768 canvas magnified, in integer
  multiples (see [docs/resolution-scaling.md](docs/resolution-scaling.md)).
- `-opengl` — the GL backend instead of the software rasterizer.
- `-nosound` / `-nomusic`, `-pofspew`, `-coords` — as retail meant them.
- `./fs2 -help` lists the lot. `-name` and `--name` are both accepted.

A one-line `data/cmdline.cfg` in the run directory supplies default arguments;
the real command line is parsed after it and wins.

Settings land in `~/.fs2/config`, a flat file standing in for retail's Windows
registry (`$HOME`, falling back to `.`). Pilots and campaign saves stay where
retail put them, under `data/players/` in the run directory.

### Environment

All `FS2_*` variables are bring-up aids, not shipping configuration; they are
meant to be deleted once the subsystems they debug settle.

- `FS2_FRAME_DUMP=<dir>` — write every Nth presented frame into `<dir>`: a PPM
  through the palette, plus the raw 8bpp indexes and both palettes, so a
  "wrote the wrong indexes" bug is distinguishable from a diverged palette.
  Honoured by both backends.
- `FS2_FRAME_DUMP_STRIDE=<n>` — the N above; default 60.
- `FS2_NO_PAUSE=1` — keep running without window focus, so a driven test
  session doesn't stall on the auto-pause.
- `FS2_GAME_ROOT=<dir>` — read by the POF oracle only, to find an install. Not
  read by the game; without it the oracle looks for a sibling `gog/` or
  `rundir/`, and skips if neither is there.

## Tests and oracles

```
meson test -C build
```

`math` runs the vecmat regressions. `pof-oracle` diffs retail's own POF loader
against the pinned dump in `tests/oracle/` — retail's loader being the only
authoritative reading of the format — and *skips* rather than fails when it
can't find an install (`$FS2_GAME_ROOT`, above), because a missing install is
not a broken loader.

The rest of `tests/` is hand-run instruments rather than pass/fail tests:
`vp_ls` walks and CRCs the VP archives, `sexp_dump` enumerates the campaign's
SEXP vocabulary against the pinned operator list, `pof_dump` is the oracle's
dumper, and `tmap_test` / `first_pixels` exercise the rasterizer.

## Docs

- [docs/notes.md](docs/notes.md) — the running port log, milestone by
  milestone, back to the 2026-07-16 restart. Start here.
- [docs/survey/](docs/survey/README.md) — the engine survey: a shallow and
  complete subsystem board, symbol-anchored so it resists going stale.
- [docs/sexp-vm.md](docs/sexp-vm.md),
  [docs/pof-model.md](docs/pof-model.md),
  [docs/resolution-scaling.md](docs/resolution-scaling.md) — the deep dives the
  survey links out to.
- [docs/itches.md](docs/itches.md) — redesigns wanted and deliberately not
  scratched yet; the standing queue the groom discipline feeds.
- [docs/fixmine.txt](docs/fixmine.txt) — the fs2open bug harvest: what was
  mined, what was applied, what was rejected and why.
- [docs/fs2-inventory.txt](docs/fs2-inventory.txt) — the subsystem inventory
  the delete/platform/port split and the port order came from.
- [docs/godot-migration-plan.md](docs/godot-migration-plan.md) — the plan the
  `godot` branch executes.
- [notes.txt](notes.txt),
  [docs/pof-oracle-findings.txt](docs/pof-oracle-findings.txt),
  [docs/hud-aabitmap-artefact.txt](docs/hud-aabitmap-artefact.txt) — analysis
  files in Emacs `compilation-mode` format; RET on a `file:line:` anchor jumps
  straight to the code.

## Legal

The source is Volition's, under the terms of its 2002 release: it may not be
sold or commercially exploited. FreeSpace 2 and its assets are the property of
their respective owners. This repository contains no game data.
