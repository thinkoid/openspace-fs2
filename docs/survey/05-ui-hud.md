# Cluster 05 — UI & HUD

Subsystems: **hud**, **missionui**, **menuui**, **ui**, **popup**, **radar**,
**controlconfig**, **stats**, **gamehelp**. The in-flight heads-up display, the
front-end screens, and the hand-rolled widget toolkit under them. See
[README](README.md) for conventions.

> **Note:** all layout here is hardcoded 4:3 (plus a second 1024 column) — the
> coordinate tables are data that isn't derivable, a recurring mining theme
> below. The bespoke `ui/` widget toolkit is the most self-contained lift in the
> cluster.

---

### hud  (~20.5k cpp master)
- **Purpose:** In-flight heads-up display — reticle, targeting box, radar, shields, ETS, weapons, escort/wingman status, messages, squad-message menu, and HUD config editor.
- **Entry points:** `hud/hud.cpp: HUD_init()`, `hud_update_frame()`, `HUD_render_2d()` / `HUD_render_3d()`, `hud_show_damage_popup()`; `hud/hudtarget.cpp: hud_target_common()`/`hud_process_homing_missiles()`; `hud/hudconfig.cpp: hud_config_do_frame()`; `hud/hudsquadmsg.cpp: hud_squadmsg_do_frame()`; `hud/hudshield.cpp: hud_shield_equalize()`.
- **Core state:** `hudconfig.cpp: HUD_CONFIG_TYPE HUD_config` (per-gauge show/popup bitfields `show_flags`/`show_flags2`, per-gauge `clr[]` alphacolors); gauge-id enums in `hud/hudgauges.h` (`HUD_ETS_GAUGE`, `HUD_WEAPONS_GAUGE`, …); per-module coord tables e.g. `hudreticle.cpp: Reticle_gauges[]`, `Reticle_frame_coords[GR_NUM_RESOLUTIONS][…]`; `hud.cpp: Pl_hud_subsys_info[SUBSYSTEM_MAX]`.
- **Mechanism:** No central gauge registry — `HUD_render_2d()` is a long hand-written sequence that, per gauge, checks `hud_gauge_active()` / `hud_gauge_is_popup()` / `hud_gauge_popup_active()`, sets `hud_set_gauge_color()`, then calls that gauge's own `hud_show_*()` renderer. Each gauge module owns its animation frames (`hud_frames`) and a `[GR_NUM_RESOLUTIONS]` coordinate table loaded once per level. Config editor (`hudconfig.cpp`) is a UI_WINDOW screen editing the same `HUD_config` bitfields.
- **Rare knowledge:** MINE — the entire per-gauge coordinate-table architecture is hardcoded 4:3 (and a second 1024 column) layout data, not derivable; the flag-bit gauge on/off/popup encoding split across two 32-bit words (`show_flags`/`show_flags2`); `hudgauges.h` gauge-id enum is the canonical gauge list. Targeting math (`hudtarget.cpp`, largest file) and lead-indicator/lock code (`hudlock.cpp`) worth mining.
- **Deps:** graphics (gr_*), bmpman/anim, ship/weapon/object (target data), io (key/mouse), gamesnd, ui (config screen), radar, localization (XSTR/EMP strings).
- **Extraction seams:** Gauge renderers are individually liftable (each reads game state + its coord table), but collectively wired into game-state globals (Player_ship, Objects). Config screen is a clean UI_WINDOW unit.
- **Port notes:** `hud.cpp: hud_show_damage_popup()` — `hud_subsys_list[SUBSYSTEM_MAX]` bounded with `if (num >= SUBSYSTEM_MAX)` guard (f32ec9e). `hudshield.cpp: hud_shield_equalize()` — added `OF_NO_SHIELDS` early-out (b287e86). `hudtargetbox.cpp` — dangling `printable_ship_class`/`temp_name` scoping fix for `#` ship-copy debris names (47a0cb9, ~line 1157).

