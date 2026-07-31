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
var player_sig := -1

var throttle := 0.0
var cam: Camera3D
var hud: CanvasLayer
var hud_left: Label
var hud_right: Label
var ticker: Label
var ticker_lines: Array[String] = []
var directives: Label
var training_msg: Label
var chatter: Label
var chatter_lines: Array[Dictionary] = []   # {line, deadline}
var radar: Control                          # the retired file's art, live
var overlay: Control                        # target bracket, drawn 2d
var target_monitor: Label
var target_sig := -1                        # hud_state's target_signature
var target_rec := {}                        # its snapshot record this frame
var target_view: SubViewport                # the monitor's little world
var target_view_root: Node3D                # the target's model, turning
var target_view_cam: Camera3D
var target_view_pof := ""                   # the model currently in the well
var lead_speed := 0.0                       # the primary's muzzle speed
var player_vel := Vector3.ZERO              # FS2 frame, lead solution input
var player_shield: Array = []               # own quadrants, for the icon
var player_shield_max := 0.0
var player_hull_frac := 1.0
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

var mouse_accum := Vector2.ZERO
var mouse_grabbed := false
const MOUSE_SENS := 0.05

func _ready() -> void:
    var args := OS.get_cmdline_user_args()
    # tolerate both the dispatched form (world <mission> <assets> [root])
    # and a direct scene run without the mode word
    if args.size() > 0 and args[0] == "world":
        args = args.slice(1)
    if args.size() < 2:
        _fatal("usage: godot --path inspect -- world <mission.fs2> <assets-dir> [game-root]")
        return

    mission_name = args[0]
    assets_dir = args[1]

    var root := args[2] if args.size() > 2 else OS.get_environment("FS2_GAME_ROOT")
    if root.is_empty():
        root = ProjectSettings.globalize_path("res://") + "../../rundir"

    if not _load_libfs2():
        return
    sim = ClassDB.instantiate("FS2")
    if not sim.load(root, mission_name, 42):
        _fatal("native load failed: %s (root %s)" % [mission_name, root])
        return

    ships_root = Node3D.new()
    ships_root.name = "Ships"
    add_child(ships_root)

    _setup_camera()
    _setup_lights()
    _setup_starfield()
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
        if entry_kind == "weapon" and node.has_meta("bolt_mat"):
            (node.get_meta("bolt_mat") as StandardMaterial3D).albedo_color = \
                rec.get("color", Color(1.0, 0.35, 0.25))
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
            if rec["player"]:
                node.set_thrusters(rec["afterburner"] or throttle > 0.0)
            else:
                node.set_thrusters((rec["vel"] as Vector3).length() > 0.5)

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
        player_pos = player_rec["pos"]
        var vel: Vector3 = player_rec["vel"]
        var fv2: Vector3 = player_rec["fvec"]
        var fwd_speed := vel.dot(fv2)
        if absf(fwd_speed) < 0.05:
            fwd_speed = 0.0            # %6.1f would print "-0.0"
        hud_right.text = "speed %6.1f\nengine %4d%%%s\ngun  %4d%%\nburn %4d%%" \
            % [fwd_speed, int(throttle * 100.0),
               "\nmatch" if match_target else "",
               int(100.0 * player_rec["weapon_energy"]
                   / maxf(player_rec["weapon_energy_max"], 1.0)),
               int(100.0 * player_rec["burner_fuel"]
                   / maxf(player_rec["burner_fuel_max"], 1.0))]
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
    else:
        target_monitor.text = "%s\n%s\nhull %3d%%  %5.0f m" % [
            target_rec["name"], target_rec["class"],
            int(100.0 * target_rec["hull"] / maxf(target_rec["hull_max"], 1.0)),
            ((target_rec["pos"] as Vector3) - ppos).length()]

    _update_target_view()
    overlay.queue_redraw()

