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
# This scene is the ORCHESTRATOR (split 2026-08-01, four cuts): it owns
# the frame loop -- step, drain events, snapshot -- object lifetime (the
# reconciler, keyed by retail's signature), input translation, the
# camera, and campaign progression. Presentation concerns live in
# passive modules it pushes state into, the radar.gd pattern: fx.gd
# (transient art + the flipbook cache), sky.gd (lights, starfield,
# backdrop), hud.gd (every 2D pixel, debrief overlay included),
# sound.gd (voice, effects, the hum). Nothing calls back; every module
# reads the frame the orchestrator hands it. The GDScript-era
# mission.tscn folded in 2026-07-31; the retired sims remain as specs
# beside their gates.
extends Node3D

const ShipClass := preload("res://ship.gd")
const SoundBankClass := preload("res://sound.gd")
const FxClass := preload("res://fx.gd")
const SkyClass := preload("res://sky.gd")
const HudClass := preload("res://hud.gd")

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
var fx                        # the transient-art shop (fx.gd)
var sky                       # lights + starfield + backdrop (sky.gd)
var player_sig := -1

var throttle := 0.0
var cam: Camera3D
var hud                       # every 2D pixel (hud.gd)
var target_sig := -1                        # hud_state's target_signature
var target_rec := {}                        # its record, assembled per frame
var births := {}              # sig -> identity record (created events);
                              # handed to _spawn, then kept in ships[sig]
var first_person := true                    # V toggles the chase camera
var player_max_speed := 0.0                 # match-speed's denominator
var match_target := false                   # M: retail's tracking mode
var sounds                     # SoundBank: voice, effects, the hum
var player_pos := Vector3.ZERO # FS2 frame, for sound attenuation

var assets_dir := ""
var mission_name := ""
var game_root := ""