### missionui  (~12.9k cpp master)
- **Purpose:** Pre/post-mission fullscreen screens — briefing, command briefing, red-alert, ship selection, weapon loadout, debriefing, plus the shared briefing-screen chrome and mission pause.
- **Entry points:** `missionui/missionbrief.cpp: brief_init()`/`brief_do_frame()`/`brief_render()`/`brief_compact_stages()`; `missionui/missiondebrief.cpp: debrief_init()`/`debrief_do_frame()`; `missionui/missionscreencommon.cpp: common_buttons_init()`/`common_check_buttons()`; `missionui/missionshipchoice.cpp` and `missionweaponchoice.cpp` do_frame loops; `missionui/missioncmdbrief.cpp`, `redalert.cpp`.
- **Core state:** `missionscreencommon.cpp: Common_buttons[3][GR_NUM_RESOLUTIONS][NUM_COMMON_BUTTONS]` (the shared Briefing/Ship/Weapon/Commit/Options button strip) and `Current_screen`; briefing globals `Briefing`, `Num_brief_text_lines`, brief_icon/brief_stage arrays; `missiondebrief.cpp` debrief coord tables (`Debrief_title_coords[]`, `Debrief_award_coords[]`, …) and stage/recommendation state.
- **Mechanism:** Each screen builds a UI_WINDOW, registers `Common_buttons` hotspots + `add_XSTR()` labels, then loops a `*_do_frame()` that runs `ui_window.process()`, dispatches button actions, and renders a mask-bitmap background with resolution-indexed coord tables. Briefing plays staged text+voice+3D icon map; ship/weapon choice are drag-and-drop loadout screens sharing the common button bar; debrief tallies scoring and awards.
- **Rare knowledge:** MINE — the `Common_buttons[Current_screen-1][res][…]` 3-screen shared navigation bar is the glue tying briefing/ship/weapon screens; briefing stage compaction (`brief_compact_stages`) and SEXP formula-gated stage evaluation; hardcoded debrief award/medal coord tables. Loadout drag-drop and weapon-bank logic in the two largest files worth mining.
- **Deps:** ui (UI_WINDOW), graphics/bmpman/anim, mission (parse, campaign, SEXP formulas), stats/scoring (debrief), gamesnd/sound (voice), hud (some shared), popup, localization.
- **Extraction seams:** Entangled — screens share `missionscreencommon` state and reach deep into mission/campaign globals. Cut best at the whole-screen boundary, keeping the common button module together.
- **Port notes:** `missionbrief.cpp: brief_render()` — skip text/voice when `Briefing->num_stages == 0` to avoid stale `Num_brief_text_lines` (2ad1f2c); `brief_compact_stages()` — clear vacated tail stage slots after compaction (2ab7e2d). `missiondebrief.cpp` — reset `Weapon_energy_cheat` on debrief so it doesn't persist across missions (1677605); simulator/non-campaign missions no longer get `SCORE_DEBRIEF_FAIL` music/tally via `Campaign.next_mission == current_mission` guard (01ba779).

### menuui  (~10.7k cpp master)
- **Purpose:** Front-end shell screens — main hall (hub), pilot/barracks management, ready room (campaign/mission select), tech room, options, credits, plus the snazzy mask-region menu engine.
- **Entry points:** `menuui/mainhallmenu.cpp: main_hall_init()`/`main_hall_do()`/`main_hall_read_table()`; `menuui/snazzyui.cpp: snazzy_menu_do()`/`read_menu_tbl()`; `menuui/playermenu.cpp: player_select_init()`/`player_select_do()`; `menuui/barracks.cpp: barracks_init_stats()`; `menuui/readyroom.cpp`, `techmenu.cpp`, `optionsmenu.cpp`, `credits.cpp` do-frame loops.
- **Core state:** `mainhallmenu.cpp: MENU_REGION Main_hall_region[NUM_MAIN_HALL_REGIONS]`, door/misc-anim instance lists, `Main_hall_region_linger_stamp`; `snazzyui.h: MENU_REGION` (mask-color → region + text/key/sound); barracks `Cur_pilot`/`scoring_struct` display; per-screen UI_WINDOW objects.
- **Mechanism:** Two UI idioms coexist. The main hall + pilot select use `snazzy_menu_do()`: a mask bitmap where each pixel color indexes a `MENU_REGION`, so mouse-over/click is resolved by reading the mask pixel under the cursor (regions/anims defined in `menu.tbl` via `read_menu_tbl()`/`main_hall_read_table()`). Other screens (barracks, tech, options, ready room) use standard UI_WINDOW + gadget button bars with resolution-indexed coord tables.
- **Rare knowledge:** MINE — the mask-color region engine (`snazzyui.cpp`) and its `menu.tbl`/`mainhall.tbl` data-driven layout, animated door/intercom scheduling in the main hall, and the tech-room ship/weapon database viewer. Fishtank is a decorative animation curiosity.
- **Deps:** graphics/bmpman/anim, ui, gamesnd/sound, playerman (pilot files), stats/medals (barracks), mission/campaign (ready room), parse (tbl reading), popup, gamesequence.
- **Extraction seams:** Main hall + snazzy engine form a fairly self-contained data-driven unit; the standard UI_WINDOW screens are individually liftable but bound to playerman/campaign globals.
- **Port notes:** `barracks.cpp: barracks_init_stats()` — bounded the ~85-line stats array that appended one line per ship-class-with-kills past ~21 fixed lines (b03cae5). This is the FS1 software UI path revived so pilot-select/barracks rendering is live.

