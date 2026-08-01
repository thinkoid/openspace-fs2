# The reconciler: the NATIVE mission world presented by Godot -- the
# migration plan's ownership table, live. libfs2 owns everything (game-path
# mission load, retail AI, arrivals, the player's flight); this scene owns
# presentation only: it instances a Ship per snapshot record (keyed by
# retail's object signature), applies transforms each tick, and never feeds
# state back. The player flies THROUGH the boundary -- controls in via
# step(), transform out via snapshot(), same as every other ship.
#
#     godot --path inspect -- world <mission.fs2> <assets-dir> [game-root]
#
# <assets-dir> holds converted GLBs by POF stem (pof2glb); a class with no
# GLB flies as a wireframe-gray box so the world stays honest about what it
# knows. Controls: mouse steers (click to grab), A/Z throttle, \ full,
# Backspace zero, Tab afterburner, Q/E roll, T target, M match speed
# (retail's tracking mode; manual throttle cancels), V view (pilot's
# seat by default, chase on toggle), H hud, Esc quit.
#
# This scene IS the lesson and the battlefield: weapons (slice 3),
# training messages and directives (slice 4), radio chatter, the combat
# HUD (bracket, monitor, radar -- radar.gd's art on native data), warp
# flashes and the sound seam. The GDScript-era mission.tscn folded in
# 2026-07-31; the retired sims remain as specs beside their gates.
extends Node3D

const ShipClass := preload("res://ship.gd")
const SoundBankClass := preload("res://sound.gd")
const RadarClass := preload("res://radar.gd")

# retail names for the keys the training sexps watch (key-pressed "a"),
# forwarded through key_mark so Control_config[].used carries the truth.
# The census over Training-1/2/3 + the range: a t M H E C . Tab \
# Backspace, plus Shift-/Alt- combinations built at forward time.
# Training-1's "Fly within 125 meters" gates on key-pressed "a" AND the
# distance -- a missing forward here stalled the lesson (field-reported).
const KEY_NAMES := {
    KEY_A: "a",
    KEY_T: "t",
    KEY_M: "M",
    KEY_H: "H",
    KEY_E: "E",
    KEY_C: "C",
    KEY_R: "R",
    KEY_J: "J",
    KEY_SLASH: "/",
    KEY_PERIOD: ".",
    KEY_TAB: "Tab",
    KEY_BACKSLASH: "\\",
    KEY_BACKSPACE: "Backspace",
}

var sim                       # FS2 (libfs2) instance
var ships_root: Node3D
var ships := {}               # signature -> {node, is_ship, kind, radius}
var flashes: Array[Dictionary] = []   # transient art: {node, deadline}
var fx_cache := {}            # ani stem -> {tex, cols, rows, frames, fps},
                              # or null where the bake is missing
var sky_root: Node3D          # the authored backdrop, riding the camera
var key_light: DirectionalLight3D
var player_shield: Array = []
var player_shield_max := 0.0
var player_hull_frac := 1.0
var player_sig := -1

var throttle := 0.0
var cam: Camera3D
var hud: CanvasLayer
var hud_left: Label
var ticker: Label
var ticker_lines: Array[String] = []
var directives: Label
var training_msg: Label
var chatter: Label
var chatter_lines: Array[Dictionary] = []   # {line, deadline}
var radar: Control                          # the retired file's art, live
var overlay: Control                        # the HUD symbology, drawn 2d
var target_monitor: Label
var target_sig := -1                        # hud_state's target_signature
var target_rec := {}                        # its snapshot record this frame
var lead_speed := 0.0                       # the primary's muzzle speed
var player_vel := Vector3.ZERO              # FS2 frame, lead solution input
var player_speed := 0.0                     # forward speed, the tape's needle
var player_energy_frac := 1.0               # gun reserve fraction
var player_burner_frac := 1.0               # afterburner fuel fraction
var target_range := 0.0                     # to the target, meters
var target_closure := 0.0                   # smoothed d(range)/dt, m/s
var hud_font: SystemFont                    # overlay text (Iosevka)
var player_team := 0                        # for hostile/friendly coloring
var first_person := true                    # V toggles the chase camera
var aim_from := Vector3.ZERO                # boresight ray for the reticle,
var aim_dir := Vector3.FORWARD              # godot frame
var player_max_speed := 0.0                 # match-speed's denominator
var match_target := false                   # M: retail's tracking mode
var sounds                     # SoundBank, voice playback
var last_text := ""
var msg_deadline := 0
var engine_hum: AudioStreamPlayer
var player_pos := Vector3.ZERO # FS2 frame, for sound attenuation

# positioned sounds attenuate by distance to the player and cull beyond
# earshot -- without this, flak eight klicks away arrives as random
# full-volume bursts (field-reported as "static")
const SND_FULL_RANGE := 150.0
const SND_CULL_RANGE := 3000.0

var assets_dir := ""
var mission_name := ""
var game_root := ""

# the campaign (boundary slice 3): non-empty campaign_name puts the
# scene in campaign mode -- Alt-J ends the mission into the debrief
# overlay (the sim freezes, the verdict shows), Enter accepts and loads
# the branch's pick in place, L takes the offered side loop
var campaign_name := ""
var debriefing := false
var debrief_data := {}
var debrief_panel: Control

var mouse_accum := Vector2.ZERO
var mouse_grabbed := false
const MOUSE_SENS := 0.05

func _ready() -> void:
    var args := OS.get_cmdline_user_args()
    # tolerate both the dispatched form (world <mission> <assets> [root])
    # and a direct scene run without the mode word
    if args.size() > 0 and args[0] == "world":
        args = args.slice(1)

    # campaign form: `campaign <name> <assets-dir> [game-root]` -- the
    # boundary resolves the mission; plain form flies args[0] directly
    if args.size() > 0 and args[0] == "campaign":
        if args.size() < 3:
            _fatal("usage: godot --path inspect -- world campaign <name> <assets-dir> [game-root]")
            return
        campaign_name = args[1]
        args = args.slice(1)
    elif args.size() < 2:
        _fatal("usage: godot --path inspect -- world <mission.fs2> <assets-dir> [game-root]")
        return

    mission_name = args[0]
    assets_dir = args[1]

    var root := args[2] if args.size() > 2 else OS.get_environment("FS2_GAME_ROOT")
    if root.is_empty():
        root = ProjectSettings.globalize_path("res://") + "../../rundir"

    # CLI paths may be shell-relative (`./build/glb`), but godot --path
    # has already chdir'd into the project by now -- resolve them against
    # the launch shell's own $PWD (field-reported: the Instructor flew as
    # a gray box because ./build/glb resolved under inspect/)
    var launch := OS.get_environment("PWD")
    if not launch.is_empty():
        if assets_dir.is_relative_path():
            assets_dir = launch.path_join(assets_dir).simplify_path()
        if root.is_relative_path():
            root = launch.path_join(root).simplify_path()

    game_root = root

    if not _load_libfs2():
        return
    sim = ClassDB.instantiate("FS2")

    if not campaign_name.is_empty():
        if not sim.load_campaign(root, campaign_name):
            _fatal("campaign load failed: %s (root %s)" % [campaign_name, root])
            return
        mission_name = sim.current_mission()
        if mission_name.is_empty():
            _fatal("campaign %s is already complete" % campaign_name)
            return

    if not sim.load(root, mission_name, 42):
        _fatal("native load failed: %s (root %s)" % [mission_name, root])
        return

    ships_root = Node3D.new()
    ships_root.name = "Ships"
    add_child(ships_root)

    _setup_camera()
    _setup_lights()
    _setup_starfield()
    _setup_backdrop()
    _setup_hud()

    sounds = SoundBankClass.new()
    add_child(sounds)
    sounds.setup(root)

    # the player's engine hum: a scene-owned loop on the tables' own
    # SND_ENGINE wav (retail runs it as an object-linked looping sound, a
    # lifecycle the one-shot event seam doesn't carry -- the ship's OWN
    # hum is presentation's to keep)
    engine_hum = AudioStreamPlayer.new()
    add_child(engine_hum)
    var hum = sounds.stream_of(sim.sound_name(4))   # gamesnd.hh SND_ENGINE
    if hum:
        if hum is AudioStreamWAV:
            hum.loop_mode = AudioStreamWAV.LOOP_FORWARD
            hum.loop_end = hum.data.size() / 2
        engine_hum.stream = hum
        engine_hum.volume_db = -80.0
        engine_hum.play()

    print("world: %s native, root %s" % [mission_name, root])

# a stream still playing at exit leaks its playback pair past ObjectDB
# cleanup (the "N instances leaked" warning); stop() alone races the mix
# thread, so the stream references drop too -- silence AND let go
func _exit_tree() -> void:
    if engine_hum:
        engine_hum.stop()
        engine_hum.stream = null
    if sounds:
        sounds.voice.stop()
        sounds.voice.stream = null
        for p in sounds.pool:
            p.stop()
            p.stream = null
        sounds.cache.clear()

func _fatal(msg: String) -> void:
    printerr("world: " + msg)
    get_tree().quit(1)