# the monitor's model well follows the target: rebuild on a class change,
# keep it turning while held
func _update_target_view() -> void:
    var pof: String = target_rec.get("pof", "")
    if pof != target_view_pof:
        target_view_pof = pof
        for c in target_view_root.get_children():
            c.queue_free()
        if not pof.is_empty():
            var stem := pof.get_basename().to_lower()
            var glb := assets_dir.path_join(stem + ".glb")
            var node: Node3D = null
            if FileAccess.file_exists(glb):
                var m = ShipClass.new()
                if m.load_ship(glb):
                    node = m
            if node == null:
                var radius: float = maxf(
                    ships.get(target_sig, {}).get("radius", 10.0), 1.0)
                var box := MeshInstance3D.new()
                var mesh := BoxMesh.new()
                mesh.size = Vector3(radius, radius * 0.5, radius * 1.6)
                box.mesh = mesh
                node = box
            target_view_root.rotation = Vector3.ZERO
            target_view_root.add_child(node)

            # frame what the eye gets: the POF radius counts far-flung
            # gear (drone01 says 27.5 for a hull that measures ~15 across)
            # and a camera backed off by it shrinks the target to a speck
            # against the transparent starfield -- the "missing" monitor
            # (field-reported). Center on the visible bounds and fill the
            # well from them.
            var bounds := _visual_bounds(node)
            var extent := maxf(bounds.get_longest_axis_size() * 0.5, 1.0)
            node.position = -bounds.get_center()
            target_view_cam.fov = 40.0
            var dist := extent / tan(deg_to_rad(20.0)) * 1.15
            target_view_cam.look_at_from_position(
                Vector3(0, dist * 0.25, dist), Vector3.ZERO, Vector3.UP)
    if not target_view_pof.is_empty():
        target_view_root.rotation.y += get_physics_process_delta_time() * 0.6

# the merged AABB of a model's visible meshes, in the model's own frame --
# accumulated transforms, no global_transform (valid in or out of tree)
static func _visual_bounds(model: Node3D) -> AABB:
    var bounds := AABB()
    var first := true
    var stack: Array = [[model, Transform3D.IDENTITY]]
    while not stack.is_empty():
        var top: Array = stack.pop_back()
        var node: Node = top[0]
        var xf: Transform3D = top[1]
        if node is Node3D:
            if not (node as Node3D).visible:
                continue
            xf = xf * (node as Node3D).transform
        if node is MeshInstance3D:
            var box: AABB = xf * (node as MeshInstance3D).get_aabb()
            bounds = box if first else bounds.merge(box)
            first = false
        for c in node.get_children():
            stack.append([c, xf])
    return bounds

# the target bracket: corners around the target's screen extent, sized by
# its radius at distance, teamed by color -- and the reticle on the
# boresight (screen center in first person, the nose's true line in chase)
func _draw_bracket() -> void:
    if cam == null:
        return

    var aim := aim_from + aim_dir * 1000.0
    if not cam.is_position_behind(aim):
        var rp := cam.unproject_position(aim)
        var rc := Color(0.4, 1.0, 0.5, 0.9)
        overlay.draw_arc(rp, 12.0, 0.0, TAU, 32, rc, 1.5)
        for d: Vector2 in [Vector2.RIGHT, Vector2.LEFT, Vector2.UP, Vector2.DOWN]:
            overlay.draw_line(rp + d * 7.0, rp + d * 16.0, rc, 1.5)

    # the pilot's own bubble and hull, the left wing of the gauge row
    var sz := overlay.size
    if not player_shield.is_empty():
        _draw_shield_icon(Vector2(sz.x * 0.5 - 470, sz.y - 122),
                          player_shield, player_shield_max / 4.0,
                          player_hull_frac, Color(0.45, 0.85, 1.0))

    if target_rec.is_empty():
        return

    # the target's bubble, framing the monitor's model well
    _draw_shield_icon(Vector2(sz.x * 0.5 - 250, sz.y - 122),
                      target_rec.get("shield", []),
                      float(target_rec.get("shield_max", 0.0)) / 4.0,
                      target_rec["hull"] / maxf(target_rec["hull_max"], 1.0),
                      Color(1.0, 0.55, 0.4) if target_rec["team"] != player_team
                      else Color(0.45, 1.0, 0.55))

    var p3: Vector3 = target_rec["pos"]
    var v := Vector3(p3.x, p3.y, -p3.z)
    if cam.is_position_behind(v):
        return
    var p := cam.unproject_position(v)
    var edge := cam.unproject_position(
        v + cam.global_basis.x * float(ships.get(target_sig, {}).get("radius", 10.0)))
    var half := clampf((edge - p).length(), 24.0, 300.0)
    var col := Color(1.0, 0.35, 0.3) if target_rec["team"] != player_team \
        else Color(0.35, 1.0, 0.4)
    var arm := half * 0.5
    for corner: Vector2 in [Vector2(-1, -1), Vector2(1, -1), Vector2(-1, 1), Vector2(1, 1)]:
        var c := p + corner * half
        overlay.draw_line(c, c - Vector2(corner.x * arm, 0), col, 2.0)
        overlay.draw_line(c, c - Vector2(0, corner.y * arm), col, 2.0)

    # the lead indicator: put the reticle on the dot and the bolts meet
    # the target -- an intercept on RELATIVE motion, because bolts
    # inherit the shooter's velocity (|R + Vt| = s*t, smallest t > 0)
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
            overlay.draw_circle(lp, 3.5, col)
            overlay.draw_arc(lp, 8.0, 0.0, TAU, 24, col, 1.5)

