# -*- mode: gdscript -*-
#
# The mission scene: a MissionData layout (mission2tres, retail's parser)
# spawned as real Ships, the player in the player-start ship under
# FlightModel, the mission's events running in SexpVM (retail's evaluator)
# with this scene as its world -- training messages and directives render,
# T/H targeting feeds the `targeted` predicate and the target monitor,
# LCtrl fires the gun (Weapons: bolts, hulls, the destroyed registry that
# answers is-destroyed-delay and hits-left), and ships with orders FLY
# them (WaypointAI: waypoint paths, the LOG_WAYPOINTS_DONE stamps that
# are-waypoints-done-delay reads, add-goal steering the Instructor from
# path to path). Training-1 puts you in Alpha 1's Myrmidon with the
# Instructor pulling away to his first waypoint;
# tests/weapons-range.fs2 is the live-fire proving ground. Still ahead:
# the full game HUD. Every ship the mission knows is present (FRED's
# view); ships without orders hold station.
#
#     godot --path inspect -- mission /abs/path/to/<mission>.tres
#
# The GLBs (and ship_params.tres) are found beside the mission .tres by
# POF stem -- convert the classes the mission uses first:
#     build/tools/pof2glb <models>/fighter2t-05.pof <dir>/fighter2t-05.glb
# Controls are fly.gd's; V toggles cockpit, R resets to the mission start.
extends Node3D

const ShipClass := preload("res://ship.gd")
const FlightClass := preload("res://flight_model.gd")
const VMClass := preload("res://sexp_vm.gd")
const TargetingClass := preload("res://targeting.gd")
const WeaponsClass := preload("res://weapons.gd")
const WaypointAIClass := preload("res://waypoint_ai.gd")

var mission: Resource
var player_ship: Node3D
var player_entry: Dictionary
var fm
var throttle := 0.0
var cam: Camera3D
var hud: CanvasLayer
var hud_left: Label
var hud_right: Label
var msg_label: Label
var directives: Label
var target_box: Label
var target_marker: Label
var ship_label := ""

var targeting                     # TargetingClass instance

# ---- weapons: the gun, its bolts, the hull ledger ----
var weapons                       # WeaponsClass instance
var ship_nodes := {}              # name -> placed Ship node (live only)
var bolt_nodes := []              # visuals pooled to weapons.projectiles
var bolt_mesh: Mesh

# the player's gun. Loadout ($Primary Banks) extraction is a refinement;
# until then the trainer's Subach, its ballistics from ship_params.tres
# (weapons.tbl through retail's weapon_init), these numbers the fallback
const PLAYER_GUN := "Subach HL-7"
const GUN_FALLBACK := {"velocity": 450.0, "damage": 15.0,
                       "lifetime": 2.0, "fire_wait": 0.2}

# ---- the waypoint-AI sliver: ships with orders move ----
var nav                           # WaypointAIClass instance
var ai_logged := {}               # degraded/no-op verbs, logged once

# AI_GOAL_* bits (aigoals.hh) as the .tres ai_goals carry them ->
# this world's verbs; guard degrades to stay-still (guard flight is a
# later slice), ignore is a no-op here (nobody attacks anybody)
const AI_VERBS := {
    1 << 3:  "waypoints-once",    # AI_GOAL_WAYPOINTS: repeat degrades
    1 << 4:  "waypoints-once",    # AI_GOAL_WAYPOINTS_ONCE
    1 << 10: "stay-still",        # AI_GOAL_GUARD, degraded
    1 << 20: "stay-still",        # AI_GOAL_STAY_STILL
    1 << 21: "stay-still",        # AI_GOAL_PLAY_DEAD
}

# ---- the events engine and its world ----
var vm                            # VMClass instance
var ship_entries := {}            # name -> mission ships entry (FS2 frame)
var key_used := {}                # key token -> vm.ms of last press
var msg_queue := []               # {at, until, text}
var messages := {}                # name -> text
var tc_speed := false             # training context: speed watch
var tc_speed_min := 0
var tc_speed_set_ms := -1