# same contract as fly.gd: the build tree's generated fs2.gdextension via
# GDExtensionManager; FS2_GDEXT overrides
func _load_libfs2() -> bool:
    if ClassDB.class_exists("FS2"):
        return true
    var gdext := OS.get_environment("FS2_GDEXT")
    if gdext.is_empty():
        gdext = ProjectSettings.globalize_path("res://") \
            + "../build/libfs2/fs2.gdextension"
    if GDExtensionManager.load_extension(gdext) != \
            GDExtensionManager.LOAD_STATUS_OK:
        _fatal("cannot load libfs2 (%s) -- build it, or set FS2_GDEXT"
               % gdext)
        return false
    return true

func _physics_process(delta: float) -> void:
    if sim == null:
        return

    # the debrief freezes the world: the sim holds, the overlay owns input
    if debriefing:
        return

    if not mouse_grabbed:
        mouse_grabbed = true
        Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

    # match mode: the set speed rides the target's actual speed, every
    # frame, until toggled off or overridden at the throttle
    if match_target and not target_rec.is_empty() and player_max_speed > 0.0:
        throttle = clampf((target_rec["vel"] as Vector3).length()
                          / player_max_speed, 0.0, 1.0)

    var burn := Input.is_key_pressed(KEY_TAB)
    var ci := {
        "pitch": clampf(_axis(KEY_UP, KEY_DOWN)
                        - mouse_accum.y * MOUSE_SENS, -1.0, 1.0),
        "heading": clampf(_axis(KEY_RIGHT, KEY_LEFT)
                          + mouse_accum.x * MOUSE_SENS, -1.0, 1.0),
        "bank": _axis(KEY_Q, KEY_E),
        "forward": throttle,
        "afterburner": burn,
        "fire_primary": mouse_grabbed and
            (Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
             or Input.is_key_pressed(KEY_CTRL)),
        "fire_secondary": mouse_grabbed and
            (Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT)
             or Input.is_key_pressed(KEY_SPACE)),
        "target_next": Input.is_key_pressed(KEY_T),
    }
    mouse_accum = Vector2.ZERO

    sim.step(delta, ci)

    for ev in sim.events():
        if ev["kind"] == "log":
            _tick("log %d: %s %s" % [ev["log_type"], ev["pname"], ev["sname"]])
        elif ev["kind"] == "destroyed" and not (ev["name"] as String).is_empty():
            _tick("destroyed: " + ev["name"])   # bolts expire nameless
        elif ev["kind"] == "sound":
            # the sim's own requests (guns, impacts, booms) through the
            # install's wavs, attenuated by distance when positioned
            var vol := 0.0
            if ev.has("pos"):
                var d: float = (ev["pos"] as Vector3).distance_to(player_pos)
                if d > SND_CULL_RANGE:
                    continue
                if d > SND_FULL_RANGE:
                    vol = -30.0 * (d - SND_FULL_RANGE) \
                        / (SND_CULL_RANGE - SND_FULL_RANGE)
            sounds.play_effect(ev["name"], vol)
        elif ev["kind"] == "message":
            # radio chatter: the line joins the window for retail's
            # text-length formula; the voice takes the one-speaker
            # channel (a new message cuts the old, retail's rule)
            var who: String = (ev["who"] as String).trim_prefix("#")
            var text: String = (ev["text"] as String).replace("$", "")
            chatter_lines.append({
                "line": "%s: %s" % [who, text],
                "deadline": Time.get_ticks_msec() + 1000 + 150 * text.length(),
            })
            if chatter_lines.size() > 4:
                chatter_lines.pop_front()
            if not (ev["wave"] as String).is_empty():
                sounds.play_voice(ev["wave"])

    # reconcile: the snapshot is the truth; nodes follow it
    var seen := {}
    var player_node: Node3D = null
    var player_rec := {}
    var ship_recs := []
    target_rec = {}
    for rec in sim.snapshot():
        var sig: int = rec["signature"]
        seen[sig] = true
        if rec.get("type", "ship") == "ship":
            ship_recs.append(rec)
        if sig == target_sig:
            target_rec = rec
        if not ships.has(sig):
            _spawn(sig, rec)
        var entry: Dictionary = ships[sig]
        var node: Node3D = entry["node"]

        # FS2 frame -> Godot frame at the visual boundary only
        var p: Vector3 = rec["pos"]
        var rv: Vector3 = rec["rvec"]
        var uv: Vector3 = rec["uvec"]
        var fv: Vector3 = rec["fvec"]
        node.position = Vector3(p.x, p.y, -p.z)
        node.basis = Basis(
            Vector3(rv.x, rv.y, -rv.z),
            Vector3(uv.x, uv.y, -uv.z),
            -Vector3(fv.x, fv.y, -fv.z))

        # the per-kind art that follows the record: a laser's color
        # cycles per retail (the boundary carries the live color), a
        # shockwave's ring rides the blast front, an explosion plays out
        # its flash-and-fade over the record's own lifetime
        var entry_kind: String = entry.get("kind", "")
        if entry_kind == "weapon":
            if entry.get("newborn", false):
                entry["newborn"] = false
            else:
                node.visible = true
            if node.has_meta("bolt_mat"):
                (node.get_meta("bolt_mat") as StandardMaterial3D) \
                    .albedo_color = rec.get("color", Color(1.0, 0.35, 0.25))
            if node.has_meta("stretch") and cam:
                # the velocity-stretched billboard: quad center between
                # head (the record pos) and tail, face to camera, long
                # axis = the flight axis' screen projection, length
                # collapsing to a fat blob end-on -- retail's
                # g3_draw_laser look, depth-tested
                var st: Dictionary = node.get_meta("stretch")
                # the TRUE flight direction, not the launch orientation:
                # bolts inherit the shooter's velocity and slide like a
                # thrown dart -- a streak drawn along fvec skews from the
                # path by up to ~9 degrees at fighter speeds (field
                # report); a streak is motion, so it follows motion
                var v3: Vector3 = rec["vel"]
                var dirw := Vector3(v3.x, v3.y, -v3.z)
                if dirw.length() < 1.0:
                    var f3: Vector3 = rec["fvec"]
                    dirw = Vector3(f3.x, f3.y, -f3.z)
                dirw = dirw.normalized()
                var bolt_len: float = st["len"]
                var center: Vector3 = node.position - dirw * (bolt_len * 0.5)
                var to_cam := (cam.global_position - center).normalized()
                var lx := dirw - dirw.dot(to_cam) * to_cam
                var lp := bolt_len * lx.length()
                var wq: float = st["r"] * 2.0
                if lx.length() < 0.05:
                    lx = to_cam.cross(Vector3.UP)
                    if lx.length() < 0.05:
                        lx = to_cam.cross(Vector3.RIGHT)
                lx = lx.normalized()
                lp = maxf(lp, wq * 1.4)
                node.position = center
                node.basis = Basis(lx * lp, to_cam.cross(lx).normalized() * wq,
                                   to_cam)
        elif node.has_meta("fx"):
            # the flipbook plays by age: frame from the sidecar's fps,
            # looped (warp) or held at the last (explosions burn out as
            # the record dies); a shockwave's quad also rides the live
            # blast radius
            var fx: Dictionary = node.get_meta("fx")
            var age: float = (Time.get_ticks_msec() - int(fx["born"])) / 1000.0
            var fr := int(age * int(fx["fps"]))
            if fx["loop"]:
                fr = fr % int(fx["frames"])
            else:
                fr = mini(fr, int(fx["frames"]) - 1)
            var cols: int = fx["cols"]
            var row := int(fr / float(cols))
            (fx["mat"] as StandardMaterial3D).uv1_offset = Vector3(
                float(fr % cols) / cols, float(row) / float(fx["rows"]), 0.0)
            if entry_kind == "shockwave":
                node.scale = Vector3.ONE * maxf(rec["radius"], 0.1)
        elif entry_kind == "shockwave":
            node.scale = Vector3.ONE * maxf(rec["radius"], 0.1)
        elif entry_kind == "fireball" and node.has_meta("born"):
            var age: float = (Time.get_ticks_msec()
                              - int(node.get_meta("born"))) / 1000.0
            var k := clampf(age / 1.2, 0.0, 1.0)
            node.scale = Vector3.ONE * (0.6 + 0.8 * k)
            var cm: StandardMaterial3D = node.get_meta("core_mat")
            var c := cm.albedo_color
            c.a = 0.7 * (1.0 - k)
            cm.albedo_color = c
            (node.get_meta("boom_light") as OmniLight3D).light_energy = \
                3.0 * (1.0 - k)

        # engine glow, field-calibrated: for the player, the burner truly
        # ON (the sim's flag, not the Tab key -- a refused engage must not
        # glow) or the engine above 0%; movers glow by their motion
        if entry["is_ship"]:
            var eng_on: bool
            if rec["player"]:
                eng_on = rec["afterburner"] or throttle > 0.0
            else:
                eng_on = (rec["vel"] as Vector3).length() > 0.5
            node.set_thrusters(eng_on)

            # the engine dress follows: flipbook frame by age, burner
            # variant by the sim's flag, glows lit with the engine
            var th = entry.get("thrust")
            if th != null:
                var burner: bool = rec["afterburner"]
                var fx: Dictionary = th["fxa"] if burner and th["fxa"] != null \
                    else th["fxn"]
                var age: float = (Time.get_ticks_msec()
                                  - int(th["born"])) / 1000.0
                var frames_n: int = fx["frames"]
                var fr := int(age * int(fx["fps"])) % maxi(frames_n, 1)
                var cols: int = fx["cols"]
                var off := Vector3(float(fr % cols) / cols,
                                   float(int(fr / float(cols)))
                                   / float(fx["rows"]), 0.0)
                for m in th["mats"]:
                    var sm := m as StandardMaterial3D
                    sm.albedo_texture = fx["tex"]
                    sm.uv1_scale = Vector3(1.0 / cols, 1.0 / fx["rows"], 1.0)
                    sm.uv1_offset = off
                var gtex = th["glow_tex_a"] if burner \
                    and th["glow_tex_a"] != null else th["glow_tex"]
                var frac: float
                if burner:
                    frac = 1.0
                elif rec["player"]:
                    frac = throttle
                else:
                    frac = clampf((rec["vel"] as Vector3).length()
                                  / maxf(rec.get("max_speed", 1.0), 1.0),
                                  0.0, 1.0)
                for g in th["glows"]:
                    (g["node"] as MeshInstance3D).visible = eng_on
                    if gtex != null:
                        var gm := g["mat"] as StandardMaterial3D
                        gm.albedo_texture = gtex["tex"]
                        gm.albedo_color = Color(1, 1, 1,
                                                0.25 + 0.75 * frac)

            # the shield taking hits: a drop in the quadrant total this
            # frame is a strike on the bubble
            var tot := 0.0
            for s: float in rec.get("shield", []):
                tot += s
            var prev: float = entry.get("shield_prev", tot)
            if tot < prev - 0.5:
                _shield_flash(node, entry["radius"])
            entry["shield_prev"] = tot

        if rec["player"]:
            player_sig = sig
            player_node = node
            player_rec = rec

    for sig in ships.keys():
        if not seen.has(sig):
            ships[sig]["node"].queue_free()
            ships.erase(sig)
            continue
        # wreckage arcs: the audible crackle (SND_DEBRIS_ARC_*,
        # debris.cc:382) gets its blue-white flash -- presentation-side
        # flicker, retail draws real arcs (model_add_arc)
        var n: Node3D = ships[sig]["node"]
        if n.has_meta("arc_mat"):
            var m: StandardMaterial3D = n.get_meta("arc_mat")
            m.emission = Color(0.5, 0.7, 1.0) * 2.0 if randf() < 0.06 \
                else Color(0, 0, 0)

    # transient art expires on its own clock; a parent freed by the
    # reconciler takes its flash with it
    var now_ms := Time.get_ticks_msec()
    var live_flashes: Array[Dictionary] = []
    for f: Dictionary in flashes:
        var fn: Node = f["node"]
        if not is_instance_valid(fn):
            continue
        if now_ms >= int(f["deadline"]):
            fn.queue_free()
            continue
        live_flashes.append(f)
    flashes = live_flashes

    if player_node:
        # own hull out of the pilot's eyes; back for the chase view
        player_node.visible = not first_person
        _update_camera(delta, player_node,
                       ships[player_sig]["radius"])
        # the sky rides the camera: infinitely far, parallax-free
        if sky_root and cam:
            sky_root.position = cam.global_position
        player_pos = player_rec["pos"]
        var vel: Vector3 = player_rec["vel"]
        var fv2: Vector3 = player_rec["fvec"]
        var fwd_speed := vel.dot(fv2)
        if absf(fwd_speed) < 0.05:
            fwd_speed = 0.0            # %6.1f would print "-0.0"
        player_speed = fwd_speed
        player_energy_frac = player_rec["weapon_energy"] \
            / maxf(player_rec["weapon_energy_max"], 1.0)
        player_burner_frac = player_rec["burner_fuel"] \
            / maxf(player_rec["burner_fuel_max"], 1.0)
        # ships only -- the node table also holds bolts, fireballs and
        # debris, and "7 ships" after a kill was a lie (field-reported)
        hud_left.text = "%s\n%d ships" % [mission_name, ship_recs.size()]

        # the hum follows the engine: silent at 0%, full voice under burn
        if engine_hum.stream:
            var output: float = absf(throttle)
            if player_rec["afterburner"]:
                output = 1.0
            engine_hum.volume_db = -80.0 if output <= 0.0 \
                else lerpf(-26.0, -8.0, output)

        _update_combat_hud(player_rec, ship_recs)

    _update_lesson()
    _update_chatter()

