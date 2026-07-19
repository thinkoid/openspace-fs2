# sexp_parse — the SEXP reader carve oracle

A standalone build of the FreeSpace 2 SEXP **reader** (parser), extracted to
prove the carve seam documented in [`../../docs/sexp-vm.md`](../../docs/sexp-vm.md):
the text→node-tree reader touches no ship/object/game state, so it can be lifted
out of `code/parse/sexp.cpp` and built and tested on its own.

## Result

```
make check          # parses ../../../missions/*.fs2 (loose retail campaign)
```

Over the full retail campaign corpus (41 loose `.fs2` missions):

- **6,745 SEXPs parsed, 0 parse failures.**
- The binary links **only** `sexp_reader.o + main.o` — **no game object files,
  no parselo, no ship/object/weapon**. `nm -u sexp_parse` shows no game symbols.
  That it links and runs at all is the seam proof.
- Peak node-pool usage in a single mission: **1469 / 2200 (67%)** (`sm2-10`).
  Confirms the retail `MAX_SEXP_NODES = 2200` sizing has real headroom.
- Max tree depth: 5.

## What's here

- `sexp_reader.{h,cpp}` — the reader, lifted **verbatim** from `sexp.cpp`
  (each function cited to its source line). This is the carved unit.
- `sexp_ops.inc` — the 197 operator names, generated from `sexp.cpp`'s
  `Operators[]` table (regenerate: see the `sed` in the git history / sexp-vm.md).
- `main.cpp` — the oracle: seeks each `$Formula:`/`…Cue:` token, parses the
  following SEXP as `missionparse.cpp` would, reports stats; `--dump`
  round-trips each tree back to text for eyeball fidelity.
- `Makefile` — `g++` build, deliberately **outside** the meson game build.

## Prototype adaptations (vs the in-game reader)

All marked `PROTOTYPE:` in the source. None change the produced tree structure:

1. the parselo cursor (`Mp`/`ignore_white_space`/`is_white_space`) is
   implemented locally instead of linked from `parselo.cpp`;
2. `Operators[]` carries only names — the reader needs the table solely to
   classify a token as operator-vs-number;
3. `true`/`false` singleton collapse keys on token text, not `OP_TRUE/OP_FALSE`;
4. `@variable` atoms keep their name (as Fred does) rather than being rewritten
   to a `Sexp_variables[]` index.

## Relationship to the carve stages (sexp-vm.md §6.4)

This tool delivers the **oracle** (stage 3 goal) using a *copied* reader. The
copy is the deliberate price of leaving `sexp.cpp` untouched. **Stage 2** —
still pending go-ahead — de-duplicates it: move the reader into a shared
`parse/sexp_reader.cpp` compiled into *both* the game and this tool, so there is
one source of truth. At that point this directory drops its copy and `#include`s
the shared unit.