# the bindings table: retail key tokens -> Godot keys and display names.
# Mission logic stays retail-faithful ($t$, key-pressed "Tab"); the
# physical keys are ours -- warp-out is Shift+Super+J by decree (the WM
# eats Alt+J). Mouse button 1 aliases "left ctrl" (guns are guns,
# SEXP-wise); button 2 is reserved for missiles.
const BINDINGS := {
    "t": [KEY_T, "T"], "m": [KEY_M, "M"], "tab": [KEY_TAB, "Tab"],
    "a": [KEY_A, "A"], "z": [KEY_Z, "Z"], "h": [KEY_H, "H"],
    "b": [KEY_B, "B"], "backspace": [KEY_BACKSPACE, "Backspace"],
    "left ctrl": [KEY_CTRL, "LCtrl"], "\\\\": [KEY_BACKSLASH, "\\"],
    "alt-j": [KEY_J, "Shift+Super+J"],
}

# actions whose slices haven't landed yet: eval true (retail returns 1
# from side-effect ops), logged once so the TODO list writes itself
const STUB_ACTIONS := ["protect-ship", "unprotect-ship",
    "ship-guardian", "ship-no-guardian", "flash-hud-gauge",
    "cap-waypoint-speed", "key-reset-multiple", "hud-disable",
    "training-context", "set-training-context-fly-path"]

# predicates whose slices haven't landed: eval false, logged once
const STUB_PREDICATES := ["special-check",
    "percent-ships-destroyed", "is-subsystem-destroyed-delay",
    "waypoints-done-delay"]

var view_chase := true
var eye_parent: Node3D = null
var eye_point := Vector3.ZERO
var eye_normal := Vector3.FORWARD

# mouse flight: relative motion accumulated between physics frames is
# the stick deflection -- mouse right yaws starboard, mouse up noses
# DOWN (stick-true, same convention as the Up arrow); stop moving and
# the turn stops. Arrows still work, summed and clamped.
var mouse_accum := Vector2.ZERO
var mouse_grabbed := false
var missiles_logged := false
const MOUSE_SENS := 0.05          # full deflection at ~20 px per tick

# FS2 world frame -> Godot: the same (x, y, -z) map as everything else
static func g_pos(v: Vector3) -> Vector3:
    return Vector3(v.x, v.y, -v.z)

static func g_basis(r: Vector3, u: Vector3, f: Vector3) -> Basis:
    return Basis(g_pos(r), g_pos(u), -g_pos(f))