# the combat gauges: radar blips through the retired file's projection
# (the data path -- radar_plot_object -- runs natively; this is the art),
# the target monitor's readout, and a redraw request for the bracket
func _update_combat_hud(prec: Dictionary, ship_recs: Array) -> void:
    var ppos: Vector3 = prec["pos"]
    var rv: Vector3 = prec["rvec"]
    var uv: Vector3 = prec["uvec"]
    var fv: Vector3 = prec["fvec"]
    var pteam: int = prec["team"]
    player_team = pteam
    aim_from = Vector3(ppos.x, ppos.y, -ppos.z)
    aim_dir = Vector3(fv.x, fv.y, -fv.z)
    player_max_speed = prec.get("max_speed", 0.0)
    player_vel = prec["vel"]
    player_shield = prec.get("shield", [])
    player_shield_max = prec.get("shield_max", 0.0)
    player_hull_frac = prec["hull"] / maxf(prec["hull_max"], 1.0)

    var blips := []
    for rec in ship_recs:
        if rec["player"] or rec["dying"]:
            continue
        var d: Vector3 = (rec["pos"] as Vector3) - ppos
        blips.append({
            "disc": RadarClass.blip_disc(
                Vector3(rv.dot(d), uv.dot(d), fv.dot(d))),
            "dim": d.length() > 1500.0,      # retail's fallback range
            "hostile": rec["team"] != pteam,
            "target": rec["signature"] == target_sig,
        })
    radar.blips = blips
    radar.queue_redraw()

    if target_rec.is_empty():
        target_monitor.text = ""
        target_range = 0.0
        target_closure = 0.0
    else:
        # a dying ship's record carries the overkill (hull below zero)
        target_monitor.text = "%s\n%s\nhull %3d%%" % [
            target_rec["name"], target_rec["class"],
            maxi(0, int(100.0 * target_rec["hull"]
                        / maxf(target_rec["hull_max"], 1.0)))]
        # range and closure (positive = closing), smoothed for the box
        var rng := ((target_rec["pos"] as Vector3) - ppos).length()
        var dt := get_physics_process_delta_time()
        if dt > 0.0 and target_range > 0.0:
            target_closure = lerpf(target_closure,
                                   (target_range - rng) / dt, 0.15)
        target_range = rng

    overlay.queue_redraw()


# retail's engine dress: the cone submodels wear the species thruster
# flipbook as their texture (retail animates the thruster maps onto the
# cones), and every POF thruster point gets a glow billboard
# (thrusterglow). The family is species-picked, burner variants included:
# index = species*2 + afterburner (ship.cc:2996). Returns the animation
# state the reconciler drives, or null when the bakes are missing.
func _dress_thrusters(ship, rec: Dictionary):
    var species: int = int(rec.get("species", 0))
    var fxn = _fx("thruster0%d" % (species + 1))
    var fxa = _fx("thruster0%da" % (species + 1))
    var glow = _fx("thrusterglow0%d" % (species + 1))
    var glow_a = _fx("thrusterglow0%da" % (species + 1))
    if fxn == null or not "thruster_nodes" in ship:
        return null

    var out := {
        "mats": [], "glows": [], "fxn": fxn, "fxa": fxa,
        "glow_tex": glow, "glow_tex_a": glow_a,
        "born": Time.get_ticks_msec(),
    }

    for tn in ship.thruster_nodes:
        var stack: Array = [tn]
        while not stack.is_empty():
            var cur = stack.pop_back()
            for c in cur.get_children():
                stack.append(c)
            if cur is MeshInstance3D:
                var mat := StandardMaterial3D.new()
                mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
                mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
                mat.cull_mode = BaseMaterial3D.CULL_DISABLED
                mat.albedo_texture = fxn["tex"]
                mat.uv1_scale = Vector3(1.0 / fxn["cols"],
                                        1.0 / fxn["rows"], 1.0)
                (cur as MeshInstance3D).material_override = mat
                out["mats"].append(mat)

    if ship.data != null and glow != null:
        for bank in ship.data.thrusters:
            var pts: PackedVector3Array = bank["points"]
            var radii: PackedFloat32Array = bank["radii"]
            for i in pts.size():
                var gm := MeshInstance3D.new()
                var gq := QuadMesh.new()
                var r: float = radii[i] * 2.4
                gq.size = Vector2(r, r)
                var gmat := StandardMaterial3D.new()
                gmat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
                gmat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
                gmat.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
                gmat.albedo_texture = glow["tex"]
                gq.material = gmat
                gm.mesh = gq
                gm.position = pts[i]
                gm.visible = false
                ship.add_child(gm)
                out["glows"].append({ "node": gm, "mat": gmat })

    return out

