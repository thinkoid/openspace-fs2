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
of *a* line. As of 2026-07-22 this repository hosts two related projects:

- **The retail Linux port** — the completed work described above. It stays
  alive and takes work in its own direction: playtesting, wrinkle fixes,
  whatever serves the authentic retail game.
- **The Godot migration** — a new project that starts from the port: the
  retail simulation, kept authoritative as a C++ library, hosted behind a
  modern engine boundary. The plan lives in
  [docs/godot-migration-plan.md](docs/godot-migration-plan.md).

Cousins, not siblings: neither line feeds the other by default, and neither
outranks the other. The lineage runs retail 2002 → Linux port 2026 → Godot
host, in one continuous history.

## Branches

- `retail` — `663b3471b`, the 2002 Volition warpcore CVS import. Pristine,
  immutable.
- `master` — the working branch; the port happens here.
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