func _ready() -> void:
    var args := OS.get_cmdline_user_args()
    if args.is_empty():
        _fatal("usage: godot --path inspect -- mission /abs/mission.tres")
        return
    var tres_path: String = args[args.size() - 1]
    var dir := tres_path.get_base_dir()

    mission = ResourceLoader.load(tres_path)
    if mission == null or mission.get_script() != preload("res://mission_data.gd"):
        _fatal("cannot load mission: " + tres_path)
        return

    var sp: Resource = null
    if FileAccess.file_exists(dir + "/ship_params.tres"):
        sp = ResourceLoader.load(dir + "/ship_params.tres")

    var placed := 0
    var missing := {}
    for e in mission.ships:
        var glb: String = dir + "/" + e["pof"] + ".glb"
        if not FileAccess.file_exists(glb):
            missing[e["pof"]] = true
            continue
        var s = ShipClass.new()
        if not s.load_ship(glb):
            missing[e["pof"]] = true
            continue
        add_child(s)
        s.position = g_pos(e["pos"])
        s.basis = g_basis(e["rvec"], e["uvec"], e["fvec"])
        ship_nodes[e["name"]] = s
        placed += 1

        if e["player_start"]:
            player_ship = s
            player_entry = e
            fm = FlightClass.new()
            fm.pos = e["pos"]           # FS2 frame; the model flies there
            fm.rvec = e["rvec"]
            fm.uvec = e["uvec"]
            fm.fvec = e["fvec"]
            ship_label = "%s\n%s" % [mission.mission_name, e["name"]]
            if sp and sp.ships.has(e["pof"]):
                fm.set_params(sp.ships[e["pof"]], s.data.mass)
                ship_label += " (%s)" % sp.ships[e["pof"]]["name"]

    for stem in missing:
        push_warning("mission: no GLB for '%s' beside the mission -- convert it" % stem)

    if player_ship == null:
        _fatal("no player start could be placed (missing GLB?)")
        return

    if player_ship.data.eyes.size() > 0:
        var e: Dictionary = player_ship.data.eyes[0]
        eye_parent = player_ship.sub_node(e["parent"])
        eye_point = e["point"]
        eye_normal = e["normal"]
    else:
        eye_point = Vector3(0.0, 0.0, player_ship.data.bbox_min.z)
    if eye_parent == null:
        eye_parent = player_ship

    _setup_camera()
    _setup_lights()
    _setup_starfield()
    _setup_hud()

    for e in mission.ships:
        ship_entries[e["name"]] = e
    for m in mission.messages:
        messages[m["name"]] = m["text"]
    vm = VMClass.new()
    vm.world = self
    vm.load_mission(mission)

    targeting = TargetingClass.new()
    targeting.setup(mission.ships, player_entry["name"])

    # the gun and the hull ledger: POF bounding sphere from the loaded
    # model, $Hitpoints from ship_params; a ship without its GLB never
    # registers -- hits-left answers NAN for it, retail's not-arrived case
    weapons = WeaponsClass.new()
    var gun: Dictionary = GUN_FALLBACK
    if sp and sp.weapons.has(PLAYER_GUN):
        gun = sp.weapons[PLAYER_GUN]
    weapons.setup(gun)
    for name in ship_nodes:
        if name == player_entry["name"]:
            continue
        var e: Dictionary = ship_entries[name]
        var hull := 100.0
        if sp and sp.ships.has(e["pof"]):
            hull = sp.ships[e["pof"]]["hull"]
        weapons.add_ship(name, ship_nodes[name].data.radius, hull,
                         bool(e.get("invulnerable", false)))
    _setup_bolt_mesh()

    # the AI sliver: ships with initial orders fly them (the
    # Instructor's waypoints) at their own table numbers; ships without
    # orders stay inert exactly as before
    nav = WaypointAIClass.new()
    nav.set_lists(mission.waypoints)
    for name in ship_nodes:
        if name == player_entry["name"]:
            continue
        var e: Dictionary = ship_entries[name]
        if e["ai_goals"].is_empty():
            continue
        var speed := 50.0
        var turn := 1.0
        if sp and sp.ships.has(e["pof"]):
            speed = sp.ships[e["pof"]]["max_vel"].z
            turn = sp.ships[e["pof"]]["max_rotvel"].y
        nav.register(name, e["pos"], e["fvec"], speed, turn,
                     ship_nodes[name].data.radius)
        var g := _top_goal(e["ai_goals"])
        _ai_command(name, int(g["mode"]), String(g["target"]))

    print("mission: \"%s\" -- %d/%d ships placed, player %s, %d events"
        % [mission.mission_name, placed, mission.ships.size(),
           player_entry["name"], vm.events.size()])

func _fatal(msg: String) -> void:
    printerr("mission: " + msg)
    get_tree().quit(1)

