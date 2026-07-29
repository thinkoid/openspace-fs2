# openspace-fs2 — project log

Restart 2026-07-16 — **DIRECTION: retail-forward Linux port.**
Repo: `/home/lnicoara/build/mine.d/fs2.d/fs2`

Branches:

- `retail` = `663b3471b` (2002 Volition warpcore CVS import; ancestor of upstream tip) — pristine, immutable
- `experimental` = working branch, reset onto retail — the port happens here
- `master` = `release_26_0_0` (`c9bb68c12`) — fix-mine + reference implementation, tracks upstream
- `reference/{2018, 2023, 2023-stash, 2023-prereboot, 2023-prereboot-stash}` = old fork lines (style guide; prereboot = 12-commit 2020-2023 line salvaged from `openspace.d/.attic`); `fork-point-2018` = `138a1d01d`

All pushed to codeberg. `upstream` remote = github scp-fs2open (fetched).

Goal: non-networked, non-multiplayer, Linux-only, retail-campaign game.
Retail numbers: 404k lines, 59 subsystems, 212 SEXP ops (campaign uses 128 — no trim needed), 20 files with `_asm`, complete software renderer + embryonic `gropengl.cpp` (17k, real code).
Delete-not-port: `directx/` `glide/` `network/` `vcodec/` `inetfile/` `demo/` `exceptionhandler/`.
Known gaps: MVE cutscene decoder absent from the drop (ffmpeg decodes MVE today); HUD/UI hardcoded 640x480/1024x768 4:3.

Port manuals: (1) upstream history `volition_import`..~2005 = the original port, commit by commit; (2) `reference/2018`+`2023` = osapi/SDL/OpenAL/cfile shapes; (3) fs2 @ `retail` = the 1998 source (ninetyeight dir deleted — same commit; regenerate GNU GLOBAL tags in `fs2/` if wanted).

Disk: openspace-fs2 → `fs2`; windows → `gog` (pristine .vp); game → `rundir` (unpacked, test runs); `missions` kept at top level; ninetyeight/scratch/openspace.d deleted (sexp.h purge saved in `.attic`).
Oracles: GOG missions corpus + `sexp-used.txt`; pofer for POF; table/mission parse vs stock.

## Subsystem inventory (2026-07-16) — full analysis in public/fs2-inventory.txt

56 subsystems, 403,852 lines. **DELETE** ~104k (26%): network 61k, directx 15k, glide, vcodec, inetfile, demo, exceptionhandler, scramble + partials (graphics D3D/DDraw/Glide backends 13.3k, io SideWinder-FF 4.3k, sound midi/redbook/dscap 1.5k).
**PLATFORM** ~20k: osapi, io (key/mouse/joy/timer), sound (ds→OpenAL, acm→own ADPCM), graphics (keep 31k software renderer → SDL surface), cutscene (MVE via ffmpeg, defer), debugconsole.
**PORT** ~280k, near-zero Win taint: ship 40k (incl. ALL AI: `aicode.cpp` 480KB), hud 26k, mission 20k, missionui 19k, menuui 16k, parse 13k, weapon 12k, model 9.6k, freespace2 9.2k, object 8.8k, ui 7.2k...

Keeps that look deletable: cryptstring = cheat hashes (keycontrol uses it); `keycontrol.cpp` in `io/` is game logic; palman needed by software renderer.
Cautions: 60 files outside `network/` touch `multi_*`/`GM_MULTIPLAYER` (stub header strategy); `_asm` has NO C fallbacks in retail (0 `NO_ASM` guards; graphics 13 files, math 2, globalincs 2); ISO-8859 bytes in `freespace.cpp` etc. (encoding sweep first — binary-skipping greps lie).
Port order: globalincs+math+parse → cfile (+archiver tool) → data/media (bmpman/anim/model...) → osapi/io/graphics-soft (SDL) → sound (OpenAL) → freespace2+gamesequence+ui/menuui → gameplay (object/physics/ship/weapon/mission/hud) → cutscene/polish.

## Point 1 (2026-07-17): DONE — foundation builds, oracle green

Sweep FIRST (staged, uncommitted): CVS `$Log` purge 452 files / -52,903 lines, token-stream verified comment-only; encoding fixed (5 files: literals → `\xNN` escapes, comments ASCII).

Build: meson (`fs2/meson.build`), C++17, flat includes via `-I` list. libfoundation = globalincs+math+localization+parselo+encrypt+`sexp.cpp` — ALL COMPILE, 0 errors, 278 warnings (mostly `-Wwrite-strings`; const-correct per subsystem later).

Central fixes: `pstypes.h` (min/max macros→templates was THE fix — unblocked ~2000 errors; stricmp/strnicmp/_isnan/_cdecl shims; `__int64`→`long long`; ccodes or/and keyword rename; malloc/free/strdup macro redefs dropped, `vm_*` now thin wrappers), `globalincs/debug.cpp` NEW (replaces `windebug.cpp`: Error/Warning/WinAssert/outwnd→stderr), `2d.h` function-pointer default args → inline wrappers, `psnet2.h` fd_set, `staticrand.h` missing include, `-Wno-changes-meaning` project-wide (renames at port time).

Tests: `fs2/tests/` math_test (passes), sexp_dump oracle. Temporary debt, all in `fs2/tests/`: `stubs.cpp` (dc console, timer, neb2), `cfile_stub.cpp` (loose files via stdio; dies at point 2), `gamestubs_gen.cpp` (GENERATED trap stubs for 118 evaluator refs; regen via scratchpad `gen_gamestubs.py` from demangled ld undefined list).

**ORACLE RESULT**: all 41 missions + `FreeSpace2.fc2` parse (exit 0); operator vocabulary matches the 2018 `sexp-used.txt` EXACTLY plus `has-time-elapsed` (SM1-08, sm3-09) which 2018 MISSED. Retail campaign vocabulary = 129 operators, not 128.
Oracle needs: `init_sexp` + `stuff_sexp_variable_list` before scanning; `free_sexp2` per tree (2200-node pool); tags = `$Formula`/`+Formula`/`$Arrival Cue`/`$Departure Cue`/`$AI Goals`.

## Point 2 (2026-07-17): DONE — cfile on std::filesystem/POSIX, VP oracle green

4 parallel agents ported `cfile.cpp`/`cfilesystem.cpp`/`cfilelist.cpp`/`cfilearchive.cpp`+archiver; shared decisions centralized first (`Cfile_block`: HANDLEs → fd+map_len POSIX mmap; `Pathtypes` lowercase forward-slash; fnmatch for disk scans; `_MAX_PATH`/strlwr/filelength shims in pstypes).
Both agents independently caught the VP 44-byte record vs 64-bit `time_t` trap (32-bit on disk).

Real retail bugs fixed in port: `Cfile_stack[8][128]` dimension swap (overflow), `cf_exist` FILE* leak, mapped-open leak; flagged-but-preserved: RLE decoder 127-byte overrun (cfilelist), `is_ext_in_list` first-dot/substring matching, `"*.*"` `cf_matches_spec` rejects everything (use `"*"`).
Post-agent fix: `strlwr(Pathtypes[].extensions)` wrote to string literals → segfault; loop removed.

**ORACLE**: `vp_ls` reads+CRCs 7,031 files / 2.5 GB from `gog/` VPs, 0 failures → `tests/oracle/vp-crc.txt`. `sexp_dump` now enumerates missions THROUGH cfile: 92 missions inside VPs parse; vocabulary = 133 ops (`tests/oracle/sexp-ops.txt`) = 129 SP + 4 multi-only (`depart-node-delay`, `exchange-cargo`, `is-disabled-delay`, `team-score`).

**FINDING**: loose `missions/` repo files are NOT pristine extractions — 2018 "trims" edited them (SM1-01: 58K loose vs 68,242 in VP; per-mission tree counts differ). VP run is the true retail baseline; `missions/` demoted to historical workbench.
Listing contract: `cf_get_file_list*` returns extensionless names; capture full names via `Get_file_list_filter` hook (`vp_ls` does).

## Point 3 (2026-07-17): DONE — data/media layer, pof_dump oracle green

Added to foundation: pcxutils, tgautils, anim/packunpack, palman, bmpman, model/modelread +modeloctant +modelinterp +modelcollide (interp needed: `model_load` calls `model_cache_init`).

**ORACLE**: `pof_dump` loads ALL 176 retail POFs via retail modelread (the only POF authority — no standard exists, per user), 0 failures; structure dump pinned `tests/oracle/pof-dump.txt` (4,135 lines: version/submodel tree/radii/bsp sizes/guns/docks/shield tris/mass).

pofer status: NOT complete (user); revived (Makefile `-lboost_system` dropped, rebuilt) — sweep: 152/176 clean, 24 warn unconsumed trailing data, 0 crashes. pofer is now the BENEFICIARY: complete it against `pof_dump` later, not vice versa.
Reference branches contain NO functional modelread fixes (mechanical sweeps only; fork base was 2018-fs2open modelread anyway). Fix-mine for symptoms = upstream 2002-2005 history.

64-bit fixes this round: `bitmap.data` uint→uintptr_t (+bmpman casts), `GR_SCREEN_PTR` macros uint→uintptr_t. Retail bugs: `tgautils.h` declared `targa_uncompress` with 4 params vs 5 (impl+caller) — header fixed; MSVC `unsigned char(x)` casts in bmpman.
NOT yet oracled: pcx/tga/anim pixel decode (`bm_lock` needs gr palette context — fold into point 4 first-pixels); `animplay.cpp` not yet compiled (gameseq deps).

## Point 4 (2026-07-17): DONE — FIRST PIXELS. SDL2 platform + software renderer live

4 agents:

1. tmapper family — 12 live asm routines → C w/ register-role names, bit formulas brute-force-verified vs asm emulation; 5 tiled files → forwarders over one generic impl (shift-parameterized); tmapgenericscans = ALL `#if 0` FS1 legacy, untouched. Found: 64x64 V:U transposition bug, 1-px-leftover garbage draw, texel-lag pipeline REPLICATED (pixel-visible).
2. `scaler.cpp` was a runtime x86 JIT (`compiled_code`) → 4 C spans; found 3-byte over-read past blend table (dword load vs byte comment).
3. font/shade/aaline/2d/tmapper small asm; found `font.cpp` brace balanced by a COMMENT under `!HARDWARE_ONLY` (retail shipped from different source?); CPUID via `__get_cpuid`; fistp → lrintf.
4. osapi/io on SDL2: window owned by osapi (`os_get_sdl_window`/`os_create_window`), SDLtoFS2 key table cribbed from `reference/2023`, SDL_QUIT → `gameseq_post_event(GS_EVENT_QUIT_GAME)`, registry → `~/.fs2/config`.

Me: `grsoft.cpp` DIB/GDI → malloc buffer + palette LUT + `grx_sdl_present` (SDL window surface); line/gradient windows.h; MulDiv compat (ROUNDS, `fix.cpp` uses it again); `2d.cpp` force-glide block removed (HARDWARE_ONLY forced Glide; software-only-for-tools Assert removed).

**DEBUG SAGA**: all-black frame → `palette_find` returned 0 for everything → palette_cache never cleared → `palette_update` early-return: Fletcher-255 checksum of grayscale ramp == 0 == initial state (3*32640 = 255*384). Pathological test palette, perturbed. Also `SDL_WINDOW_OPENGL` breaks dummy driver → flag dropped.

