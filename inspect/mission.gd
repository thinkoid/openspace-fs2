# -*- mode: gdscript -*-
#
# The mission scene: a MissionData layout (mission2tres, retail's parser)
# spawned as real Ships, the player in the player-start ship under
# FlightModel. The first walk into a retail mission space -- Training-1
# puts you in Alpha 1's Myrmidon with the Instructor off your port bow.
# No AI, no events, no directives yet: the other ships hold station
# (arrival cues and wings are the events slice's business; every ship the
# mission knows is present, FRED's view).
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

var mission: Resource
var player_ship: Node3D
var player_entry: Dictionary
var fm
var throttle := 0.0
var cam: Camera3D
var hud: Label
var ship_label := ""

var view_chase := true
var eye_parent: Node3D = null
var eye_point := Vector3.ZERO
var eye_normal := Vector3.FORWARD

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
        placed += 1

        if e["player_start"]:
            player_ship = s
            player_entry = e
            fm = FlightClass.new()
            fm.pos = e["pos"]           # FS2 frame; the model flies there
            fm.rvec = e["rvec"]
            fm.uvec = e["uvec"]
            fm.fvec = e["fvec"]
            ship_label = "%s -- %s" % [mission.mission_name, e["name"]]
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

    print("mission: \"%s\" -- %d/%d ships placed, player %s"
        % [mission.mission_name, placed, mission.ships.size(),
           player_entry["name"]])

func _fatal(msg: String) -> void:
    printerr("mission: " + msg)
    get_tree().quit(1)

func _physics_process(delta: float) -> void:
    if fm == null:
        return
    var ci := {
        "pitch": _axis(KEY_UP, KEY_DOWN),
        "heading": _axis(KEY_RIGHT, KEY_LEFT),
        "bank": _axis(KEY_Q, KEY_E),
        "forward": throttle,
    }
    fm.read_flying_controls(ci, delta)
    fm.sim(delta)

    player_ship.position = g_pos(fm.pos)
    player_ship.basis = g_basis(fm.rvec, fm.uvec, fm.fvec)

    for r in player_ship.rotators():
        r["node"].rotate_object_local(r["axis"], 0.5 * delta)

    _update_camera(delta)
    hud.text = "%s\nspeed %5.1f   throttle %3d%%\n%s" % [
        ship_label, fm.fspeed, int(throttle * 100.0),
        "arrows fly, Q/E roll, A/Z throttle, 0 cut, V view, R reset, Esc quit"]

static func _axis(pos: Key, neg: Key) -> float:
    return (1.0 if Input.is_key_pressed(pos) else 0.0) \
        - (1.0 if Input.is_key_pressed(neg) else 0.0)

func _unhandled_input(event: InputEvent) -> void:
    if not (event is InputEventKey and event.pressed):
        return
    match event.keycode:
        KEY_A:
            throttle = clampf(throttle + 0.1, -1.0, 1.0)
        KEY_Z:
            throttle = clampf(throttle - 0.1, -1.0, 1.0)
        KEY_0:
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
        KEY_H:
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

func _setup_hud() -> void:
    var layer := CanvasLayer.new()
    add_child(layer)
    hud = Label.new()
    hud.position = Vector2(16, 12)
    hud.add_theme_font_size_override("font_size", 22)
    hud.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.8))
    hud.add_theme_constant_override("shadow_offset_x", 2)
    hud.add_theme_constant_override("shadow_offset_y", 2)
    layer.add_child(hud)