func _physics_process(delta: float) -> void:
    if fm == null:
        return

    # pointer capture: grabbing in _ready races the WM (window not yet
    # mapped/focused, the grab fails silently) -- assert it on the first
    # tick instead; any click re-grabs if the WM dropped it
    if not mouse_grabbed:
        mouse_grabbed = true
        Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
    var ci := {
        "pitch": clampf(_axis(KEY_UP, KEY_DOWN)
                        - mouse_accum.y * MOUSE_SENS, -1.0, 1.0),
        "heading": clampf(_axis(KEY_RIGHT, KEY_LEFT)
                          + mouse_accum.x * MOUSE_SENS, -1.0, 1.0),
        "bank": _axis(KEY_Q, KEY_E),
        "forward": throttle,
    }
    mouse_accum = Vector2.ZERO
    fm.read_flying_controls(ci, delta)
    fm.sim(delta)

    player_ship.position = g_pos(fm.pos)
    player_ship.basis = g_basis(fm.rvec, fm.uvec, fm.fvec)

    for r in player_ship.rotators():
        r["node"].rotate_object_local(r["axis"], 0.5 * delta)

    # training speed context: armed by set-training-context-speed, the
    # `speed` predicate reads how long the player has held the band
    if tc_speed:
        if fm.fspeed >= tc_speed_min:
            if tc_speed_set_ms < 0:
                tc_speed_set_ms = vm.ms
        else:
            tc_speed_set_ms = -1

    # the trigger: LCtrl or mouse button 1 held fires at the gun's
    # cadence, bolts from just past the nose so the shooter's own
    # sphere never eats them
    if Input.is_key_pressed(KEY_CTRL) \
            or Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
        var muzzle: Vector3 = fm.pos \
            + fm.fvec * (player_ship.data.radius + 2.0)
        weapons.try_fire(vm.ms, muzzle, fm.fvec, fm.vel)

    # the movers move first, so this frame's collisions and SEXP
    # answers see fresh positions
    for c in nav.step(delta, vm.mt_fix):
        print("waypoints done: %s, %s" % [c["ship"], c["path"]])
    _update_ai_ships()

    var positions := {}
    for name in ship_nodes:
        positions[name] = ship_entries[name]["pos"]
    for h in weapons.step(delta, vm.ms, vm.mt_fix, positions):
        if h["killed"]:
            _kill_ship(h["name"])

    vm.frame(delta)
    _update_bolts()
    _update_messages()
    _update_directives()
    _update_target_box()

    _update_camera(delta)
    hud_right.text = "speed %6.1f\nengine %4d%%" % [fm.fspeed, int(throttle * 100.0)]

static func _axis(pos: Key, neg: Key) -> float:
    return (1.0 if Input.is_key_pressed(pos) else 0.0) \
        - (1.0 if Input.is_key_pressed(neg) else 0.0)

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventMouseMotion:
        mouse_accum += event.relative
        return
    if event is InputEventMouseButton and event.pressed:
        if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
            Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
        if event.button_index == MOUSE_BUTTON_LEFT:
            # guns are guns: firing by mouse satisfies the SEXP token
            key_used["left ctrl"] = vm.ms if vm else 0
        elif event.button_index == MOUSE_BUTTON_RIGHT \
                and not missiles_logged:
            missiles_logged = true
            print("missiles: button 2 bound, the secondary slice "
                  + "hasn't landed yet")
        return
    if not (event is InputEventKey and event.pressed):
        return
    # retail Control_config[].used: the ms clock at last press, read by
    # key-pressed formulas through the bindings table
    for token in BINDINGS:
        if event.keycode == BINDINGS[token][0]:
            key_used[token] = vm.ms if vm else 0
    match event.keycode:
        KEY_A:
            throttle = clampf(throttle + 0.1, -1.0, 1.0)
        KEY_Z:
            throttle = clampf(throttle - 0.1, -1.0, 1.0)
        KEY_0:
            throttle = 0.0
        KEY_T:      # retail: target next
            targeting.next_target(vm.ms)
        KEY_H:      # retail: target next hostile
            targeting.next_hostile(int(player_entry["team"]), vm.ms)
        KEY_M:      # match speed: our targets are inert, so hold station
            if targeting.target != "":
                throttle = 0.0
        KEY_V:
            view_chase = not view_chase
            player_ship.model.visible = view_chase
        KEY_R:
            fm.pos = player_entry["pos"]
            fm.rvec = player_entry["rvec"]
            fm.uvec = player_entry["uvec"]
            fm.fvec = player_entry["fvec"]
            fm.vel = Vector3.ZERO
            fm.rotvel = Vector3.ZERO
            fm.prev_ramp_vel = Vector3.ZERO
            throttle = 0.0
        KEY_F1:     # HUD toggle (H now targets hostiles, retail's key)
            hud.visible = not hud.visible
        KEY_ESCAPE:
            get_tree().quit()

