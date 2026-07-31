# Itch list

Redesigns wanted but deliberately not scratched yet — the standing queue the
groom discipline feeds ("write it down instead of scratching it"). Each entry
carries enough design to start cold. Chronological log stays in notes.md;
this file is only the itches.

## docs: the test book — every gate's why and how (2026-07-30)

Requested by the user at slice 1: a detailed markdown treatment of ALL the
meson gates (20 at time of writing) — per gate: what question it answers
(the why), the mechanism (oracle, driver, comparison discipline, tolerances
and their justification), what proved it bites, and what it deliberately
does not cover. The material is scattered today across tests/meson.build
comments, the gate scripts' headers, and the migration plan's slice-facts
blocks; the book gathers it in one place. Natural home: docs/tests.md.
Write it when the boundary slices settle (each slice adds/inverts gates —
a book written mid-inversion goes stale weekly).

## linklist: thin the sentinel — `links_t<T>` retrofit (2026-07-28) — SCRATCHED same day, REVERTED 2026-07-29

**Reverted (2026-07-29).** Second thoughts won: the retrofit was churn on a
battle-tested C89 core — our own "don't put lipstick on a pig" clause,
applied to our own surgery. The retail macro header and the fat sentinel
are back wholesale. What survived, because it was knowledge rather than
uniform:

- the aicode.cc shockwave-avoidance guard (the audit's bug): an explicit
  `END_OF_LIST(&obj_used_list)` token check instead of leaning on the
  sentinel's zeroed payload;
- the inert respellings that document retail sloppiness: seven
  `GET_NEXT(&head)` → `GET_FIRST`, four `GET_LAST(elem)` → `GET_PREV`, and
  the four weapons.cc assignments that mint the "not homing on anything"
  token, now spelled `END_OF_LIST(&obj_used_list)`.

All respellings expand macro-identically; verified by clean rebuild
(warning set back to the pre-surgery baseline), math + POF oracle green,
headless boot renders. The entry below stays as the design record. The
escalation path (Boost.Intrusive, all or nothing) is unchanged and is now
the only forward path — no more half-measures on this file.

**Outcome (2026-07-28, 37 files).** Landed as designed, with three deltas
found during surgery:

- The self-looping `list_t` constructor was dropped: retail memsets nodes
  *and* the structs embedding heads (`Players`, `ship`), then `list_init`s
  every head — a non-trivial type fights that idiom (`-Wclass-memaccess`
  ×7). Both types are deliberately trivial; `list_init`-before-use stays
  the contract, exactly retail's. The layout static_asserts live inside
  `sentinel()`, so every node type is checked at instantiation.
- Retail sloppinesses the macros tolerated, now caught by types: seven
  `GET_NEXT(&head)` calls that meant `GET_FIRST` (aicode, hudtarget,
  ship); `&obj_used_list` stored as a *value* meaning "no homing object"
  (weapons/hudtarget, 11 assignments + ~20 comparisons — now spelled
  `END_OF_LIST(&obj_used_list)`); and `advance_subsys`'s `GET_LAST` on an
  element (= `GET_PREV`, four sites).
- muzzleflash.cc's entire pooled-list implementation turned out to be
  commented out by Volition — dead code left verbatim.

Measured: `sizeof(ship)` 1744 → 1352 (`Ships[]` −78 KB); every head 16
bytes. Build warning-set identical to pre-surgery; math + POF oracle green.
Campaign playtest pending.

**Post-landing audit (same day).** Auditing the header's "only links are
ever touched through a sentinel pointer" claim found retail violating it
once: aicode.cc's shockwave-avoidance read `homing_object->type` guarded
against NULL but not against the stored "not homing on anything" token —
for 28 years the fat sentinel's dead payload absorbed that read
(zero-initialized `type` = benign skip); the thin sentinel turned it into
an out-of-bounds read. Guarded now; the fat sentinel was load-bearing at
exactly one address in the engine. Also recorded in the header comment:
the pun is formally UB under the C++17 object model (no `T` lives at the
head's address; `std::launder` cannot help) — it is ABI-defined and
static_assert-pinned, the `container_of` category, not a conformance
claim.

**Escalation path, if call sites ever modernize wholesale:**
`boost::intrusive::list` (base hooks). Its iterator `end()` is not a
storable `T*`, so the sentinel-as-value idiom becomes inexpressible —
which would have flushed the aicode bug at compile time — and it is fully
conforming (it does Variant B). Not adopted now because its API costs the
~620-site rewrite this replay was designed to avoid, for a container with
no remaining maintenance of its own. Half-adopting (our API over their
engine) would combine the costs; go all the way or not at all.

### The entry as written (design record)

**The itch.** `globalincs/linklist.hh` is a circular doubly-linked list with
a *fat* sentinel: every list head is a full node struct whose payload is
meaningless — 416 dead bytes per `object` head, 392 per `ship_subsys`. The
same-type sentinel is what keeps the C89 macros four dumb assignments, but it
plants state that exists and denotes nothing, and it hands the sentinel a
citizen's passport: walk past `END_OF_LIST` and the payload reads as a valid
node, silently. Measured (2026-07-28, gdb on the debug build): ~10 KB of dead
payload across the global heads, plus the one concentration — `ship.subsys_list`
embeds a full 408-byte `ship_subsys` head in each of MAX_SHIPS=200 slots:
~78 KB, 23% of `Ships[]`. The bytes are noise against the static pools; the
meaningless state and the type hole are the offense.

**The design.** Sentinel shrinks to the links and nothing else; nodes inherit
their links instead of hand-declaring them:

```cpp
template <class T>
struct list_links_t {
    T *next = nullptr;
    T *prev = nullptr;
};

template <class T>
struct list_t : list_links_t<T> {
    // The one confined pun: the sentinel wears a T* uniform but owns only
    // links. Layout makes it safe -- links_t is at offset 0 of both the
    // head and every node (first base, no virtuals) -- and only next/prev
    // are ever touched through it. Formally impure, stated exactly once.
    T *sentinel() { return reinterpret_cast<T *>(this); }

    list_t() { this->next = this->prev = sentinel(); }
};

struct object : list_links_t<object> { ... };   // drops its manual next/prev

list_t<object> obj_free_list;                   // 16 honest bytes
```

Naming (settled 2026-07-28): `list_t` / `list_links_t`. The head is the
owner's handle, so it takes the role name — and every head variable in the
tree already says `*_list`, as do the `list_*` ops. Rejected: `list_head_t`
(implementation's view, not the owner's), `list_node_t` (overclaims — the
node is T itself, the base is only plumbing), `intrusive_list_t` (a
qualifier with no in-tree rival to distinguish from; the base in the struct
declaration already advertises intrusiveness where it's structural news),
`intrusive_ring_t` (honest topology, but the ring is the *how*, not the
*what*, and it would contradict the `*_list` variables and `list_*` ops at
every use). Since heads self-loop at construction, surviving explicit
`list_init` calls become genuine re-inits (pool resets) — informative
instead of ceremonial.

The macro set becomes function templates with the same names and call shape
(`list_init`, `list_append`, `list_insert`, `list_insert_before`,
`list_remove`, `list_merge` — the last has zero live callers, kill it);
`GET_FIRST`/`GET_NEXT`/`END_OF_LIST` become inline functions,
`END_OF_LIST(h)` = `h->sentinel()`. `list_remove` finally drops its dead
head parameter (it never read it — the missing `&` at object.cc:595
compiles today only because the macro discards the argument).

**Pin the layout premise** or the pun rots:

```cpp
static_assert(offsetof(object, next) == 0);
static_assert(sizeof(list_t<object>) == 2 * sizeof(void *));
```

**What it buys.** ~90 KB back (mostly `Ships[]`); the meaningless-state lie
extinguished everywhere but one commented cast; single-evaluation and real
type checking at all ~620 call sites; and the walk-past-the-end bug class
turns from *silently reads a valid dead struct* into *out-of-bounds access a
sanitizer flags* — the fat sentinel made that bug invisible by construction.

**Variant B, if full purity is wanted:** kernel-shaped — links typed
`list_links_t<T>*`, every access funneled through accessors that downcast
(valid: real nodes ARE derived), comparisons in links space, no pun at all.
Costs converting every raw `->next`/`->prev` touch on list-managed structs
to accessor calls; census that count before choosing it. Variant A keeps raw
field access working and confines the impurity to `sentinel()`.

**Cost census (variant A).** ~20 node structs swap manual `next`/`prev` for
the base; ~30 head declarations change type; `list_remove` call sites lose
the dead first argument (~40); the ~620 traversal sites are untouched.

**Verification.** This is a redesign, not a groom — `sizeof(ship)` changes,
so no disassembly-diff inertness claim. Gate on: build + meson tests + POF
oracle byte-identical + a campaign-mission playtest; the two static_asserts
pin the layout premise permanently.

**Provenance.** The 2026-07-28 linklist review: sentinel-ring design
affirmed correct (std::list itself is a thin-sentinel ring — libstdc++
`_List_node_base` header, empty = self-loop); the fat head identified as
the sole genuine wart, plus its cousin, the sentinel/node type confusion.

## tools: DDS decode — texture the MediaVP models (2026-07-31)

The MediaVPs (assets/ = mv_assets + _s + _t, 2020 vintage) are the
art-revamp lane. pof2glb eats their POFs clean (MediaVP Fighter01 =
5503 tris vs retail's ~500; sample bake in build/glb-mv/), but every
MediaVP map is DDS (BC1/BC3 block compression) and both texture
consumers speak only PCX — pof2glb's stb transcode and pofview's
pcx.cc. One BC1/BC3 decoder textures both. The formats are small
(4x4 blocks, two endpoint colors + 2-bit indices; BC3 adds an alpha
block), so a hand-rolled decoder in the PCX/SHA-256 tradition is the
likely shape — but it is a dependency-vs-hand-roll question to ASK
before starting. Separate gap: the Ulysses hull map (ulsss) is not in
these three VPs at all — MV_Root.vp must be fetched.

## tools: pof2glb — bake flat-poly colors into materials (2026-07-31)

POF models carry textureless FLAT polys with an RGB color ("poly flat
R G B" in the model dumps; the corpus gate has seen them since the
bomber05 sliver). pof2glb emits them with no material color, so they
render white in Godot — visible on debris chunks' break faces (the
retail-art lane made hull debris wear the ship's own model, and the
white caps stand out). The bake: a per-color material with albedo =
the flat RGB. The GLB checker should grow the color check; the tres
side is untouched (flat polys carry no UVs).

## tests: the lesson-replay gate — a scripted pilot (2026-07-31)

Training-1's event chain gates on PLAYER behavior the hands-off lesson
gate cannot produce: keys pressed (a, \, Tab...), speed bands held
(set-training-context-speed 73..77 + `speed 2`), distances closed and
reopened (< 126, > 199). The A-key and speed-context stalls were both
found by the user flying, not by a gate. The itch: a scripted pilot —
sim_dump grows a keyframed control track (throttle/keys per frame
range) or a small replay file, and the gate asserts the full Training-1
directive sequence fires in order. game_do_training_checks (relocated
to missiontraining.cc 2026-07-31) is the machinery under test.
