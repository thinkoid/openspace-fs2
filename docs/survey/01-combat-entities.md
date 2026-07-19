# Cluster 01 — Combat entities

Subsystems: **ship**, **weapon**, **cmeasure**. The entities that fight, and the
projectiles and decoys between them. See [README](README.md) for conventions
(symbol-first anchors; `[retail]` = immutable branch).

> **Structural note:** `code/ship/` holds *two* subsystems. This file covers the
> ship **entity** (~14k). The flight/order **AI** (`aicode.cpp`, `aibig.cpp`,
> `aigoals.cpp`, `aiturret.cpp`, ~17k) lives in the same directory but is a
> distinct subsystem — it is **not yet surveyed** and is a high-value mining
> target (the goal/order state machine). Tracked as a gap below.

---

### ship  (~14k master, excluding the ~17k AI files aicode/aibig/aigoals which are a separate subsystem)
- **Purpose:** The player/AI combatant entity: instantiation from `ships.tbl`, per-frame lifecycle, weapon firing, subsystem/hull/shield damage, death sequence, and warp effects.
- **Entry points:** `ship/ship.cpp: ship_create()`, `ship_set()`, `ship_process_pre()` / `ship_process_post()` (per-frame), `ship_delete()`, `ship_render()`, `ship_fire_primary()` / `ship_fire_secondary()`, `parse_shiptbl()` / `parse_ship()`; damage in `ship/shiphit.cpp: ship_apply_local_damage()` / `ship_apply_global_damage()` / `do_subobj_hit_stuff()` / `ship_hit_kill()`; death & warp in `ship/shipfx.cpp: shipfx_blow_up_model()` / `shipfx_warpin_start()` / `shipfx_warpout_start()` / `shipfx_large_blowup_init()`.
- **Core state:** `ship/ship.cpp: Ships[MAX_SHIPS]` (live instances) indexed by `object.instance`; `Ship_info[MAX_SHIP_TYPES]` (parsed table archetypes, `Num_ship_types`); per-instance `ship_weapon weapons` (bank ammo/state); subsystem pool `Ship_subsystems[MAX_SHIP_SUBOBJECTS]` handed out via the `ship_subsys_free_list` intrusive linked list; ship-object index list `Ship_objs[]` / `Ship_obj_list`; shield damage records `shield.cpp: Shield_hits[MAX_SHIELD_HITS]`.
- **Mechanism:** `parse_shiptbl()` fills `Ship_info[]` once at boot; `ship_create()` grabs an `object`, an `Ships[]` slot, allocates per-instance `ship_subsys` nodes off the free list, and calls `physics_ship_init()`. Every frame `ship_process_post()` runs ETS energy, afterburners, subsystem-disruption checks, and `ship_dying_frame()`. Damage flows shield→hull through `apply_damage_to_shield()` and `do_subobj_hit_stuff()`, and death triggers the `shipfx` blow-up/debris pipeline.
- **Rare knowledge:** The subsystem free-list allocator (`GET_FIRST(&ship_subsys_free_list)` with a dummy sentinel node) is a classic 1998 fixed-pool-with-intrusive-list idiom worth mining. `shipfx.cpp: shipfx_large_blowup_*` implements the multi-stage capital-ship death animation; the shield is a per-triangle hit-decal system (`gshield_tri` / `Global_tris`) that mirrors the model's shield mesh — non-obvious and undocumented elsewhere. ETS (Engine/Shield/Weapon energy transfer) coupling lives right in the post-frame path.
- **Deps:** object/physics, model (`polymodel`, submodels, shield mesh), ai (`Ai_info`, `ai_index`), weapon (firing creates `Weapons[]`), fireball/particle/debris (death fx), hud/ets, network (`nprintf` sync notes).
- **Extraction seams:** Deeply entangled — `Ships[]`/`Ship_info[]` are referenced across nearly every subsystem via `object.instance`. Shield (`shield.cpp`) and afterburner (`afterburner.cpp`) are the cleanest sub-cuts; the death/warp fx in `shipfx.cpp` are separable behind `object`+`polymodel`. The AI files (aicode/aibig/aigoals) are already a distinct cut despite living in `ship/`.
- **Port notes:** Light. Demo-build gating remains: `shield.cpp` compiles a full renderer under `#ifndef DEMO` and a set of no-op stubs (`add_shield_point(){}`, `apply_damage_to_shield(){return damage;}`) under `#else`. Dead `#if 0` block in `ship_render()` (big-ship attack-point debug draw). Retail-era authorial comments preserved (`// AL 1-6-98`, `// MK, 3/12/98`, `Int3()` / "get allender" asserts). No fs2open-hash bugfix references seen in this subsystem.

