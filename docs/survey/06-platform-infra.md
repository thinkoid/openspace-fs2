# Cluster 06 — Platform / infra / audio

Subsystems: **freespace2**, **io**, **cfile**, **cfilearchiver**, **sound**,
**gamesnd**, **osapi**, **math**, **globalincs**, **localization**, **cmdline**,
**playerman**, **anim**, **cutscene**, **debugconsole**, **cryptstring**. The
integration hub, the platform shims (SDL/OpenAL), the VP virtual filesystem, the
math core, and the file-format codecs. See [README](README.md) for conventions.

> **Extraction-rich cluster.** `cfile` (VP virtual filesystem), `math`
> (vecmat/fix/staticrand), `anim/packunpack` (ANI RLE codec), and `cryptstring`
> (jcrypt) are near-freestanding lifts. `freespace2` is the opposite — the
> non-liftable hub where you read the boot order.
>
> **Cutscene correction:** MVE is **stubbed, not ffmpeg-ported** — `movie_play()`
> is called (cutscenes.cpp, freespace.cpp) but undefined project-wide, and there
> is no avcodec in the tree. Treat `cutscene/` as a menu shell around a dead
> playback call; ffmpeg MVE decode remains a future task.

---

### freespace2  (~8000 master; freespace.cpp ~6520 + levelpaging)
- **Purpose:** Top-level game glue — init, the master game loop, and per-state dispatch tying every subsystem together.
- **Entry points:** `freespace2/freespace.cpp: game_init()`, `game_frame()` (freespace.cpp:3135), `game_do_frame()` (freespace.cpp:3452), `game_do_state()`, `game_level_init()` (freespace.cpp:912), `game_shutdown()` (freespace.cpp:468); `freespace2/levelpaging.cpp: level_page_in()`.
- **Core state:** `Game_mode`, `Game_current_mission_filename`, `Framerate`/`flFrametime`, `Player`, `Viewer_obj`; ties into `gamesequence` state stack (game_do_state dispatches per GS_STATE).
- **Mechanism:** `game_init()` brings up osapi/cfile/sound/graphics in sequence; the loop runs `game_frame()` (render one frame) driven off `gameseq_process_events()` which calls `game_do_state()` for the active state. `game_do_frame()` runs the actual gameplay-tick path (physics, AI, object move/render). levelpaging.cpp force-loads all bitmaps/models/sounds a mission references.
- **Rare knowledge:** — (glue, not codec). Note MVE playback is gone: `movie_play("intro.mve")` / `"fstrailer2.mve"` are called (freespace.cpp:3552, 5125) but `movie.h` is commented out (freespace.cpp:106) and no `movie_play` is defined anywhere — the whole cutscene/MVE decode path is excised, not ffmpeg-ported.
- **Deps:** Everything — gamesequence, object/ship/ai/physics, graphics, cfile, sound/gamesnd, playerman, mission/*, io, osapi.
- **Extraction seams:** Not liftable; this is the integration hub. It is where you read the boot order and subsystem lifecycles.
- **Port notes:** Heavy port surface — SDL window/timer boot, dead-movie calls, `#include "movie.h"` and many Win-only paths commented out. Good place to see which retail backends were stubbed.

### io  (~4900 master: key 751, keycontrol 2327, mouse 373, joy 557, timer 405)
- **Purpose:** SDL-backed input (keyboard, mouse, joystick), the frame timer, and the control-binding-to-game-action mapping.
- **Entry points:** `io/key.cpp: key_init()`, `key_getch()`; `io/mouse.cpp: mouse_init()`; `io/joy.cpp: joy_init()`; `io/timer.cpp: timer_init()` (timer.cpp:43), `timer_get_microseconds()` (timer.cpp:92); `io/keycontrol.cpp: process_key_controls()` / `game_process_keys()`.
- **Core state:** `io/timer.cpp: Timer_base` (CLOCK_MONOTONIC reading at init); key state arrays in key.cpp; `Control_config[]` mapping consumed by keycontrol.cpp.
- **Mechanism:** mouse/key/joy pull from SDL events (`#include <SDL.h>` at top of each). timer.cpp reads `CLOCK_MONOTONIC` and subtracts `Timer_base` for a never-rolling microsecond clock. keycontrol.cpp is the huge (2327-line) table translating raw keys into gameplay actions each frame.
- **Rare knowledge:** — mostly mundane; keycontrol.cpp is a giant hand-written dispatch worth skimming only for control semantics.
- **Deps:** osapi (SDL init), globalincs; keycontrol leans on nearly all gameplay subsystems.
- **Extraction seams:** timer.cpp and key/mouse/joy are cleanly SDL-isolated and liftable; keycontrol.cpp is glue and not portable in isolation.
- **Port notes:** Heavy — retail `timer.cpp` used `QueryPerformanceCounter`/`QueryPerformanceFrequency` [retail]; port swaps to `CLOCK_MONOTONIC`. Input backends fully rewritten from DirectInput to SDL. Old force-feedback headers (`sw_force.h`, `sw_error.hpp`, `sw_guid.hpp`) remain in-tree but inert.

### cfile  (~4000 master: cfile 1470, cfilesystem 1065, cfilelist ~330, cfilearchive 211)
- **Purpose:** Virtual filesystem — transparently opens files from disk or from packed `.vp` archives via one `cfopen`/`cfread` API.
- **Entry points:** `cfile/cfile.cpp: cfopen()` (cfile.cpp:493), `cfclose()` (cfile.cpp:649), `cf_open_packed_cfblock()` (cfile.cpp:719); `cfile/cfilearchive.cpp: cfread()` (cfilearchive.cpp:171), `cfseek()` (cfilearchive.cpp:122), `cf_init_lowlevel_read_code()` (cfilearchive.cpp:28); `cfile/cfilesystem.cpp: cf_build_file_list()`, `cf_search_root_pack()` (cfilesystem.cpp:405), `cf_build_root_list()`.
- **Core state:** `cfilesystem.cpp: cf_root`/`cf_root_block`/`Root_blocks[]`, `cf_file` entries; `cfile.cpp: Cfile_block_list[]`; on-disk structs `VP_FILE_HEADER {char id[4]; int version, index_offset, num_files;}` and `VP_FILE {int offset,size; char filename[32]; int write_time;}` (cfilesystem.cpp:390).
- **Mechanism:** At boot it scans root dirs, indexes every `.vp` pack (each = 16-byte header + concatenated file blobs + a directory index at `index_offset`), and builds a flat file table keyed by name+pathtype. `cfopen` resolves a name to either a loose file or a `(pack file, offset, size)` triple; packed reads go through `cf_init_lowlevel_read_code` which clamps seeks/reads to the sub-range so a packed file behaves like a standalone `FILE`. `cfread_int/float/vector/...` are versioned typed readers.
- **Rare knowledge:** MINE — the VP archive format and the whole virtual-FS resolution/override order live here; the `.vp` layout (`'VPVP'` magic id, 32-char names, 32-bit `write_time`) is fully reconstructable from cfilesystem.cpp.
- **Deps:** Nearly standalone — globalincs, localization (localized-file lookup at cfilesystem.cpp:670), osapi for root dir.
- **Extraction seams:** Very liftable — a near-freestanding VFS. Cut at `cfopen`/`cfread`; only the localized-filename hook and pstypes need stubbing.
- **Port notes:** Comment "32-bit time_t as stored on disk" flags an explicit port-safety note vs retail's raw `time_t write_time` [retail]; memory-mapped path (`cf_open_mapped_fill_cfblock`, cfile.cpp:753) reworked for POSIX `mmap`/fd.

### cfilearchiver  (~254 master)
- **Purpose:** Standalone command-line tool that builds a `.vp` archive from a directory tree.
- **Entry points:** `cfilearchiver/cfilearchiver.cpp: main()` (cfilearchiver.cpp:194), `write_header()` (cfilearchiver.cpp:41), `pack_file()`, `add_directory()` (dir-marker writer, cfilearchiver.cpp:126).
- **Core state:** `vp_header {char id[4]; int version; int diroffset;}` (cfilearchiver.cpp:24), global `Total_size` running offset.
- **Mechanism:** Recurses a directory, appends each non-zero-length file's bytes, records offset/size/name/mtime index entries, emits directory markers for subdirs, then patches the header. Mirror image of cfilesystem.cpp's reader.
- **Rare knowledge:** MINE — authoritative writer side of the VP format; confirms field order and the "skip 0-length files, they break the directory structure" quirk (cfilearchiver.cpp:81).
- **Deps:** None — pure libc, separate `main()`.
- **Extraction seams:** Fully self-contained utility; already isolated.
- **Port notes:** Light — just `struct stat`/`st_mtime` POSIX file walking; trivially portable.

### sound  (~5300 master: audiostr 1745, ds 1516, sound 1244, acm 592, ds3d 153)
- **Purpose:** Low-level audio — OpenAL buffer/source management, streaming music/voice, and ADPCM decode. DirectSound-shaped API, OpenAL underneath.
- **Entry points:** `sound/sound.cpp: snd_init()` (sound.cpp:99), `snd_load()` (sound.cpp:261), `snd_play()` (sound.cpp:445), `snd_play_3d()` (sound.cpp:519); `sound/ds.cpp: ds_init()` (ds.cpp:522), `ds_load_buffer()` (ds.cpp:309), `ds_parse_wave()` (ds.cpp:176); `sound/audiostr.cpp: WaveFile` class + audiostream service; `sound/acm.cpp: adpcm_parse_header()` (acm.cpp:309).
- **Core state:** `ds.cpp: sids[]`/`hids[]` (AL source/buffer ids), `ds_sound_device`/`ds_sound_context` (`alcOpenDevice`/`alcCreateContext`, ds.cpp:537); `sound.cpp: Sounds[]`; `audiostr.cpp: SDL_mutex *Global_service_lock`, `WaveFile m_pwavefile`.
- **Mechanism:** `ds_init` opens the AL device/context and pre-generates a pool of AL sources (`ds_init_channels`, ds.cpp:434). `snd_load` parses a WAV (PCM or ADPCM), decodes via acm.cpp, uploads with `alBufferData`, then `snd_play*` binds it to a free channel with 3D position/velocity. audiostr.cpp streams music/voice on an SDL timer thread guarded by SDL mutexes, refilling AL buffers in chunks.
- **Rare knowledge:** MINE — ADPCM decoder in acm.cpp is lifted "with permission from SDL_sound" (acm.cpp:79); note the on-disk `WAVEFORMATEX` is 18 bytes but the struct pads to 20 (acm.cpp:65, audiostr.cpp:52) — a real deserialization gotcha.
- **Deps:** cfile (WAV loading), math (3D positions), osapi/SDL, OpenAL.
- **Extraction seams:** ds.cpp+acm.cpp are a fairly clean AL wrapper; audiostr.cpp is more entangled (threads + WaveFile). Cut at the `ds_*` layer.
- **Port notes:** Heavy — entire backend swapped DirectSound→OpenAL (`OpenAL_ErrorCheck` macros, `alGenBuffers`/`alGenSources`). MIDI, real-time voice capture (rtvoice), and DirectSound capture (dscap) excised; `ds3d.cpp` reduced to a thin `alSource3f` shim (153 lines). SDL threads/mutexes replace Win32 in audiostr.

### gamesnd  (~1650 master: eventmusic 1386, gamesnd 263)
- **Purpose:** Higher-level sound layer — the `sounds.tbl` game-sound registry and the dynamic event/battle-music state machine.
- **Entry points:** `gamesnd/gamesnd.cpp: gamesnd_parse_soundstbl()`, `gamesnd_play_iface()` (gamesnd.cpp:27), `gamesnd_preload_common_sounds()`, `gamesnd_load_gameplay_sounds()` (gamesnd.cpp:61); `gamesnd/eventmusic.cpp: event_music_init()`, `event_music_do_frame()`.
- **Core state:** `gamesnd.cpp: Snds[MAX_GAME_SOUNDS]`, `Snds_iface[]`, `Snds_flyby[][]` (game_snd entries); `eventmusic.cpp: Pattern[]`/`SONG_*` transition tables (eventmusic.cpp:142), measure-count tables from `music.tbl`.
- **Mechanism:** gamesnd.cpp parses table files into `game_snd` records that wrap a sound.cpp handle plus default volume/priority. eventmusic.cpp is a pattern-graph sequencer: each song pattern declares its default successor (NRML→BTTL→VICT transitions), and per frame it decides when the current measure ends and which pattern to cross-fade to based on combat state.
- **Rare knowledge:** MINE-lite — the retail dynamic-music transition graph (SONG_NRML_1/AARV/BTTL/VICT successor tables) is fully encoded in eventmusic.cpp.
- **Deps:** sound (playback/streaming), cfile/parse (tables), gamesequence/mission for combat state.
- **Extraction seams:** eventmusic is conceptually standalone (a sequencer) but bound to audiostr for playback; gamesnd is a thin table layer over sound.
- **Port notes:** Moderate — music that was MIDI/CD-audio in retail now streams as digital via audiostr/OpenAL; `#include`s reference the OpenAL-shaped sound API.

### osapi  (~900 master: osregistry 313, osapi 267)
- **Purpose:** OS abstraction — SDL window/app lifecycle and a config store standing in for the Windows registry.
- **Entry points:** `osapi/osapi.cpp: os_init()` (osapi.cpp:61), `os_set_title()`, `os_cleanup()`, `os_poll()`; `osapi/osregistry.cpp: os_init_registry_stuff()` (osregistry.cpp:135), `os_config_read_string()`, `os_config_write_string()` (osregistry.cpp:189).
- **Core state:** `osapi.cpp: SDL_Window *sdl_window` (osapi.cpp:31); `osregistry.cpp` in-memory key/value list backed by a text config file (`config_file_name()`).
- **Mechanism:** `os_init` calls `SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)` and creates the window. osregistry replaces `RegOpenKey`/`RegQueryValue` with a flat `key=value` text file read/written via `fopen` (osregistry.cpp:92/115).
- **Rare knowledge:** — mundane, but the "Win32 registry replaced by a plain key=value text file" note (osregistry.cpp:22) is the canonical port-decision marker.
- **Deps:** SDL, globalincs; consumed by nearly everything at boot.
- **Extraction seams:** Small and liftable; it is the SDL/registry shim boundary itself.
- **Port notes:** Heavy by nature — entire file is the port surface. Retail used real Win32 window + registry [retail]; here SDL window + text-file config. `monopub.h`/`outwnd.h` (Win debug-window) headers remain but inert.

### math  (~5100 master: vecmat 2617, fvi 1417, spline 295, floating 156, staticrand 104, fix 29)
- **Purpose:** Core math — vectors/matrices, fixed-point, RNG, splines, and find-vector-intersection collision primitives.
- **Entry points:** `math/vecmat.cpp: vm_vec_normalize()` (vecmat.cpp:408), `vm_vec_mag()` (vecmat.cpp:278), `vm_angles_2_matrix()`, `vm_matrix_x_matrix()`; `math/fvi.cpp: fvi_ray_sphere()`, `fvi_polyedge_sphereline()`; `math/fix.cpp: fixmul()`/`fixdiv()`; `math/floating.cpp: frand()` (floating.cpp:102), `fl_isqrt_c()`; `math/staticrand.cpp: static_rand()` (staticrand.cpp:29).
- **Core state:** `staticrand.cpp: Semirand[SEMIRAND_MAX]` seeded table (staticrand.cpp:14); `floating.cpp: fl_magic = 0x59C00000` (2^51+2^52 float-int magic); `iSqrt[]` inverse-sqrt lookup table.
- **Mechanism:** vecmat.cpp is the bulk — `*_quick` variants trade accuracy for speed. static_rand hashes an index into three `Semirand` slots XORed together for reproducible per-object randomness (staticrand.cpp:40). fix.cpp does Q16.16 fixed point via 64-bit intermediates (`fixmul` = `(a*b)>>16`).
- **Rare knowledge:** MINE — the x87 float-trick helpers are here but neutered: the fast inverse-sqrt (`fl_isqrt_c`) and its lookup-table builder are commented out and replaced by `1.0f/sqrt()` (floating.cpp:57), and the `fl2f`/`float2int` "add magic constant, reinterpret bits" tricks survive only as commented blocks (floating.cpp bottom). The `fl_magic = 0x59C00000` constant is the surviving artifact. `frand()` carries the port's RNG-overflow fix.
- **Deps:** Nearly none — globalincs/pstypes only. fvi uses vecmat.
- **Extraction seams:** Very liftable — vecmat/fix/staticrand/floating are freestanding numeric code with almost no dependencies. Cut anywhere.
- **Port notes:** Heavy in floating.cpp — the frand fix is explicit: retail divided `rand()` by `RAND_MAX+1` assuming MSVC's 0x7FFF; glibc's `RAND_MAX==INT_MAX` overflowed the sum to INT_MIN making every frand negative, so the port masks `myrand() & 0x7fff` (floating.cpp:104-108) [retail]. x87 asm inverse-sqrt disabled for portability (`fl_isqrt_asm` call commented out).

### globalincs  (~1900 master: pstypes.h 17K, systemvars 520, version 100, alphacolors 90, crypt/debug)
- **Purpose:** Project-wide primitives — base typedefs/macros, global game vars, version info, palette colors, and password crypt.
- **Entry points:** `globalincs/systemvars.cpp` (globals + `set_detail_level()` etc.); `globalincs/version.cpp: get_version_string()`; `globalincs/alphacolors.cpp: init_alphacolors()`; `globalincs/crypt.cpp: crypt_string()`; `globalincs/pstypes.h` (typedefs).
- **Core state:** `systemvars.cpp: Game_detail_level`, `Framecount`, `Skill_level`, detail presets; `pstypes.h: vector`/`matrix`/`angles`/`fix`/`ubyte` typedefs; `Int3()`/`Assert` macros; `linklist.h` intrusive list macros.
- **Mechanism:** Header-heavy; pstypes.h is the type substrate every other file includes. systemvars.cpp holds cross-cutting global state and detail-level tables.
- **Rare knowledge:** — mundane substrate; `crypt.cpp`/`crypt.h` share the `jcrypt` algorithm used by the cryptstring tool.
- **Deps:** None (it is the base layer).
- **Extraction seams:** pstypes.h must travel with anything you lift; otherwise not a "subsystem" so much as shared headers.
- **Port notes:** Moderate — pstypes.h is where Win typedefs (DWORD/`__int64`) get remapped for GCC/Linux (`longlong`, `MulDiv` shim used by fix.cpp).

### localization  (~1570 master: localize 1352, fhash 215)
- **Purpose:** Runtime string localization — the `XSTR()` externalized-string table and its lookup hash.
- **Entry points:** `localization/localize.cpp: lcl_init()`, `lcl_ext_localize()`, `lcl_ext_get_text()` (localize.cpp:123), `lcl_ext_get_id()`, `lcl_translate_dir()`; `localization/fhash.cpp: fhash_add_str()`/`fhash_get()`.
- **Core state:** `localize.cpp: Xstr_table[XSTR_SIZE]` (XSTR_SIZE=1570, localize.cpp:66) of `lcl_xstr`; current `Lcl_current_lang`; `tstrings.tbl` scan tables.
- **Mechanism:** Each user-facing string is written `XSTR("English default", id)`; at load `lcl_ext_localize` parses the `id` and swaps in the localized text from `tstrings.tbl` (indexed by id), falling back to the embedded default. fhash.cpp accelerates name→entry lookup during table parse.
- **Rare knowledge:** — mostly mundane; the XSTR id-vs-default parsing (localize.cpp:120-125) is the one subtlety, and cfile's localized-filename resolution hooks into `lcl_translate_dir`.
- **Deps:** cfile (loads tstrings.tbl), parse, globalincs.
- **Extraction seams:** Fairly self-contained string service; liftable if you stub cfile for the table load.
- **Port notes:** Light — text-table parsing; minor `char`/wide-char and path handling adjustments.

### cmdline  (~304 master)
- **Purpose:** Command-line parsing into the global `Cmdline_*` flags read across the engine.
- **Entry points:** `cmdline/cmdline.cpp: os_init_cmdline()` (cmdline.cpp:174), `parse_cmdline()`.
- **Core state:** the `Cmdline_*` globals — `Cmdline_window`, `Cmdline_opengl`, `Cmdline_freespace_no_sound`, `Cmdline_freespace_no_music`, `Cmdline_use_last_pilot`, `Cmdline_gimme_all_medals` (cmdline.cpp:42-51).
- **Mechanism:** Tokenizes the command line, sets matching global flags; other subsystems branch on those globals at init.
- **Rare knowledge:** — mundane, but a quick map of every supported switch.
- **Deps:** globalincs only.
- **Extraction seams:** Fully self-contained.
- **Port notes:** Light — `Cmdline_window`/`Cmdline_opengl` reflect the SDL/OpenGL port target; retail Glide/D3D switches gone.

### playerman  (~3000 master: playercontrol 1814, managepilot 1173)
- **Purpose:** Pilot files and player-object control — persisting the `.plr` pilot and driving the player ship each frame.
- **Entry points:** `playerman/managepilot.cpp: read_pilot_file()` (managepilot.cpp:324), `write_pilot_file()` / `write_pilot_file_core()` (managepilot.cpp:624), `verify_pilot_file()` (managepilot.cpp:101); `playerman/playercontrol.cpp: read_player_controls()`, `player_control_reset_ci()`, `do_thrust_keys()`.
- **Core state:** `player.h: player` struct (callsign, stats, hud config, multi options); `managepilot.cpp: PLR_FILE_ID = 'FPSF'` (stored as "FSPF"), `CURRENT_PLAYER_FILE_VERSION = 140` (managepilot.cpp:32); `Player`/`Players[]`.
- **Mechanism:** `read_pilot_file` opens `<callsign>.plr` via cfile in the single/multi players dir, checks the `'FPSF'` id and version, then reads fields with versioned `cfread_*` calls (many `if (version >= N)` gates). playercontrol.cpp samples io/keycontrol each frame into a `control_info` and feeds it to physics.
- **Rare knowledge:** MINE — the `.plr` binary layout: 4-byte `'FPSF'` magic (byte-swapped to "FSPF" on disk), int version, then a version-gated field stream (scoring struct went from int→ushort alltime-kills at v3, squad-logo at v2, hud-config resize at v130; version history table at managepilot.cpp:37-43). FS2-demo (v135) and FS1-medal-in-plr legacy handling at managepilot.cpp:588.
- **Deps:** cfile (I/O), io/keycontrol (input), physics, ship, stats/scoring, hud config.
- **Extraction seams:** managepilot.cpp is a fairly self-contained serializer (cut at cfile); playercontrol.cpp is tightly bound to physics/ship and not liftable alone.
- **Port notes:** Moderate — endianness matters here: the `'FPSF'`/"FSPF" note and raw struct writes are byte-order-sensitive on the ported target; watch fixed-size `cfwrite` of structs. Known issue: RLE decoder overrun surfaces at pilot-file work (tracked in project notes).

### anim  (~2300 master: animplay 1143, packunpack 1154)
- **Purpose:** `.ani` animation format — the RLE frame codec plus playback/instancing and blitting.
- **Entry points:** `anim/packunpack.cpp: unpack_frame()` (RLE decoder), `unpack_frame_from_file()`, `pack_key_frame()`/`packer` (encoder, packunpack.cpp:325), `init_anim_instance()`; `anim/animplay.cpp: anim_play()`, `anim_render_all()`, `anim_get_next_frame()`.
- **Core state:** `packunpack.h: anim` (packunpack.h:37 — width/height/palette/keyframes), `anim_instance` (packunpack.h:69 — per-playback cursor, `data`/`file_offset`), `key_frame` (packunpack.h:27); `packer_code = PACKER_CODE` (packunpack.cpp:17).
- **Mechanism:** Two RLE variants: `PACKING_METHOD_RLE_KEY` uses a `packer_code` escape byte + run length (packunpack.cpp:329), `PACKING_METHOD_STD_RLE_KEY` uses a high-bit `STD_RLE_CODE` flag in the count byte (packunpack.cpp:386). Decode walks runs writing pixels, optionally applying a `palette_translation` LUT and up-converting to `bpp`. Frames may be keyframes (full) or delta; instances can decode from an in-memory buffer or stream from file (`unpack_frame_from_file`).
- **Rare knowledge:** MINE — the full ANI RLE codec (both packing methods, the escape-byte/high-bit encodings, keyframe deltas, palette-translate + bpp upconvert) is entirely here. This is a prime mining target and the codec noted for a decode-overrun risk.
- **Deps:** cfile (frame streaming), bmpman/graphics (blit target), palman.
- **Extraction seams:** Very liftable — packunpack.cpp is a near-freestanding codec; cut it from animplay.cpp (the graphics-bound playback half).
- **Port notes:** Watch the RLE decode-overrun known issue — the encoder guards `packed_size + N >= max` (packunpack.cpp:363,399) but the decode path is where the retail overrun surfaces; verify bounds when porting. Otherwise endianness of frame headers.

### cutscene  (~622 master: cutscenes.cpp)
- **Purpose:** The cutscene/tech-room movie menu — lists viewable `.mve` cutscenes, handles CD-check, and (would) launch playback.
- **Entry points:** `cutscene/cutscenes.cpp: cutscene_init()` (cutscenes.cpp:41), `cutscenes_screen_play()` (cutscenes.cpp:288 — the menu loop), `cutscenes_validate_cd()` (cutscenes.cpp:217), `cutscene_mark_viewable()` (cutscenes.cpp:105), `cutscenes_get_cd_num()`.
- **Core state:** `Cutscenes[MAX_CUTSCENES]` (cutscene_info), `Num_cutscenes`, `Cutscenes_viewable`, layout tables `Cutscene_list_coords[]`.
- **Mechanism:** Parses `cutscenes.tbl`, tracks which the player has unlocked, and renders a selectable list; selecting one calls `cutscenes_validate_cd()` then `movie_play(full_name)` (cutscenes.cpp:301).
- **Rare knowledge:** MINE (by absence) — the note "cutscene decodes MVE (ffmpeg in this port)" does NOT match the tree: `movie.h` is commented out (cutscenes.cpp:18), `movie_play` is undefined project-wide, and there is no ffmpeg/avcodec anywhere. MVE decode is excised/stubbed, not ffmpeg-ported; this file is only the menu shell around a dead playback call.
- **Deps:** cfile/parse (tables), ui/menuui (list rendering), popup (CD prompt), the absent movie player.
- **Extraction seams:** Self-contained menu; the interesting decode dependency it points at simply isn't present.
- **Port notes:** Heavy excision, not a port — the entire MVE decoder is gone; `movie_play` calls are live (cutscenes.cpp:301) but unresolved/dead. Flag as a stubbed subsystem.

### debugconsole  (~711 master: console.cpp)
- **Purpose:** In-game debug console — registers `dc_*` commands and parses typed input.
- **Entry points:** `debugconsole/console.cpp: debug_console()` (console.cpp:556, the modal loop taking a per-caller `_func`), `dc_get_arg()` (console.cpp:242), `dc_printf()` (console.cpp:520).
- **Core state:** command registry (DCF macros), argument-parse state consumed by `dc_get_arg(ARG_*)`, console text buffer.
- **Mechanism:** `debug_console()` runs a mini REPL reading keystrokes; commands defined elsewhere via `DCF(...)` register handlers, which pull typed tokens through `dc_get_arg` (ARG_INT/ARG_FLOAT/ARG_ANY) and print via `dc_printf`.
- **Rare knowledge:** — mundane debug tooling.
- **Deps:** io/key (input), graphics (text), globalincs.
- **Extraction seams:** Fairly self-contained but only useful with the DCF commands scattered across the codebase.
- **Port notes:** Light — key input via the SDL-backed io/key; otherwise unchanged. (Project note: `gamestubs_gen.cpp` leaves debug_console as the one remaining stub; port later.)

### cryptstring  (~63 master)
- **Purpose:** Standalone command-line tool that emits the obfuscated hash of a string (used to generate crypted passwords/keys for tables).
- **Entry points:** `cryptstring/cryptstring.cpp: main()` (cryptstring.cpp:15), `jcrypt()` (cryptstring.cpp:41).
- **Core state:** `static char cryptstring[CRYPT_STRING_LENGTH+1]` output buffer; shares `CRYPT_STRING_LENGTH`/`jcrypt` with globalincs/crypt.
- **Mechanism:** `jcrypt` is a tiny non-cryptographic mixer: for each output byte it XOR-accumulates over input chars with a rolling `plainstring[i%(t+1)]` term, folds mod 90, and biases +33 into printable ASCII (cryptstring.cpp:52-58); truncates/tails input to `CRYPT_STRING_LENGTH`.
- **Rare knowledge:** MINE — this is the exact obfuscation algorithm the engine uses to verify crypted strings (mirrors globalincs/crypt.cpp `jcrypt`); trivial to reproduce.
- **Deps:** globalincs/crypt.h only.
- **Extraction seams:** Fully self-contained (own `main`); already a standalone utility.
- **Port notes:** Light — pure libc; portable as-is.