### ui  (~4.9k cpp master)
- **Purpose:** The engine's hand-rolled retained-mode widget toolkit (window + gadgets) used by every front-end/config screen.
- **Entry points:** `ui/window.cpp: UI_WINDOW::create()`/`process()`/`draw()`/`destroy()`; `ui/gadget.cpp: UI_GADGET::base_create()`; per-widget `create()`/`process()`/`draw()` in `button.cpp`, `listbox.cpp`, `slider2.cpp`, `inputbox.cpp`, `checkbox.cpp`, `scroll.cpp`, `keytrap.cpp`.
- **Core state:** `ui/ui.h` class hierarchy — base `UI_GADGET` (parent/children/prev/next linked family, `bmap_ids[MAX_BMAPS_PER_GADGET]`, hotspot linkage) subclassed by `UI_BUTTON`, `UI_CHECKBOX`, `UI_RADIO`, `UI_SCROLLBAR`, `UI_LISTBOX`, `UI_INPUTBOX`, `UI_DOT_SLIDER(_NEW)`, `UI_KEYTRAP`; `UI_WINDOW` owns the gadget list, mask bitmap, focus, tooltips, and `UI_XSTR` label list.
- **Mechanism:** Each frame a screen calls `UI_WINDOW::process(key)` which walks the gadget family calling each gadget's virtual `process()` (mouse/focus/hotkey), then `draw()` blits per-state bitmap frames from `bmap_ids[]`. Widgets are linked to background-mask hotspots so art defines geometry; focus switches via `check_focus_switch_keys()`. Localized labels are attached with `add_XSTR()` and rendered by `draw_xstrs()`.
- **Rare knowledge:** MINE — this is a bespoke 1998 widget framework: virtual `process()`/`draw()` dispatch, mask-hotspot binding, multi-frame bitmap button states, `UI_XSTR` localization overlay, `UI_DOT_SLIDER` discrete sliders. Reproducing its exact focus/hotspot/tooltip semantics is the value.
- **Deps:** graphics (gr_*), bmpman, io (key/mouse), localization (XSTR), gamesnd.
- **Extraction seams:** Very self-contained — pure toolkit depending only on graphics/io/bmpman. Cleanest cut in the cluster; could be lifted wholesale as a library.
- **Port notes:** — (no listed fixes; underlies all the revived software-UI screens).

### popup  (~1.6k cpp master)
- **Purpose:** Modal dialog system (yes/no/generic message boxes) and the death/respawn popup.
- **Entry points:** `popup/popup.cpp: popup()` (varargs entry), `popup_init()`, `popup_do()` (loop), `popup_close()`, `popup_till_condition()`; `popup/popupdead.cpp` death-screen loop.
- **Core state:** `popup/popup.h: popup_info` (text lines, button count/text, choice), module-static active-popup state; `popupdead` respawn/end-mission choice state.
- **Mechanism:** `popup()` fills a `popup_info`, calls `popup_init()` to build a small UI_WINDOW with 1-3 buttons, then spins its own `popup_do()` message loop (blocking the caller) splitting text via `popup_split_lines()` and returning the chosen button index. Keyboard shortcuts and default-button sound handled in `popup_process_keys()`.
- **Rare knowledge:** — mostly mundane; the reentrant blocking-loop-within-a-frame pattern is the only subtlety.
- **Deps:** ui, graphics/bmpman, io, gamesnd, localization.
- **Extraction seams:** Self-contained given ui; clean lift.
- **Port notes:** —

### radar  (~0.7k cpp master)
- **Purpose:** The cockpit radar gauge — plots ships/objects as colored blips on the round radar bitmap.
- **Entry points:** `radar/radar.cpp: radar_init()`, `radar_mission_init()`, `radar_frame_init()`, `radar_plot_object()`, `radar_blip_color()`, `radar_draw_circle()`, `radar_blip_draw_distorted()`.
- **Core state:** `radar.cpp` blip pools/lists (`blip` structs, N/S bright/dim blip chains), radar range setting, distortion/flicker timers.
- **Mechanism:** Per frame `radar_frame_init()` clears blip lists; `radar_plot_object()` is called per object to project its relative position onto radar coords, bucketed by IFF color (`radar_blip_color()`) and range band; blips are then drawn, with EMP/nebula distortion and flicker effects. Rendered as a HUD gauge (called from `hud.cpp: hud_show_radar()`).
- **Rare knowledge:** MINE — the range-band bucketing, distortion/flicker EMP handling, and IFF blip-color logic; small but self-contained gauge with its own projection math.
- **Deps:** hud (color/gauge integration), object/ship (blip sources), graphics/bmpman, iff.
- **Extraction seams:** Small and fairly self-contained; couples to object/ship globals and HUD color state.
- **Port notes:** —

