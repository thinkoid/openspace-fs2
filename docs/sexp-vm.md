# The SEXP VM — anatomy and carve plan

A deep dive on `code/parse/sexp.cpp`, the embedded mission-scripting virtual
machine that is the engine's crown jewel. This document delineates the VM into
its three parts — **reader**, **evaluator**, **vocabulary** — and ends with a
concrete plan for separating the reader (parser) into a standalone unit.

Companion to the survey ([survey/03-mission-sexp.md](survey/03-mission-sexp.md)).
Conventions ([survey/README.md](survey/README.md)): anchors are **symbol-first**;
line hints are against `master` (the working tree) and are hints only — the
symbol is the anchor, git manages staleness. The parser spine is
**retail-faithful** (verified against `retail:code/parse/sexp.cpp`); the design
described here is Volition's, so it is stable archaeology.

---

## 1. What it is

SEXP ("s-expression") is a small Lisp-like language embedded in the game.
Mission files (`.fs2`) contain SEXP source as parenthesised prefix expressions;
every mission trigger — goal formulas, event formulas, ship arrival/departure
cues, AI orders, training checks — is one SEXP tree. The VM:

1. **reads** SEXP text into a tree of nodes drawn from one fixed global pool,
2. **evaluates** that tree on demand (usually once per frame) through a
   212-operator dispatch, caching results so settled branches never re-walk,
3. is described by a flat **vocabulary** table (`Operators[]`) plus per-operator
   argument/return type metadata.

The mission loader never evaluates: `missionparse.cpp` calls the reader and
stores the returned **node index** (an `int`) in each goal/event/cue. Evaluation
is driven later by `missiongoals.cpp`.

---

## 2. The data model (shared core)

Everything below is shared by reader and evaluator alike — it is the substrate
both sides stand on, and therefore the part that must travel with the parser in
any carve.

### 2.1 The node

`sexp.h: struct sexp_node` (sexp.h:462):

```c
typedef struct sexp_node {
    char text[TOKEN_LENGTH];  // the atom's text (op name, number-as-string, or string)
    int  type;                // SEXP_ATOM | SEXP_LIST | SEXP_NOT_USED  (+ flag bits)
    int  subtype;             // SEXP_ATOM_{LIST,OPERATOR,NUMBER,STRING}
    int  first;               // CAR: index of child list, or -1
    int  rest;                // CDR: index of next sibling, or -1
    int  value;               // result cache: SEXP_KNOWN_*/NAN/NUM_EVAL/UNKNOWN
} sexp_node;
```

The tree is a classic CAR/CDR cons structure over array indices, not pointers:
`CAR(n) = Sexp_nodes[n].first`, `CDR(n) = Sexp_nodes[n].rest`,
`CADR(n) = first of rest` (sexp.h:323-325). A list node holds its children under
`first`; siblings chain through `rest`. `-1` is nil.

Flag bits are OR'd into `type`: `SEXP_FLAG_PERSISTENT (1<<31)` (survives between
missions), `SEXP_FLAG_VARIABLE (1<<30)` (atom references a `@variable`).

### 2.2 The node pool

