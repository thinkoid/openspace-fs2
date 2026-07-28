# Itch list

Redesigns wanted but deliberately not scratched yet — the standing queue the
groom discipline feeds ("write it down instead of scratching it"). Each entry
carries enough design to start cold. Chronological log stays in notes.md;
this file is only the itches.

## linklist: thin the sentinel — `links_t<T>` retrofit (2026-07-28)

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