func _setup_camera() -> void:
    cam = Camera3D.new()
    cam.far = 40000.0
    add_child(cam)
    cam.position = player_ship.position + Vector3(0, 4, 20)

func _update_camera(delta: float) -> void:
    if not view_chase:
        var g := eye_parent.global_transform
        var pos := g * eye_point
        var dir := (g.basis * eye_normal).normalized()
        cam.global_position = pos
        cam.look_at(pos + dir * 100.0, player_ship.basis.y)
        return
    var r: float = maxf(player_ship.data.radius, 1.0)
    var target: Vector3 = player_ship.position \
        + player_ship.basis * Vector3(0.0, r * 0.6, r * 2.2)
    var k := 1.0 - exp(-6.0 * delta)
    cam.position = cam.position.lerp(target, k)
    cam.look_at(player_ship.position
        + player_ship.basis * Vector3(0, 0, -r * 4.0), player_ship.basis.y)

# bolt visuals: one shared elongated unshaded mesh, a node pool sized to
# weapons.projectiles each frame -- the sim owns the bolts, these mirror
func _setup_bolt_mesh() -> void:
    var box := BoxMesh.new()
    box.size = Vector3(0.4, 0.4, 12.0)
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.albedo_color = Color(1.0, 0.35, 0.25)
    box.material = mat
    bolt_mesh = box

func _update_bolts() -> void:
    while bolt_nodes.size() < weapons.projectiles.size():
        var b := MeshInstance3D.new()
        b.mesh = bolt_mesh
        add_child(b)
        bolt_nodes.append(b)
    while bolt_nodes.size() > weapons.projectiles.size():
        bolt_nodes.pop_back().queue_free()
    for i in bolt_nodes.size():
        var p: Dictionary = weapons.projectiles[i]
        var b: MeshInstance3D = bolt_nodes[i]
        b.position = g_pos(p["pos"])
        var dir := g_pos(p["vel"]).normalized()
        var up := Vector3.UP if absf(dir.y) < 0.99 else Vector3.BACK
        b.basis = Basis.looking_at(dir, up)

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
    s.radius = 6.0
    s.height = 12.0
    s.radial_segments = 4
    s.rings = 2
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.albedo_color = Color(0.9, 0.9, 1.0)
    s.material = mat
    mm.mesh = s
    mm.instance_count = 800
    var rng := RandomNumberGenerator.new()
    rng.seed = 0x46533200
    for i in mm.instance_count:
        var dir := Vector3(rng.randfn(), rng.randfn(), rng.randfn()).normalized()
        mm.set_instance_transform(i,
            Transform3D(Basis(), dir * rng.randf_range(4000.0, 7000.0)))
    var mmi := MultiMeshInstance3D.new()
    mmi.multimesh = mm
    add_child(mmi)