# a debris chunk as retail renders it: the record names the source model
# and the submodel piece, so a hull chunk wears the ship's own hull (the
# GLB carries the pof's debris submodels; the Ship loader hides them at
# load, this carves the named one out and shows it alone, transform
# reset -- the sim's record IS the piece's world pose). Gray-box
# fallback where the model or piece is missing.
func _debris_node(rec: Dictionary) -> Node3D:
    var stem := (rec.get("pof", "") as String).get_basename().to_lower()
    var piece: String = rec.get("piece", "")
    if not stem.is_empty() and not piece.is_empty():
        var glb := assets_dir.path_join(stem + ".glb")
        if FileAccess.file_exists(glb):
            var m = ShipClass.new()
            if m.load_ship(glb):
                var pn := m.find_child(piece, true, false) as Node3D
                if pn != null:
                    var root := Node3D.new()
                    pn.owner = null      # else Godot warns: the piece's
                                         # owner stays the GLB scene root
                    pn.get_parent().remove_child(pn)
                    root.add_child(pn)
                    pn.transform = Transform3D.IDENTITY
                    pn.visible = true
                    m.queue_free()
                    ships_root.add_child(root)
                    return root
                m.queue_free()
    return _simple_node("debris", rec["radius"])

# the flight HUD, jet symbology (docs/hud-design.md): boresight where
# the guns POINT, the velocity vector where the ship GOES, a speed tape
# with the commanded caret, energy/burner bars, shield-quadrant glyphs,
# the target box with range and closure AT the box, an edge chevron when
# the target leaves the view, and the lead intercept. Every number is
# the sim's; only the drawing is the scene's.
const HUD_LINE := Color(0.4, 1.0, 0.5, 0.9)
const HUD_DIM := Color(0.4, 1.0, 0.5, 0.45)
const HUD_MATCH := Color(0.45, 0.8, 1.0, 0.95)

func _ui(k: float) -> float:
    return k * overlay.size.y / 1080.0

func _draw_hud() -> void:
    if cam == null:
        return
    var vp := overlay.size
    var fsz := int(_ui(26.0))

    # boresight: the gun line
    var aim := aim_from + aim_dir * 1000.0
    if not cam.is_position_behind(aim):
        var rp := cam.unproject_position(aim)
        overlay.draw_arc(rp, _ui(12.0), 0.0, TAU, 32, HUD_LINE, 1.5)
        for d: Vector2 in [Vector2.RIGHT, Vector2.LEFT, Vector2.UP,
                           Vector2.DOWN]:
            overlay.draw_line(rp + d * _ui(7.0), rp + d * _ui(16.0),
                              HUD_LINE, 1.5)

    # the flight-path marker: where the ship actually GOES -- the one
    # symbol a real HUD is built around; with inertia it rarely agrees
    # with the boresight
    var vel_g := Vector3(player_vel.x, player_vel.y, -player_vel.z)
    if vel_g.length() > 0.5:
        var vv := aim_from + vel_g.normalized() * 1000.0
        if not cam.is_position_behind(vv):
            var vc := cam.unproject_position(vv)
            overlay.draw_arc(vc, _ui(6.0), 0.0, TAU, 24, HUD_LINE, 1.5)
            overlay.draw_line(vc + Vector2(-_ui(15.0), 0),
                              vc + Vector2(-_ui(6.0), 0), HUD_LINE, 1.5)
            overlay.draw_line(vc + Vector2(_ui(6.0), 0),
                              vc + Vector2(_ui(15.0), 0), HUD_LINE, 1.5)
            overlay.draw_line(vc + Vector2(0, -_ui(13.0)),
                              vc + Vector2(0, -_ui(6.0)), HUD_LINE, 1.5)

    _draw_speed_tape(vp, fsz)

    if target_rec.is_empty():
        return

    var col := Color(1.0, 0.35, 0.3) if target_rec["team"] != player_team \
        else Color(0.35, 1.0, 0.4)

    var p3: Vector3 = target_rec["pos"]
    var v := Vector3(p3.x, p3.y, -p3.z)

    # off the view: an edge chevron pointing the way, range beside it
    var onscreen := not cam.is_position_behind(v)
    var p := Vector2.ZERO
    if onscreen:
        p = cam.unproject_position(v)
        onscreen = Rect2(Vector2.ZERO, vp).has_point(p)
    if not onscreen:
        var local: Vector3 = cam.global_transform.affine_inverse() * v
        var dir2 := Vector2(local.x, -local.y)
        if dir2.length() < 0.001:
            dir2 = Vector2.DOWN
        dir2 = dir2.normalized()
        var centre := vp * 0.5
        var edge_pt := centre + dir2 * (minf(vp.x, vp.y) * 0.5 - _ui(80.0))
        var perp := Vector2(-dir2.y, dir2.x)
        overlay.draw_colored_polygon(PackedVector2Array([
            edge_pt + dir2 * _ui(18.0), edge_pt + perp * _ui(9.0),
            edge_pt - perp * _ui(9.0)]), col)
        overlay.draw_string(hud_font,
                            edge_pt - dir2 * _ui(26.0) + Vector2(-_ui(36.0),
                                                                 _ui(9.0)),
                            "%.0f m" % target_range,
                            HORIZONTAL_ALIGNMENT_LEFT, -1, fsz, col)
        return

    # the box, sized by radius at distance; range + closure ride its side
    var edge := cam.unproject_position(
        v + cam.global_basis.x
        * float(ships.get(target_sig, {}).get("radius", 10.0)))
    var half := clampf((edge - p).length(), _ui(24.0), _ui(300.0))
    var arm := half * 0.5
    for corner: Vector2 in [Vector2(-1, -1), Vector2(1, -1), Vector2(-1, 1),
                            Vector2(1, 1)]:
        var c := p + corner * half
        overlay.draw_line(c, c - Vector2(corner.x * arm, 0), col, 2.0)
        overlay.draw_line(c, c - Vector2(0, corner.y * arm), col, 2.0)
    overlay.draw_string(hud_font, p + Vector2(half + _ui(10.0), 0),
                        "%.0f m" % target_range,
                        HORIZONTAL_ALIGNMENT_LEFT, -1, fsz, col)
    overlay.draw_string(hud_font,
                        p + Vector2(half + _ui(10.0), fsz + _ui(4.0)),
                        "%+.0f m/s" % target_closure,
                        HORIZONTAL_ALIGNMENT_LEFT, -1, fsz, col)

    # the lead intercept: put the boresight on the dot and the bolts
    # meet the target -- solved on RELATIVE motion, bolts inherit the
    # shooter velocity (|R + Vt| = s*t, smallest t > 0)
    if lead_speed <= 0.0:
        return
    var rp3 := (target_rec["pos"] as Vector3) - player_pos
    var rv3 := (target_rec["vel"] as Vector3) - player_vel
    var a := rv3.dot(rv3) - lead_speed * lead_speed
    var b := 2.0 * rp3.dot(rv3)
    var cc := rp3.dot(rp3)
    var t := -1.0
    if absf(a) < 1e-3:
        if absf(b) > 1e-6:
            t = -cc / b
    else:
        var disc := b * b - 4.0 * a * cc
        if disc >= 0.0:
            var sq := sqrt(disc)
            var t1 := (-b - sq) / (2.0 * a)
            var t2 := (-b + sq) / (2.0 * a)
            t = t1 if t1 > 0.0 else t2
    if t > 0.0 and t < 10.0:
        var lead: Vector3 = (target_rec["pos"] as Vector3) \
            + (target_rec["vel"] as Vector3) * t
        var lv := Vector3(lead.x, lead.y, -lead.z)
        if not cam.is_position_behind(lv):
            var lp := cam.unproject_position(lv)
            overlay.draw_circle(lp, _ui(3.5), col)
            overlay.draw_arc(lp, _ui(8.0), 0.0, TAU, 24, col, 1.5)