# the shield/hull icon: four arcs in retail's display order -- front on
# top, then right, rear, left (Quadrant_xlate {1,0,2,3}, hudshield.cc:88,
# over get_quadrant's octants, shield.cc:824) -- each arc's brightness is
# its quadrant's charge; the center bar is the hull
func _draw_shield_icon(center: Vector2, quads: Array, qmax: float,
                       hull_frac: float, base: Color) -> void:
    if quads.size() >= 4 and qmax > 0.0:
        var order: Array[int] = [1, 0, 2, 3]          # top, right, bottom, left
        var mid: Array[float] = [-PI / 2, 0.0, PI / 2, PI]
        for i in 4:
            var frac := clampf(float(quads[order[i]]) / qmax, 0.0, 1.0)
            if frac <= 0.02:
                continue
            var c := base
            c.a = 0.2 + 0.8 * frac
            overlay.draw_arc(center, 34.0, mid[i] - 0.55, mid[i] + 0.55,
                             12, c, 5.0)
    var hc := Color(0.4, 1.0, 0.5) if hull_frac > 0.5 \
        else (Color(1.0, 0.8, 0.3) if hull_frac > 0.25 else Color(1.0, 0.35, 0.3))
    overlay.draw_rect(Rect2(center - Vector2(16, 6),
                            Vector2(32.0 * clampf(hull_frac, 0.0, 1.0), 12)), hc)

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
        ships[sig] = { "node": node, "is_ship": false, "kind": "weapon",
                       "radius": rec["radius"] }
        _muzzle_flash(rec)
        return
    if kind == "shockwave":
        # the blast front: the record's radius IS the live front; the
        # ring just follows it (reconcile scales the unit torus)
        ships[sig] = { "node": _simple_node("shockwave", 1.0),
                       "is_ship": false, "kind": "shockwave", "radius": 1.0 }
        return
    if kind == "fireball" or kind == "debris":
        # the boundary tags fireballs: an arrival's warp effect is not
        # an explosion
        var art := kind
        if rec.get("class", "") == "warp":
            art = "warp"
        var node: Node3D
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