# the campaign (boundary slice 3): non-empty campaign_name puts the
# scene in campaign mode -- the flown departure (or a lost fight) ends
# the mission into the debrief overlay (the sim freezes, the verdict
# shows), Enter accepts and loads the branch's pick in place. A
# loop-offering debrief goes two-phase, retail's own order: Enter
# accepts INTO the loop-brief screen, where L flies the optional
# mission and Enter declines onto the main line.
var campaign_name := ""
var debriefing := false
var loop_briefing := false
var debrief_data := {}

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
        # a mission argument with a path in it opens by fopen (cfile's
        # full-path branch); a bare name resolves through the file index
        # and must stay bare
        if mission_name.contains("/") and mission_name.is_relative_path():
            mission_name = launch.path_join(mission_name).simplify_path()

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

    fx = FxClass.new()
    fx.setup(assets_dir, ships_root)

    _setup_camera()
    sky = SkyClass.new()
    add_child(sky)
    sky.setup(sim, fx)
    sky.setup_lights()
    sky.setup_starfield()
    sky.setup_backdrop()

    sounds = SoundBankClass.new()
    add_child(sounds)
    sounds.setup(root)

    hud = HudClass.new()
    add_child(hud)
    hud.setup(cam, sounds)

    sounds.attach_hum(sim.sound_name(4))   # gamesnd.hh SND_ENGINE

    print("world: %s native, root %s" % [mission_name, root])

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
        "bank": 0.0,       # roll unbound: Q was roll, E is retail's
                           # escort key -- both freed (field request)
        "forward": throttle,
        "afterburner": burn,
        "fire_primary": mouse_grabbed and
            (Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
             or Input.is_key_pressed(KEY_CTRL)),
        "fire_secondary": mouse_grabbed and
            (Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT)
             or Input.is_key_pressed(KEY_SPACE)),
        "target_next": Input.is_key_pressed(KEY_T),
        "target_hostile": Input.is_key_pressed(KEY_H),
        "target_escort": Input.is_key_pressed(KEY_E),
        "target_subsys": Input.is_key_pressed(KEY_S),
        "cycle_primary": Input.is_key_pressed(KEY_PERIOD),
        "cycle_secondary": Input.is_key_pressed(KEY_SLASH),
        # the jump key: Alt-J is retail's binding; Shift-Super-J is the
        # house alternative (the WM here owns Alt-J). The boundary flies
        # retail's staged warpout off this edge -- collision and engine
        # gates, the autopilot, the hole -- and a second press during
        # stage 1 aborts. The debrief comes later, when the departure
        # lands on the mission log.
        "warp_out": Input.is_key_pressed(KEY_J)
            and (Input.is_key_pressed(KEY_ALT)
                 or (Input.is_key_pressed(KEY_SHIFT)
                     and Input.is_key_pressed(KEY_META))),
    }
    mouse_accum = Vector2.ZERO

    sim.step(delta, ci)

    for ev in sim.events():
        if ev["kind"] == "created":
            # identity crosses ONCE, here; the packed frame() rows carry
            # only what changes after birth
            births[ev["signature"]] = ev["rec"]
        elif ev["kind"] == "log":
            hud._tick("log %d: %s %s" % [ev["log_type"], ev["pname"], ev["sname"]])
        elif ev["kind"] == "destroyed":
            births.erase(ev["signature"])   # born and died between drains
            if not (ev["name"] as String).is_empty():
                hud._tick("destroyed: " + ev["name"])   # bolts expire nameless
        elif ev["kind"] == "sound":
            sounds.play_event(ev, player_pos)
        elif ev["kind"] == "message":
            hud.add_chatter(ev)
        elif ev["kind"] == "hud_text":
            hud.add_hud_line(ev["text"])

    # reconcile: the packed frame is the truth (frame() -- parallel
    # arrays, one row per object; identity arrived at birth); nodes
    # follow the rows
    var frm: Dictionary = sim.frame()
    var sigs: PackedInt32Array = frm["sig"]
    var fpos: PackedVector3Array = frm["pos"]
    var frv: PackedVector3Array = frm["rvec"]
    var fuv: PackedVector3Array = frm["uvec"]
    var ffv: PackedVector3Array = frm["fvec"]
    var fvel: PackedVector3Array = frm["vel"]
    var fhull: PackedFloat32Array = frm["hull"]
    var frad: PackedFloat32Array = frm["radius"]
    var fshield: PackedFloat32Array = frm["shield"]
    var fflags: PackedInt32Array = frm["flags"]
    var frgb: PackedByteArray = frm["rgb"]

    var seen := {}
    var player_node: Node3D = null
    var player_i := -1
    var ship_recs := []
    target_rec = {}
    for i in sigs.size():
        var sig := sigs[i]
        seen[sig] = true

        if not ships.has(sig):
            # a row with no birth record is a contract breach -- the
            # created event precedes the first row, same drain
            var brec: Dictionary = births.get(sig, {})
            if brec.is_empty():
                push_warning("world: no birth record for sig %d" % sig)
                continue
            _spawn(sig, brec)
            ships[sig]["birth"] = brec
            births.erase(sig)
        var entry: Dictionary = ships[sig]
        var node: Node3D = entry["node"]

        var flags := fflags[i]
        var dying := (flags & 1) != 0
        var burner := (flags & 2) != 0
        var is_player := (flags & 4) != 0

        if entry["is_ship"]:
            var birth: Dictionary = entry["birth"]
            ship_recs.append({
                "signature": sig, "pos": fpos[i], "team": birth["team"],
                "player": is_player, "dying": dying,
            })
            if sig == target_sig:
                target_rec = {
                    "signature": sig, "name": birth["name"],
                    "class": birth["class"], "team": birth["team"],
                    "pos": fpos[i], "vel": fvel[i],
                    "hull": fhull[i], "hull_max": birth["hull_max"],
                }

        # FS2 frame -> Godot frame at the visual boundary only
        var p := fpos[i]
        var rv := frv[i]
        var uv := fuv[i]
        var fv := ffv[i]
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
                    .albedo_color = Color(frgb[i * 3] / 255.0,
                                          frgb[i * 3 + 1] / 255.0,
                                          frgb[i * 3 + 2] / 255.0)
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
                var v3 := fvel[i]
                var dirw := Vector3(v3.x, v3.y, -v3.z)
                if dirw.length() < 1.0:
                    dirw = Vector3(fv.x, fv.y, -fv.z)
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
            var fb: Dictionary = node.get_meta("fx")
            var age: float = (Time.get_ticks_msec() - int(fb["born"])) / 1000.0
            var fr := int(age * int(fb["fps"]))
            if fb["loop"]:
                fr = fr % int(fb["frames"])
            else:
                fr = mini(fr, int(fb["frames"]) - 1)
            var cols: int = fb["cols"]
            var row := int(fr / float(cols))
            (fb["mat"] as StandardMaterial3D).uv1_offset = Vector3(
                float(fr % cols) / cols, float(row) / float(fb["rows"]), 0.0)
            if entry_kind == "shockwave":
                node.scale = Vector3.ONE * maxf(frad[i], 0.1)
        elif entry_kind == "shockwave":
            node.scale = Vector3.ONE * maxf(frad[i], 0.1)
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
            if is_player:
                eng_on = burner or throttle > 0.0
            else:
                eng_on = fvel[i].length() > 0.5
            node.set_thrusters(eng_on)

            # the engine dress follows: flipbook frame by age, burner
            # variant by the sim's flag, glows lit with the engine
            var th = entry.get("thrust")
            if th != null:
                var tfx: Dictionary = th["fxa"] if burner and th["fxa"] != null \
                    else th["fxn"]
                var age: float = (Time.get_ticks_msec()
                                  - int(th["born"])) / 1000.0
                var frames_n: int = tfx["frames"]
                var fr := int(age * int(tfx["fps"])) % maxi(frames_n, 1)
                var cols: int = tfx["cols"]
                var off := Vector3(float(fr % cols) / cols,
                                   float(int(fr / float(cols)))
                                   / float(tfx["rows"]), 0.0)
                for m in th["mats"]:
                    var sm := m as StandardMaterial3D
                    sm.albedo_texture = tfx["tex"]
                    sm.uv1_scale = Vector3(1.0 / cols, 1.0 / tfx["rows"], 1.0)
                    sm.uv1_offset = off
                var gtex = th["glow_tex_a"] if burner \
                    and th["glow_tex_a"] != null else th["glow_tex"]
                var frac: float
                if burner:
                    frac = 1.0
                elif is_player:
                    frac = throttle
                else:
                    frac = clampf(fvel[i].length()
                                  / maxf(float(entry["birth"].get(
                                      "max_speed", 1.0)), 1.0),
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
            var tot := fshield[i * 4] + fshield[i * 4 + 1] \
                + fshield[i * 4 + 2] + fshield[i * 4 + 3]
            var prev: float = entry.get("shield_prev", tot)
            if tot < prev - 0.5:
                fx._shield_flash(node, entry["radius"])
            entry["shield_prev"] = tot

        if is_player:
            player_sig = sig
            player_node = node
            player_i = i

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

    fx.reap(Time.get_ticks_msec())

    if player_node:
        # own hull out of the pilot's eyes; back for the chase view
        player_node.visible = not first_person
        _update_camera(delta, player_node,
                       ships[player_sig]["radius"])
        if sky and cam:
            sky.follow(cam.global_position)
        var pbirth: Dictionary = ships[player_sig]["birth"]
        player_pos = fpos[player_i]
        player_max_speed = pbirth.get("max_speed", 0.0)
        var fwd_speed := fvel[player_i].dot(ffv[player_i])
        if absf(fwd_speed) < 0.05:
            fwd_speed = 0.0            # %6.1f would print "-0.0"

        # the HUD reads the frame through pushed fields -- the radar.gd
        # pattern writ large; ships only in the count (the node table
        # also holds bolts and debris, and "7 ships" after a kill was a
        # lie, field-reported)
        hud.player_speed = fwd_speed
        hud.player_pos = player_pos
        hud.throttle = throttle
        hud.match_target = match_target
        hud.hud_left.text = "%s\n%d ships" % [mission_name, ship_recs.size()]

        var hum_out: float = absf(throttle)
        if fflags[player_i] & 2:
            hum_out = 1.0
        sounds.hum_level(hum_out)

        # the player's combat record, joined from the row and the birth
        var prec := {
            "pos": fpos[player_i], "rvec": frv[player_i],
            "uvec": fuv[player_i], "fvec": ffv[player_i],
            "vel": fvel[player_i], "team": pbirth["team"],
            "max_speed": player_max_speed,
            "shield": [fshield[player_i * 4], fshield[player_i * 4 + 1],
                       fshield[player_i * 4 + 2], fshield[player_i * 4 + 3]],
            "shield_max": pbirth["shield_max"],
            "hull": fhull[player_i], "hull_max": pbirth["hull_max"],
        }
        hud.update_combat(prec, ship_recs, target_sig, target_rec,
                          float(ships.get(target_sig, {}).get("radius", 10.0)))

    # the boundary's HUD freight: the orchestrator keeps the target
    # signature (the reconciler keys on it); the rest is the HUD's --
    # the player's energy/fuel gauges ride here now, keeping frame()
    # uniform across object kinds
    var h: Dictionary = sim.hud_state()
    target_sig = int(h.get("target_signature", -1))
    hud.player_energy_frac = float(h.get("weapon_energy", 0.0)) \
        / maxf(float(h.get("weapon_energy_max", 0.0)), 1.0)
    hud.player_burner_frac = float(h.get("burner_fuel", 0.0)) \
        / maxf(float(h.get("burner_fuel_max", 0.0)), 1.0)
    hud.update_lesson(h)
    hud.update_chatter()

    # the departure: the ship flew through the hole and retail logged it
    # -- the mission is over. Campaign mode goes to the debrief; a lone
    # mission has nowhere to go and the flight simply ends.
    if bool(h.get("departed", false)) and not debriefing:
        if campaign_name.is_empty():
            print("world: departed")
            get_tree().quit()
        else:
            _enter_debrief()

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
            node = fx._bolt_node(rec)
        # born AT the muzzle, streak trailing through the cockpit for one
        # frame -- the flash owns the birth moment, the bolt shows from
        # its first step forward
        node.visible = false
        ships[sig] = { "node": node, "is_ship": false, "kind": "weapon",
                       "radius": rec["radius"], "newborn": true }
        fx._muzzle_flash(rec)
        return
    if kind == "shockwave":
        # the blast front: retail's own expanding-ring flipbook
        # (shockwave01), frame by age, size riding the record's live
        # radius; the torus stand-in only if the bake is missing
        var art_fx = fx._fx("shockwave01")
        var node: Node3D
        if art_fx != null:
            node = fx._flipbook_node(art_fx, 2.0, true, false)
        else:
            node = fx._simple_node("shockwave", 1.0)
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
            node = fx._debris_node(rec)
        if art == "fireball" or art == "warp":
            var art_fx = fx._fx((rec.get("pof", "") as String).to_lower())
            if art_fx != null:
                if art == "warp":
                    # the vortex stands in the arrival plane: a fixed
                    # quad, the reconciler's basis turns it; loops while
                    # the effect lives
                    node = fx._flipbook_node(art_fx, rec["radius"] * 2.0,
                                             false, true)
                else:
                    node = fx._flipbook_node(art_fx, rec["radius"] * 2.0,
                                             true, false)
        if node == null:
            if art == "fireball":
                node = fx._explosion_node(rec["radius"])
            else:
                node = fx._simple_node(art, rec["radius"])
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
        ships[sig]["thrust"] = fx._dress_thrusters(node, rec)
    hud._tick("arrived: %s (%s)" % [rec["name"], rec["class"]])

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
    # the debrief overlay owns the keys while it is up. Retail's order
    # for a loop point: Accept leaves the debrief INTO the loop-brief
    # screen (missionloopbrief.cc), and the choice happens there.
    if debriefing:
        match event.keycode:
            KEY_ENTER, KEY_KP_ENTER:
                if not loop_briefing and debrief_data.get("loop_offer", false):
                    loop_briefing = true
                    hud.build_loop_brief(debrief_data)
                else:
                    _accept_debrief(false)
            KEY_L:
                if loop_briefing:
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
        KEY_ESCAPE:
            get_tree().quit()

# ----------------------------------------------------------------------
# the campaign flow: debrief overlay in, accept out, next mission in place

func _enter_debrief() -> void:
    loop_briefing = false
    debrief_data = sim.debrief()
    debriefing = true
    Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
    mouse_grabbed = false
    hud.build_debrief(mission_name, debrief_data)

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
    sky.setup_backdrop()

    hud.drop_debrief()
    debriefing = false
    loop_briefing = false
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

    fx.reset()

    sky.reset()

    player_sig = -1
    target_sig = -1
    target_rec = {}
    births.clear()
    throttle = 0.0
    match_target = false
    hud.reset()

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