# the speed tape: 10 m/s ticks scrolling past the fixed needle, the
# commanded caret (throttle x max; match-mode blue when it drives)
func _draw_speed_tape(vp: Vector2, fsz: int) -> void:
    var x := vp.x * 0.16
    var half_h := _ui(220.0)
    var cy := vp.y * 0.5
    var span := 60.0
    # the tape floors at zero -- there is no reverse (retail's set-speed
    # never goes negative; the throttle is already clipped 0..1)
    var y_floor := minf(cy + player_speed / span * half_h, cy + half_h)
    overlay.draw_line(Vector2(x, cy - half_h), Vector2(x, y_floor),
                      HUD_DIM, 1.5)
    var m := maxi(0, int(floor((player_speed - span) / 10.0)) * 10)
    while m <= int(ceil((player_speed + span) / 10.0)) * 10:
        var y := cy + (player_speed - m) / span * half_h
        if y >= cy - half_h and y <= cy + half_h:
            var major := m % 30 == 0
            var w := _ui(16.0) if major else _ui(9.0)
            overlay.draw_line(Vector2(x - w, y), Vector2(x, y), HUD_DIM, 1.5)
            if major:
                overlay.draw_string(hud_font,
                                    Vector2(x - w - _ui(76.0),
                                            y + fsz * 0.35),
                                    "%d" % m, HORIZONTAL_ALIGNMENT_RIGHT,
                                    int(_ui(68.0)), fsz, HUD_DIM)
        m += 10
    overlay.draw_line(Vector2(x - _ui(20.0), cy), Vector2(x - _ui(2.0), cy),
                      HUD_LINE, 3.0)
    overlay.draw_string(hud_font, Vector2(x + _ui(12.0), cy + fsz * 0.35),
                        "%.0f" % player_speed, HORIZONTAL_ALIGNMENT_LEFT,
                        -1, fsz, HUD_LINE)
    var cmd := throttle * player_max_speed
    var cy2 := clampf(cy + (player_speed - cmd) / span * half_h,
                      cy - half_h, cy + half_h)
    var cc2 := HUD_MATCH if match_target else HUD_LINE
    overlay.draw_colored_polygon(PackedVector2Array([
        Vector2(x + _ui(2.0), cy2), Vector2(x + _ui(11.0), cy2 - _ui(6.0)),
        Vector2(x + _ui(11.0), cy2 + _ui(6.0))]), cc2)



# the chatter window: recent radio lines, each shown for its own
# text-length window (the voice may run longer -- lines scroll off,
# the voice plays out)
func _update_chatter() -> void:
    var now := Time.get_ticks_msec()
    var kept: Array[Dictionary] = []
    var lines: Array[String] = []
    for c in chatter_lines:
        if c["deadline"] >= now:
            kept.append(c)
            lines.append(c["line"])
    chatter_lines = kept
    chatter.text = "\n".join(lines)

# the lesson gauges, fed by the boundary's hud_state: the directives list
# (status-marked, key lines indented) and the training message, with
# Sensky's voice played once per message through the SoundBank
func _update_lesson() -> void:
    var h: Dictionary = sim.hud_state()
    target_sig = int(h.get("target_signature", -1))
    lead_speed = float(h.get("primary_speed", 0.0))

    var lines: Array[String] = []
    for d in h["directives"]:
        if d["key"]:
            lines.append("      " + (d["text"] as String).replace("$", ""))
            continue
        var mark := "  "
        match int(d["state"]):
            1: mark = "> "     # EVENT_CURRENT
            2: mark = "+ "     # EVENT_SATISFIED
            3: mark = "x "     # EVENT_FAILED
        lines.append(mark + (d["text"] as String).replace("$", ""))
    if lines.is_empty():
        directives.text = ""
    else:
        directives.text = "directives\n" + "\n".join(lines)

    # The message window is PRESENTATION-owned: headless retail can't time
    # a voice it cannot play (the sim exposes the text for as long as the
    # message is current), so the scene shows it while its own playback
    # runs, with retail's text-length fallback (missiontraining.cc:787)
    # when there is no wave.
    var text: String = h["training_text"]
    var voice: String = h["training_voice"]
    if text != last_text:
        last_text = text
        msg_deadline = Time.get_ticks_msec() + 1000 + 150 * text.length()
        if not voice.is_empty():
            sounds.play_voice(voice)

    var visible_now: bool = not text.is_empty() \
        and (sounds.voice.playing or Time.get_ticks_msec() < msg_deadline)
    training_msg.text = text.replace("$", "") if visible_now else ""

# a new signature enters the world: a bolt for weapons, a glow for
# fireballs, a tumbling chunk for debris, a Ship if the assets carry the
# class's GLB, an honest gray box otherwise
func _spawn(sig: int, rec: Dictionary) -> void:
    var kind: String = rec.get("type", "ship")
    if kind == "weapon":
        # a missile crosses with its POF and flies as a model; a laser
        # is a slug in its tbl color and size. Either way the birth
        # point IS the gun muzzle -- the flash goes there.
        var node: Node3D = null
        var stem := (rec.get("pof", "") as String).get_basename().to_lower()
        if not stem.is_empty():
            var glb := assets_dir.path_join(stem + ".glb")
            if FileAccess.file_exists(glb):
                var m = ShipClass.new()
                if m.load_ship(glb):
                    node = m
                    ships_root.add_child(node)
        if node == null:
            node = _bolt_node(rec)
        # born AT the muzzle, streak trailing through the cockpit for one
        # frame -- the flash owns the birth moment, the bolt shows from
        # its first step forward
        node.visible = false
        ships[sig] = { "node": node, "is_ship": false, "kind": "weapon",
                       "radius": rec["radius"], "newborn": true }
        _muzzle_flash(rec)
        return
    if kind == "shockwave":
        # the blast front: retail's own expanding-ring flipbook
        # (shockwave01), frame by age, size riding the record's live
        # radius; the torus stand-in only if the bake is missing
        var fx = _fx("shockwave01")
        var node: Node3D
        if fx != null:
            node = _flipbook_node(fx, 2.0, true, false)
        else:
            node = _simple_node("shockwave", 1.0)
        ships[sig] = { "node": node, "is_ship": false, "kind": "shockwave",
                       "radius": 1.0 }
        return
    if kind == "fireball" or kind == "debris":
        # the boundary tags fireballs: an arrival's warp effect is not
        # an explosion -- and names the flipbook it plays (the pof slot
        # carries the ani stem, fireball_art_name's crossing)
        var art := kind
        if rec.get("class", "") == "warp":
            art = "warp"
        var node: Node3D = null
        if art == "debris":
            node = _debris_node(rec)
        if art == "fireball" or art == "warp":
            var fx = _fx((rec.get("pof", "") as String).to_lower())
            if fx != null:
                if art == "warp":
                    # the vortex stands in the arrival plane: a fixed
                    # quad, the reconciler's basis turns it; loops while
                    # the effect lives
                    node = _flipbook_node(fx, rec["radius"] * 2.0, false,
                                          true)
                else:
                    node = _flipbook_node(fx, rec["radius"] * 2.0, true,
                                          false)
        if node == null:
            if art == "fireball":
                node = _explosion_node(rec["radius"])
            else:
                node = _simple_node(art, rec["radius"])
        ships[sig] = { "node": node, "is_ship": false, "kind": art,
                       "radius": rec["radius"] }
        return

    var stem := (rec["pof"] as String).get_basename().to_lower()
    var glb := assets_dir.path_join(stem + ".glb")

    var node: Node3D
    var is_ship := false
    var radius := 10.0
    if FileAccess.file_exists(glb):
        var ship = ShipClass.new()
        if ship.load_ship(glb):
            node = ship
            is_ship = true
            radius = maxf(ship.data.radius, 1.0)
    if node == null:
        push_warning("world: no GLB for %s (%s) -- box stand-in" %
                     [rec["class"], stem])
        var box := MeshInstance3D.new()
        var mesh := BoxMesh.new()
        mesh.size = Vector3(8, 4, 16)
        var mat := StandardMaterial3D.new()
        mat.albedo_color = Color(0.5, 0.5, 0.55)
        mesh.material = mat
        box.mesh = mesh
        node = box

    node.name = rec["name"]
    ships_root.add_child(node)
    ships[sig] = { "node": node, "is_ship": is_ship, "radius": radius }
    if is_ship:
        ships[sig]["thrust"] = _dress_thrusters(node, rec)
    _tick("arrived: %s (%s)" % [rec["name"], rec["class"]])

# the non-ship visuals: unshaded primitives sized by the sim's own radius
func _simple_node(kind: String, radius: float) -> Node3D:
    var mi := MeshInstance3D.new()
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    match kind:
        "shockwave":
            # a unit ring in the horizontal plane; the reconciler scales
            # it to the record's live blast radius every frame
            var tm := TorusMesh.new()
            tm.inner_radius = 0.85
            tm.outer_radius = 1.0
            mat.albedo_color = Color(1.0, 0.6, 0.3, 0.6)
            mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
            tm.material = mat
            mi.mesh = tm
        "warp":
            # the arrival flash: retail's warp effect is a plane normal
            # to the ship's approach -- a blue-white disc, facing the
            # record's fvec (the mesh turns; the node's basis stays the
            # reconciler's to set)
            var wm := CylinderMesh.new()
            wm.top_radius = maxf(radius, 1.0)
            wm.bottom_radius = wm.top_radius
            wm.height = 0.4
            mat.albedo_color = Color(0.5, 0.75, 1.0, 0.85)
            mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
            wm.material = mat
            mi.mesh = wm
            mi.rotation_degrees = Vector3(90, 0, 0)
            var wrap := Node3D.new()
            wrap.add_child(mi)
            ships_root.add_child(wrap)
            return wrap
        "debris":
            var db := BoxMesh.new()
            var s: float = clampf(radius, 0.5, 6.0)
            db.size = Vector3(s, s * 0.6, s * 1.4)
            mat.shading_mode = BaseMaterial3D.SHADING_MODE_PER_PIXEL
            mat.albedo_color = Color(0.35, 0.35, 0.38)
            mat.emission_enabled = true
            mat.emission = Color(0, 0, 0)
            db.material = mat
            mi.mesh = db
            mi.set_meta("arc_mat", mat)   # the crackle's visual half
    ships_root.add_child(mi)
    return mi

