# -*- mode: gdscript -*-
#
# The first flyable scene: a Ship under keyboard control, moved by
# FlightModel -- retail's own integrator, oracle-pinned by flight-check --
# with a chase camera and a starfield for motion reference. First waypoint
# on the road to flying the first training mission.
#
#     godot --path inspect res://fly.tscn -- /abs/path/to/ship.glb
#
# Controls:  Up/Down    pitch (stick-style: Up pushes the nose down)
#            Left/Right turn, direction-true (auto-banks into the turn)
#            Q/E        roll left/right   A/Z  throttle   0  cut throttle
#            V          chase <-> cockpit (the POF eye point; retail's view)
#            R reset    H help   Esc quit
#
# The flight parameters are FlightModel.FIGHTER (synthetic; ships.tbl is a
# later slice), whatever ship model is loaded. The model flies in FS2's
# frame; only the visual transform crosses into Godot's, through the same
# (x, y, -z) map as the geometry (tools/pof2glb.cc "the axis map").
extends Node3D

# preloaded rather than by class_name: the global class registry is not
# primed on a cold headless run (no .godot cache), preload always resolves
const ShipClass := preload("res://ship.gd")
const FlightClass := preload("res://flight_model.gd")

var ship: Node3D      # ShipClass instance
var fm                # FlightClass instance
var throttle := 0.0
var cam: Camera3D
var hud: CanvasLayer
var hud_left: Label
var hud_right: Label

# cockpit view rides the POF eye point: parent-relative (the .tres's one
# non-model-frame field), anchored to the parent submodel's node so it moves
# with articulation. Retail flies from here, HUD over empty space -- FS2 has
# no cockpit geometry, so the hull is hidden exactly as retail hides it.
var view_chase := true
var eye_parent: Node3D = null
var eye_point := Vector3.ZERO
var eye_normal := Vector3.FORWARD
var ship_label := ""

# mouse flight (same shape as mission.gd): relative motion is the stick
# deflection -- mouse right yaws starboard, mouse up noses down
var mouse_accum := Vector2.ZERO
var mouse_grabbed := false
const MOUSE_SENS := 0.05          # full deflection at ~20 px per tick

func _ready() -> void:
    var args := OS.get_cmdline_user_args()
    if args.is_empty():
        _fatal("usage: godot --path inspect -- fly /abs/ship.glb")
        return

    # last arg is the GLB: tolerates both the `-- fly <glb>` dispatch from
    # inspect.gd and a direct `res://fly.tscn -- <glb>` invocation
    var glb: String = args[args.size() - 1]

    ship = ShipClass.new()
    if not ship.load_ship(glb):
        _fatal("cannot load ship: " + glb)
        return
    add_child(ship)

    fm = FlightClass.new()

    # real numbers when available: shiptbl2tres's ship_params.tres beside
    # the GLB (retail's parse_shiptbl, oracle-checked by shiptbl-check),
    # keyed by POF stem; the synthetic FIGHTER otherwise
    var params_path := glb.get_base_dir() + "/ship_params.tres"
    var stem := glb.get_file().get_basename().to_lower()
    if FileAccess.file_exists(params_path):
        var sp: Resource = ResourceLoader.load(params_path)
        if sp and sp.ships.has(stem):
            fm.set_params(sp.ships[stem], ship.data.mass)
            ship_label = "%s (ships.tbl)" % sp.ships[stem]["name"]
    if ship_label.is_empty():
        ship_label = "%s (synthetic params)" % stem

    if ship.data.eyes.size() > 0:
        var e: Dictionary = ship.data.eyes[0]
        eye_parent = ship.sub_node(e["parent"])
        eye_point = e["point"]
        eye_normal = e["normal"]
    else:
        # no EYE chunk (capitals, drones): synthesize a nose view on the
        # hull centerline
        eye_point = Vector3(0.0, 0.0, ship.data.bbox_min.z)
    if eye_parent == null:
        eye_parent = ship

    _setup_camera()
    _setup_lights()
    _setup_starfield()
    _setup_hud()

    print("fly: %s as %s -- %d free rotators"
        % [glb.get_file(), ship_label, ship.rotators().size()])

