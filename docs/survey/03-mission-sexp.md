# Cluster 03 — Mission & SEXP VM

Subsystems: **parse** (the **SEXP VM** — the crown jewel), **mission**,
**gamesequence**. The mission scripting language, the mission-file loader, and
the top-level state machine. See [README](README.md) for conventions.

> **Phase-2 target.** `parse/sexp.cpp` is flagged for parser extraction. The
> seam is identified below under *SEAM*: the reader path (`get_sexp_main` /
> `get_sexp` / `alloc_sexp` / `identify_operator`) touches **no** ship/object/
> game state — all game coupling lives in `check_sexp_syntax()`, gated on
> `Fred_running`, which the runtime parser never calls.

---

### parse/sexp.cpp — Mission & SEXP VM (crown jewel) (~9,290 SLOC master)
- **Purpose:** A complete embedded Lisp-ish scripting VM: parses `.fs2` SEXP text into a shared node-pool tree, then evaluates a 212-operator dispatch to drive mission goals, events, messages, training and AI orders.
- **Entry points:**
  - Parser: `parse/sexp.cpp: get_sexp_main()` (top-level, one expression) → `get_sexp()` (recursive atom/list reader). Interning helpers `identify_operator()`, `find_operator()`, `alloc_sexp()`, `find_free_sexp()`. Variable list reader `stuff_sexp_variable_list()`.
  - Evaluator: `eval_sexp()` (the giant switch, ~6808–7643). Arithmetic helpers `add_sexps/sub_sexps/mul_sexps/div_sexps/mod_sexps/rand_sexp` (~1980–2095), booleans `sexp_or/sexp_and/sexp_and_in_sequence/sexp_not` (~2100+), plus ~200 `sexp_*` operator implementations.
  - Vocabulary/metadata: `Operators[]` table (line 60), `query_operator_return_type()` (7694), `query_operator_argument_type()`, `check_sexp_syntax()` (795), `sexp_query_type_match()`.
  - Text emit (Fred/round-trip): `build_sexp_string()` (1838), `build_sexp_text_string()` (1784). Resolver `CTEXT()` (9003).
- **Core state (symbol-anchored):**
  - `sexp_node Sexp_nodes[MAX_SEXP_NODES]` (line 359; `MAX_SEXP_NODES`=2200) — the single global node pool. Struct fields `text[32], type, subtype, first, rest, value` (sexp.h:462). `first`=CAR link, `rest`=CDR link; macros `CAR/CDR/CADR` (sexp.h:323). Node `type` = SEXP_LIST/SEXP_ATOM/SEXP_NOT_USED with flag bits `SEXP_FLAG_PERSISTENT (1<<31)`, `SEXP_FLAG_VARIABLE (1<<30)`; `subtype` = SEXP_ATOM_{LIST,OPERATOR,NUMBER,STRING}.
  - `sexp_oper Operators[]` (line 60) with `Num_operators = sizeof/sizeof` (344). Each row `{text, value, min, max}` — value is an `OP_*` constant (sexp.h:101–318) encoding category in high nibble (`OP_CATAGORY_*`) plus `OP_NONCAMPAIGN_FLAG` etc.
  - `sexp_variable Sexp_variables[MAX_SEXP_VARIABLES]` (100 slots) — the `@var` symbol table (name/text/type).
  - `Locked_sexp_true`, `Locked_sexp_false` — two singleton interned nodes created in `init_sexp()`; `alloc_sexp` funnels every "true"/"false" operator atom to these two indices.
  - `node.value` field is the short-circuit/result cache: `SEXP_KNOWN_TRUE/FALSE (-2/-1)`, `SEXP_NAN/-_FOREVER`, `SEXP_CANT_EVAL`, `SEXP_NUM_EVAL (-7)`, `SEXP_UNKNOWN (-3)` (sexp.h:398–406).
  - Side-channel globals: `Sexp_useful_number` (set 0 by eval to signal "not current yet"), `Directive_count`, `Event_index`, `Players_target`.