# the baked retail flipbooks (ani2png's atlases beside the models):
# stem -> {tex, cols, rows, frames, fps}, null cached for missing bakes
func _fx(stem: String) -> Variant:
    if fx_cache.has(stem):
        return fx_cache[stem]
    var dir := assets_dir.path_join("effects")
    var out = null
    var meta_path := dir.path_join(stem + ".json")
    if FileAccess.file_exists(meta_path):
        var side = JSON.parse_string(FileAccess.get_file_as_string(meta_path))
        if side is Dictionary:
            var img := Image.load_from_file(dir.path_join(side["atlas"]))
            if img:
                out = {
                    "tex": ImageTexture.create_from_image(img),
                    "cols": int(side["cols"]), "rows": int(side["rows"]),
                    "frames": int(side["frames"]), "fps": int(side["fps"]),
                    "w": int(side["width"]), "h": int(side["height"]),
                }
    fx_cache[stem] = out
    return out

# a retail flipbook on a quad -- additive, unshaded (the effects families
# are light on black); the material's UV window is one frame, and the
# reconciler advances uv1_offset by age. Billboards face the camera; the
# warp plane keeps its basis (visible from both sides).
func _flipbook_node(fx: Dictionary, size: float, billboard: bool,
                    loop: bool) -> MeshInstance3D:
    var mi := MeshInstance3D.new()
    var qm := QuadMesh.new()
    qm.size = Vector2(size, size)
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
    mat.albedo_texture = fx["tex"]
    mat.uv1_scale = Vector3(1.0 / fx["cols"], 1.0 / fx["rows"], 1.0)
    if billboard:
        mat.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
        mat.billboard_keep_scale = true
    else:
        mat.cull_mode = BaseMaterial3D.CULL_DISABLED
    qm.material = mat
    mi.mesh = qm
    mi.set_meta("fx", {
        "mat": mat, "cols": fx["cols"], "rows": fx["rows"],
        "frames": fx["frames"], "fps": fx["fps"], "loop": loop,
        "born": Time.get_ticks_msec(),
    })
    ships_root.add_child(mi)
    return mi

# a laser bolt in retail's own art: the body streak (@Laser Bitmap)
# stretched along the flight axis as crossed additive quads -- readable
# from any angle without a shader -- and the head glow (@Laser Glow) as
# a camera billboard tinted by the cycle color, which the reconciler
# follows per frame (the "bolt_mat" meta). Capsule fallback in the tbl
# color where the bake is missing.
func _bolt_node(rec: Dictionary) -> Node3D:
    var r: float = maxf(rec.get("laser_radius", 0.4), 0.15)
    var length: float = maxf(rec.get("laser_length", 6.0), 2.0 * r)
    var wrap := Node3D.new()

    var body = _fx((rec.get("laser_bitmap", "") as String)
                   .get_basename().to_lower())
    if body != null:
        # retail's g3_draw_laser stretches the sprite between the
        # PROJECTED head and tail -- a screen-space draw. The 3D
        # equivalent (depth-tested, unlike a HUD overlay) is a
        # velocity-stretched billboard: a unit quad whose basis the
        # reconciler rebuilds every frame -- face to the camera, long
        # axis along the flight axis' screen projection, length
        # collapsing toward a fat blob end-on. A plane merely
        # CONTAINING the axis is edge-on to your own fire and vanishes
        # (field-reported "not retail").
        var mi := MeshInstance3D.new()
        var qm := QuadMesh.new()
        qm.size = Vector2.ONE
        var mat := StandardMaterial3D.new()
        mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
        mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
        mat.cull_mode = BaseMaterial3D.CULL_DISABLED
        mat.albedo_texture = body["tex"]
        qm.material = mat
        mi.mesh = qm
        wrap.add_child(mi)
        wrap.set_meta("stretch", { "len": length, "r": r })

        var glow = _fx((rec.get("laser_glow", "") as String)
                       .get_basename().to_lower())
        if glow != null:
            # rides the HEAD end (+X/2 through the stretched basis);
            # billboard keep_scale stays false, so the parent's stretch
            # never inflates it. Head-circle proportions: bigger washes
            # the first-person view flat red (additive saturation at
            # muzzle distance).
            var gm := MeshInstance3D.new()
            gm.position = Vector3(0.5, 0, 0)
            var gq := QuadMesh.new()
            gq.size = Vector2(r * 2.6, r * 2.6)
            var gmat := StandardMaterial3D.new()
            gmat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
            gmat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
            gmat.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
            gmat.albedo_texture = glow["tex"]
            gmat.albedo_color = rec.get("color", Color.WHITE)
            gq.material = gmat
            gm.mesh = gq
            wrap.add_child(gm)
            wrap.set_meta("bolt_mat", gmat)
    else:
        var mi := MeshInstance3D.new()
        var cm := CapsuleMesh.new()
        cm.radius = r
        cm.height = length
        var mat := StandardMaterial3D.new()
        mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
        mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
        mat.albedo_color = rec.get("color", Color(1.0, 0.35, 0.25))
        cm.material = mat
        mi.mesh = cm
        mi.rotation_degrees = Vector3(90, 0, 0)
        wrap.add_child(mi)
        wrap.set_meta("bolt_mat", mat)

    ships_root.add_child(wrap)
    return wrap

# an explosion, presentation-owned: the sim's fireball record times and
# sizes it (the node lives exactly as long as the record); the art -- a
# fading core, one burst of embers, a dying light -- is the scene's
func _explosion_node(radius: float) -> Node3D:
    var root := Node3D.new()
    var r := maxf(radius, 1.0)

    # additive, not matte: the core must read as light -- an alpha disc
    # at explosion size paints the sky khaki (field-reported)
    var mi := MeshInstance3D.new()
    var sm := SphereMesh.new()
    sm.radius = r * 0.45
    sm.height = sm.radius * 2.0
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
    mat.albedo_color = Color(1.0, 0.55, 0.2, 0.7)
    sm.material = mat
    mi.mesh = sm
    root.add_child(mi)

    var p := GPUParticles3D.new()
    p.one_shot = true
    p.explosiveness = 1.0
    p.amount = 40
    p.lifetime = 1.2
    p.emitting = true
    var pm := ParticleProcessMaterial.new()
    pm.direction = Vector3.ZERO
    pm.spread = 180.0
    pm.initial_velocity_min = r
    pm.initial_velocity_max = r * 2.5
    pm.gravity = Vector3.ZERO
    pm.scale_min = 0.15
    pm.scale_max = 0.5
    pm.color = Color(1.0, 0.55, 0.2)
    p.process_material = pm
    var pmesh := SphereMesh.new()
    pmesh.radius = 0.35
    pmesh.height = 0.7
    var pmat := StandardMaterial3D.new()
    pmat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    pmat.albedo_color = Color(1.0, 0.6, 0.25)
    pmesh.material = pmat
    p.draw_pass_1 = pmesh
    root.add_child(p)

    var l := OmniLight3D.new()
    l.light_color = Color(1.0, 0.7, 0.3)
    l.light_energy = 3.0
    l.omni_range = r * 8.0
    root.add_child(l)

    root.set_meta("born", Time.get_ticks_msec())
    root.set_meta("core_mat", mat)
    root.set_meta("boom_light", l)
    ships_root.add_child(root)
    return root

# the gun flash: a weapon record is born AT the muzzle, so the birth
# position is the flash position -- brief, bright, gone
func _muzzle_flash(rec: Dictionary) -> void:
    var p: Vector3 = rec["pos"]
    var n := Node3D.new()

    # the weapon's own glow art as a soft billboard -- a solid sphere at
    # muzzle distance reads as a hard-edged sticker (field-capture
    # lesson); small additive sphere only if no glow is baked
    var glow = _fx((rec.get("laser_glow", "") as String)
                   .get_basename().to_lower())
    var mi := MeshInstance3D.new()
    if glow != null:
        var gq := QuadMesh.new()
        gq.size = Vector2(1.0, 1.0)
        var gmat := StandardMaterial3D.new()
        gmat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
        gmat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
        gmat.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
        gmat.albedo_texture = glow["tex"]
        # dim enough that the radial gradient survives additive
        # saturation -- full white bakes a poker chip
        gmat.albedo_color = Color(0.9, 0.7, 0.4)
        gq.material = gmat
        mi.mesh = gq
    else:
        var sm := SphereMesh.new()
        sm.radius = 0.35
        sm.height = 0.7
        var mat := StandardMaterial3D.new()
        mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
        mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
        mat.albedo_color = Color(1.0, 0.8, 0.45, 0.8)
        sm.material = mat
        mi.mesh = sm
    n.add_child(mi)

    var l := OmniLight3D.new()
    l.light_color = Color(1.0, 0.8, 0.4)
    l.light_energy = 2.0
    l.omni_range = 12.0
    n.add_child(l)

    n.position = Vector3(p.x, p.y, -p.z)
    ships_root.add_child(n)
    flashes.append({ "node": n, "deadline": Time.get_ticks_msec() + 50 })