`first_pixels` test: gradient/rect/circle drawn through `gr_*` API, framebuffer self-checks PASS, PPM verified visually, presented on real display. ALL THREE ORACLES STILL MATCH BASELINES.
Watch items: signed z-compare in scaler (JIT jle, uint z); texel-lag replicated; `2d.h` dead `gr_string_win` decls; scaler dead `#else` branches drift; `cfile_init` CHDIRS to game root (tools' relative output lands in `gog/` — clean up any first_pixels.ppm there).

## Point 6 (2026-07-17): IN PROGRESS — THE GAME BOOTS, pilot select renders (`556dbd772`)

All 133 gameplay sources compile. `fs2` binary runs from `rundir/` (copy `build/fs2` there — symlink fails: `/proc/self/exe` resolves; VPs symlinked from `gog/`). Full table parse works, gamesequence pumps, pilot-select screen renders TEXT+UI (verified via `FS2_FRAME_DUMP=dir` PPM hook in grsoft, TEMPORARY — remove when stable).

Stub system: `gen_gamestubs.py` (scratchpad) two-tier: `tests/gamestubs_gen.cpp` (fs2+tests) + `tests/freespacestubs_gen.cpp` (tests-only, `freespace.cpp` symbols). QUIET_PREFIXES = sound/multi families return -1 silently; rest trap. Regen dance: build lib → probe undefined (fs2 first, then tests WITH gamestubs obj) → regen → rebuild. `windebug_memwatch_init` hand-quieted.

Runtime dragons slain: `bm_set_components` NULL + color guns zero (software mode now sets 1555 ARGB guns in `gr_soft_init`, cribbed from grglide init); `debris_vclip.name` char*→literal (strcpy target, MSVC writable literals); `localize.h` had a SECOND XSTR decl (char*) splitting the symbol; MSVC-tolerated duplicate globals (Buttons x3, Goal_text) staticized; `playermenu.cpp` software-palette code was COMMENTED IN RETAIL (hardware-only rot) — restored, pointed at background PCX palette (ChoosePilotPalette asset doesn't exist in VPs).

Gamesequence name tables DRIFT from numeric defines — log names lie; trust numbers (state 38 = INITIAL_PLAYER_SELECT).

**UPDATE (`047a05e2c`): PILOT SELECT FULLY RENDERS** — background, panel, buttons+icons, emblem. Revived: `gr8_bitmap_ex` + `gr8_aabitmap_ex` (`#if 0` in retail!), `common_set_interface_palette` body ("ugh - we don't need this anymore"), alphacolor.table union. HARDWARE_ONLY now UNDEFINED (build is the software renderer); lnaa8 pair = Int3 traps (asm never converted). `bm_lock`/`bm_convert_format` bpp policing widened for software mode. `common_set_interface_palette(NULL)`=`palette01.pcx` at `game_init` BEFORE interface art locks (`pal_changed=0` is hardcoded in `bm_lock` — pre-palette locks bake black FOREVER; order matters).

NEXT: create pilot (type callsign + ENTER on real display — dummy driver ignores input) → MAINHALL (expect new dragons: mainhall ANIs, music via silent stubs); then point 5 OpenAL (ds/ds3d/audiostr/acm over the neutered `ds.h` WAVEFORMATEX).
Note kept from bring-up: FS2_FRAME_DUMP + FS2 mask bitmaps lock at 8bpp now.

**UPDATE (`93edcdafd`): MAINHALL LIVE**, user plays it. Pilot create/remove/popups/door anims all work. Key systemic pieces: (1) 8bpp PCX locks translate via `bm_swizzle_8bit_for_fred` into CURRENT palette (masks exempt: 8bpp+BMP_AABITMAP = raw region ids); (2) `bm_lock` pal_changed restored (checksum-based re-bake on palette switch — popups switch palettes!); (3) anim translation restored end-to-end (`anim_set_palette` real table, ANF_XPARENT propagation, `unpack_pixel` 8bpp store, animplay bpp match). alphacolor pool 256. `keycontrol.cpp` in build. `netmisc_calc_checksum` real (stubs.cpp).

NEXT: user is entering mission load from Ready Room (briefing floats parse). Expect dragons in: briefing UI, ship/weapon select anims, then GAME_PLAY = 3D engine (tmapper scanlines' first real test, model render, HUD). Sound still silent stubs (point 5 OpenAL after).
Phenomenon glossary for captures: "16-as-8 static" = bpp mismatch; "speckled wrong colors, structure intact" = palette-space mismatch (stale translation).

**UPDATE (`14723ddf6`): THE MISSION RUNS.** User plays TSM-104 (campaign's first training-sim mission): starfield renders in 3D, pause overlay, targeting 'T' works, graceful exit. Fixed en route: InterfacePalette→palette01 fallback; `Net_player=&Net_players[0]` invariant in stubs (MY_NET_PLAYER_NUM arithmetic); software texture paging (no D3D preload, 8bpp); joy_ff no-ops; demo_/oo_/send_/process_ quiet; `netmisc_calc_checksum` real; `grx_tmapper` flag mask + degradation ladder (FS2 draw flags vs FS1 table); alphacolor value-dedup (FS2 HUD passes STACK color objects per frame; retail hardware never used this pool).

OPEN QUESTIONS: (1) ship controls in TSM-104 — probably RETAIL instructor-lockdown (voice files silent); decisive test = Ready Room Mission Simulator, non-training mission, arrows + 'A' throttle. (2) HUD not visible in mission — may be training-progressive; check in combat mission. (3) ship MODELS unverified in-mission (couldn't turn to look); tech room "weaponry door works somewhat" per user — capture wanted. THEN: point 5 OpenAL (sound is the biggest playability gap — instructor voice gates training progression by DURATION of silent stubs?), MVE cutscenes via ffmpeg, delete sweep (network/demo/etc call sites).
Debug kit: `ulimit -c unlimited` in rundir; `gdb ./fs2 core.N`; `FS2_FRAME_DUMP=dir` for PPMs.

**FINAL (session end)**: alphacolor tables palette-scoped (`palette_checksum` in struct). CAPTURE EVIDENCE: training message gauge RENDERS in mission ("DON'T TOUCH THE CONTROLS UNTIL YOU ARE TOLD TO DO SO") → controls/HUD state is SCRIPTED training gating, and lesson pacing hangs on silent voice durations. ⇒ POINT 5 (OpenAL) IS THE GAMEPLAY GATE. NEXT SESSION: build `sound/` on OpenAL (`ds.cpp`/ds3d/audiostr/acm over neutered `ds.h` WAVEFORMATEX; `snd_*` family replaces silent stubs in gen script QUIET_PREFIXES; remember to remove sound fams from quiet list + regen the two-tier stubs). Also verify: full HUD in a REAL combat mission (simulator room), ship models in-flight.

## Point 5 (2026-07-17): SOUND ON OPENAL BUILT — awaiting speaker test

Port manual: in-repo upstream history. 3 reference tiers extracted (`scratchpad/soundref*`): 2004 Great Linux Merge `bf40c2ebf` (WIP-grade: ushort sizes, broken timer), 2005 `c05391aa4`, and THE KEEPER `b287ecfef~1` (2010, pre-"new sound code" rewrite): mature OpenAL `ds.cpp` (USE_OPENAL ifdefs; flattened via `scratchpad/flatten.py`), `acm-openal.cpp`, `audiostr-openal.cpp`. 2018/2023 forks = same shape + ffmpeg (point-8 dependency, not needed for wav).

`code/sound/` now real in build:

- `ds.cpp` — OpenAL backend, wholesale rewrite behind retail `ds.h` API — retail channel-pool eviction logic kept verbatim; EAX stubs return -1; `Ds_use_ds3d=0` so `snd_play_3d` uses retail manual pan/attenuation = the classic retail sound path.
- `ds3d.cpp` — AL listener/source attrs; `ds3d_set_sound_cone` on AL cone.
- `acm.cpp` — self-contained MS-ADPCM decoder — WAVE_FORMAT_ADPCM=2 IS Microsoft ADPCM, not IMA; stream API for audiostr; 2 reference bugs fixed: max_dest_bytes overrun + frame straddling buffer end.
- `audiostr.cpp` — retail Timer/WaveFile/AudioStream classes on AL queue buffers, SDL_AddTimer 250ms service, local SDL_mutex locking — osapi CRITICAL_SECTION is a no-op, do NOT use for sound; retail comma-operator bug in `ACM_stream_open` call fixed; EOS fix: `readingdone && processed==queued`.
- `sound.cpp` — retail, 4 edits: includes, Sleep→os_sleep, MessageBox→nprintf, sound_get_ds.

`ds.h`: WAVEFORMATEX + WAVE_FORMAT_PCM/ADPCM + OpenAL_ErrorCheck/Print/C_ErrorCheck macros + `openal_error_string()`. `channel.h`: OpenAL channel struct (ALuint source_id, int buf_id).

**CONTRACT TRAP (caught)**: on-disk fmt-chunk header is 18 bytes (packed); our WAVEFORMATEX sizeof is 20 (unpacked). ADPCM coef table lives at byte 18 — `acm.cpp` and `audiostr.cpp` both use `WAVEFORMATEX_DISK_SIZE=18`, and `ds.cpp` parses the chunk field-by-field. NEVER append extra fmt bytes at sizeof(WAVEFORMATEX).
ADPCM decodes to 16-bit ONLY (decoder asserts dest_bps==16); `ds_load_buffer` passes 16 and sets bits=16 after convert (retail asked Windows ACM for 8 there — dead path now).

`osapi.cpp`: SDL_Init now VIDEO|TIMER (audiostr service callbacks need the timer subsystem). meson: openal dep on foundation + fs2 + all 5 test exes; 5 sound sources added.
`gen_gamestubs.py`: sound families REMOVED from QUIET_PREFIXES (left: rtvoice_/dscap_/midi_ + multi/standalone families); `windebug_memwatch_init` added to VOID_QUIET (regen had clobbered the hand-quieting → instant abort at boot; now survives regens). Two-tier regen re-run: only sound symbol still stubbed anywhere = `dscap_close` (multi voice capture, by design).

**VERIFIED**: full build green, meson tests pass, game boots in rundir: "OpenAL Soft 1.25.2" init clean, `sounds.tbl` parses, zero SOUND/OpenAL errors in 37k-line boot log, graceful exit. NOT YET VERIFIED (needs ears): actual audio out, instructor voice pacing un-gating the training mission, event music streaming, ADPCM decode correctness in anger.

**SPEAKER TEST PASSED**: button sounds, mainhall ambient, door hiss, briefing voice all ON; training chain advances on real voice durations (capture: mid-mission targeting lesson).

## Post-sound runtime fixes (2026-07-17 evening)

`oo_display` trap at mission start: regen had clobbered hand-quieted families → 'oo_','send_' restored to QUIET_PREFIXES (committed stubs were the source of truth: 40 symbols).

**BLACK BRIEFING ICONS + MISSING HUD (one root cause)**: `grx_set_color_fast` had `Current_alphacolor = &Alphacolors[...]` COMMENTED IN RETAIL (hardware-era rot, same class as commented palette loaders) → always NULL → `gr8_aabitmap_ex` RETURNS WITHOUT DRAWING (icons/gauges/reticle invisible) and font falls back to solid nearest-palette color (HUD green → black in interface palette). Restored; `colors.cpp`.

**FROZEN UNIVERSE** (distance-to-target never changed while thrusting): `obj_move_all` does `if(multi_oo_is_interp_object(objp)) multi_oo_interp(objp); else PHYSICS`. Quiet stub returned -1 = TRUTHY → every object took the no-op interp branch, `obj_move_call_physics` never ran. ⇒ `gen_gamestubs.py` grew ZERO_QUIET (predicates that must read FALSE in SP): `multi_oo_is_interp_object`, `multi_ts_is_locked`, `multi_ts_disabled_slot`/`_high_slot` (these 3 = "weapon select works somewhat"!), `multi_msg_eval_ship`/`wing_squadmsg`, `multi_msg_message_text`, `multi_show_ingame_ping`, `multi_message_should_broadcast`. NOTE `multi_can_message` stays -1 (truthy = "can message" is the correct SP reading at its unguarded sites). Lesson: quiet stubs feeding `if()` conditions need per-symbol semantics, -1 is not universally safe.

**JOYSTICK PORTED** (user has one, no numpad — retail steers on NUMPAD, arrows are shield augment!): `joy.cpp` rewritten on SDL2 from the 2004 `joy-unix.cpp` shape (`scratchpad/joy-unix-2004.cpp`); `joy_process()` pumped from `os_poll` (joy.h decl added); retail button bookkeeping (down_count/down_time/18Hz pollrate semantics), retail axis scaling/sensitivity/deadzone; `SDL_InitSubsystem(JOYSTICK)` inside `joy_init`; ff surface still no-op (SDL2 haptics later). CurrentJoystick registry key selects stick 0 by default.

RETEST 2 RESULTS: mouse + joystick + physics work. Ships/lasers/star-streaks STILL invisible (only engine glow + static backdrop) → led to the biggest find yet:

**THE INVISIBLE-3D-WORLD BUG**: `fl_round_2048` (`math/floating.h`) returned 0 FOR EVERY INPUT on x86_64. Retail's 2^52+2^51 magic-add trick did `float + float`: on x87 that ran in 80-bit registers so x survived; under SSE2 it's real 24-bit float math, x vanishes into the magic, low word reads 0. Every tmapper edge-walk y collapsed to 0 → zero scanlines → no ships/lasers/streaks. Engine glow lives because `g3_draw_bitmap` goes through gr_scaler, NOT the tmapper. FIX: `lrintf(x)` (= the round-to-nearest-even x87 actually computed). Users: `tmapper.cpp`, `modelinterp.cpp`. LESSON: suspect every x87 float-bit trick in retail; grep for type-punned magic constants when a subsystem "runs but draws nothing".

**NEW ORACLE**: `tests/tmap_test.cpp` — headless (SDL_VIDEODRIVER=dummy, run from rundir: `SDL_VIDEODRIVER=dummy ../fs2/build/tests/tmap_test .`): pushes `g3_draw_poly` (plain + modelinterp flag set), `g3_draw_laser`, `g3_draw_bitmap` through the software renderer, counts lit pixels + dominant palette index, dumps `tmap_test.ppm`. Reproduced the in-game symptom exactly before the fix; all 4 paths green after.

**SHARD ARTIFACTS** (post-fl_round fix): giant torn wedges w/ bright streak edges, worse when turning/firing. ROOT CAUSE: my point-6 scanline-fallback ladder degraded the SCANLINE but not the FLAGS → laser polys (TEXTURED|CORRECT|XPARENT) ran the perspective per-span setup (`fl_dudx_wide`) while the chosen linear scanline lnt8 read stale fx_u/fx_v from a previous poly and walked off its bitmap into arbitrary memory. FIX: fallback ladder now reassigns flags with the scanline (`tmapper.cpp`). USER CONFIRMED artifacts gone. LESSON: any table-dispatch fallback must keep the selector and the setup in the same degraded state.

`tmap_test` grew: 7 POF orientations (incl clipped close/edge/near), lit-path render with a real directional light, 60-laser storm. All green; model renders verified by eyeball (textured Ulysses, clean clip edges) — PPM/PNGs in scratchpad.

**VERIFIED IN-GAME by user**: sound (full stack), HUD, briefing colors, mouse+joystick flight, physics, ship models, weapon fire, no artifacts.
OPEN: target monitor content unconfirmed after fixes (probably fine now — same render path); star streaks unconfirmed; combat mission unflown.
NEXT: combat mission via simulator room; point 8 (MVE via ffmpeg); delete sweep; RLE overrun at pilot-file work; remove FS2_FRAME_DUMP hook when stable.

## Multiplayer/network excision (2026-07-17 night) — DONE, user-verified

Promoted from point 8 on user request. -69,480/+1,469 lines over 179 files; tree now 272,014 raw .cpp/.h lines (vs 404,132 on retail); sloccount ~178k SLOC. Zero multi_/psnet_/send_/oo_/std_/standalone symbols at link level; generated trap stubs shrank 1294 → 395 lines (only directx/glide/demo/Fred families remain = next sweep's map).

DELETED WHOLESALE: `code/network/` (30 modules, 55k), `code/inetfile/`, `menuui/optionsmenumulti.*`, `missionui/chatbox.*`, `hud/hudobserver.*` (multi-only files Volition filed by screen type, not feature). `GM_MULTIPLAYER`, `GM_STANDALONE_SERVER`, `Is_standalone`, `GS_STATE/EVENT_MULTI_*`/`STANDALONE_*`/`TEAM_SELECT`/`INGAME_PRE_JOIN`, `PF_NO_NETWORKING` no longer exist as identifiers (tripwire: nothing can creep back).

Method: 19 parallel agents on disjoint file clusters against a written excision contract (`scratchpad/EXCISION-CONTRACT.md`); coordinator swept the cross-file residue (signature flattening, wrong-home hoists, includes), then deleted the trees and let the build enumerate stragglers.

KEPT deliberately (format/vocabulary sacred):

- `.plr` byte layout: `multi_local_options`/`multi_server_options` structs inlined verbatim into `player.h` as inert fields; protocol slot still written (=1).
- Mission format: all `$multi` fields parse-and-discard; `"$multi_text"` / `"$end_multi_text"` are multi-LINE text tokens, NOT multiplayer (trap!).
- `campaign_types[]` "multi coop"/"multi teams" strings = recognize-and-skip vocabulary for multi .fc2 files in retail VPs.
- SEXP table complete at 212 ops; multi-op bodies return neutral constants.
- Control_config indices, button/mask tables, XSTR ids: entries stay, actions unlinked (multi buttons glow + fail-beep; mainhall multi door = no-op).
- `code/observer/` (151 lines): no net deps, retail says "possibly single player later" (fs2open used it for cameras); guards inert.

Wrong-home rescues (SP logic in net headers): `timestamp_elapsed_safe` → `timer.h`; `REPAIR_INFO_*` rearm codes (`multi.h`) → `ai.h`.
Signatures flattened to retail-SP shape: `message_send_builtin_to_player` 8→6 args (29 sites); `pause_init/do/close` lost `int multi`; `ssm_create` lost override; `hud_squadmsg` entry points lost player_num/objp; `parse_wing_create_ships` lost force/specific_instance; `button_function_critical` lost `net_player*`.

Behavior decision: `object.cpp` `obj_delete` OBJ_GHOST — retail's precedence bug (`!Game_mode & GM_MULTIPLAYER`) shipped the else = always delete ghosts, incl. SP. Kept SHIPPED behavior over authorial intent (agent had flipped it).

New retail bugs cataloged (all preserved): `shiphit.cpp` `is_subsys_destroyed` bare `false;`; `hudsquadmsg.cpp:2175` comma-op no-op filter + `:2171` TEAM_FRIENDLY vs player-team; `debrief_choose_badge_voice` missing return → `Campaign.missions[-1]`; missionparse default-ship fallback tests `[ptr->default_ship]` not `[i]`; missionparse ~1700 `!flags & DEBRIS_USED`; missiongoals ~730 `!flags & MGF_NO_MUSIC`; `-password` truncated game_name (deleted with block); `popup.cpp` `Game_mode && GM_MULTIPLAYER` (&&-for-&, resolved by excision).

Lessons: (1) uppercase vocabularies escape lowercase grep inventories — `GM_STANDALONE_SERVER` (8 files), `GS_STATE_MULTI` (contexthelp), `MULTI_MESSAGE_*` (controlsconfig), `PLAYER_SELECT_MODE_MULTI` (playermenu/barracks needed a 19th agent). Inventory by ALL the vocabularies, not one pattern. (2) zsh ls-alias bit AGAIN: `$(ls ...)` fed ANSI escapes to c++, probe "succeeded" with 0 undefined symbols. Use find/print in substitutions. (3) The link is the only honest census: agents' "only callers were in network/" claims were right ~90% — the build found the rest.

User-verified in game: multi buttons inert (glow + eh-eh), everything else plays as before; background nebulas/galaxies confirmed restored by the `fl_round_2048`/tmapper fixes (earlier session's renderer work).
NEXT: combat mission via simulator; then the remaining delete sweep (`directx/` `glide/` `vcodec/` `demo/` `exceptionhandler/` + gr D3D/DDraw/Glide backends + SideWinder FF) — `gamestubs_gen.cpp` IS the work list; MVE via ffmpeg; 4:3 HUD later.

## Dead-subsystem delete sweep (2026-07-18) — DONE, committed `5b2148062`, pushed

Sweep 2, per user: "everything and anything not currently in use or not related to Linux port". Same playbook: contract (scratchpad `SWEEP2-CONTRACT.md`) + 10 parallel agents on disjoint clusters + coordinator consolidation.

Numbers: raw .cpp/.h 272,014 → 221,614 (-50.4k); sloccount 178.5k → 145,915 SLOC. `gamestubs_gen.cpp` 238 → 23 lines: ONE stub left (`debug_console`, void-quiet). `freespacestubs_gen` regenerated (43 tests-only symbols, all freespace.cpp game glue). All 20 targets link, math test OK, tmap oracle all-green, headless boot+shutdown clean, binary deployed to rundir.

DELETED WHOLESALE: `directx/` (15.1k, D3D SDK headers+libs) `glide/` `vcodec/` `demo/` `exceptionhandler/` `scramble/` trees; graphics `grd3d*`/`grglide*`/`grdirectdraw*`/`gropengl*` + `tmapgenericscans.cpp` (zero refs); io `joy_ff.*`/`swff_lib.*` (SideWinder FF); sound dscap/rtvoice/midifile/midiseq/winmidi*/rbaudio; `globalincs/windebug.cpp` + `osapi/outwnd.cpp` (`globalincs/debug.cpp` is the port's replacement for both; `outwnd.h` kept, debug.cpp implements it).

KEPT deliberately: `debugconsole/` (fully portable in-game console — gr_/key/osapi only; DCF machinery lives in pstypes.h, ctor no-op'd in tests/stubs.cpp, port it later); `cryptstring/` (jcrypt = retail cheat codes); cfilearchiver (live build target); `-window` flag (parm renamed d3d_window → window_arg); gropengl DELETED (embryonic; recover from retail, master has the real one).

Call-site families: (A) demo recorder — GM_DEMO_RECORD/PLAYBACK guards false, demo_POST_* hooks deleted, GM_DEMO defines gone from systemvars.h. freespace.cpp's demo blocks were LIVE code (demo.h defined DEMO_SYSTEM). (B) renderer modes — GR_DIRECT3D/GR_GLIDE/GR_OPENGL/GR_DIRECTDRAW defines deleted from 2d.h (tripwire); `gr_screen.mode` hard-set GR_SOFTWARE in gr_init (mode param vestigial); 2d.cpp 904→611; DCF(gr) deleted whole; `D3D_enabled` + `Cmdline_force_32bit` (-32bit) gone. (C) joy_ff_* — 18 no-op stubs lived at the END of io/joy.cpp (port-era), whole surface deleted with ~25 call sites across 10 files. (D) dead sound — dscap_close from snd_close, winmidi.h include from hudmessage.

Cascades found by agents: `shield.cpp` high-detail per-triangle explosion path was D3D-exclusive (6 whole functions deleted; software retail ALWAYS ran low-detail — behavior identical). `neb.cpp` fog machinery collapsed to one table (retail read the _glide fields unconditionally even under D3D; _d3d twins were dead in retail). starfield subspace inner-tunnel second model_render pass was D3D-only. TotalRam/Model_ram/pm->ram_used chain deleted everywhere (windebug's malloc counter; constant 0 on Linux).

Retail-vocabulary traps this sweep: THREE meanings of "demo" — (1) demo recorder (excised), (2) FS2_DEMO/DEMO shareware-build ifdefs (KEPT), (3) Cheat_code_demo retail cheat string (KEPT). "Glide" in movement comments is not the API.

Port-necessary deviations (documented in-place): neb2 software fog arm was Assert(Fred_running) / Int3() in retail (software mode was FRED-only); now feeds real fog values. keycontrol `button_function_demo_valid` renamed `button_function_always` (agent renamed it to `button_function_critical` which COLLIDED with the real retail function of that name — caught via clangd, fixed by coordinator; "no external references" agent claims need the link).

Retail bugs noticed, NOT fixed: `starfield.cpp:774-777` updates subspace_offset_u_inner but wrap-checks subspace_offset_u (copy-paste); modelinterp identical-branch D3D if/else (collapsed, no behavior change); `bm_set_components_argb_d3d_32_tex` wrote ushort not 32-bit (deleted anyway).

Orphans deleted by coordinator: GS_EVENT_TOGGLE_FULLSCREEN/_GLIDE defines (gamesequence.cpp string tables untouched — position-indexed); d3d_test() husk in mainhallmenu; bmpman.h d3d decls + BM_PIXEL_FORMAT_D3D twins; windebug_memwatch_init call+decl; rbaudio.h include in keycontrol, exceptionhandler.h include in freespace.

NEXT: user playtest; combat mission via simulator; MVE via ffmpeg; port debugconsole/ someday; 4:3 HUD later.

## Upstream (26.0.0) tree survey — kept for reference; master is the fix-mine

### Top level

```
code/            engine core, 81 subsystems (fork had ~70 under src/)
freespace2/      the game executable proper (entry point, main loop) — KEEP
fred2/           original MFC FRED2 mission editor, Windows-only (2.6M) — FALLS OFF
qtfred/          Qt cross-platform FRED rewrite (5.6M) — FALLS OFF
wxfred2/         abandoned wxWidgets FRED attempt, dead upstream too (1.5M) — FALLS OFF
lib/             vendored third-party sources + cmake finders for system libs
parsers/         ANTLR4 grammars + pre-generated C++ parsers (see below)
tools/           embedfile = build step embedding code/def_files into the binary — KEEP
                 strings_tool = translation table helper
test/            googletest unit tests — KEEP (safety net the 2018 fork never had)
ci/ cmake/ .github/ .circleci/ coverity/   build + CI plumbing
documentation/ scripts/                    docs, perf-graph scripts
```

### parsers/ (the ANTLR pair)

```
parsers/action_expression/ActionExpression.g4  -> consumed by code/actions
parsers/arg_parser/ArgumentList.g4             -> consumed by code/scripting (doc_parser.cpp, ade.cpp — Lua API doc generation)
```

`lib/antlr4-cpp-runtime` backs both. arg_parser falls off with Lua; action_expression only if code/actions goes.

### lib/ by fate

```
KEEP:      SDL2, OpenAL, FFmpeg (cutscenes/audio), freetype, libpng, libjpeg, zlib,
           lz4 (compressed VPs), utfcpp, opengl, accidental-noise (procedural/volumetric nebulae)
NETWORK:   mongoose (standalone-server web UI), mdns, libpcpnatpmp — FALL OFF
LUA-TIED:  lua, libRocket (backs code/scpui) — fall off with scripting
AUDIT:     jansson (scripting docs/options/multi — find remaining users after multi+lua gone),
           md5 (multi checksums + table hashes), hidapi (raw HID controllers vs SDL),
           imgui (lab + debug UI only), openxr (VR), discord (rich presence), vulkan (renderer WIP)
```

### code/ newcomers since the 2018 fork point

```
actions/ + executor/ + expression   mini-language ("on spawn/impact" programs) for weapons/animations;
                                    the ANTLR ActionExpression feeds this. Retail campaign predates it.
scripting/ (2.6M)                   Lua API (ade) — far deeper than 2018; removal is bigger surgery now
scpui/                              libRocket+Lua UI layer for mods; retail UI is still native menuui/missionui
missioneditor/                      editor-shared logic (missionsave, sexp_tree models) split out for
                                    fred2/qtfred — FALLS OFF with the editors
options/                            new in-game options framework
def_files/                          embedded default tables/shaders (paired with tools/embedfile)
ktxutils/                           KTX texture loading
prop/                               brand-new decorative-object system
cheats_table/, headtracking/        cheats tables; TrackIR head tracking — headtracking FALLS OFF
libs/                               thin wrappers over lib/ (ffmpeg, discord, renderdoc, antlr, jansson)
```

### code/ that falls off with the goals

```
network/ (2.2M)     all multiplayer — plus tendrils in pilotfile/stats (multi ranks)
inetfile/           HTTP/FTP fetch (PXO, validation) — networking sidecar
exceptionhandler/   Windows SEH crash dumps
external_dll/       Windows DLL-loading shim
windows_stub/       POSIX stubs for Win APIs — CAUTION: parts of this make the Linux build work today;
                    simplify after de-Windows-ing, don't delete first
voice_rec.cmake     Windows voice recognition (already optional)
```

### Cautions

- "non-SEXP" can only mean the 2023 trim (retail operator set, cf. `missions/sexp-used.txt`), not removal: the retail campaign IS SEXP-scripted; evaluator lives in `code/parse`.
- Lua removal now kills scpui + libRocket + arg_parser too; retail UI survives natively.
- Build first, strip second: baseline cmake build on Arch before replaying anything.

## Software renderer at 1024x768 (2026-07-18) — DONE, committed `b150a1d09`, pushed

Two-line policy change, not a renderer change (rasterizer is resolution-parametric; FRED ran it at window size):

- `code/graphics/grsoft.cpp`: deleted retail gate in `gr_soft_init` (`Assert(res == GR_640)` + force-back block)
- `code/freespace2/freespace.cpp:1574`: `gr_init(has_sparky_hi ? GR_1024 : GR_640, GR_SOFTWARE, 8)` — hi-res iff `sparky_hi_fs2.vp` present (retail's own condition for hardware modes)

Verified: headless boot clean exit-0; FS2_FRAME_DUMP PPMs are 1024x768; pilot-select renders correct hi-res art/layout. Likely first-ever hi-res game UI on the retail software renderer (Volition ran software-at-1024 only in FRED). Retail-untested combos to watch in playtest: hi-res fonts/aabitmaps (tech room, briefings), popups (`gr8_save/restore_screen` — code parametric, never exercised at this size), in-mission HUD gauges. Only remaining 640 hardcode: retail debug frame dump (`gr8_dump_frame_start`), unreachable behind stubbed debug_console.

## OpenGL backend session 1: skeleton (2026-07-18) — DONE, boots on real GL

The find that shaped it: retail's own `gropengl.cpp` (deleted in the sweep, alive on `retail`) is a pure skeleton Volition left behind — `gr_opengl_*` stubs + the vtable binding block, zero GL calls. Revived it instead of writing fresh.

What landed:

- `code/graphics/gropengl.{cpp,h}` restored from `retail`, adapted: SDL2 GL context (attrs before window; `SDL_GL_CreateContext` in `gr_opengl_init`), flip = FS2_FRAME_DUMP glReadPixels (back buffer, pre-swap, row-flipped) + `SDL_GL_SwapWindow` + post-swap clear (kills double-buffer strobe — the two back buffers alternate stale contents until the game draws everything). Full 46-entry vtable bound (retail's skeleton only bound ~30; `gr_init` memsets gr_screen, unbound = NULL-call crash). `gf_set_font` = `grx_set_font` (generic, font.cpp — a no-op there means NULL Current_font at first string-size). `init_alphacolor` = D3D shape (real rgba on color struct). Hardware mode is 16bpp to the game: 1555 guns like grsoft's aux block. Local zbuffer-mode trio (like D3D had), `save_screen` returns -1 (popups cope).
- `2d.h`: GR_OPENGL restored (retail's 104). `2d.cpp`: mode dispatch at the six `gr_soft_*` seams (init/cleanup x2/force_windowed/activate) + `gr_bitmap(_ex)`.
- cmdline `-opengl` (software stays default), freespace.cpp picks mode+depth, `os_create_window(w,h,use_opengl=0)` adds SDL_WINDOW_OPENGL only on request, meson: gl dep (foundation + fs2 + all tests — link_with doesn't propagate).

CRASH FIXED en route: `gr_bitmap`/`gr_bitmap_ex` in 2d.cpp are NOT vtable entries (`gf_bitmap` commented out in the screen struct); the sweep flattened retail's mode dispatch to bare `grx_bitmap` → software blit into a framebuffer that doesn't exist in GL mode → memcpy(NULL) at `bitblt.cpp:322` the moment pilot select drew its background. Retail's 2d.cpp had the switch; restored (minus dead backends). LESSON for the fill-in phase: the sweep's flattenings are invisible until GL mode exercises them — the retail branch is the oracle for every "who dispatched here" question.

Verified: GL 4.6 compat context on Mesa (zink/NVK, RTX 2060 SUPER); boots to pilot select at 1024x768, ~29fps with the pixel-loop bitmap fallback, frame dump = pure black 1024x768 (mean 0 — clear+readback good, drawing all no-ops yet, correct for skeleton); clean SDL_QUIT shutdown path (exit 0); software renderer regression boot clean (dummy driver, exit 124 = alive at kill).

References parked in scratchpad: `retail-grd3d*.cpp` (the vtable contract, 6.7k lines), `fs2open-2002-gropengl.cpp` (3.3k lines, the fill-in manual), `retail-gropengl.cpp` (what we started from).

NEXT (session 2 = menus render): tcache (`grd3dtexture.cpp` transcription, palettized→RGBA upload) + tmapper + textured-quad bitmap/aabitmap + ortho projection + blend modes. Then fonts land free via aabitmap. Mouse cursor draw in GL flip (software drew it into its buffer; GL flip must draw a quad).

## OpenGL backend session 2: menus render (2026-07-18) — DONE, oracle-matched

`gropengl.cpp` grew from skeleton to working 2D renderer (~1500 lines) by transcribing the 2002 fs2open GL backend (scratchpad `fs2open-2002-gropengl.cpp`) onto our SDL2 context. Landed: full tcache (per-handle GL texture, signature short-circuit, AABITMAP/NORMAL/XPARENT/SECTION types, 1555 BGRA uploads, `Detail.hardware_textures` divisor, frame aging + vram flush), tmapper with blend/zbuffer state machine (`set_state`), textured-quad bitmap path, aabitmap + string (per-glyph quads from font atlas), line/gradient/circle/rect/shade/flash, scissor clip, fog (EXP2), zbuffer trio on the shared grzbuffer globals, save/restore_screen via glReadPixels+bm_create, ortho top-left projection, 565 fb / 1555 tex / 4444 ta color guns (the 2002 contract for bmpman). Section machinery kept but `tcache_init(0)` — nothing in this tree passes TMAP_FLAG_BITMAP_SECTION (`g3_draw_2d_poly_bitmap` has zero callers).

THREE BUGS found+fixed during bring-up (each verified by frame dump):

1. Green tint on all art: `bitmap_internal` locked with flags=0 → bmpman converted through the 565 FRAMEBUFFER guns, but upload said 1555. Lock with BMP_TEX_XPARENT (1555 texture guns + green-key transparency — also fixed the cursor's opaque green box).
2. Missing text (2002 wart): `aabitmap_ex_internal` early-returned for non-alphacolors, killing black drop-shadow/selected text. Now draws alpha 255 for plain colors.
3. THE SUBTLE ONE — solid-white text blocks: tcache's `GL_last_bitmap_id` fast path skips glBindTexture, but `bitmap_internal` binds+deletes its own throwaway texture without invalidating the cache → next cached font draw sampled a DELETED texture → incomplete → fixed-function drew raw glColor quads. One line: `GL_last_bitmap_id = -1` after the throwaway delete. Diagnosed by instrumenting aabitmap calls (glyphs+uv were perfect; only draws AFTER a gr_bitmap went solid). LESSON: any GL state touched outside the cache's view must invalidate the cache — grep for glBindTexture when adding paths.

Also: font upload switched GL_LUMINANCE_ALPHA → GL_RGBA8 (legacy-format safety; dropped 2002's dead `xlat[15]=xlat[1]` quirk — font.cpp clamps to 14).

**VERIFIED**: pilot select at 1024 GL is pixel-comparable to the software oracle (`gldump/{gl,sw}-final.png` in scratchpad): art tones identical, MAVERICK + Single/Multi + copyright text, transparent ani cursor with hover highlight. ~29fps debug build. Software renderer regression clean.

UNTESTED under GL (next playtest): mainhall (ani playback via bitmap path), barracks/techroom, popups (save/restore_screen readback), briefing, and the big one — in-mission 3D (tmapper flags beyond TEXTURED, fog/nebula, glowing effects additive blend). `Detail.hardware_textures` divisor shrinks 3D textures below detail 4 (retail behavior, keep in mind when judging blur).

## Wrinkle sweep (playtest round 3, software renderer)

**FIXED: mainhall "New user tip" popup displayed "null".**
Root cause was not the tips at all: `math/floating.cpp` `frand()` divided by `(RAND_MAX + 1)`. MSVC RAND_MAX is 0x7fff; glibc's is INT_MAX, so the sum overflowed to INT_MIN and every frand() returned a value in (-1, 0]. `frand_range(0, Num_player_tips-1)` went negative → `Player_tips[negative]` → zeroed BSS → "(null)". Fix: mask `myrand()` to 15 bits and divide by 0x8000 — exactly the MSVC numerics retail was tuned against, strictly < 1.0. (The 2004 Linux merge rejected RAND_MAX then divided by RAND_MAX, which still rounds to exactly 1.0f in float for the top ~64 values — our fix avoids that class entirely.)

Blast radius of the original bug — everything downstream of frand(): `rand_chance()` was ALWAYS true (negative < positive frametime), so every per-frame chance event fired constantly; `frand_range(-x,+x)` was biased to [-3x,-x); random sound/tip/index picks read below their arrays. Suspect any "too busy / too uniform" effect seen before this point. Verified post-fix in gdb: eight frand() draws in (0,1), tip indexes 4/18/4.

NOT a bug: `starfield.cpp:986` `f2fl(myrand()-RAND_MAX/2)` debris positions — scale differs from MSVC but the vector is normalized on the next line; uniform-cube direction either way. Census: all other RAND_MAX uses are ratio/comparison forms that scale correctly with glibc.

LESSON (adds to the x87/SSE2 entry): retail arithmetic that embeds MSVC platform constants (RAND_MAX, 15-bit rand granularity) breaks silently on glibc. grep for RAND_MAX when a subsystem misbehaves randomly — and "randomly" may mean "always", since overflow made the RNG deterministic-ly wrong, not noisy.

**FIXED: command brief screen "blued out" (blue mask look, software).**
Not a renderer bug. `cmd_brief_new_stage` (`missioncmdbrief.cpp`) memcpys the stage ani's palette into the FIRST 128 entries of a static Palette[768] and calls gr_set_palette with the whole thing. Retail meant a base palette file to fill the rest — but that load (BarracksPalette //CommandBriefPalette) is commented out in retail, and NEITHER file exists in the shipped VPs. So entries 128-255 were zeros: screen palette = ani blues/greens + 128 blacks, background art remapped to nearest → blue wash. The cb ani itself rendered correctly (its own entries survived), which was the tell in the screenshot.
Fix: as with pilot select, the background PCX (`2_CommandBrief.pcx`) carries the screen's palette — `bm_get_palette(Background_bitmap)` + gr_set_palette in `cmd_brief_init`, stage ani overlay unchanged.
Census while here: NONE of the 8 named interface palettes (InterfacePalette, BriefingPalette, ShipPalette, WeaponPalette, DebriefPalette, TechDataPalette, ChoosePilotPalette, BarracksPalette) exist in retail data. Screens using `common_set_interface_palette` already fall back to palette01 (earlier port fix); techmenu's direct load is commented out in retail; cmd brief was the last live victim. Retail Windows never noticed because hardware modes convert bitmaps through their OWN palettes — the screen palette only matters at 8bpp software. LESSON: "palette weirdness on one screen" = check who set gr_set_palette last and from what source; the named palette files are all vapor.

**FIXED (likely): in-mission posterized/"dithered" nebula + HUD arc gauges letting background through (software).**
Root cause is retail rot, same class as the interface palettes: `game_load_palette()` exists but its `palette_load_table()` call is commented out IN RETAIL, and all three call sites are commented out too — post-1.0 retail required hardware accel, where the game palette is irrelevant. So missions rendered under whatever palette the last interface screen set (palette01): every texture, the nebula art, and the HUD alphacolor blend tables remapped through an interface palette.
Fix: revived the call at mission load (freespace.cpp, before `game_post_level_init` so HUD_init rebuilds alphacolors against it), loading `gamepalette1-01` — the ONLY member of retail's intended palette family (gamepalette{1..3}-{01..99} + gamepalette-subspace) that shipped in the VPs. hudconfig.cpp's commented reload sites left as-is (the per-hud-color variants they'd want don't exist).
Verified: build clean, tmap oracle passes, headless boot normal. Some nebula dither at 8bpp is authentic retail software behavior — judge post-fix appearance before hunting further.

**FIXED: screen-wide "palette flash" wash** (the garish pastel flashes during sun glare / damage / EMP, software). `grx_flash` implements gf_flash by REWRITING THE DISPLAY PALETTE (all 256 entries +rgb, per-channel clamp → hue shifts). Retail intended it config-gated: the os_config_read of "PaletteFlash" (default 0!) is commented out in retail and the global initialized to 1, so software always flashed; hardware modes draw gf_flash as a subtle additive overlay, so retail never saw it. Revived retail's own commented config line (`freespace.cpp:1527`). Proven by frame forensics: during "garbled" frames Soft_palette diverged from gr_palette by exactly +71/+71/+71 = Sun_spot*128 sun glare.
This ONE cause explained: garbled target box during narration flashes, "reduced-color nebula with no reds" (whole screen washed while facing the sun), red background flashes (damage flash is +r,-g,-b).

**DIAGNOSED, left open**: after the palette-flash fix, a residual artifact remains — the sun-glow billboard (`stars_draw_sun_glow`, additive 0.5 blend) renders through the TRANSPARENT interior of the target monitor and gauge gaps. Forensics: interior pixels during "garble" are raw scene indexes (nebula/glow); the monitor interior is transparent BY DESIGN (its black is empty space behind the HUD); the blend table, alphacolors, and palette_find are all verified correct (bright green HUD table can only emit green-ramp indexes — measured, confirmed). Open question: the glow pulses ~0.2s on/off, too rhythmic for the instructor occluding the sun ray — suspect `Sun_drew` or `shipfx_eye_in_shadow` flickering per frame. NEXT: instrument Sun_drew / eye_in_shadow, compare the same scene under the GL backend (proper additive glow), and check what retail software 640 looked like.
Also reported: afterburner gauge "bands of green and dark when full" — probably the authentic aabitmap luminance modulation; verify later.

Debug hooks added this session (all env-gated, delete when stable): `FS2_NO_PAUSE=1` (game_poll: don't pause on focus loss — driven test sessions), `FS2_FRAME_DUMP_STRIDE=n` (frame dump every nth frame), and the frame dump now also writes .pgm raw index buffers + .pal files (Soft_palette then gr_palette) alongside the .ppm — that pair is what cracked the palette-flash case.

METHOD note: full playtest loop is now automatable: xdotool drives (clicks need ~150ms spacing and a settled cursor; UI bracket buttons often need keyboard — Enter acts as click-on-hovered-region in the mainhall, popup hotkeys work, ctrl must be held across the Return for briefing commit), `import -window` captures, FS2_NO_PAUSE keeps the mission clock honest, frame dumps + magick crop stats find anomalous frames, and gdb batch scripts verify runtime state headlessly. Alt+J reaches the game via `xdotool key --window` even when the WM grabs it. The tips popup accepts Enter; pilot select needs click+Up+Enter.

**VERDICT (energy banks "red seep" + afterburner "bands"): AUTHENTIC 8bpp software behavior, not a port bug.** Proven from the art itself — wrote a python ANI parser (format from anim_read_header/unpack_frame; gotcha: RLE run count is stored MINUS ONE, ++count before stuffing) and decoded `2_energy2`/`2_leftarc` alpha masks: for aabitmaps the ANI pixel index IS the alpha level (`unpack_pixel` "don't run through the palette"). The full-state energy swoosh is an alpha GRADIENT (2→14) with tick lines, and everything else alpha 0; the arc fill is alpha 14 with 1-6 outlines and 0 gaps. In the 8bpp blend model: alpha-0 texels are identity (`lookup[0][i]=i` — raw scene shows through, so red nebula "seeps" exactly where the art is transparent); drawn texels emit ONLY gauge-hue palette-ramp entries (green table cannot produce red — verified numerically), modulated by background intensity, and a smooth alpha gradient quantizes through 16 rows x ~13 greens = BANDS. The art was authored for hardware alpha blending; retail 1999 software mode looked exactly like this. Polish path = the GL backend, not software fixes.

Automation notes: synthetic (XTEST) key HOLDS do not register with the continuous-control path (`key_down_timef`) — ship refuses to turn from xdotool keydown; discrete presses (targeting, commit, Alt+J) work. Targeted `--window` events only work while the window has X focus (SDL drops SendEvent keys); windowactivate first. Frame dumps at stride 2 filled 40GB of tmpfs in ~30min of play — rm frames/ after each burst; the dump silently stops when tmpfs fills (fopen fails). import(1) returns stale window contents when the window is unmapped/other workspace — use the frame dumps as eyes instead.

**FIXED: software output darker than 1999 retail screenshots.** Two-part explanation of the reference JPEGs (user-provided): (a) the in-mission shots are HARDWARE-renderer captures by definition — 1024 was hardware-only in retail, and the smooth alpha gradients are impossible at 8bpp; that gap is the GL backend's job. (b) The real difference: gamma. Boot reads Gamma from config (user: 1.95) and `gr8_set_gamma` builds `Gr_gamma_lookup` — but the block applying it to the display palette is commented out IN RETAIL (same rot pattern as the palette family). fs2open-era grsoft (checked via master history) carried the same commented block until the file was deleted — nothing to cherry-pick. Revived as a display-side ramp: `grx_change_palette` applies Gr_gamma_lookup, `grx_set_palette_internal` routes through it, `gr8_set_gamma` re-pushes on change, `gr_soft_init` builds the table before the first palette write. Logic palette + all derived tables stay linear (exactly a hardware gamma ramp). Mainhall mean brightness 9 → 81 at gamma 1.95; blacks stay black. Gamma is tunable live in the F2 options screen if 1.95 reads washed on a modern panel.

## OpenGL backend session 3: first GL playtest wrinkles (all fixed)

User-piloted GL playtest (menus + training mission). Worked out of the box: 3D scene (models, smooth nebula, lasers), targeting, text, fonts, briefings. Three bugs, all fixed and user-verified:

1. Menu/campaign-room anis rendered green/purple garbage. TWO stacked causes in the streamed-anim path (animplay/packunpack):
   a) `Gr_bitmap_poly` was never set: retail's own switch for "bitmaps are textured polys" (its 32-bit D3D mode) selects TEX-format guns before the 16bpp unpack; unset, frames packed 565 and uploaded as 1555. `gr_opengl_init` now sets it.
   b) `anim_set_palette`: the earlier port fix that restored the REAL palette translation (which 8bpp software needs) broke hardware: the 16bpp unpack looks the translated index up in the ani's OWN palette and keys transparency by RGB, so game-palette indexes scrambled every color. Translation is now mode-aware: identity at 16bpp (retail hardware behavior), real mapping + 255-marker at 8bpp. LESSON: a port fix for one renderer can be a regression for the other — the translation table has two different consumers with opposite contracts.
   This also fixed the in-mission "solid green rectangle" gauges — shield icons etc. stream through `hud_anim_render`, same path.
2. Popup backgrounds black (pilot-select tip, quit dialog). `gr_opengl_save_screen` read GL_FRONT, which is undefined under a compositor (doubly so through XWayland). Now: every flip stashes the finished back buffer GPU-side (glCopyTexSubImage2D into a scratch texture, ~free), save_screen glGetTexImage's the stash (frozen while a popup owns it), restore draws it. LESSON: never read GL_FRONT on a composited desktop.
3. GL frame dump now honors FS2_FRAME_DUMP_STRIDE like the software hook.

Still on the GL polish list: nothing known. Software renderer: DONE (gamma'd, palette-correct, authentic-8bpp look; remaining sun-glow pulse question applies to software only and GL renders glare properly).

## fs2open FIX-MINE — low-hanging retail bugs (2026-07-18 cont.)

Mined fs2open branch 2002-07..2005-12 (~2525 commits, three parallel sweeps) for fixes to bugs that exist in the Volition import, in subsystems we kept, still live on master @ `5fd0b29fb`. Full annotated list with anchors: `public/fixmine.txt` (compilation-mode).

Headline: 11 Tier-1 picks — campaign-affecting, tiny, unambiguous:

1. multi-ship sexps stop at first ship (protect-ship & friends — ALL used by campaign) — `sexp.cpp:5311` etc.
2. never-warp copy-paste (both arms WARP_BROKEN) — `sexp.cpp:4961`
3. `#token#` inverted test → NULL strncpy stack smash (training messages) — `missiontraining.cpp:591`
4. `Personas[-1]` OOB read (wrong talking head) — `missionmessage.cpp:805`
5. `brief_compact_stages` stale tail → double-free — `missionbrief.cpp:743`
6. zero-stage briefing shows previous mission's text — `missionbrief.cpp:1063`
7. player-entry-delay carries across missions — `missionparse.cpp:457`
8. dangling temp_name in debris target box (ship-copy '#') — `hudtargetbox.cpp:1159`
9. `physics_apply_whack` ignores mass param (docked over-whack) — `physics.cpp:738`
10. dying ships with dead engines keep firing (player deathroll) — `aicode.cpp:10717`
11. empty secondary bank's fire_wait leaks into next bank — `ship.cpp:4961`

Tier 2 (real, low/latent impact): simulator-room debrief music (relevant to our NEXT list!), is-iff infinite-loop hang (campaign never calls it), damage-popup >12-subsystem overflow, support-without-rearm-dockpoint CTD, squad_filename self-strncpy UB, barracks stat overflow, weapon-energy cheat persists, shieldless-Q sound, is-tagged misses TAG-B.

FLAGGED never-port: rand re-roll (`77db45827`) — retail caches the rand sexp result and missions are tuned to that; SCP later added rand-multiple as a SEPARATE op for a reason.

FAMOUS-BUT-BOGUS (fs2open regressions, master already correct): hud_config uninit pointer, "insidious beam bug" (Bobboau shield_factor), warp-always-knossos, initial-status hull/shield, ai_select_primary null deref, pilot-select cancel corruption. LESSON: a large share of early-SCP "bugfixes" repair SCP's own additions; always check the touched code exists in retail before crediting it as a Volition bug.

Method note: verified campaign relevance against `missions/sexp-used.txt` (is-iff/is-tagged/order are NOT used by the retail campaign; protect-ship/never-warp/invulnerable/guardian ARE). Stock weapons.tbl never sets `$Weapon Range` → that fs2open fix is a no-op for retail data.

## fix-mine APPLIED — 20 commits, `5fd0b29fb`..`b287e865b` (2026-07-18 cont.)

All Tier-1 + Tier-2 items from fixmine.txt landed, one bug per commit, each citing its fs2open provenance hash. Every commit build-verified (meson, all targets); binary copied to rundir/fs2. NOT pushed, NOT playtested (user: "no test runs").

Deviations/notes discovered while applying:

- ships_visible/invulnerable/guardian already had for+continue in retail; only protect/beam-protect had the while/return form. All five had the not-yet-arrived `break`.
- brief_render zero-stage bug is real despite the `Num_brief_stages<=0` early-out: `brief_init` can set `Num_brief_stages = num_stages + 1`.
- physics.cpp has a second `1.0f / pi->mass` in `physics_collide_whack` — that one is CORRECT (no mass param); only apply_whack changed.
- secondary-bank fix follows DTP: the new bank keeps its own remaining delay, 250ms minimum enforced only if its stamp already elapsed.
- barracks: took Goober's sizing (21 + MAX_SHIP_TYPES) AND the release-safe break.
- debrief music: third `next_mission==current_mission` site (~:864) was already correctly gated by GM_CAMPAIGN_MODE in retail — untouched.
- `Weapon_energy_cheat` reset placed in `debrief_accept` (per fs2open).

Still unapplied (flagged in fixmine.txt): rand re-roll (never), order-sexp wiring, talking-head anim_free contract change, anim corrupt-file robustness, weapon_range AI capping (stock no-op), cfile buffer refactor, `1a70dcbd4` exited-ship crash.

NEXT here: playtest sweep to sanity the 20 fixes (esp. secondary-bank cycling feel, briefing screens, talking heads), then push on request.

## POF oracle, steps 1-3 (2026-07-21) — machinery DONE, one decision open

Four-step plan, agreed in this order:

1. carve libpof out of pofview into its own repo — **DONE**
2. fs2-side POF dump from retail's loader, made canonical — **DONE**
3. libpof dump in the same format + differential test — **MACHINERY DONE**
4. finish pofer against the oracle — **NOT STARTED**

The oracle is retail's own loader, driven by `tests/pof_dump.cpp` inside the fs2 build — modelread is NOT carved out, the game already builds so it just loads and dumps. libpof (PCS2-derived) is the thing under test. pofer stays independent as a third opinion and is finished last.

Full analysis with file:line anchors: `public/pof-oracle-findings.txt`

Step 1 — libpof is now its own repo (codeberg thinkoid/libpof, odroid backup created). It is a CLONE of pofview's repo, so all 331 commits and the PCS2 lineage survive: git log on any surviving file still reaches the 2010 checkin. pofview lost the library, its three tests and PCS2's 35 wx editor icons, and pulls libpof through `subprojects/libpof.wrap`. Verified by fresh-cloning pofview and letting the wrap fetch from codeberg.

Step 2 — the dump already existed (past-me built it; its header comment already named this exact plan). What was missing was "canonical": the model-name header was whatever cfile reported, so `gog/` gave BeamSaber.POF and `rundir/` gave beamsaber.pof, and since the list sorts by name the whole file reordered. 1597-line diff, pure reordering, no regression — it reproduced byte-exact from gog/. Folded the name for display and sort. Then pinned three surfaces and wired them into meson test as `pof-oracle`: summary of all 176 models, --full for 8 models (753K, diffable), sha256 of --full over the whole corpus. The 8 were picked on measured coverage — flat polys survive in only 6 of 176 models, so two flat-heavy ones are in deliberately. Verified the test can actually fail (77 skip, corrupt checksum, one-digit vertex change). Under a second.

Step 3 — both sides now emit a canonical geometry projection (`--geom` on the fs2 side, `pof-dump` on the libpof side): polygons resolved to plain coordinates, tree dropped, lines sorted. Sorting is required because the two readers visit BSP branches in different orders (retail pre/back/on/front/post, libpof front/back/pre/post/on) — same polygons, different sequence. libpof's parse-time X mirror is undone on the way out, and signed zero is folded on both sides.

FOUND TWO REAL libpof DEFECTS:

**FIXED** — LoadPOF ended by overwriting the version it had just read from the file with a hardcoded 2117 (`handler.cc:661`). Editor behaviour stranded in the load path when the writers were removed. t-laser.pof declares 2116 (checked the bytes); libpof said 2117. model_t now keeps it via GetVersion().

**OPEN, NEEDS YOUR CALL** — libpof fabricates normals. `pmf_pof.cc:372` ends every load with transform(identity, {}), and `types.cc:122` inside it does safe_unit_from on every normal. Identity leaves positions alone (which is why positions matched exactly) but normalises every normal, and zeroes any under 1e-5. Retail POFs genuinely hold non-unit normals: t-laser has 5 of 24 at ~0.692, subspacenode's are ~1e-8 and become exact zeros. Confirmed by parsing the file's normal table: the oracle's value sits at normal indices 10-11, libpof's value is NOT IN THE FILE AT ALL. Blast radius: 1393 of 1793 polygons (78%), all 8 sample models. Applying libpof's own safe_unit_from rule to both sides collapses the residual to last-digit flips traceable to double rounding in the comparison script, not to libpof — so ONE root cause explains every real difference, no second defect hiding. Cannot just be deleted: the same pass computes bounding boxes, subobject radius and header.max_radius. The normalisation has to be separated from the bounds pass. My recommendation: a read-only library should report what the file says and let pofview normalise at render time (Quick3D shaders normalise anyway) — but it changes library semantics and touches the viewer, so it was left for you.

Deliberately did NOT pin a libpof oracle: pinning now would enshrine the fabricated normals as expected output.

Also flagged (nothing changed): the case-insensitive-filename Windows-ism. 503 case-folding compares across code/, worst sexp.cpp 88, localize.cpp 57, ship.cpp 51. `code/cfile/` itself has ONE, so the workaround is re-done at every call site rather than contained. Not all 503 are filenames — SEXP operator and ship-class tokens are folded too, which is a parser choice. Memory: openspace-case-insensitivity.

UNPUSHED at park: fs2 `9d7a4f137` + `af1f69d31` + `79004bae8` (on top of the pre-existing backlog), libpof `72e9980`, pofview `f5865af`.

NEXT on reconnect: decide the normal-normalisation question, then pin a libpof oracle and wire its differential test, then step 4 (pofer). pofer's own defects are already listed in `public/pof-oracle-findings.txt`.

## POF oracle, step 4 — pofer (2026-07-21)

Step 4 done. pofer's new `--geom` is byte-identical to retail's `pof_dump --geom` across all 176 models, 128693 lines, zero diff. It matched on the first run: pofer never mirrored X the way libpof does, so the dump needed no coordinate fix-up on the way out.

Two real bugs, both measured by reverting the fix and re-running the corpus:

- poly normal indices were never rebased onto the model-wide normal array (vertices were). 88333 of 122535 polygon lines wrong — 72%.
- the submodel offset was baked into vertex positions. 47471 wrong — 39%. Not undoable in the dump: v + off - off is not the identity in float. Same principle as the libpof decision — report what the file says.

One fix that changes nothing on retail data, stated precisely because it would be easy to overclaim: SORTNORM followed only front/back. All 99318 sortnorm nodes do carry pre/on/post offsets, but every one points at a bare EOF chunk — the FS2 compiler emitted the fields and never used them. Front/back-only output is byte-identical. Fixed anyway, with a zero-offset guard (a zero offset used to mean re-entering the same chunk forever).

Also: ASSERT no longer stops checking under NDEBUG (it kept the side effect and dropped the check, so unknown chunks silently continued); 'OHDR' no longer falls through into 'HDR2'; pow(mass, 2/3) integer division; normal counts read unsigned; second POINT_DEF replaces rather than appends; BSP recursion capped at 256; BOX_DEF boxes copied out; the GL include gone. Paths and insignia still deliberately unparsed — pof_t has nowhere to put them and --geom would not check them.

Pinned in pofer as `make check`: an 8-model sample (line-diffed) plus a whole-corpus sha256, same shape as the fs2 oracle. The pinned bytes are pofer's own output but were verified equal to retail's first, so pofer needs no fs2 build to check itself — only an unpacked install (POF_MODELS_DIR or a sibling rundir/); without one it skips 77. Verified it fails when it should by running it against binaries with each fix reverted.

Trap worth remembering: the oracle orders models by lowercased name compared BYTEWISE. A shell `sort` under a UTF-8 locale does not, and two retail models are mixed-case (EMPulse, Platform2T-01). Getting it wrong gave a 5256-line diff that looked exactly like a geometry bug. check.sh uses LC_ALL=C.

Standing: three commits pushed to codeberg+odroid at the start of the session (fs2 `79004bae8`, libpof `72e9980`, pofview `f5865af`). libpof's normalisation decision is TAKEN — stop fabricating — but NOT yet implemented; that is the next piece of work, and nothing is pinned for libpof until it lands.

## A viewer on top of pofer (2026-07-21, later)

Asked for: "slap a viewer on top of pofer, can be the same in pofview." Done without a GUI in pofer and without a second reader in pofview:

```
pof --mesh model.pof > model.mesh
pofview model.mesh
```

pofer gained `--mesh`: the --geom projection at full precision, plus submodel offsets, detail levels, texture names, radius, and the source path. pofview gained `src/viewer/mesh_dump.cc`, which reads one and builds the same Quick3D groups libpof does. --geom is untouched; make check green throughout.

VERIFIED, all 176 models / 201336 corners: zero position-or-UV differences from libpof, 61% differing in normals only. That 61% is the libpof defect, corpus-wide — the 78% on record was over 8 samples.

Two traps worth remembering:

1. %.6g is not enough to READ BACK. capital01 has submodel offsets to 1066; rounding offset and vertex separately then adding them moved 1271 of 4347 corners. --mesh uses %.9g. --geom keeps %.6g — it is the oracle's format, it compares rather than reconstructs.
2. libpof's X negation (`bsp/funcs.cc:10`) is NOT a PCS2 quirk to drop. POF winds faces for a left-handed world; the mirror is what makes them front-facing to Quick3D. The mesh reader does the same, but at the viewer's edge — the display convention lives in the renderer, not in the reader. Same principle as the normals decision.

Two comparison attempts were thrown away before the real one: bitwise (--geom sorts its polygons, libpof walks the BSP) and exact-float sets (the dump is text). Both said "100% differ", which is what a broken oracle looks like, not a broken reader.

Uncommitted in pofer and pofview. libpof's de-normalisation is still the next real piece of work; nothing here changed it, but the viewer now shows it.

**COMMITTED AND PUSHED (both remotes, 2026-07-21):** pofer `92f4814` "Print a drawable dump"; pofview `f7c5dad` "Open pofer's mesh dumps".

## Retail uses normal MAGNITUDE as a light multiplier (2026-07-21)

Chased the question step 5 left open. Retail never normalises:

```
modelinterp.cpp:387   Interp_norms[n] = src   (raw ptr into BSP bytes)
lighting.cpp:612      ltmp = -vm_vec_dot(light->local_vec, norm) * ...
vecmat.cpp:262        dot is x*x+y*y+z*z, nothing else
lighting.cpp:170      lights are rotated INTO MODEL SPACE, so the
                      normal is never transformed either
```

So |norm| scales the diffuse term. 0.692 means 69.2% of the light. The magnitudes are authored shading, not slop.

389844 normals in the 176 models:

```
unit                183607  47.1%   libpof changes nothing
near-zero <1e-5        816   0.2%   -> (0,0,0); dot 0 either way, OK
everything else     205421  52.7%   RESCALED UP -> brighter than retail
```

~75000 of those are below 0.9; 1206 below 0.1.

Irony: safe_unit_from's <1e-5 clamp — its crudest part — is the only bit that is accidentally correct. Ordinary faces are what it wrecks.

⇒ the libpof fix is a RENDERING BUG, not hygiene.
⇒ `fs2/tests/pof_dump.cpp` should grow --mesh too, so retail's own reading can go on screen beside libpof's.

Also settled: fixing libpof does NOT remove the need for --geom/--mesh. They differ because comparing and reconstructing want opposite things (sorted vs not, %.6g pinned to retail's dumper vs %.9g, no placement vs placement). Fixing libpof only demotes --mesh from "the only way to see the truth" to "a second opinion".

## Normalisation fix landed; the real disease named (2026-07-21)

`libpof/src/model/types.cc:104,122` — dropped safe_unit_from. There was nothing to separate: model_t::transform's ONLY caller in the whole tree is the load (`pmf_pof.cc:370`), and it passes identity, so the call was a bare rescale, not part of a transform.

RESULT: all three readers byte-identical. libpof pof-dump == pofer --geom over 176 models / 128693 lines, sha256 == the pinned oracle. Viewer path 201336 corners, 0 position and 0 normal differences. libpof tests 3/3.

Signed zero faked a 3513-corner diff first (pofer folds -0, then the X mirror makes it -0 again). Third time in this project. Fold in harnesses.

THE BIGGER FINDING, from the "why isn't the model complete?" question: it IS complete. POF stores radius, bounding boxes, geometric centres, moment of inertia, normals. model_t::transform is an EDITOR fixup — recompute stale bounds after an edit, canonicalise normals for the BSP recompiler — and libpof runs it at LOAD on data nobody edited. So it also OVERWRITES authored bounds: 15 of 176 models: file radius != libpof's recomputed radius; debris01 file 4.1764 vs libpof 2.1120 — 49% off. Both directions.

Next question is now sharper: should a read-only load recompute bounds at all, when the file states them? (Old framing "separate normalise from bounds" was wrong — it assumed transform was transforming.)

Untouched on purpose: turret/eye/shield/hardpoint/insignia/thrust-glow normals (`types.cc:178,190,222,249,250,304,320`). thrust_glow derives its radius from the normal length first — handle with care.

UNCOMMITTED in libpof.

## Excision + retail lighting (2026-07-21)

libpof `655ef2a` "Stop remediating the model at load": 20 insertions, 514 deletions. transform(identity,{}) call gone, model_t::transform gone, `src/model/types.cc` DELETED (351 lines, 19 transform methods and nothing else), _override/_overridden fields gone.

The naming was the tell: header.max_radius_override held the FILE's value, header.max_radius the RECOMPUTED one, *_overridden meant "author disagrees with geometry" — an editor warning light nothing ever read.

After: oracle byte-identical (176 models/128693 lines), 0 position and 0 normal differences, radius mismatches 15 → 0, tests 3/3.

pofview: retail lighting, replacing the studio rig entirely (user chose "replace"). `src/viewer/shading.cc` = light_apply's LM_BRIGHTEN path, ambient 0.15, reflective 0.75, clamp 0.75. PER VERTEX — FS2 interpolates brightness, not normals — so it is baked as a vertex colour and the material is NoLighting + vertexColorsEnabled. Vertex layout gained colour(4), stride 32 → 48, in the one place both loaders share.

TRAP AVOIDED: FS2's own tech room (`techmenu.cpp:534`) sets up a light and then renders MR_NO_LIGHTING, which flat-shades every vertex to 191 (`modelinterp.cpp:458`). Its light setup is vestigial — nearly reported it as canonical. The real reference is the mission path (`starfield.cpp:540`).

VERIFIED BY LOOKING (grim screenshots). With no scene lights at all, any shading must be the vertex colours; it is plainly not flat. .pof and .mesh renders differ by 2 pixels of 450000 on an AA edge.

TODO: docs/img screenshots are stale (old studio lighting). And libpof.wrap points at codeberg, so pofview needs libpof PUSHED to build clean; local checkout's origin repointed at ../libpof meanwhile.

**COMMITTED AND PUSHED (both remotes, 2026-07-21):** libpof `655ef2a` "Stop remediating the model at load" (-514 lines); pofview `0ce773e` "Shade models the way FreeSpace 2 does"; pofview `3d8928b` "Retake the README screenshots under retail lighting"; pofer `92f4814` (earlier). All three trees clean, origin == backup == local.

Screenshots: modes are click-only (no keyboard shortcut), and there is no input-synthesis tool available (/dev/uinput is root-only), so the shots were taken by TEMPORARILY setting Main.qml's renderMode default and adding an origin eulerRotation for the 3/4 view, then reverting. river draws a ~16px focus border on the window; crop must inset 16 or it lands in the shot.

CORRECTION: the "remaining safe_unit_from sites" I flagged earlier are ALREADY GONE — all seven lived in the ::transform methods that types.cc deletion removed. grep now finds exactly one line: the uncalled definition at `libpof/src/geometry/vec3d.hh:122`. thrust_glow_t's radius-from-normal rescale went with it (no loss: at load it was radius *= sqrt(1)). Only open bit: whether to delete safe_unit_from itself.

6 vs 9 DIGITS, settled: they are the two IEEE binary32 constants and answer opposite questions. FLT_DIG=6 — any 6-digit DECIMAL survives a trip through float (for data starting as decimal). FLT_DECIMAL_DIG=9 — every distinct FLOAT reads back bit-identical (for data starting as float). printf %g defaults to 6, which is why 6 looks like "the" float precision; it is the wrong one for a dump that gets read back. Measured: of 151845 distinct float32 coordinates, 145611 (95.9%) fail to round-trip through %.6g; 0 fail through %.9g.

## Ordering, and the RFC (2026-07-21 cont.)

Sparked by: "suppose retail parses in the same order as pofer — would the dump be identical?" MEASURED: yes. With sorting disabled on both sides (scratch POF_NOSORT, reverted), pofer and the retail dumper are byte-identical over all 176 models / 128693 lines. Caveat stated in the findings: pofer was given retail's dumper walk deliberately, so that is a fact about my implementation choice, not about the format. What it does establish: the sort absorbs exactly ONE reader, libpof.

RETAIL HAS NO SINGLE TRAVERSAL ORDER (`modelinterp.cpp:741`). Its renderer flips back↔front per node per frame by which side of the split plane the eye is on — painter's algorithm, no depth buffer in 1998. Collision uses a fixed order; pof_dump mirrors collision. So traversal order is a property of a reader's PURPOSE. I had proposed mandating an order in the spec; that is now REJECTED (retail could not satisfy it).

THE REAL DEFECT: the canonical order is defined by the SERIALISATION. Both dumpers sort the formatted string, so order is a function of print precision. 4 of 5 sampled models order the same polygons differently in --geom than in --mesh. The dumpers are coupled through their printf strings.

USER'S CANDIDATE ANSWER: parsers unchanged; define a total order on a submodel's polygons FROM THE DATA, applied to a VIEW (model const, no mutation). Key = the COMPLETE record (a partial key makes ties material and resolves them by traversal order — silent failure on coincident faces). Submodels NOT reordered: their index is referenced by parent, LOD, debris, eye, turret.

RULE SETTLED for canonicalising at all: picking a representative of a REAL equivalence class is fine (-0 and +0 denote the same number, so zero() is justified); asserting an equivalence that does not hold is data loss (0.692 != 1.0, which is what libpof did). Open: whether scoping an equivalence to current use is sound (RFC q6).

RFC lives at `pofer/doc/comparison-format-rfc.md`, 239 lines, commits `6bb3f04` + `8491212`, linked from pofer's README. **PUSHED.**

OPEN DECISION, NOT TAKEN — pin libpof against the oracle. NOT making libpof an oracle (my earlier wording was wrong; retail is the oracle). The pinned sha256 fe696cb0... is a fact about the corpus with retail provenance, like NIST test vectors. FOR: libpof's 3 unit tests never touch the corpus, and we just cut 514 lines from it — a regression would be silent. AGAINST: third copy of one constant; the RFC may change the format and invalidate all three. I recommended pinning. User has not decided.

REPO STATE, all clean and pushed to codeberg + odroid: libpof `655ef2a`, pofview `3d8928b`, pofer `8491212`.

## THE MODEL DUMP LANDS: --geom/--mesh duality dead (2026-07-21 evening)

The RFC got answered in-house the same day it was written. Sequence ran as agreed (spec → retail → pofer → libpof; user's calls: %.9g, compare everything, bridge deferred):

- `pofer/doc/model-dump-spec.md` — THE format. Dump = the polymodel: what retail's loader retains, after its interpretation, nothing else. Polygons value-key-sorted (complete record, user's design); all else file order; -0=+0 the only canonicalisation; NaN aborts.
- fs2 pof_dump `--model` (--geom deleted, --full/summary stay); turrets caught by priming retail's own subsystem path with the model's submodel names.
- pofer `--model` (--geom/--mesh deleted); PATH/INSG/EYE-n retention added.
- libpof `model_t::dump` + retention it had been silently dropping: muzzle lights (1,078 of them in 91 models!), flat RGB, poly center/radius, shield indices+neighbors, path turret ids, raw movement ints (39 submodels carry type 0, which MNONE had folded away), obj_flags, ACEN presence.
- pofview reads `.model` (mesh_dump → model_dump, git mv); own-offset accumulation in reader; flat polys now drawn in their authored colour (t-laser is RED); source line gone.

**THREE-WAY BYTE IDENTITY, first diff, all 176 models, 159,596 lines:** sha256 `0b9e0b703f248459dca6e7ac7b79080f82a344fda4b5cef44fb5c5f239ecee20` pinned in fs2 `tests/oracle/`, pofer `oracle/`, libpof `tests/`. The "pin libpof?" question answered itself — the differential test IS the pin (4/4 green).

Corpus surprises: 5 duplicate-parent turret banks (Leviathan flak turrets ship with a dead first bank each; last-writer-wins is retail's retention); eyes max 1 (retail allows 10); zero NaN/Inf anywhere.

Suites: fs2 pof-oracle OK (now 4 checks incl. model sha; old --full/summary pins untouched+green), pofer make check green (9-model sample now incl. cruiser2t-01), libpof 4/4, pofview 3 visual verifications (fighter01, capital01 pixel-identical across both loaders, t-laser red).

UNCOMMITTED in all four repos. After push: purge+update pofview's subprojects/libpof wrap checkout so it rebuilds against the new libpof. Full detail: `pof-oracle-findings.txt` STEP 12.

**2026-07-21 addendum: ALL PUSHED** (codeberg + odroid). fs2 `d26d452ea`, pofer `484b86d`, libpof `c3b5305`, pofview `6c23f00`. pofview's libpof wrap purged and re-fetched at c3b5305 (its checkout had drifted to a local-path origin); clean rebuild, fighter01 renders via the new libpof.

## 2026-07-22 — cmdline, deploy, the github move, and the aabitmap page-in bug

Morning housekeeping, three commits: `41d0cd1c9` docs adoption (fixmine/fs2-inventory/pof-oracle-findings/notes + survey into `docs/`), `e72bbf759` cmdline: retail parser → getopt_long, `fc48eb11e` build: deploy the game binary to `../rundir` on every build (copy, not symlink — `/proc/self/exe` resolves symlinks and the game chdirs to the result).

**HOSTING MIGRATION: codeberg → github.** thinkoid.org had been tied to codeberg; link severed, whole public estate moved to github under account **thinkoid** (renamed from think-c). This repo: `origin` = `git@github.com:thinkoid/openspace-fs2.git`, `backup` = `ssh://odroid.local/git/openspace-fs2` (normalized). All 12 public repos migrated (all branches+tags); the site (thinkoid.org) rehosted on GitHub Pages. Codeberg copies still exist pending deletion. fs2-retail/fs2-assets (copyrighted game data) remain odroid-ONLY, as ever. The old codeberg notes-references ("public/fixmine.txt" etc.) now mean `docs/` in this repo.

**THE AABITMAP PAGE-IN BUG — fixed `465234074`, forensics `docs/hud-aabitmap-artefact.txt` (adopted `4f9984eec`).**
User compared GL vs software screenshots of the training mission: the afterburner and weapon-energy gauges (and the weapons panel) rendered as grey/black/wrong-green bands under the software rasterizer. Root cause chain: `bm_page_in_aabitmap()` set `used_flags = 0` — the dead-subsystem sweep (5b2148062) collapsed retail's `if (D3D_enabled) BMP_AABITMAP else 0` to the WRONG (Glide-era) branch — so `bm_page_in_stop()` unpacked every HUD aabitmap ANI as a regular bitmap (pixels palette-translated to game-palette indices); `bm_lock`'s reload test never compares flags (and its pal_changed test exempts aabitmap locks), so the HUD's aabitmap locks returned the translated data forever; `gr8_aabitmap_ex` fed indices 74..254 into the 16x256 alphacolor table — reads past it into neighboring Alphacolors slots = the deterministic banding. Hardware escaped by accident: 16bpp tcache locks forced a bitdepth reload (the GL log's "Reloading ... from bitdepth 8 to 16" vs software's "Reloading ... to remap palette" was the tell). Fix: `used_flags = BMP_AABITMAP;` — the other `bm_page_in_*` variants' 0 is CORRECT for software (tmapper/bitblt lock plain textures with 0). **User-verified in a test flight: artefacts gone.**
NOTE vs the 2026-07-18 VERDICT above: the *gradient banding* analysis stands (authentic 8bpp quantization); today's grey/black corruption was a distinct, real bug on top of it — the earlier "bands of green and dark... probably authentic; verify later" flag was this.
Optional hardening left on the table: an aabitmap-mismatch reload condition in `bm_lock` (compare requested flags vs the stamped `used_flags`) would make the cache self-healing against any flag-mismatched lock order.

## Itch list founded — docs/itches.md (2026-07-28)

Standing redesign queue now lives in `docs/itches.md` (the groom discipline's
"write it down instead of scratching it" gets a home). Founding entry: the
linklist `links_t<T>` retrofit — thin the fat sentinel (416 dead bytes per
`object` head; `ship.subsys_list` = 23% of `Ships[]`) to a 16-byte templated
head, kill the sentinel/node type confusion, drop `list_remove`'s dead head
parameter and the caller-less `list_merge`. Full design, cost census, and
verification gate in the entry. Born of the 2026-07-28 linklist review, which
otherwise *affirmed* the sentinel-ring design (std::list is the same ring
with a thin sentinel).

## linklist list_t<T> retrofit — LANDED (2026-07-28), 37 files

The itch list's founding entry, scratched the day it was written (design
record + outcome deltas in `docs/itches.md`). The fat sentinels are gone:
heads are `list_t<T>` (16 bytes), nodes inherit `list_links_t<T>`, the
macros are function templates with the same names and call shape,
`list_remove` lost its dead head parameter (~40 sites), caller-less
`list_merge` died. `sizeof(ship)` 1744 → 1352. Type checking flushed out
retail quirks the preprocessor had been swallowing: 7× `GET_NEXT(&head)`
meaning GET_FIRST, `&obj_used_list` stored as a "no homing object" sentinel
value (now `END_OF_LIST(&obj_used_list)`), `GET_LAST`-on-element in the
subsys/target advance helpers, and muzzleflash.cc's pooled-list machinery
revealed as Volition-commented dead code (left verbatim). Verified: full
build warning-set identical to pre-surgery, math + pof-oracle tests green.
Campaign playtest pending.

## linklist follow-up: the fat sentinel was load-bearing once (2026-07-28)

Auditing the new header's safety comment ("only links are ever touched
through a sentinel pointer") against the tree found one retail violation:
aicode.cc shockwave avoidance read `homing_object->type` with a NULL guard
but no guard against the "not homing on anything" token (weapons.cc stores
END_OF_LIST(&obj_used_list) at missile birth and on lost aspect lock). The
fat sentinel's zero-initialized payload absorbed that read benignly for 28
years; the thin sentinel made it out-of-bounds. One guard added at the
site. Every other payload access through a possibly-sentinel pointer
(weapon_home, hudtargetbox, swarm, missionbrief, missionhotkey) checks
first — audited. Header comment rewritten to state the C++17 truth: the
sentinel pun is formally UB (no T object at the head's address), defined
in practice by the ABI and pinned by static_asserts — container_of
territory, documented as such. Boost.Intrusive recorded in the itch entry
as the escalation path if call sites ever modernize wholesale.

## Resolution: presentation scaling landed — the game presents at 2048x1536 (2026-07-28)

`-res WxH` (integer canvas multiples): engine renders the authored 1024
canvas unchanged; software present magnifies s×s per pixel, GL magnifies via
viewport (and re-rasterizes the 3-D world at real resolution — free fidelity
there). Mouse ÷s in, ×s on warp; GL scissor ×s, region reads decimated.
PROVEN presentation-only: canvas frame dumps byte-identical with and without
-res. The doc's native canvas-scale plan (resolution-scaling.md §4-5) was
started (stage-1 framebuffer commit caf4ef7d5, reverted into window
semantics) and deliberately deferred after survey found clip_* dual-space
reads + the need for 8bpp magnifying blitters — SCP's per-call disease;
recorded in the doc §6. GL-native is the follow-up era. Campaign playtest
now unblocked at playable window size: `./fs2 -window -res 2048x1536` (and
`-opengl` variant needs a real-display check).

## Retail numeric-margin asserts in vecmat interpolation — demoted (2026-07-28)

Training-mission playtest died on `Assert(fl_abs(theta_goal.z) < 0.001f)`
(vecmat.cc, vm_forward_interpolate; identical twin in vm_matrix_interpolate,
plus a local_rot_axis.x sibling in the bank arm). CATALOGUE: these are
diagnostics, not gates — the checked residual is discarded immediately below
(bank comes from delta_bank / the rvec arm), and retail SHIPPED with asserts
compiled out, so no player build ever gated on them. Volition knew the
margin was fragile: Andsager's debug-replay rig sits around the aicode.cc
caller. Why we hit it and 1999 rarely did: MSVC/x87 ran the math in 80-bit
intermediates; gcc/SSE keeps 32-bit floats throughout, eroding the 1e-3
margin. fs2open's answer was replacing the whole function (PR 2668,
vm_angular_move_forward_vec, deliberate behavior change) — not fix-mine
material. Ours: all three asserts demoted to nprintf("Physics", ...) with
the residual value; shipped behavior (residual discarded, flight continues)
wins.

## Linklist retrofit reverted — retail macros and fat sentinel restored (2026-07-29)

Second thoughts, one day in: the list_t<T>/list_links_t<T> retrofit was
churn on a battle-tested C89 core — our own "lipstick on a pig" clause
turned on our own surgery. All 37 files restored to their pre-retrofit
state (templates gone, macro header back, node structs carry their manual
next/prev again, sizeof(ship) back to 1744). What the surgery *learned*
stays, re-applied on retail vocabulary because it is knowledge, not
uniform: the aicode.cc shockwave-avoidance guard against the "not homing
on anything" token (the audit's bug — retail read the token's ->type and
survived on the fat sentinel's zeroed payload; now guarded explicitly),
plus the inert respellings that document retail sloppiness — 7×
GET_NEXT(&head)→GET_FIRST, 4× GET_LAST(elem)→GET_PREV, 4× weapons.cc
token mints spelled END_OF_LIST(&obj_used_list). Every respelling expands
macro-identically. Verified: clean rebuild, warning set back at the
pre-surgery baseline (4756); math + POF oracle green; headless boot
renders (73 frames dumped). The itch entry keeps the full design record;
Boost.Intrusive remains the only sanctioned forward path for this file —
all or nothing.

## Fossil sweep: dead Win32-era headers and API surface (2026-07-29)

An osapi survey turned up fossils; a tree-wide zero-includer scan completed
the census. Deleted files (no includers anywhere): osapi/monopub.hh (1993
Microsoft DDK header — mono-monitor kernel-driver IOCTLs for the dead
outwnd debug window), io/sw_force.hh (SideWinder Force Feedback, the FF
subsystem was swept 07-18), freespace2/freespaceresource.hh (MSVC-generated
resource IDs for a FreeSpace.rc that no longer exists), hud/hudresource.hh
(zero bytes), missionui/missionstats.hh + missionrecommend.hh (empty
include guards; their .cc files are real and never included them). Pruned
API: os_get_window (HWND-shaped stub returning 0, zero callers —
os_get_sdl_window is the accessor), os_suspend/os_resume (empty,
single-threaded now), os_toggle_fullscreen (declared, never defined), the
THREADED critical-section macro block (zero users), Os_debugger_running
(hardwired 0 since the SDL port; its one reader in gr_force_windowed was a
debugger+DirectDraw mode-switch workaround, dead with it), and outwnd's
remains: FILTER_NAME_LENGTH, load_filter_info (empty stub + its one call),
Log_debug_output_to_file (set by the debug-console `log' toggle, read by
nobody — the toggle lied). Verified: clean rebuild, normalized warning set
byte-identical; tests green; headless boot renders.

## Fruit-sweep sections A+C applied — bug fixes and zero-risk deletions (2026-07-29)

From the 2026-07-29 low-hanging-fruit survey (full census in the rolling
notes.txt).  Section A, the port-divergence fixes: debris.cc hull-arc
probability un-overflowed (RAND_MAX*2/3 was sized for MSVC's 15-bit
RAND_MAX; on glibc it overflowed and hull debris never arced -- retail
arcs 2/3 of pieces); missionmessage.cc's three sprintf-append-to-self UB
sites respelled sprintf(p+strlen(p),...); atan2_safe's header declaration
un-swapped to match the (y,x) definition + a do-not-swap-for-libm warning.
Section C, the deletions: sw_error.hh + sw_guid.hh (orphaned by the
sw_force.hh deletion -- includer-cascade); the stale CVS-era src/Makefile
(21 Network/multi_*.o ghosts); the dead inverse-sqrt LUT apparatus in
floating.cc + the shadowed fl_isqrt extern + 4 dead macros (fl_is_nan
used MSVC-only _isnan); asqrt(); the never-activated fhash module
(localization/fhash.{cc,hh}, whose init memset also overflowed its
pointer array 4x -- a live retail buffer overflow, now moot) + parselo's
two dead fhash branches; vm_strdup/vm_free_all/VM_MALLOC/VM_FREE; 40
verified phantom declarations across 16 headers (declared, defined
nowhere; 3 candidates found live and kept); 31 unused file-scope statics;
9 set-but-never-read locals (incl. hudtarget's vestigial
nearest_turret_subsys auto-target-turret remnant and vecmat's tv plane
residual).  Retail bugs deliberately NOT fixed (catalogue, section B of
the survey): keycontrol.cc:1970 match-target precedence, the !x&FLAG
assert family, rand_alt's impotent reseed.  Verified: warning-set diff
accounts for every removal with zero additions (4756 -> 4697); tests
green; headless boot renders.  Census bonus: gcc's
-Wunused-but-set-variable warnings carry a trailing `=' in the bracket
tag -- category greps must allow it.

## Survey D applied: dead build configs collapsed, Fred/Pofview folded (2026-07-29)

The two big mechanical collapses from the 07-29 survey.  Build configs:
every FS2_DEMO / OEM_BUILD / E3_BUILD / PD_BUILD / PRESS_TOUR_BUILD /
MULTIPLAYER_BETA(_BUILD) / FS1-era DEMO conditional resolved (dead
branches deleted, inverted live guards unwrapped), RELEASE_REAL unwrapped
and its define retired -- the shipping retail configuration is now the
only build; GERMAN_BUILD survives as the one localization knob.  Done
with a scratchpad mini-unifdef (no unifdef on the box, no new dependency)
scoped to exactly those macros -- #if 0 blocks and NDEBUG guards
untouched.  Fred_running (assigned once, to 0) and Pofview_running +
Nebedit_running (same; pofview lives in pcs2 now) folded through ~136
sites; cascade deletions: missiongrid.cc entirely (grid globals moved to
missionbriefcommon.cc, their sole consumer), the gf_aascaler dispatch
(gr8_aascaler 209 lines, gr_opengl_aascaler, the 2d.hh member+macro) and
the calc_alphacolor*_old chain + alphacolor_old struct, six FRED-only
sexp.cc functions (~180 lines incl. query_referenced_in_sexp), the three
parselo *_fred variants, physics_sim_editor, and (fold fallout)
read_mission_goal_list; gr_init lost its fred_x/fred_y params.  The fold
script's blind spot -- single-statement if with an else -- produced
orphan elses that the compiler enumerated and we fixed against the
pristine originals; a hunk-audit confirmed every deleted line came from a
dead branch.  One latent debug-crash also died: obj_delete's
OBJ_WAYPOINT/JUMP_NODE arm asserted Fred_running, i.e. would have fired
in our debug build the first time a jump-node object was deleted in-game.
Verified: clean rebuild, warning set accounted (4691 -> 4676; -12
write-strings from deleted FRED code, +0 new); tests green; headless
boot renders.

## Survey F underway: const-correctness sweep, 4676 -> 1808 warnings (2026-07-29)

Three tranches of the -Wwrite-strings wall, hub-first instead of
site-by-site: (1) the controlconfig cluster (six Scan_code/Joy_button
tables, config_item.text, translate_key/textify_scancode) and the
gr_printf/gr_string/gf_string dispatch -- const-ing those two graphics
sinks alone removed ~1000 literal-site warnings tree-wide; (2) a scripted
pass const-ing all 69 literal string tables + 40 externs, then a
compile-chase that surfaced the real distinction: LITERAL tables const
cleanly, but RUNTIME-OWNER tables (Campaign_names, Cargo_names,
Ship_class_names, Pilot_image_names, Ai_class_names, Weapon_names --
strdup'd/filled at parse time, some freed) must stay char*; hubs const'd
along the way: bm_load family, UI_WINDOW::set_mask_bmap/
set_foreground_bmap, UI_GADGET::set_bmaps + bm_filename, the slider
creates, parselo needles (required_string/optional_string/skip_to_string
/required_string_either/_3) + strlist APIs as const char *const[]
(accepts both char** and const char**), token_found, hud_anim_init,
nebula_init, wing_name_lookup, cf_add_ext; (3) struct-field hubs:
sexp_oper.text (the 133-warning Operators[] table), UI_XSTR.xstr (every
menu screen's template tables; the window's strdup'd copies keep
ownership -- one commented (void*) cast at the free site), cfopen +
cf_create_default_path_string.  Verified at each tranche: zero errors,
clean rebuild, tests green; headless boot renders after the batch.
REMAINING: ~1500 write-strings (top: optionsmenu 196, hudconfig 157,
controlsconfig 82, missiondebrief 65, barracks 63 -- mostly ui_button_info
/ per-screen fname tables and Error/Warning format params) + the ~300
non-write-strings tail.  Same recipe continues.

## Survey F tranches 4-6: write-strings 1808 -> 504 total, 80 remaining (2026-07-29)

Continued hub-first: the per-screen button-struct clones (17 structs --
options_buttons, HC_gauge_region, barracks_buttons/bitmaps, wl/ss/brief/
goal/hotkey/sim_room/scrollback/techroom/credits/gameplay-help buttons,
op_sliders -- all the same filename-member+ctor idiom, const'd by a
block-aware script); static and multi-dim literal tables the first regex
missed (34 more); ui_button_info itself; the os_config_* family (params
and read_string returns); parselo's second layer (error_display/
diag_printf formats, find_and_stuff id/description, stuff_string
terminators, copy/advance_to_eoln, read_file_text); UI_CHECKBOX/RADIO/
ICON/INPUTBOX creates + add_XSTR (all strdup/copy semantics);
HUD_printf/HUD_sourced_printf/emp_hud_*; sexp_error_message;
popup_background + popup_get_button_filename; snazzy_menu_add_region;
common_set_interface_palette; cf_get_file_list filter + cf_matches_spec;
ship_name_lookup/ship_type_name_lookup; g3_start_frame_func;
load_animating_pointer; gamesnd_parse_line; gr_get_string_size;
Osreg_* singles.  Two decl/def-drift link errors caught by the linker
(os_config_read_string default param, match_and_stuff continuation
line) -- the mismatch shows as an undefined reference, not a compile
error; align continuation lines when const-ing wrapped prototypes.
Verified: zero errors, tests green, headless boot renders 75 frames.
Warning totals: 4756 (survey start) -> 504; write-strings 4278 -> 80.
The 80 are scattered singles (~30 files, 1-9 each); the ~420
non-write-strings tail (multichar, unknown-pragmas, conversion-null,
char-subscripts, parentheses) is E-material.

## Survey F COMPLETE: -Wwrite-strings extinct, 4278 -> 0 (2026-07-29)

The final tranche took the last 80 scattered singles: the remaining
lookup/parse params (check_for_string, copy_text_until endstr, model_load
/read_model_file, bm_load_sub, cfputs/cfwrite_string/cfwrite source
buffers, alloc_sexp/find_operator, event_music score names, gr_init_font/
gr_create_font, read_menu_tbl menu name, message_log_add_seg,
message_queue_message who_from chain, Skill_level_names return,
mission_campaign_load/get_info/maybe_add, game_do_cd_check_specific,
lcl_ext_associate, set_valid_chars, ui_string_centered,
stars_set_background_model, hud helpers), the last literal-table struct
fields (shield_ani, popup_background), and a handful of const locals.
One deliberate cast survives: credits.cc's no-credits fallback aliases a
literal into the mutable Credit_text buffer pointer (retail design;
nothing writes on that path) -- commented at the site.  Every tranche
gate ran clean; final state: 424 warnings total, ZERO write-strings,
tests green, headless boot renders 75 frames.  The 424 that remain are
the pre-existing tail (multichar 24, unknown-pragmas 22, conversion-null
19, char-subscripts, parentheses, unused-value/function, sign-compare,
register, ...) -- survey section E.

## Survey B resolved: the !x & FLAG family respelled (2026-07-29)

The precedence-slip census (`if (!x & FLAG)` reading `(!x) & FLAG`) came
to six sites; analysis against the flag values turned up that all five
flags involved are bit 0, so the buggy form degrades to "flags word
entirely zero" -- which decides each verdict:

- keycontrol.cc TOGGLE_AUTO_MATCH_TARGET_SPEED: the ONE live bug.  The
  test sits inside the branch that just set AUTO_MATCH_SPEED (bit 3), so
  flags is provably nonzero and the condition always false -- the
  player_match_target_speed() call under it was dead code on every
  platform since 1999.  Toggling auto-match on never engaged matching
  until the next target switch.  FIXED (fs2open precedent; the code
  documents the intent).
- asteroid.cc Assert, sound.cc Sound_spew counter, missiongoals.cc
  goal-failed music: exact by accident -- in each case the bit-0 flag is
  the only flag, so flags==0 <=> bit clear.  Respelled inert.
- missionparse.cc debris sweep: DEBRIS_EXPIRE (1<<1) exists, but
  debris.cc zeroes the whole word on init and delete, so reachable
  states are {0, USED, USED|EXPIRE} and the buggy test was exact.  One
  future flag away from going live.  Respelled inert.
- staticrand.cc rand_alt: static int x = Rnd_seed initializes on first
  call, so srand_alt can never reseed afterward -- but srand_alt has
  ZERO callers and rand_alt exactly one (aicode rearm-retry jitter,
  fine with a fixed seed).  Landmine, not a fault; left as-is.
  srand_alt is a C-pile deletion candidate for the next sweep.

Gate: build clean, tests green, headless boot renders 73 frames.

## Survey E1: mechanical tail -- pragmas, register, multichar, endif (2026-07-29)

Warnings 419 -> 370, all four categories extinct, zero additions.

- 22 MSVC pragmas deleted (warning push/pop/disable, optimize, auto_inline)
  across 9 files.  Curio: aicode.cc/aibig.cc set optimize("",off) and never
  restore it -- retail's AI compiled at -O0 on MSVC.
- pcxutils.cc: 2 C++17-deprecated register specifiers dropped.
- freespace.cc: "#else if !defined(NDEBUG)" respelled plain #else (the
  trailing tokens were always ignored; the intended condition is the exact
  complement of the #if, so behavior identical).
- multichar magics: new constexpr fourcc() in pstypes.hh builds the
  little-endian int from the ON-DISK byte order, so ids now read as the
  file bytes (fourcc("HDR2")) instead of reversed multichar ('2RDH').
  Swapped: 17 modelsinc.hh chunk ids, pofparse.cc 'OPSP' -> "PSPO",
  palman PAL_ID -> "VPAL", managepilot PLR_FILE_ID -> "FSPF".  fourcc
  returns int (multichar's type) to keep comparison signedness.  Proven
  by a 22-identity static_assert battery vs the multichar originals AND
  the pof-oracle byte-identical gate.

Gate: build clean, tests green, boot renders 74 frames (exercises
PLR_FILE_ID + PAL_ID reads).

## Survey E2: per-site verifications -- warnings 370 -> 308 (2026-07-29)

62 warnings across 12 categories, every site read in context.  The finds:

- beam.cc x2 (beam_fire/beam_fire_targeting): the range guard spelled
  "instance < 0 && instance >= MAX_SHIPS" -- a contradiction, always
  false, so the range check never rejected anything.  The Assert directly
  above spells the intent; respelled to ||.
- popup.cc: "if ((PF_ALLOW_DEAD_KEYS) && ...)" tested the bare macro --
  always true, so EVERY in-mission popup processed the dead-key set.
  Respelled to (flags & PF_ALLOW_DEAD_KEYS); no caller passes it, so the
  block is now dead -- fs2open reached the same verdict ("unused even in
  retail") and deleted flag and block.
- freespace.cc view-target HUD: jump_node_name was declared inside the
  OBJ_JUMP_NODE case and read (dangling) after the switch.  Hoisted.
- medals.cc: sprintf(base, "%s%c", base, ...) self-overlap UB -- same
  class as the missionmessage find; now appends via base + strlen(base).
  (This one statement carried both the restrict AND a format-overflow
  warning -- 61 sites, 62 warnings.)
- ship.cc show_ship_subsys_count(): dead debug statistic (Ship_subsys_hwm
  never read) containing a real bug -- Ships[objp->type] where instance
  was meant.  Deleted function, global, and call.
- keycontrol.cc: "k & ~KEY_SHIFTED + KEY_ALTED" masks with ~0x1000+0x2000
  == 0xFFF -- accidentally right for every k the switch admits.
  Respelled to k & ~(KEY_SHIFTED + KEY_ALTED).
- readyroom/missiondebrief/sound: array-address checks (always true)
  respelled to name[0] intent; the readyroom one drew a bare ".fc2"
  extension when no campaign was loaded.
- Inert mechanics: 13 char-subscript (int) casts (a 14th died with
  show_ship_subsys_count), parens documenting shipped precedence
  (aicode x3, collideshipship), braces pinning else bindings (aicode x3,
  window.cc), int-typed campaign file magics (0xbeefcafe & co, fixing 4
  sign-compares), NULL->0/0.0f family (model_load x9, cfread defaults x6,
  beam sigs, shiphit, bmpman ptr_u, strnicmp==NULL), is_training_mission
  declared in missionparse.hh instead of function-local in hudbrackets.

Gate: delta fully accounted (62 removed, 0 added), tests green, boot
renders 73 frames.