- **Mechanism —**

  **(a) THE PARSER (text → node tree).** `parse_main`→`parse_mission` fills `Mission_text[]`; the reader walks the raw buffer via the parselo cursor `char *Mp` (parselo.h:20). `get_sexp_main()` asserts `*Mp=='('`, consumes it, calls `get_sexp()`. `get_sexp()` is the classic recursive reader: loop until `')'`; on `'('` recurse and wrap in a `SEXP_LIST/SEXP_ATOM_LIST` node whose `first` = child list; on `'"'` read a quoted string (`strcspn` to closing quote) into a `SEXP_ATOM_STRING` node; otherwise scan a whitespace/`)`-delimited token then `identify_operator(token)` — if found it becomes `SEXP_ATOM_OPERATOR`, else it is stored as `SEXP_ATOM_NUMBER` (numbers are NOT parsed to int at read time — they stay as text and are `atoi`'d lazily during eval). Sibling links are stitched via `Sexp_nodes[last].rest = node`. Every node comes from `alloc_sexp()`→`find_free_sexp()` (linear scan of the pool for `SEXP_NOT_USED`). **Interning:** operators are interned only by string match against `Operators[]`; true/false collapse to the two locked singletons; `@`-prefixed tokens/strings are variables — `get_sexp_text_for_variable()` rewrites the node text to the numeric `Sexp_variables[]` index (in-game) or keeps the name (Fred), flagged `SEXP_FLAG_VARIABLE`. **Validation/error paths:** the reader itself only `Assert`s (unterminated string, token too long, pool exhausted, EOF). Semantic validation is `check_sexp_syntax()` — arg counts (`query_sexp_args_count`), OPF type matching, and (Fred-only) name lookups; it returns the negative `SEXP_CHECK_*` codes (sexp.h:409–443) surfaced by `sexp_error_message()`. Crucially, in-game `get_sexp_main()` **skips** `check_sexp_syntax` entirely — after building the tree it only does one `identify_operator` on the root and `Error`s if unknown; the whole validation-with-game-lookups block runs only when `Fred_running`.

  **(b) THE EVALUATOR.** `eval_sexp(cur_node)` is a tree-walker over the same pool. Order: (1) short-circuit — if `node.value == SEXP_KNOWN_TRUE/FALSE` return immediately (this is the campaign cache: once an event's formula is known it is never re-walked). (2) If `first != -1` the node is a list wrapper → recurse into `CAR`, propagate the child's `value` up, return. (3) Otherwise it's an operator atom: `node = CDR(cur_node)` gives the argument list, `op_num = find_operator(CTEXT(cur_node))`, then a ~800-line `switch(op_num)` dispatches to a `sexp_*` handler, each of which pulls its args by walking `Sexp_nodes[n].rest` and calling `eval_sexp`/`num_eval`/`atoi(CTEXT())` on children (arguments are evaluated left-to-right, lazily, by each handler — there is no pre-evaluated argv). **Result caching / postprocess (7599–7641):** after the switch, `SEXP_KNOWN_TRUE/FALSE` and `SEXP_NAN_FOREVER` are written back into `Sexp_nodes[cur_node].value` so the branch is permanently pinned; `SEXP_NAN` (ship not yet arrived) and `SEXP_CANT_EVAL` set the node false-for-now but leave it re-evaluable and clear `Sexp_useful_number`; plain truthy/falsey get cached as transient `SEXP_TRUE/FALSE`. **rand pinning:** `rand_sexp()` (2061) is the archetype — first eval computes `rand_internal(low,high)`, then (unless `multiple`) sets `node.value = SEXP_NUM_EVAL` and **overwrites the node's own text with the rolled number via `sprintf(Sexp_nodes[n].text,...)`**; every later eval sees `SEXP_NUM_EVAL` and returns `atoi(CTEXT(n))` — the roll is frozen for the mission's life. Campaign missions that branch on a `rand` depend on this so re-evaluation each frame doesn't reroll. The boolean combinators (`sexp_and/or/and_in_sequence`) deliberately keep evaluating all clauses (not C short-circuit) so mission-log entries get marked essential, then fold child `value`s into `SEXP_KNOWN_*`.

  **(c) THE VOCABULARY.** `Operators[]` (line 60) is the flat table of 212 rows `{text, OP_*, min, max}`. `OP_*` constants (sexp.h:101–318) pack a category (`OP_CATAGORY_ARITHMETIC/LOGICAL/OBJECTIVE/TIME/STATUS/CHANGE/CONDITIONAL/AI/TRAINING/GOAL_EVENT/UNLISTED/DEBUG`) plus flags `OP_NONCAMPAIGN_FLAG`/`OP_CAMPAIGN_ONLY_FLAG`. Argument type metadata is the `OPF_*` family (sexp.h:34–70, e.g. OPF_SHIP, OPF_WING, OPF_SUBSYSTEM, OPF_IFF, OPF_AI_GOAL) returned per-arg by `query_operator_argument_type()`; return types are `OPR_*` (sexp.h:72–79) from `query_operator_return_type()`; `sexp_query_type_match()` reconciles OPF↔OPR for the validator. `sexp_ai_goal_link` maps AI-order operators to AI goal codes.

  **SEAM — standalone parser cut line.** A text→node-tree parser can be lifted cleanly because the reader path (`get_sexp_main`/`get_sexp`/`alloc_sexp`/`find_free_sexp`/`identify_operator`) touches **no ship/object/game state at all** — its only couplings are: (1) the parselo cursor + buffer (`Mp`, `Mission_text`, `ignore_white_space`, `is_white_space`, `EOF_CHAR`) — a thin lexer dependency; (2) the shared `Sexp_nodes[]` pool + `Operators[]` table (both needed by parser and evaluator, so they belong in the shared core, not in the game side); (3) the `Sexp_variables[]` symbol table via `get_sexp_text_for_variable`/`get_index_sexp_variable_name`. Everything that reaches into the game — `ship_name_lookup`, `wing_name_lookup`, `waypoint_lookup`, `Team_names[]`, `query_sexp_ai_goal_valid`, `ship_docking_valid` — lives inside `check_sexp_syntax()` and is **gated on `Fred_running`**; the runtime parser never calls it. So the concrete cut: {reader + pool + operator table + variable table} is the standalone parser; {`eval_sexp` + all `sexp_*` handlers + `check_sexp_syntax`'s Fred lookups} is the evaluator/game side. The one shared mutable is the `Sexp_nodes[].value`/`.text` cache — a pure parser must treat those as write-only-by-evaluator. Note `CTEXT()` couples node→variable resolution and would move with the shared core.

- **Rare knowledge:**
  - `true`/`false` are physically the same two node indices for the whole process (`Locked_sexp_*`), and `init_sexp()` post-patches their `type` back to `SEXP_ATOM` because `alloc_sexp` created them as `SEXP_LIST`.
  - Numbers live as strings in `node.text` until eval; `rand` *mutates* that text to memoize its roll (the `SEXP_NUM_EVAL` trick), which is why a node's text can differ from the mission file after play.
  - `SEXP_FLAG_PERSISTENT` survives `init_sexp()` across missions (campaign-persistent variables/red-alert); the pool free-scan skips persistent nodes.
  - `Sexp_useful_number` is an out-of-band "is this directive current yet" flag threaded from deep handlers back up to `mission_process_event` — not a value, a signal.
  - Pool is only 2200 nodes and the header comments record real campaign overflow history ("Dan ran out", bumped 1200→1600→2000→2200).
- **Deps:** parselo (`Mp`, stuffing), ship/object/wing/AI/message/mission-log subsystems (evaluator handlers), `Fred_running`, `Missiontime`.
- **Extraction seams:** (headline) see SEAM above — parser is game-free today; the only real work to split is relocating `Sexp_nodes[]`/`Operators[]`/`Sexp_variables[]`/`CTEXT` into a shared core and stubbing `Fred_running` out of the reader path.
- **Port notes (visible fixes landed here):**
  - `90af4e23b` multi-ship for-loop: `protect-ship`, `beam-protect-ship`, `ship-visible`, `ship-invulnerable`, `ship-guardian` used `return`/`break` that aborted the ship list at the first departed/not-yet-arrived ship; changed to `continue`/loop-through (campaign relies on all five).
  - `ffea8f068` never-warp: `sexp_deal_with_warp` parse-object branch assigned `P_SF_WARP_BROKEN` in both arms; `OP_WARP_NEVER` on a not-yet-arrived ship now sets the never-warp flag (op at sexp.h:213, table line 197).
  - `85ef77646` is-iff infinite loop: `sexp_is_iff` `continue` skipped `n = CDR(n)`; loop converted to `for` so a gone ship advances instead of hanging.
  - `62701a4ec` is-tagged: `sexp_is_tagged` (6344) now calls `ship_is_tagged()` so level-2 (TAG-B / `level2_tag_left`) tags register.
  - Structural: `d30e4d290` multiplayer excision removed ~410 lines from this file (net -410 vs `retail:code/parse/sexp.cpp`), so multi-only operator bodies/handlers are gone or stubbed — expect thinner switch arms than retail.

---

### mission/missionparse.cpp (+ pipeline) (~4,545 SLOC master)
- **Purpose:** Loads and parses a whole `.fs2`/`.fc2` mission file into the in-memory `mission`, ship/wing/object/goal/event/message tables, then post-processes into live game objects.
- **Entry points:** `missionparse.cpp: parse_main()` (3254, opens CFILE, `read_file_text`, `setjmp(parse_abort)`) → `parse_mission()` (2939) → section parsers `parse_player`, `parse_objects`/`parse_object`/`parse_create_object` (1047/1425/1839), `parse_wings` (2337), `parse_events`/`parse_event` (2415), `parse_goals`/`parse_goal` (2426), `parse_briefing` (757), `parse_waypoints`. `post_process_mission()` (2985) turns parse-objects into ships. Lightweight header read: `get_mission_info()` (3203).
- **Core state:** `The_mission` (global `mission`), `Mission_goals[]`/`Mission_events[]` (filled here, owned by missiongoals), `p_object` parse-objects, `Sexp_variables[]` via `stuff_sexp_variable_list()`. The SEXP hook: every `$Formula:` / arrival/departure cue calls `get_sexp_main()` and stores the returned node index (`goalp->formula`, `event->formula`, ship `arrival_cue`/`departure_cue`).
- **Mechanism:** Recursive-descent over parselo tokens; each mission section is a `required_string`/`optional_string` block. Every logical trigger in the file is a SEXP subtree parsed by `get_sexp_main()` and kept as an int index into `Sexp_nodes[]`; the parser never evaluates. `setjmp/longjmp(parse_abort)` is the error unwind.
- **Rare knowledge:** ship/wing arrival & departure *cues* are SEXP node indices evaluated every frame elsewhere — mission parse just wires the index. Goal/event `formula` = index into the shared sexp pool, so mission tables are lightweight handles onto `Sexp_nodes[]`.
- **Deps:** parselo, sexp (`get_sexp_main`), ship/wing/object creation, cfile, localization (`lcl_ext_open`).
- **Port notes:** net −724 lines vs retail (multiplayer/network + dead-subsystem excision). Fix `888369f8d`: `Entry_delay_time` global leaked across missions when `+Player Entry Delay:` absent — now reset when the token is missing.

---

### mission/missiongoals.cpp — goal & event evaluator driver (~1,324 SLOC master)
- **Purpose:** Per-frame driver that evaluates the stored goal/event SEXP formulas and updates directive/objective state.
- **Entry points:** `missiongoals.cpp: mission_eval_goals()` (905) — the frame tick; `mission_process_event()` (811) — evaluates one event's formula via `eval_sexp(sindex)`.
- **Core state:** `Mission_goals[MAX_GOALS]` (`.formula` = sexp index, `.satisfied`), `Mission_events[MAX_MISSION_EVENTS]` (`.formula`, `.result`, `.repeat_count`, `.interval`, `.chain_delay`, `.born_on_date`, flags `MEF_*`), `Event_index`, `Directive_count`, `Mission_goal_timestamp` (missiongoals.h:50–103).
- **Mechanism:** Iterates events; honors chaining (`chain_delay`: previous event must be true and aged, next must be false), sets `Sexp_useful_number=1`, calls `eval_sexp`. Uses the sexp value cache: if `Sexp_nodes[sindex].value == SEXP_KNOWN_FALSE` the event is retired (`formula=-1`, `repeat_count=-1`) so it never re-evals — the payoff of the evaluator's short-circuit pinning. Handles repeat counts, interval timestamps, directive-special (`DIRECTIVE_WING_ZERO`) hack.
- **Rare knowledge:** `DIRECTIVE_WING_ZERO (-999)` returned via `Directive_count` marks a directive true between waves; the known-false retirement is what keeps event eval O(active) not O(all) over a mission.
- **Deps:** sexp (`eval_sexp`, node value cache), missionlog, `Missiontime`.
- **Port notes:** net −378 vs retail (multiplayer team-scoring/net-sync paths removed).

---

### mission/missionmessage.cpp — message queue & personas (~1,648 SLOC master)
- **Purpose:** Runtime queue for in-mission messages (SEXP `send-message`, builtins), voice/anim playback, personas.
- **Entry points:** `missionmessage.cpp: message_queue_process()` (879) — the tick; `message_queue_message()` (1160), `message_send_unique_to_player()` (1339), `message_send_builtin_to_player()` (1403), `message_parse()` (286).
- **Core state:** `message_q` priority queue, `Messages[]`, `Personas[]`, `MessageWaves`, timing/priority enums; the `who_from`/`source` come from SEXP `OPF_WHO_FROM`.
- **Mechanism:** SEXP `send-message`/`send-random-message`/`send-message-list` handlers enqueue; `message_queue_process` sorts by priority, plays wave + talking-head anim, distorts text for enemy sources.
- **Rare knowledge:** persona index −1 (head ani but no persona) is a legitimate fallback case, not an error.
- **Deps:** sexp, anim/sound, ship (for persona), training.
- **Port notes:** net −463 vs retail. Fix `82aae9a65`: `message_play_anim` indexed `Personas[persona_index]` with −1 → OOB read; −1 now falls through to the no-persona head fallback.

---

### mission/missiontraining.cpp — training message system (~935 SLOC master)
- **Purpose:** Renders training directives/messages and drives the `OP_CATAGORY_TRAINING` sexps (key-pressed, targeted, speed, path-flown).
- **Entry points:** `missiontraining.cpp: training_check_objectives()`, `training_process_msg()` (107), `message_translate_tokens()` (108), `training_mission_init()` (281), `training_obj_display()` (147).
- **Core state:** `Training_buf/Training_text[8192]`, `Training_lines[]`, `Training_obj_lines[]`, `Training_failure`, `Training_context*` globals (also declared in sexp.h:489–497), `SPECIAL_CHECK_TRAINING_FAILURE (2000)`.
- **Mechanism:** Training SEXP operators write the `Training_context_*` globals (speed/path/waypoint) which sexp handlers read back; `message_translate_tokens` expands `$`/`#` tokens (key binds, ship names) into display text.
- **Rare knowledge:** the training context is a global side-channel shared with sexp.cpp — training ops and their checks communicate through `Training_context*`, not arguments.
- **Deps:** sexp (training ops), missionmessage, HUD.
- **Port notes:** net −255 vs retail. Fix `9413f97ee`: `message_translate_tokens` `#`-branch had an inverted `if (toke1) break` (broke on *finding* the close `#`), causing `strncpy(temp, text, NULL-text)` overflow into `char temp[40]` and well-formed `#tokens#` never translating; test corrected to match the `$` branch.

---

### gamesequence/gamesequence.cpp — game state machine (~395 SLOC master)
- **Purpose:** The event-driven, stack-based finite state machine sequencing all top-level game screens/modes (menu, briefing, gameplay, debrief…).
- **Entry points:** `gamesequence.cpp: gameseq_process_events()` (370) — pump; `gameseq_post_event()` (204), `gameseq_set_state()` (244), `gameseq_push_state()` (272)/`gameseq_pop_state()` (305), `gameseq_get_state()`/`gameseq_get_depth()`, `gameseq_init()` (184).
- **Core state:** `state_stack gs[GS_STACK_SIZE=10]` (each holds `current_state` + a 20-slot circular `event_queue`), `gs_current_stack` (top index), reentry guards `state_reentry`, `state_processing_event_post`, `state_in_event_processer`; `GS_event_text[]` names. `GS_STATE_*`/`GS_EVENT_*` defined in gamesequence.h.
- **Mechanism:** Events are posted to the top stack frame's ring buffer; `gameseq_process_events` dequeues and calls out to `game_process_event` (in freespace.cpp) which calls `gameseq_set_state`/`push`/`pop`; state transitions invoke `game_leave_state`/`game_enter_state`. Push/pop implement a screen stack (e.g. pause over gameplay). Heavy `Assert(state_reentry==1)` guards enforce that state changes happen only inside event processing.
- **Rare knowledge:** setting the same state without `override` is a no-op; `gameseq_set_state` flushes the pending event queue on transition, but `push` deliberately does not (commented-out flush). The actual per-state logic lives in `freespace.cpp`, not here — this file is pure mechanism.
- **Deps:** freespace.cpp (`game_process_event`, `game_enter/leave_state`); otherwise self-contained.
- **Port notes:** smallest, essentially untouched vs retail aside from the tree-wide network excision; no dedicated bugfix commits.