# the shield shimmer: quadrant totals dropped this frame, so the bubble
# takes a hit -- a translucent shell over the hull, briefly (retail
# lights the struck mesh section; the whole-bubble flash is the honest
# approximation until shield geometry crosses)
func _shield_flash(node: Node3D, radius: float) -> void:
    var mi := MeshInstance3D.new()
    var sm := SphereMesh.new()
    sm.radius = maxf(radius, 1.0) * 1.15
    sm.height = sm.radius * 2.0
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.albedo_color = Color(0.45, 0.7, 1.0, 0.35)
    mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    sm.material = mat
    mi.mesh = sm
    node.add_child(mi)
    flashes.append({ "node": mi, "deadline": Time.get_ticks_msec() + 220 })

func _tick(line: String) -> void:
    ticker_lines.append(line)
    while ticker_lines.size() > 5:
        ticker_lines.pop_front()
    if ticker:
        ticker.text = "\n".join(ticker_lines)

static func _axis(pos: Key, neg: Key) -> float:
    return (1.0 if Input.is_key_pressed(pos) else 0.0) \
        - (1.0 if Input.is_key_pressed(neg) else 0.0)

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventMouseMotion:
        mouse_accum += event.relative
        return
    if event is InputEventMouseButton and event.pressed \
            and Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
        Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
        return
    if not (event is InputEventKey and event.pressed):
        return
    # the debrief overlay owns the keys while it is up
    if debriefing:
        match event.keycode:
            KEY_ENTER, KEY_KP_ENTER:
                _accept_debrief(false)
            KEY_L:
                if debrief_data.get("loop_offer", false):
                    _accept_debrief(true)
            KEY_ESCAPE:
                get_tree().quit()
        return
    # the training sexps watch retail key names; forward the press,
    # modifiers spelled the way translate_key_to_index reads them
    if sim != null and KEY_NAMES.has(event.keycode):
        var key_name: String = KEY_NAMES[event.keycode]
        if event.shift_pressed:
            key_name = "Shift-" + key_name
        if event.alt_pressed:
            key_name = "Alt-" + key_name
        sim.key_mark(key_name)
    match event.keycode:
        KEY_A:
            # snappedf: ±0.1 float steps leave 5.5e-17 residue at "0%",
            # and the glow's `throttle > 0` believes it (field-reported).
            # 0..1 only: retail's set-speed never goes negative -- the
            # physics knows reverse (max_rear_vel) but no player key
            # commands it. Manual throttle input cancels match mode.
            match_target = false
            throttle = snappedf(clampf(throttle + 0.1, 0.0, 1.0), 0.1)
        KEY_Z:
            match_target = false
            throttle = snappedf(clampf(throttle - 0.1, 0.0, 1.0), 0.1)
        KEY_0:
            match_target = false
            throttle = 0.0
        KEY_BACKSLASH:
            match_target = false
            throttle = 1.0
        KEY_BACKSPACE:
            match_target = false
            throttle = 0.0
        KEY_M:
            # match speed, retail's semantics: a MODE that tracks the
            # target's speed each frame until toggled off or overridden
            # by a manual throttle input (field-corrected from the
            # press-again-and-again snapshot)
            match_target = not match_target
        KEY_V:
            first_person = not first_person
        KEY_H:
            hud.visible = not hud.visible
        KEY_J:
            # end the mission into the debrief (campaign mode only -- a
            # lone mission has nowhere to go). Alt-J is retail's jump
            # binding; Shift-Super-J is the house alternative, because
            # the WM here owns Alt-J (field request)
            if (event.alt_pressed
                    or (event.shift_pressed and event.meta_pressed)) \
                    and not campaign_name.is_empty():
                _enter_debrief()
        KEY_ESCAPE:
            get_tree().quit()

# ----------------------------------------------------------------------
# the campaign flow: debrief overlay in, accept out, next mission in place

func _enter_debrief() -> void:
    debrief_data = sim.debrief()
    debriefing = true
    Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
    mouse_grabbed = false
    _build_debrief_panel()

func _build_debrief_panel() -> void:
    debrief_panel = Control.new()
    debrief_panel.set_anchors_preset(Control.PRESET_FULL_RECT)
    hud.add_child(debrief_panel)

    var dim := ColorRect.new()
    dim.color = Color(0.0, 0.02, 0.0, 0.85)
    dim.set_anchors_preset(Control.PRESET_FULL_RECT)
    debrief_panel.add_child(dim)

    var margin := MarginContainer.new()
    margin.set_anchors_preset(Control.PRESET_FULL_RECT)
    for side in ["left", "top", "right", "bottom"]:
        margin.add_theme_constant_override("margin_" + side, int(_ui(90.0)))
    debrief_panel.add_child(margin)

    var col := VBoxContainer.new()
    col.add_theme_constant_override("separation", int(_ui(14.0)))
    margin.add_child(col)

    var fsz := int(_ui(26.0))

    var title := Label.new()
    title.text = "DEBRIEFING -- %s" % mission_name
    _debrief_style(title, int(fsz * 1.3), HUD_LINE)
    col.add_child(title)

    # the goals, status spelled out; retail's colors by verdict
    for g in debrief_data.get("goals", []):
        var line := Label.new()
        var status: String = ["FAILED", "COMPLETE", "INCOMPLETE"][g["status"]]
        line.text = "  %-11s %s" % [status, g["text"]]
        var c := HUD_DIM
        if not g["invalid"]:
            c = Color(0.35, 1.0, 0.4) if g["status"] == 1 \
                else Color(1.0, 0.35, 0.3) if g["status"] == 0 else HUD_DIM
        _debrief_style(line, fsz, c)
        col.add_child(line)

    # the selected stages, scrollable prose
    var scroll := ScrollContainer.new()
    scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
    col.add_child(scroll)

    var stages := VBoxContainer.new()
    stages.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    stages.add_theme_constant_override("separation", int(_ui(18.0)))
    scroll.add_child(stages)

    for s in debrief_data.get("stages", []):
        var para := Label.new()
        para.text = s["text"]
        para.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
        para.size_flags_horizontal = Control.SIZE_EXPAND_FILL
        _debrief_style(para, fsz, HUD_LINE)
        stages.add_child(para)
        if not (s["recommendation"] as String).is_empty():
            var rec := Label.new()
            rec.text = s["recommendation"]
            rec.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
            rec.size_flags_horizontal = Control.SIZE_EXPAND_FILL
            _debrief_style(rec, fsz, HUD_DIM)
            stages.add_child(rec)

    # the verdict and the keys
    var verdict := Label.new()
    var next: String = debrief_data.get("next_mission", "")
    if next.is_empty():
        verdict.text = "CAMPAIGN COMPLETE      [Enter] finish"
    else:
        verdict.text = "next: %s      [Enter] accept" % next
        if debrief_data.get("loop_offer", false):
            verdict.text += "      [L] optional mission: %s" \
                % debrief_data.get("loop_desc", "")
    _debrief_style(verdict, fsz, HUD_LINE)
    col.add_child(verdict)

func _debrief_style(l: Label, size: int, color: Color) -> void:
    l.add_theme_font_override("font", hud_font)
    l.add_theme_font_size_override("font_size", size)
    l.add_theme_color_override("font_color", color)

func _accept_debrief(take_loop: bool) -> void:
    sim.accept(take_loop)

    var next: String = sim.current_mission()
    if next.is_empty():
        print("campaign %s complete" % campaign_name)
        get_tree().quit()
        return

    _reset_world()
    mission_name = next
    if not sim.load(game_root, next, 42):
        _fatal("native load failed: %s" % next)
        return
    _setup_backdrop()

    debrief_panel.queue_free()
    debrief_panel = null
    debriefing = false
    debrief_data = {}
    Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
    mouse_grabbed = true
    print("world: %s native (campaign %s)" % [mission_name, campaign_name])

# strip the scene back to mission-free: ship nodes, transient art, the
# authored sky (plus any secondary-sun lights it hung on the root), and
# every piece of per-mission HUD state. The art caches and sound bank
# survive -- they are mission-independent.
func _reset_world() -> void:
    for sig in ships:
        ships[sig]["node"].queue_free()
    ships.clear()

    for f in flashes:
        f["node"].queue_free()
    flashes.clear()

    if sky_root:
        sky_root.queue_free()
        sky_root = null
    for child in get_children():
        if child is DirectionalLight3D and child != key_light:
            child.queue_free()

    player_sig = -1
    player_shield = []
    player_hull_frac = 1.0
    target_sig = -1
    target_rec = {}
    target_range = 0.0
    target_closure = 0.0
    throttle = 0.0
    match_target = false

    ticker_lines.clear()
    chatter_lines.clear()
    ticker.text = ""
    chatter.text = ""
    directives.text = ""
    training_msg.text = ""
    last_text = ""
    msg_deadline = 0
    radar.blips = []