# HUD corners in Iosevka (a SystemFont: fontconfig resolves the installed
# family, nothing is bundled): mission + ship upper-left, engine readouts
# upper-right, the key help along the bottom edge. H hides the lot.
func _setup_hud() -> void:
    hud = CanvasLayer.new()
    add_child(hud)

    hud_left = _hud_label()
    hud_left.position = Vector2(16, 12)
    hud_left.text = ship_label
    hud.add_child(hud_left)

    hud_right = _hud_label()
    hud_right.set_anchors_preset(Control.PRESET_TOP_RIGHT)
    hud_right.grow_horizontal = Control.GROW_DIRECTION_BEGIN
    hud_right.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
    hud_right.offset_top = 12
    hud_right.offset_right = -16
    hud.add_child(hud_right)

    var help := _hud_label()
    help.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
    help.grow_vertical = Control.GROW_DIRECTION_BEGIN
    help.offset_left = 16
    help.offset_bottom = -12
    help.text = "mouse steers, M1 guns, M2 missiles, Q/E roll, A/Z throttle, T/H target, M match, V view, R reset, F1 hud, Esc quit"
    hud.add_child(help)

    # the target monitor's data, lower-left (retail's corner)
    target_box = _hud_label()
    target_box.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
    target_box.grow_vertical = Control.GROW_DIRECTION_BEGIN
    target_box.offset_left = 16
    target_box.offset_bottom = -70
    target_box.add_theme_color_override("font_color", Color(1.0, 0.85, 0.5))
    hud.add_child(target_box)

    # brackets over the target in the view
    target_marker = _hud_label()
    target_marker.text = "[    ]"
    target_marker.visible = false
    target_marker.add_theme_color_override("font_color",
                                           Color(1.0, 0.85, 0.5))
    hud.add_child(target_marker)

    # Sensky talks here: training messages, top center, wrapped inside the
    # middle 70% so they never collide with the corner readouts
    msg_label = _hud_label()
    msg_label.anchor_left = 0.15
    msg_label.anchor_right = 0.85
    msg_label.offset_top = 160
    msg_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    msg_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    msg_label.add_theme_color_override("font_color",
                                       Color(0.65, 0.9, 1.0))
    hud.add_child(msg_label)

    # the directives list, right edge at a third down -- the lesson's
    # current task (retail's directives gauge, rebuilt lean)
    directives = _hud_label()
    directives.set_anchors_preset(Control.PRESET_CENTER_RIGHT)
    directives.grow_horizontal = Control.GROW_DIRECTION_BEGIN
    directives.grow_vertical = Control.GROW_DIRECTION_BOTH
    directives.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
    directives.offset_right = -16
    directives.add_theme_color_override("font_color",
                                        Color(0.6, 1.0, 0.6))
    hud.add_child(directives)

# ---- the world interface: predicates and actions for SexpVM ----
# retail's own semantics where the sim can answer (sexp.cc functions named
# per case); honest logged stubs where a slice hasn't landed yet.

func _pos_of(pname: String) -> Vector3:
    if pname == player_entry["name"]:
        return fm.pos
    if weapons.destroyed.has(pname):   # gone: distance goes NAN, retail's
        return Vector3.INF             # failed ship_name_lookup
    var e: Dictionary = ship_entries.get(pname, {})
    return e["pos"] if e.has("pos") else Vector3.INF

# a kill: the ship leaves the scene, the target cycle, the AI, and the
# world's answers -- the destroyed registry (stamped in weapons.step) is
# what is-destroyed-delay reads from here on
func _kill_ship(name: String) -> void:
    if ship_nodes.has(name):
        ship_nodes[name].queue_free()
        ship_nodes.erase(name)
    targeting.remove(name)
    nav.ships.erase(name)
    print("destroyed: ", name)

# retail keeps MAX_AI_GOALS prioritized slots; this world flies one
# order at a time, the highest-priority initial goal
func _top_goal(goals: Array) -> Dictionary:
    var best := {}
    for g in goals:
        if best.is_empty() or int(g["priority"]) > int(best["priority"]):
            best = g
    return best

func _ai_command(name: String, mode: int, target: String) -> void:
    var verb: String = AI_VERBS.get(mode, "")
    if verb == "":
        if not ai_logged.has(mode):
            ai_logged[mode] = true
            print("ai: goal bit %d unsupported, %s holds station"
                  % [mode, name])
        return
    if mode == 1 << 10 and not ai_logged.has("guard"):
        ai_logged["guard"] = true
        print("ai: ai-guard degrades to stay-still (guard flight is a "
              + "later slice)")
    if not nav.command(name, verb, target):
        print("ai: cannot command %s %s %s" % [name, verb, target])

# add-goal's decoded form (sexp_add_goal -> ai_add_goal_sub_sexp): the
# goal sublist's head op is the verb, its first argument the target
func _add_goal(ship: String, gop: String, target: String) -> void:
    match gop:
        "ai-waypoints-once", "ai-waypoints":
            if not nav.command(ship, "waypoints-once", target):
                print("ai: add-goal cannot start %s on %s" % [ship, target])
        "ai-stay-still", "ai-play-dead":
            nav.command(ship, "stay-still")
        "ai-guard":
            _ai_command(ship, 1 << 10, "")
        _:
            if not ai_logged.has(gop):
                ai_logged[gop] = true
                print("ai: add-goal %s is a no-op in this world" % gop)