### weapon  (~3.3k weapons.cpp; ~10k with beam/swarm/corkscrew/emp/flak/trails/muzzleflash/shockwave aux files)
- **Purpose:** Projectile/missile entities: table-driven weapon archetypes, spawn, per-frame flight + homing, and impact/detonation handling.
- **Entry points:** `weapon/weapons.cpp: weapon_create()`, `weapon_process_pre()` / `weapon_process_post()` (per-frame), `weapon_home()` (guidance), `weapon_hit()` (impact), `weapon_set_tracking_info()`, `weapon_delete()`, `weapon_render()`, `parse_weaponstbl()` / `parse_weapon_expl_tbl()`.
- **Core state:** `weapons.cpp: Weapons[MAX_WEAPONS]` (live projectiles, `Num_weapons`) indexed by `object.instance`; `Weapon_info[MAX_WEAPON_TYPES]` (parsed archetypes, `Num_weapon_types`) with `wi_flags` (`WIF_HOMING_HEAT`/`WIF_HOMING_ASPECT`/`WIF_BOMB`/`WIF_BEAM`…) and `subtype` (`WP_LASER`/`WP_MISSILE`); per-instance homing fields `homing_object`, `homing_subsys`, `target_sig`, `creation_time`, `lifeleft`.
- **Mechanism:** `parse_weaponstbl()` loads `weapons.tbl` into `Weapon_info[]`; ship firing calls `weapon_create()`, which linearly scans `Weapons[]` for a free slot (`weapon_info_index < 0`) and, when near `MAX_WEAPONS`, calls `collide_remove_weapons()` to reclaim. `weapon_process_post()` runs `weapon_home()` for guided types — a 0.25s arming delay, heat/aspect retarget logic keyed on target `signature` vs `target_sig`, and speed-clamp steering via `phys_info.desired_vel`. `weapon_hit()` dispatches damage, shockwaves, EMP, and spawn (child) weapons.
- **Rare knowledge:** Signature-based target validation (`homing_object->signature != wp->target_sig`) is how the engine cheaply detects that a homing target was destroyed and its object slot recycled — worth mining. Beam weapons are explicitly excluded from `weapon_create()` (`Assert(!(wi_flags & WIF_BEAM))`) and handled entirely in `beam.cpp`, a separate ~3k continuous-damage subsystem. Countermeasure spoofing is driven from here via `Cmeasures_homing_check`. Commented-out "bombs drop for a bit" and aspect-lock code preserves the original design reasoning verbatim.
- **Deps:** object/physics, model, ship (parent `objnum`, `ship_subsys` targets), cmeasure (homing decoys), fireball/particle (`trails.cpp`, `muzzleflash.cpp`), shockwave/emp/flak on impact, collision.
- **Extraction seams:** Fairly self-contained around `Weapons[]`/`Weapon_info[]` + `object`/`phys_info`. The effect aux files (`trails`, `swarm`, `corkscrew`, `emp`, `flak`, `shockwave`, `muzzleflash`) are individually liftable behind small headers; `beam.cpp` is already a near-independent cut. Cleanest boundary: homing depends on ship `signature`/subsystem, so lift with a target-abstraction shim.
- **Port notes:** Minimal — no `#if 0`/fs2open markers in `weapons.cpp`. Retail authorial comments intact (`// AL 4-8-98`, `// MK, 3/12/98`); commented-out original blocks (bomb-drop, delete-for-AI-ships) left in place rather than removed.

### cmeasure  (~294 master)
- **Purpose:** Countermeasure decoys ("chaff/flares") ejected by ships to spoof homing missiles.
- **Entry points:** `cmeasure/cmeasure.cpp: cmeasure_create()`, `cmeasure_process_pre()` / `cmeasure_process_post()` (per-frame), `cmeasure_delete()`, `cmeasure_render()`, `cmeasure_select_next()`, `cmeasure_init()`.
- **Core state:** `cmeasure.cpp: Cmeasures[MAX_CMEASURES]` (live decoys, `Num_cmeasures`) with `subtype` (== `CMEASURE_UNUSED` marks a free slot), `lifeleft`, `team`; archetypes `Cmeasure_info[MAX_CMEASURE_TYPES]`; the global handshake flag `Cmeasures_homing_check` read by `weapon_home()`.
- **Mechanism:** `cmeasure_create()` linear-scans `Cmeasures[]` for a `CMEASURE_UNUSED` slot, spawns an `OBJ_CMEASURE` object with physics/render, randomizes `lifeleft` and a launch displacement, and sets `Cmeasures_homing_check = 2` so the missile-homing code rescans all objects for two frames (the comment notes one frame causes sync problems from end-of-frame object creation). Decoys are dumb ballistic objects; the spoofing decision lives in `weapon_home()`, which weights `CMF_DUD_HEAT`/`CMF_DUD_ASPECT` flags against the seeker type.
- **Rare knowledge:** The two-frame `Cmeasures_homing_check` handshake is a subtle era workaround for objects created at end-of-frame not yet being visible to homing scans — hard to find explained today. The countermeasure itself carries no homing logic; it's a passive attractor evaluated by the weapon seeker, an inversion many would not expect.
- **Deps:** object/physics, ship (source `objnum`, `team`), weapon (homing evaluation), model/render. Debug gate on `Countermeasures_enabled`/`Ai_firing_enabled`.
- **Extraction seams:** Highly self-contained (single ~300-line file + header). The only outward coupling is the `Cmeasures_homing_check` global consumed by `weapon_home()` and the `OBJ_CMEASURE` object type — lift cleanly by exposing that flag through a weapon-side accessor.
- **Port notes:** — (no `#if 0`/fs2open markers; only `#ifndef NDEBUG` debug-enable guard and retail `// MK, 3/12/98`-style comments).

---
### GAP: AI  (aicode.cpp, aibig.cpp, aigoals.cpp, aiturret.cpp, ~17k — in code/ship/)
Not yet surveyed. The flight/order/goal state machine — retail's `Ai_info[]`,
the goal stack, `ai_frame()` dispatch, and the SEXP-driven order system. High
mining value (couples to the SEXP VM in cluster 03). Survey in a follow-up pass.
