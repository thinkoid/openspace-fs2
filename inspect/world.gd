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
var target_rec := {}                        # its snapshot record this frame
var first_person := true                    # V toggles the chase camera
var player_max_speed := 0.0                 # match-speed's denominator
var match_target := false                   # M: retail's tracking mode
var sounds                     # SoundBank: voice, effects, the hum
var player_pos := Vector3.ZERO # FS2 frame, for sound attenuation

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
    }
    mouse_accum = Vector2.ZERO

    sim.step(delta, ci)

    for ev in sim.events():
        if ev["kind"] == "log":
            hud._tick("log %d: %s %s" % [ev["log_type"], ev["pname"], ev["sname"]])
        elif ev["kind"] == "destroyed" and not (ev["name"] as String).is_empty():
            hud._tick("destroyed: " + ev["name"])   # bolts expire nameless
        elif ev["kind"] == "sound":
            sounds.play_event(ev, player_pos)
        elif ev["kind"] == "message":
            hud.add_chatter(ev)

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
                fx._shield_flash(node, entry["radius"])
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

    fx.reap(Time.get_ticks_msec())

    if player_node:
        # own hull out of the pilot's eyes; back for the chase view
        player_node.visible = not first_person
        _update_camera(delta, player_node,
                       ships[player_sig]["radius"])
        if sky and cam:
            sky.follow(cam.global_position)
        player_pos = player_rec["pos"]
        player_max_speed = player_rec.get("max_speed", 0.0)
        var vel: Vector3 = player_rec["vel"]
        var fv2: Vector3 = player_rec["fvec"]
        var fwd_speed := vel.dot(fv2)
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
        hud.player_energy_frac = player_rec["weapon_energy"] \
            / maxf(player_rec["weapon_energy_max"], 1.0)
        hud.player_burner_frac = player_rec["burner_fuel"] \
            / maxf(player_rec["burner_fuel_max"], 1.0)
        hud.hud_left.text = "%s\n%d ships" % [mission_name, ship_recs.size()]

        var hum_out: float = absf(throttle)
        if player_rec["afterburner"]:
            hum_out = 1.0
        sounds.hum_level(hum_out)

        hud.update_combat(player_rec, ship_recs, target_sig, target_rec,
                          float(ships.get(target_sig, {}).get("radius", 10.0)))

    # the boundary's HUD freight: the orchestrator keeps the target
    # signature (the reconciler keys on it); the rest is the HUD's
    var h: Dictionary = sim.hud_state()
    target_sig = int(h.get("target_signature", -1))
    hud.update_lesson(h)
    hud.update_chatter()

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