# mirror the movers: entries carry the live position (distance, the
# weapons step, the target box all read it), nodes carry the visual
func _update_ai_ships() -> void:
    for name in nav.ships:
        var st: Dictionary = nav.ships[name]
        if st["mode"] == "still" or not ship_nodes.has(name):
            continue
        ship_entries[name]["pos"] = st["pos"]
        var f: Vector3 = st["fvec"]
        var r: Vector3 = Vector3.UP.cross(f)
        if r.length_squared() < 1e-9:
            r = Vector3.RIGHT       # flying straight up: pick a wing
        r = r.normalized()
        var n: Node3D = ship_nodes[name]
        n.position = g_pos(st["pos"])
        n.basis = g_basis(r, f.cross(r), f)

func sexp_op(op: String, n, v) -> int:
    match op:
        "targeted":                 # sexp.cc:6528 via Targeting; the
            # subsystem arg belongs to the turrets, false until then
            if n["rest"] != null and n["rest"]["rest"] != null:
                return 0
            var delay := 0
            if n["rest"] != null:
                delay = v.num(n["rest"])
            return 1 if targeting.targeted_check(
                v.ctext(n), delay, v.ms) else 0

        "key-pressed":              # sexp.cc:6494
            var used: int = key_used.get(v.ctext(n).to_lower(), 0)
            if used == 0:
                return 0
            if n["rest"] == null:
                return 1
            return 1 if v.timestamp_has_time_elapsed(
                used, v.num(n["rest"]) * 1000) else 0

        "key-reset":                # sexp.cc:6516
            key_used.erase(v.ctext(n).to_lower())
            return 1

        "training-msg":             # sexp_send_training_message, sexp.cc:6731
            var delay := 0
            var length := -1
            if n["rest"] != null and n["rest"]["rest"] != null:
                delay = v.num(n["rest"]["rest"]) * 1000
                if n["rest"]["rest"]["rest"] != null:
                    length = v.num(n["rest"]["rest"]["rest"])
            var mname: String = v.ctext(n)
            if not (v.events[v.event_index]["repeat_count"] > 1
                    or n["rest"] == null):
                mname = v.ctext(n["rest"])
            msg_queue.append({
                "at": v.timestamp(delay),
                "until": v.timestamp(delay + (length if length > 0 else 8)
                                     * 1000),
                "text": _subst(messages.get(mname, mname)),
            })
            return 1

        "distance":                 # sexp.cc:3994, center-to-center meters
            var p1 := _pos_of(v.ctext(n))
            var p2 := _pos_of(v.ctext(n["rest"]))
            if p1 == Vector3.INF or p2 == Vector3.INF:
                return v.NAN_
            return int((p1 - p2).length())

        "facing":                   # sexp.cc:6612
            var target := _pos_of(v.ctext(n))
            if target == Vector3.INF:
                return v.KNOWN_FALSE
            var dot: float = fm.fvec.normalized().dot(
                (target - fm.pos).normalized())
            return 1 if dot >= cos(deg_to_rad(v.num(n["rest"]))) else 0

        "speed":                    # sexp.cc:6565, training context
            if tc_speed and tc_speed_set_ms >= 0 \
                    and v.timestamp_has_time_elapsed(
                        tc_speed_set_ms, v.num(n) * 1000):
                return v.KNOWN_TRUE
            return 0

        "set-training-context-speed":
            tc_speed = true
            tc_speed_min = v.num(n)
            tc_speed_set_ms = -1
            return 1

        "are-waypoints-done-delay": # sexp.cc:3543 via the nav log
            return nav.are_waypoints_done_delay(
                v.ctext(n), v.ctext(n["rest"]), v.num(n["rest"]["rest"]),
                v.mt_fix, weapons.destroyed)

        "add-goal":                 # sexp_add_goal; the world flies the
            # sliver's verbs, degrades or no-ops the rest (logged)
            var goal = n["rest"]["first"]
            var gtarget := ""
            if goal["rest"] != null:
                gtarget = v.ctext(goal["rest"])
            _add_goal(v.ctext(n), goal["text"], gtarget)
            return 1

        "is-destroyed-delay":       # sexp.cc:3314 via the destroyed
            # registry (wing names are the wings refinement: an unknown
            # name is simply never destroyed)
            var names := []
            var m = n["rest"]
            while m != null:
                names.append(v.ctext(m))
                m = m["rest"]
            return weapons.is_destroyed_delay(names, v.num(n), v.mt_fix)

        "hits-left":                # sexp.cc:3835
            return weapons.hits_left(v.ctext(n))

        "has-arrived-delay":        # every ship stands at t=0 (Fred's view;
            # arrival cues are the wings refinement) -- true after the delay
            return v.KNOWN_TRUE if v.f2i(v.mt_fix) >= v.num(n) else 0

        _:
            if op in STUB_ACTIONS:
                v.log_stub(op, "action stubbed true (its slice hasn't landed)")
                return 1
            if op in STUB_PREDICATES:
                v.log_stub(op, "predicate awaiting its slice, false")
                return 0
            v.log_unknown(op)
            return 0