### controlconfig  (~2.7k cpp master)
- **Purpose:** Keyboard/joystick control binding table and the in-game control-config editor screen.
- **Entry points:** `controlconfig/controlsconfig.cpp: control_config_init()`/`control_config_do_frame()`/`control_config_close()`; `controlconfig/controlsconfigcommon.cpp: control_config_common_init()`, `translate_key()`, key/joy default setup.
- **Core state:** `controlsconfigcommon.cpp: config_item Control_config[CCFG_MAX + 1]` — the master action table (each with `key_id`, `joy_id`, `key_default`, `joy_default`, text) indexed by action enums like `TARGET_SHIP_IN_RETICLE`.
- **Mechanism:** `Control_config[]` is the single source of truth for every bindable action, with defaults assigned in `control_config_common_init()`. The editor screen (UI_WINDOW list) lets the player rebind, storing chords via `KEY_ALTED/SHIFTED/CTRLED` bitmask on `key_id`; conflict detection and undo are handled in the do-frame loop; gameplay code queries bindings through this table.
- **Rare knowledge:** MINE — the `Control_config[]` action table (order, defaults, text ids) is canonical game-wide input data; the ALT/SHIFT/CTRL chord bit-encoding on key ids.
- **Deps:** io (key/joy), ui, graphics, gamehelp (control-line display), localization.
- **Extraction seams:** The `controlsconfigcommon` table is a clean, data-only unit; the editor screen depends on ui.
- **Port notes:** —

### stats  (~1.7k cpp master)
- **Purpose:** Scoring/medals — per-mission and career stat accumulation, kill/assist evaluation, rank/badge/medal awards, and the medals display screen.
- **Entry points:** `stats/scoring.cpp: scoring_level_init()`, `scoring_eval_kill()`, `scoring_eval_assists()`, `scoring_eval_hit()`, `scoring_do_accept()`/`scoring_backout_accept()`, `scoring_eval_rank()`/`scoring_eval_badges()`; `stats/medals.cpp: medal_main_init()`/`medal_main_close()`; `stats/stats.cpp` scoreboard blit.
- **Core state:** `scoring_struct` (kills, assists, score, rank, medals, badge counts) held per-player; `medals.cpp` medal/badge coord + bitmap tables.
- **Mechanism:** During a mission `scoring_add_damage()`/`scoring_eval_hit()` track damage contribution; on a ship death `scoring_eval_kill()` awards the kill (with assist splitting by damage share) and updates rank/badge thresholds. `scoring_do_accept()` commits mission stats to the career total at debrief (with `scoring_backout_accept()` as the undo). Medals screen renders the awards mask/bitmap grid.
- **Rare knowledge:** MINE — the exact kill/assist attribution rules (damage-share thresholds), rank/badge/medal award tables and thresholds (`scoring_eval_rank`/`_badges`), and the accept/backout career-commit protocol.
- **Deps:** ship/object (kill events), playerman (career persistence), missionui/debrief (accept trigger), graphics/ui (medals screen), localization.
- **Extraction seams:** Scoring logic (`scoring.cpp`) is fairly self-contained rules code keyed on ship/object; medals screen is a separate UI unit.
- **Port notes:** — (barracks displays these stats; the debrief accept path is where the missiondebrief cheat-reset fix lives).

### gamehelp  (~1.6k cpp master)
- **Purpose:** Player help — the F1 context-help overlay (per-screen hotspot annotations) and the fullscreen gameplay/controls help screen.
- **Entry points:** `gamehelp/contexthelp.cpp: context_help_init()`, `launch_context_help()`, `context_help_grey_screen()`; `gamehelp/gameplayhelp.cpp: gameplay_help_init()`, `gameplay_help_blit_control_line()`, `gameplay_help_goto_prev/next_screen()`, `gameplay_help_set_title()`.
- **Core state:** context-help overlay tables (per-game-state help regions/text parsed from `help.tbl`); gameplay-help page/title state and control-line list.
- **Mechanism:** `launch_context_help()` greys the current screen and overlays help arrows/text tied to the active game state's hotspot table. The gameplay help screen paginates control descriptions, pulling live binding text from `Control_config[]` (`gameplay_help_blit_control_line()`).
- **Rare knowledge:** MINE — the `help.tbl`-driven per-screen context-help overlay data and the coupling of the help screen to the controlconfig binding table for live control-name display.
- **Deps:** controlconfig (control text), graphics/bmpman, io, ui, parse (help.tbl), localization, gamesequence.
- **Extraction seams:** Self-contained overlay/help unit; only real coupling is reading `Control_config[]`.
- **Port notes:** —
