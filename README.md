# openspace-fs2

A retail-forward Linux port of the 1998 Volition FreeSpace 2 source — the 2002
CVS drop, ported as-is: non-networked, non-multiplayer, single-player, the
retail campaign with the retail SEXP vocabulary and nothing more.

This is <u>**NOT**</u> a port or fork of fs2open, and deliberately so. The
port starts from the pristine Volition import and moves forward one subsystem
at a time, keeping
retail's own code — including its complete 8bpp software rasterizer, which now
runs at 1024x768 on an SDL2 surface, something retail itself never shipped.
An OpenGL backend (revived from Volition's own abandoned `gropengl.cpp`
skeleton) sits beside it. Playable: the campaign boots, briefs, flies, and
talks, on OpenAL sound.

Mining the knowledge in the 1998 engine is a first-class goal, not just a
means: the asm-to-C conversions are brute-force-verified against emulation,
retail bugs are catalogued rather than silently fixed, and behavior decisions
are recorded (shipped behavior wins over authorial intent). The port's running
log lives in [docs/notes.md](docs/notes.md); analysis notes with `file:line`
anchors sit beside it (Emacs `compilation-mode` files).

## Two projects, one lineage

The Linux port is complete, and it is not the end of the line — it is the end
of *a* line. As of **2026-07-30 this branch is mothballed** at its
survey-complete milestone: the campaign plays, the codebase survey is closed
(warnings 4,756 → 15, every survivor a catalogued decision in
[docs/notes.md](docs/notes.md)), and further chipping would mean modernizing
subsystems — the renderer above all — that the next stage replaces outright.

The live project is the **Godot migration on the `godot` branch**, revived
the same day along its own mothball note's exit route: GDExtension-first.
The simulation stays this port's C++, compiled as a native module; Godot
owns presentation. That branch's README carries the map and the plan.

Mothballed is not abandoned: fixes still land here on their merits, and this
branch remains the migration's authoritative retail reference — the code the
GDExtension compiles and the behavior its oracles diff against. The lineage
runs retail 2002 → Linux port 2026 → Godot host, in one continuous history.

## Branches

- `retail` — `663b3471b`, the 2002 Volition warpcore CVS import. Pristine,
  immutable.
- `master` — the retail-forward port; mothballed 2026-07-30 at its
  survey-complete milestone, live as the migration's reference implementation.
- `godot` — the working branch: the Godot migration, revived 2026-07-30,
  GDExtension-first; see its own README.
- `fs2open` — fs2open `release_26_0_0`, reference implementation and fix-mine
  source. Read, cherry-pick ideas, never build on.
- `reference/*` — abandoned 2018/2023 fork lines; style reference only.

## Building

Meson + ninja, C++17, Linux only:

```
meson setup build
ninja -C build
```

Dependencies: SDL2, OpenAL, zlib. The build deploys the game binary to a
sibling `../rundir` if one exists.

## Game data

You need the retail FreeSpace 2 data (the `.vp` archives) from your own copy
of the game — the GOG release works. No game data is included here or ever
will be. Point a run directory at the archives and run `fs2` from it.

## Legal

The source is Volition's, under the terms of its 2002 release: it may not be
sold or commercially exploited. FreeSpace 2 and its assets are the property of
their respective owners. This repository contains no game data.