func directive_satisfied(e) -> void:
    print("directive satisfied: ", e["objective_text"])

func goal_changed(g) -> void:
    print("goal %s: %s" % ["COMPLETE" if g["satisfied"] == 1 else "FAILED",
                           g["name"]])

# $key$ tokens substitute through the bindings table -- Sensky names OUR
# keys, remaps included
func _subst(text: String) -> String:
    var re := RegEx.create_from_string("\\$([^$]+)\\$")
    var out := text
    for m in re.search_all(text):
        var token := m.get_string(1).to_lower().trim_prefix("press ")
        var disp: String = BINDINGS[token][1] if BINDINGS.has(token) \
            else m.get_string(1)
        out = out.replace(m.get_string(0), disp)
    return out

func _update_messages() -> void:
    var live := []
    for m in msg_queue:
        if m["at"] <= vm.ms and vm.ms < m["until"]:
            live.append(m["text"])
    msg_label.text = "\n\n".join(live)

# the target monitor's data half (retail's is lower-left bitmap art):
# name, class, range, speed, hull -- hull is live from the weapons ledger.
# Inert ships: speed 0. The marker brackets the target in the 3D view.
func _update_target_box() -> void:
    if targeting.target == "":
        target_box.text = ""
        target_marker.visible = false
        return
    var e: Dictionary = ship_entries[targeting.target]
    var dist := int((_pos_of(targeting.target) - fm.pos).length())
    var hull: int = weapons.hits_left(targeting.target)
    target_box.text = "%s\n%s\nrange %5d   speed %3d   hull %3d%%" \
        % [e["name"], e["ship_class"], dist,
           int(nav.speed_of(targeting.target)), maxi(hull, 0)]

    var gpos := g_pos(e["pos"])
    if cam.is_position_behind(gpos):
        target_marker.visible = false
        return
    target_marker.visible = true
    var p := cam.unproject_position(gpos)
    target_marker.position = p - target_marker.size / 2.0

func _update_directives() -> void:
    var lines := []
    for e in vm.events:
        if e["objective_text"] == "":
            continue
        if e["satisfied_time"] != 0:
            # linger green for a few seconds after completion
            if vm.mt_fix - e["satisfied_time"] < 5 * 65536:
                lines.append("[done] " + _subst(e["objective_text"]))
        elif e["current"] and e["formula"] != null:
            var line: String = _subst(e["objective_text"])
            if e["objective_key_text"] != "":
                line += "  (%s)" % _subst(e["objective_key_text"])
            lines.append(line)
    directives.text = "\n".join(lines)

# legible on a big display: large type, shadowed against bright hulls
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