# a laser bolt in its tbl size and color -- an additive-glow capsule, so
# it reads as plasma from any angle (a box showed its square cross-section
# dead astern, field-reported). The capsule's long axis is Y; the wrap
# node takes the record's basis, the mesh inside turns Y onto -Z. The
# material rides on the node so the reconciler can follow retail's color
# cycle every frame.
func _bolt_node(rec: Dictionary) -> Node3D:
    var mi := MeshInstance3D.new()
    var cm := CapsuleMesh.new()
    var r: float = maxf(rec.get("laser_radius", 0.4), 0.15)
    cm.radius = r
    cm.height = maxf(rec.get("laser_length", 6.0), 2.0 * r)
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
    mat.albedo_color = rec.get("color", Color(1.0, 0.35, 0.25))
    cm.material = mat
    mi.mesh = cm
    mi.rotation_degrees = Vector3(90, 0, 0)
    var wrap := Node3D.new()
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

    var mi := MeshInstance3D.new()
    var sm := SphereMesh.new()
    sm.radius = 0.8
    sm.height = 1.6
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.albedo_color = Color(1.0, 0.85, 0.5, 0.9)
    mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
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
    flashes.append({ "node": n, "deadline": Time.get_ticks_msec() + 90 })

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
        KEY_ESCAPE:
            get_tree().quit()

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
    var key := DirectionalLight3D.new()
    key.rotation_degrees = Vector3(-35.0, 40.0, 0.0)
    add_child(key)
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
    mm.instance_count = 800
    var rng := RandomNumberGenerator.new()
    rng.seed = 0x46533200  # deterministic sky ("FS2\0")
    for i in mm.instance_count:
        var dir := Vector3(rng.randfn(), rng.randfn(), rng.randfn()).normalized()
        var t := Transform3D(Basis(), dir * rng.randf_range(2500.0, 4000.0))
        mm.set_instance_transform(i, t)
    var mmi := MultiMeshInstance3D.new()
    mmi.multimesh = mm
    add_child(mmi)

func _setup_hud() -> void:
    hud = CanvasLayer.new()
    add_child(hud)

    hud_left = _hud_label()
    hud_left.position = Vector2(16, 12)
    hud.add_child(hud_left)

    hud_right = _hud_label()
    hud_right.set_anchors_preset(Control.PRESET_TOP_RIGHT)
    hud_right.grow_horizontal = Control.GROW_DIRECTION_BEGIN
    hud_right.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
    hud_right.offset_top = 12
    hud_right.offset_right = -16
    hud.add_child(hud_right)

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

    target_monitor = _hud_label()
    target_monitor.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
    target_monitor.offset_left = 140
    target_monitor.offset_bottom = -12
    target_monitor.grow_vertical = Control.GROW_DIRECTION_BEGIN
    hud.add_child(target_monitor)

    # the target view: a little world of its own with the target's model
    # turning in it -- retail's target monitor, left of the radar; the
    # overlay draws the target's shield arcs around this box
    var view_box := SubViewportContainer.new()
    view_box.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
    view_box.offset_left = -360
    view_box.offset_right = -140
    view_box.offset_top = -232
    view_box.offset_bottom = -12
    view_box.stretch = true
    view_box.mouse_filter = Control.MOUSE_FILTER_IGNORE
    target_view = SubViewport.new()
    target_view.own_world_3d = true
    target_view.transparent_bg = true
    target_view_cam = Camera3D.new()
    target_view.add_child(target_view_cam)
    # the little world has no ambient of its own -- retail hulls are dark
    # art, so light them like a display case: key plus a soft fill
    var key_light := DirectionalLight3D.new()
    key_light.rotation_degrees = Vector3(-35, 40, 0)
    key_light.light_energy = 1.5
    target_view.add_child(key_light)
    var fill_light := DirectionalLight3D.new()
    fill_light.rotation_degrees = Vector3(-15, 220, 0)
    fill_light.light_energy = 0.6
    target_view.add_child(fill_light)
    target_view_root = Node3D.new()
    target_view.add_child(target_view_root)
    view_box.add_child(target_view)
    hud.add_child(view_box)

    overlay = Control.new()
    overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
    overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
    overlay.draw.connect(_draw_bracket)
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
    help.text = "mouse steers + fires (RMB missile), Q/E roll, A/Z throttle, \\ full, Tab burner, T target, M match, V view, H hud, Esc quit"
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