func _fatal(msg: String) -> void:
    printerr("fly: " + msg)
    get_tree().quit(1)

func _physics_process(delta: float) -> void:
    if fm == null:   # _fatal quits deferred; don't simulate meanwhile
        return

    # pointer capture: grabbing in _ready races the WM (window not yet
    # mapped/focused) -- assert on the first tick; a click re-grabs
    if not mouse_grabbed:
        mouse_grabbed = true
        Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
    # Signs, from retail's frame (+X starboard, positive heading yaws right,
    # positive bank rolls LEFT): pitch is stick-true (Up pushes the nose
    # down), turn and roll are direction-true (Left turns left, Q rolls
    # left) -- user-calibrated 2026-07-25.
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

    # FS2 frame -> Godot frame at the visual boundary only
    ship.position = Vector3(fm.pos.x, fm.pos.y, -fm.pos.z)
    ship.basis = Basis(
        Vector3(fm.rvec.x, fm.rvec.y, -fm.rvec.z),
        Vector3(fm.uvec.x, fm.uvec.y, -fm.uvec.z),
        -Vector3(fm.fvec.x, fm.fvec.y, -fm.fvec.z))

    # the free rotators turn (dishes, panels -- loaded ROT only; the rate is
    # inspection-flavor, retail's per-subsystem turn rate is ships.tbl data)
    for r in ship.rotators():
        r["node"].rotate_object_local(r["axis"], 0.5 * delta)

    _update_camera(delta)
    hud_right.text = "speed %6.1f\nengine %4d%%" % [fm.fspeed, int(throttle * 100.0)]

# +1 when `pos` is held, -1 for `neg` -- keyboard stick
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
    match event.keycode:
        KEY_A:
            throttle = clampf(throttle + 0.1, -1.0, 1.0)
        KEY_Z:
            throttle = clampf(throttle - 0.1, -1.0, 1.0)
        KEY_0:
            throttle = 0.0
        KEY_V:
            view_chase = not view_chase
            ship.model.visible = view_chase
        KEY_R:
            fm = FlightClass.new()
            throttle = 0.0
        KEY_H:
            hud.visible = not hud.visible
        KEY_ESCAPE:
            get_tree().quit()

func _setup_camera() -> void:
    cam = Camera3D.new()
    cam.far = 20000.0
    add_child(cam)
    cam.position = Vector3(0, 4, 20)

# chase: settle toward a point behind and above the ship, always look ahead.
# cockpit: rigid on the eye point, sighting along the eye normal.
func _update_camera(delta: float) -> void:
    if not view_chase:
        var g := eye_parent.global_transform
        var pos := g * eye_point
        var dir := (g.basis * eye_normal).normalized()
        cam.global_position = pos
        cam.look_at(pos + dir * 100.0, ship.basis.y)
        return
    var r: float = maxf(ship.data.radius, 1.0)
    var target: Vector3 = ship.position \
        + ship.basis * Vector3(0.0, r * 0.6, r * 2.2)
    var k := 1.0 - exp(-6.0 * delta)
    cam.position = cam.position.lerp(target, k)
    cam.look_at(ship.position + ship.basis * Vector3(0, 0, -r * 4.0), ship.basis.y)

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

# a shell of unshaded points, far enough to read as stars, near enough to
# give roll/turn parallax
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

# HUD corners in Iosevka (a SystemFont: fontconfig resolves the installed
# family, nothing is bundled): identity upper-left, engine readouts
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
    help.text = "mouse steers, Q/E roll, A/Z throttle, 0 cut, V view, R reset, Esc quit"
    hud.add_child(help)

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