func _setup_camera() -> void:
    cam = Camera3D.new()
    cam.far = 20000.0
    add_child(cam)
    cam.position = Vector3(0, 4, 20)

func _update_camera(delta: float, node: Node3D, radius: float) -> void:
    var r := maxf(radius, 1.0)

    # the pilot's seat: rigid on the hull, eye a touch above center (the
    # hull itself is hidden in this view); V swaps to the chase camera
    if first_person:
        cam.position = node.position + node.basis * Vector3(0.0, r * 0.12, 0.0)
        cam.basis = node.basis
        return

    var target: Vector3 = node.position \
        + node.basis * Vector3(0.0, r * 0.6, r * 2.2)
    var k := 1.0 - exp(-6.0 * delta)
    cam.position = cam.position.lerp(target, k)
    cam.look_at(node.position + node.basis * Vector3(0, 0, -r * 4.0),
                node.basis.y)

func _setup_lights() -> void:
    key_light = DirectionalLight3D.new()
    key_light.rotation_degrees = Vector3(-35.0, 40.0, 0.0)
    add_child(key_light)
    var env := Environment.new()
    env.background_mode = Environment.BG_COLOR
    env.background_color = Color(0.02, 0.02, 0.04)
    env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
    env.ambient_light_color = Color(0.25, 0.25, 0.3)
    var we := WorldEnvironment.new()
    we.environment = env
    add_child(we)

func _setup_starfield() -> void:
    var mm := MultiMesh.new()
    mm.transform_format = MultiMesh.TRANSFORM_3D
    var s := SphereMesh.new()
    s.radius = 4.0
    s.height = 8.0
    s.radial_segments = 4
    s.rings = 2
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.albedo_color = Color(0.9, 0.9, 1.0)
    s.material = mat
    mm.mesh = s
    # the mission's own star count ($Num stars), retail's dial
    mm.instance_count = maxi(int(sim.num_stars()), 200)
    var rng := RandomNumberGenerator.new()
    rng.seed = 0x46533200  # deterministic sky ("FS2\0")
    for i in mm.instance_count:
        var dir := Vector3(rng.randfn(), rng.randfn(), rng.randfn()).normalized()
        var t := Transform3D(Basis(), dir * rng.randf_range(2500.0, 4000.0))
        mm.set_instance_transform(i, t)
    var mmi := MultiMeshInstance3D.new()
    mmi.multimesh = mm
    add_child(mmi)

# the mission's authored sky, retail's own numbers: each element sits
# along its instance matrix's uvec at retail's angular scale (a sun's
# radius is 0.05*scale rad, a patch spans 10 deg * scale), painted on a
# far shell that follows the camera (no parallax). stars.tbl decides the
# blend -- $BitmapX green-key art by alpha, $Bitmap intensity art
# additive -- and gives each sun the RGBI that takes over the key light.
const SKY_R := 3200.0

func _setup_backdrop() -> void:
    sky_root = Node3D.new()
    add_child(sky_root)

    var suns := 0
    for e in sim.backdrop():
        var u: Vector3 = e["uvec"]
        var r3: Vector3 = e["rvec"]
        var f3: Vector3 = e["fvec"]
        var dir := Vector3(u.x, u.y, -u.z)
        var ax := Vector3(r3.x, r3.y, -r3.z)
        var ay := Vector3(f3.x, f3.y, -f3.z)
        var fx = _fx((e["name"] as String).to_lower())

        if e["sun"]:
            suns += 1
            var li := key_light if suns == 1 and key_light \
                else DirectionalLight3D.new()
            if li.get_parent() == null:
                add_child(li)
            li.light_color = e["color"]
            li.light_energy = maxf(e["intensity"], 0.1)
            li.basis = Basis.looking_at(-dir)      # shines FROM the sun

            var size: float = 2.0 * SKY_R * tan(0.05 * e["scale_x"])
            var glow = _fx((e["glow"] as String).to_lower())
            if glow != null:
                sky_root.add_child(_sky_quad(glow["tex"], dir, ax, ay,
                                             size * 2.2, size * 2.2,
                                             false, true))
            if fx != null:
                sky_root.add_child(_sky_quad(fx["tex"], dir, ax, ay,
                                             size, size, false, true))
        elif fx != null:
            var sx: float = 2.0 * SKY_R * tan(deg_to_rad(5.0 * e["scale_x"]))
            var sy: float = 2.0 * SKY_R * tan(deg_to_rad(5.0 * e["scale_y"]))
            sky_root.add_child(_sky_quad(fx["tex"], dir, ax, ay, sx, sy,
                                         e["xparent"], false))

func _sky_quad(tex: Texture2D, dir: Vector3, ax: Vector3, ay: Vector3,
               sx: float, sy: float, alpha: bool,
               billboard: bool) -> MeshInstance3D:
    var mi := MeshInstance3D.new()
    var qm := QuadMesh.new()
    qm.size = Vector2(sx, sy)
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    if alpha:
        mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    else:
        mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
    mat.albedo_texture = tex
    mat.cull_mode = BaseMaterial3D.CULL_DISABLED
    # depth-tested, NOT no_depth_test: the shell sits at SKY_R and every
    # ship is nearer, so hulls occlude the sky the ordinary way --
    # no_depth_test painted the nebula OVER the Instructor
    # (field-reported). Priority still sorts it first among transparents
    # (bolts and explosions draw over the sky).
    mat.render_priority = -100
    if billboard:
        mat.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
    qm.material = mat
    mi.mesh = qm
    mi.position = dir * SKY_R
    if not billboard:
        mi.basis = Basis(ax, ay, dir)
    return mi

func _setup_hud() -> void:
    hud = CanvasLayer.new()
    add_child(hud)

    hud_left = _hud_label()
    hud_left.position = Vector2(16, 12)
    hud.add_child(hud_left)

    directives = _hud_label()
    directives.position = Vector2(16, 120)
    hud.add_child(directives)

    training_msg = _hud_label()
    training_msg.set_anchors_preset(Control.PRESET_CENTER_TOP)
    training_msg.grow_horizontal = Control.GROW_DIRECTION_BOTH
    training_msg.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    training_msg.offset_top = 60
    training_msg.autowrap_mode = TextServer.AUTOWRAP_WORD
    training_msg.custom_minimum_size = Vector2(900, 0)
    hud.add_child(training_msg)

    # the radio: mid-left, clear of the directives gauge above and the
    # help line below
    chatter = _hud_label()
    chatter.position = Vector2(16, 560)
    chatter.autowrap_mode = TextServer.AUTOWRAP_WORD
    chatter.custom_minimum_size = Vector2(760, 0)
    hud.add_child(chatter)

    # the combat gauges: the retired radar art bottom-center, the target
    # monitor beside it, the bracket overlay across the whole view
    radar = RadarClass.new()
    radar.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
    radar.offset_left = -110
    radar.offset_right = 110
    radar.offset_top = -232
    radar.offset_bottom = -12
    radar.mouse_filter = Control.MOUSE_FILTER_IGNORE
    hud.add_child(radar)

    # the target text block: bottom-left corner, above the help line;
    # its shield glyph draws beside it on the overlay
    target_monitor = _hud_label()
    target_monitor.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
    target_monitor.offset_left = 16
    target_monitor.offset_bottom = -56
    target_monitor.grow_vertical = Control.GROW_DIRECTION_BEGIN
    hud.add_child(target_monitor)

    hud_font = SystemFont.new()
    hud_font.font_names = ["Iosevka"]

    overlay = Control.new()
    overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
    overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
    overlay.draw.connect(_draw_hud)
    hud.add_child(overlay)

    # the ticker sits ABOVE the help line's row -- at wide help texts the
    # two used to meet in the bottom corner (field-reported overlap)
    ticker = _hud_label()
    ticker.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
    ticker.grow_horizontal = Control.GROW_DIRECTION_BEGIN
    ticker.grow_vertical = Control.GROW_DIRECTION_BEGIN
    ticker.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
    ticker.offset_right = -16
    ticker.offset_bottom = -56
    hud.add_child(ticker)

    var help := _hud_label()
    help.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
    help.grow_vertical = Control.GROW_DIRECTION_BEGIN
    help.offset_left = 16
    help.offset_bottom = -12
    help.add_theme_font_size_override("font_size", 24)
    help.text = "mouse steers + fires (RMB missile), Q/E roll, A/Z throttle, \\ full, Tab burner, T target, M match, V view, H hud, Shift-Super-J end mission, Esc quit"
    hud.add_child(help)


static func _hud_label() -> Label:
    var font := SystemFont.new()
    font.font_names = ["Iosevka"]
    var l := Label.new()
    l.add_theme_font_override("font", font)
    l.add_theme_font_size_override("font_size", 36)
    l.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.8))
    l.add_theme_constant_override("shadow_offset_x", 2)
    l.add_theme_constant_override("shadow_offset_y", 2)
    return l