`sexp.cpp: sexp_node Sexp_nodes[MAX_SEXP_NODES]` (sexp.cpp:359). `MAX_SEXP_NODES`
is **2200** (sexp.h:22 — the comment `// Reduced from 2000 to 1200 by MK on
4/1/98` records the 1998 tuning history; it was bumped, not reduced, over the
campaign's life). One global pool serves the whole process.

- **Allocation** — `alloc_sexp(text, type, subtype, first, rest)` (sexp.cpp:399)
  → `find_free_sexp()` (sexp.cpp:451), a **linear scan** for the first
  `SEXP_NOT_USED` slot. O(n) per node; fine at 2200.
- **Free** — `free_sexp(num)` (sexp.cpp:529) frees a node and its whole subtree;
  `free_one_sexp` frees a single node leaving links intact. Freeing just flips
  `type = SEXP_NOT_USED`.
- **Reset** — `init_sexp()` (sexp.cpp:377) marks every non-persistent node
  `SEXP_NOT_USED` at mission start, then re-creates the two locked singletons.
- **Persistence** — `sexp_mark_persistent(n)` / `sexp_unmark_persistent(n)`
  (sexp.cpp:475/495) recursively set/clear `SEXP_FLAG_PERSISTENT` so a tree
  (campaign-persistent variables, red-alert carryover) survives `init_sexp()`.

### 2.3 The locked singletons

`Locked_sexp_true`, `Locked_sexp_false` (sexp.cpp:343) are two node indices
created once in `init_sexp()`. `alloc_sexp` funnels **every** `true`/`false`
operator atom to these two indices (sexp.cpp:404-408): all "true" atoms in the
whole process are physically the same node. `init_sexp` then patches their
`type` back from `SEXP_LIST` to `SEXP_ATOM` (sexp.cpp:392,395 — the
`// fix bypassing value` comment) because `alloc_sexp` created them as lists.

### 2.4 Variables

`sexp_variable Sexp_variables[MAX_SEXP_VARIABLES]` (sexp.cpp:360) is the `@var`
symbol table: `{int type; char text[]; char variable_name[];}` (sexp.h:471).
Tokens prefixed with `SEXP_VARIABLE_CHAR` (`@`) are interned by
`get_sexp_text_for_variable()` — in-game the node text becomes the numeric
`Sexp_variables[]` index; under Fred it keeps the name. The node is flagged
`SEXP_FLAG_VARIABLE`.

### 2.5 The vocabulary table

`sexp_oper Operators[]` (sexp.cpp:60) is the flat operator table; each row is
`{char *text; int value; int min, max;}` (sexp.h:456) — name, an `OP_*`
constant, and min/max argument counts. `Num_operators = sizeof/sizeof`
(sexp.cpp:344). The `OP_*` constants (sexp.h) pack a category in their high bits
(`OP_CATEGORY_*` — arithmetic/logical/objective/time/status/change/conditional/
ai/training/goal-event/…) plus flags (`OP_NONCAMPAIGN_FLAG`, `OP_CAMPAIGN_ONLY_FLAG`).

Two lookups walk this table by case-insensitive name:
- `identify_operator(token)` (sexp.cpp:754) → **table index** or -1.
- `find_operator(token)` (sexp.cpp:767) → **`OP_*` value** or 0.

Per-operator type metadata is separate: `query_operator_argument_type()` returns
the `OPF_*` expected type of each argument (OPF_SHIP, OPF_WING, OPF_SUBSYSTEM,
OPF_IFF, OPF_AI_GOAL, …); `query_operator_return_type()` returns the `OPR_*`
result type; `sexp_query_type_match()` reconciles them for the validator.

---

## 3. The reader (parser) — text → node tree

The part flagged for extraction. **It touches no ship/object/game state.**

### 3.1 Entry — `get_sexp_main()` (sexp.cpp:7651)

Reads exactly one top-level expression from the parselo cursor `Mp`:

```c
ignore_white_space();
// ... "( )" empty-list special-case nudges savep
Assert(*Mp == '(');
Mp++;
start_node = get_sexp(token);
if ( Fred_running || (start_node == -1) )   // <-- the ONLY game coupling
    return start_node;
// in-game: re-identify the root operator, Error if unknown
op = identify_operator(CTEXT(start_node));
if (op == -1) Error(LOCATION, "Can't find operator %s ...", CTEXT(start_node));
return start_node;
```

The in-game path does **no** semantic validation — it only confirms the root
names a known operator. The whole validating path (`check_sexp_syntax`, §3.4)
runs only under Fred. This is the linchpin of the carve: the runtime reader's
sole game dependency is the `Fred_running` branch at sexp.cpp:7667, and it
guards work the standalone parser simply omits.

### 3.2 The recursive reader — `get_sexp(char *token)` (sexp.cpp:1613)

The classic recursive-descent s-expression reader. Loops until `)`, and per
token dispatches on the first character:

- **`(`** (sexp.cpp:1630) — recurse; wrap the child list in a node:
  `alloc_sexp("", SEXP_LIST, SEXP_ATOM_LIST, get_sexp(token), -1)`.
- **`"`** (sexp.cpp:1635) — a quoted string. `strcspn` finds the close quote;
  `@`-prefixed strings are interned as variables (`SEXP_FLAG_VARIABLE`,
  `SEXP_ATOM_STRING`), otherwise a plain `SEXP_ATOM_STRING`.
- **else** (sexp.cpp:1664) — an operator or a number. Scan a token delimited by
  whitespace/`)`, honour a leading `@` (variable). Then
  `identify_operator(token)`: **found → `SEXP_ATOM_OPERATOR`; not found →
  `SEXP_ATOM_NUMBER`.** Numbers are **not** parsed to int here — the text is
  kept and `atoi`'d lazily during evaluation.

Sibling links are stitched as it goes (sexp.cpp:1697): the first node this call
allocates becomes `start` (returned as the list head / `first`); each subsequent
node is hung off `Sexp_nodes[last].rest`. Returns the head index; `Mp` is
advanced past the closing `)` (sexp.cpp:1709).

### 3.3 The reader's external surface

Everything `get_sexp`/`get_sexp_main` call, and where it lives:

| Dependency | Source | Notes |
|---|---|---|
| `Mp`, `ignore_white_space()`, `is_white_space()`, `EOF_CHAR`, `Mission_text` | **parselo** (`parselo.h`/`.cpp`) | the lexer/cursor — a thin, game-free dependency |
| `alloc_sexp`, `find_free_sexp`, `free_sexp` | shared core (§2.2) | the node pool |
| `identify_operator`, `find_operator` | shared core (§2.5) | the `Operators[]` table |
| `get_sexp_text_for_variable`, `Sexp_variables[]` | shared core (§2.4) | `@var` interning |
| `TOKEN_LENGTH`, `SEXP_*` consts, `SEXP_VARIABLE_CHAR`, `CTEXT` | `sexp.h` | types/macros |
| `Fred_running` | game global | **`get_sexp_main` only**, omittable (§3.1) |
| `Assert`, `Error`, `nprintf` | globalincs | diagnostics |

There is **no** `ship_*`/`wing_*`/`Objects`/`Ships` reference anywhere in the
reader path. That is the whole point.

### 3.4 Validation (Fred-only) — `check_sexp_syntax()` (sexp.cpp:795)

Semantic checking — argument counts (`query_sexp_args_count`, sexp.cpp:780), OPF
type matching, and name lookups (`ship_name_lookup`, `wing_name_lookup`,
`waypoint_lookup`, team names, AI-goal validity) — lives here and returns the
negative `SEXP_CHECK_*` codes (sexp.h:409-443). **This is where every game
coupling of the "parse" phase actually sits, and it is gated on `Fred_running`.**
A runtime standalone parser does not call it; a Fred-grade one would, and would
bring the game-lookup dependencies with it. So there are really two parser
grades: the *reader* (game-free, this document's target) and the *validating
editor parser* (game-coupled, out of scope for the lift).

---

## 4. The evaluator — tree → truth/number (stays game-side)

`eval_sexp(int cur_node)` is a tree-walker over the same pool. Not part of the
lift; summarised for completeness (see survey/03 for the fuller map).

- **Short-circuit cache.** If `Sexp_nodes[cur_node].value` is already
  `SEXP_KNOWN_TRUE/FALSE`, return immediately — a settled event formula never
  re-walks. This is what makes per-frame goal evaluation O(active), not O(all):
  `missiongoals.cpp` retires an event whose node cached `SEXP_KNOWN_FALSE`.
- **Dispatch.** A list node recurses into `CAR` and propagates the child's
  `value`. An operator atom takes `CDR` as its argument list, resolves
  `find_operator(CTEXT)` to an `OP_*`, and a large `switch` calls the matching
  `sexp_*` handler. Arguments are evaluated **lazily, left-to-right by each
  handler** — there is no pre-built argv.
- **Result write-back.** After the switch, terminal results
  (`SEXP_KNOWN_TRUE/FALSE`, `SEXP_NAN_FOREVER`) are written into
  `Sexp_nodes[cur_node].value` to pin the branch; `SEXP_NAN`/`SEXP_CANT_EVAL`
  (ship not yet arrived) leave it re-evaluable and clear `Sexp_useful_number`.
- **`rand` pinning.** `rand_sexp()` is the archetypal memoiser: first eval rolls
  the number, sets `value = SEXP_NUM_EVAL`, and **overwrites the node's own
  `text`** with the rolled value via `sprintf`; every later eval returns
  `atoi(CTEXT(n))`. The roll is frozen for the mission's life — **retail campaign
  missions depend on this** (a re-rolling `rand` would desync scripted branches;
  SCP added a separate rand-multiple op rather than change this).
- **Non-short-circuit booleans.** `sexp_and`/`sexp_or`/`sexp_and_in_sequence`
  deliberately evaluate *all* clauses (not C short-circuit) so every sub-event
  gets its mission-log/directive marking, then fold children into `SEXP_KNOWN_*`.

The evaluator's mutation of shared node state (`.value`, and `rand`'s `.text`)
is the one thing a carved parser must respect: **the parser writes those fields
at creation; only the evaluator mutates them thereafter.**

---

## 5. Rare knowledge / gotchas

- `true`/`false` are one shared node each for the whole process (§2.3).
- Numbers live as **strings** until eval; `rand` rewrites its node text to
  memoise — a node's text can differ from the mission file after play (§4).
- `SEXP_FLAG_PERSISTENT` nodes survive `init_sexp()`; the free-scan and reset
  both skip them (§2.2).
- `Sexp_useful_number` (sexp.h:487) is an **out-of-band signal**, not a value —
  handlers set it to tell `mission_process_event` whether a directive is
  "current yet". A side channel, threaded by global.
- The pool is only 2200 nodes; overflow is a real historical failure mode
  (the tuning comment at sexp.h:22).
- `DIRECTIVE_WING_ZERO (-999)` (sexp.h:479) is a directives-display hack to mark
  a directive satisfied between wing waves.

---

## 6. Carve plan — separating the reader

Goal: a standalone **text → node-tree** parser with no game dependencies,
suitable to build and test on its own (and to oracle against the `missions/`
corpus). Staged so each step is independently verifiable and reversible.

### 6.1 What the parser *is* (the movable set)

- **Functions:** `get_sexp_main`, `get_sexp`, `alloc_sexp`, `find_free_sexp`,
  `free_sexp`/`free_one_sexp`, `init_sexp` (pool reset), `identify_operator`,
  `find_operator`, the variable interner `get_sexp_text_for_variable`, and the
  persistence helpers.
- **Data:** `Sexp_nodes[]`, `Operators[]` + `Num_operators`, `Sexp_variables[]`,
  the locked singletons.
- **Types/macros:** `sexp_node`, `sexp_oper`, `sexp_variable`, `CAR/CDR/CADR`,
  the `SEXP_*` / `OP_*` / `OPF_*` / `OPR_*` constants.

### 6.2 What it depends on (and how to sever)

- **parselo cursor** (`Mp`, `ignore_white_space`, `is_white_space`, `EOF_CHAR`,
  `Mission_text`). Two options: keep parselo as a (game-free) dependency, or
  introduce a tiny cursor abstraction (`const char *` + position) so the parser
  reads from any buffer. The latter makes the standalone test harness trivial.
- **`Fred_running`** (sexp.cpp:7667). Sever by splitting `get_sexp_main` into a
  pure `sexp_parse(cursor) → root_index` (reader only) and a thin game-side
  wrapper that adds the in-game root-operator `Error` check. The standalone
  parser exposes only the pure form.
- **Diagnostics** (`Assert`/`Error`/`nprintf`). Keep — they are globalincs, not
  game state; or map to a small parser-local error callback.
- **The `Operators[]` values.** The `OP_*` constants are shared with the
  evaluator. The parser only needs the **table** (name→row) to classify a token
  as operator-vs-number; it does not need operator *semantics*. So `Operators[]`
  moves into the shared core header, and both sides include it.

### 6.3 Proposed file layout (retail-idiomatic, minimal churn)

Keep the existing style — lowercase filenames, `char*`, C-ish idiom (a faithful
lift, not a modern rewrite; per house style, don't put lipstick on a pig):

- `parse/sexp_node.h` — the data model: `sexp_node`/`sexp_oper`/`sexp_variable`,
  the pool + `Operators[]` externs, `CAR/CDR/CADR`, the `SEXP_*`/`OP_*` consts
  (split out of `sexp.h`, which keeps `#include`ing it for source compatibility).
- `parse/sexp_reader.cpp` — the reader + pool management + operator lookup +
  variable interning (the §6.1 functions). Compiles with no game headers.
- `parse/sexp.cpp` — unchanged except: loses the moved functions, keeps
  `eval_sexp` and all `sexp_*` handlers and `check_sexp_syntax`.

Public reader API (the narrow entry point):

```c
int  sexp_parse(const char *text);   // buffer -> root node index, or -1
// node accessors already exist as CAR/CDR/CADR + Sexp_nodes[].{type,subtype,text}
```

### 6.4 Stages

0. **Delineate** — this document. *(done)*
1. **Non-invasive API** — introduce `sexp_node.h` and a `sexp_parse` wrapper
   over the existing functions, no code moved. Build stays green; proves the
   surface compiles standalone-ish. *Reversible.*
2. **Physical split** — move the §6.1 functions into `sexp_reader.cpp`, add it to
   the meson target, verify the full game still builds and a mission parses
   identically (diff node trees before/after).
3. **Standalone target + oracle** — a tiny `sexp_parse` test binary that reads
   the `missions/` corpus SEXP blocks and dumps/round-trips node trees, run as a
   parse oracle (the corpus is usable before cfile — loose `.fs2` files). This is
   where the parser earns independent test coverage the game never gave it.

Stages 1-3 are a **decision point**: they mutate living code and the build.
This document (stage 0) stands on its own regardless.

---
*Status: delineation complete (stage 0). Stages 1-3 pending go-ahead.*
