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
# Backspace zero, Tab afterburner, Q/E roll, H hud, Esc quit.
#
# The GDScript lesson scene (mission.tscn) stays alive beside this one
# until the native path carries weapons (slice 3) and message/directive
# events (slice 4) -- this scene is the world made visible, not yet the
# lesson.
extends Node3D

const ShipClass := preload("res://ship.gd")

var sim                       # FS2 (libfs2) instance
var ships_root: Node3D
var ships := {}               # signature -> {node, is_ship, radius}
var player_sig := -1

var throttle := 0.0
var cam: Camera3D
var hud: CanvasLayer
var hud_left: Label
var hud_right: Label
var ticker: Label
var ticker_lines: Array[String] = []

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

    if not mouse_grabbed:
        mouse_grabbed = true
        Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

    var burn := Input.is_key_pressed(KEY_TAB)
    var ci := {
        "pitch": clampf(_axis(KEY_UP, KEY_DOWN)
                        - mouse_accum.y * MOUSE_SENS, -1.0, 1.0),
        "heading": clampf(_axis(KEY_RIGHT, KEY_LEFT)
                          + mouse_accum.x * MOUSE_SENS, -1.0, 1.0),
        "bank": _axis(KEY_Q, KEY_E),
        "forward": throttle,
        "afterburner": burn,
    }
    mouse_accum = Vector2.ZERO

    sim.step(delta, ci)

    for ev in sim.events():
        if ev["kind"] == "log":
            _tick("log %d: %s %s" % [ev["log_type"], ev["pname"], ev["sname"]])
        elif ev["kind"] == "destroyed":
            _tick("destroyed: " + ev["name"])

    # reconcile: the snapshot is the truth; nodes follow it
    var seen := {}
    var player_node: Node3D = null
    var player_rec := {}
    for rec in sim.snapshot():
        var sig: int = rec["signature"]
        seen[sig] = true
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

        var moving: bool = (rec["vel"] as Vector3).length() > 0.5
        if entry["is_ship"]:
            node.set_thrusters(moving or (rec["player"] and
                                          (throttle > 0.0 or burn)))

        if rec["player"]:
            player_sig = sig
            player_node = node
            player_rec = rec

    for sig in ships.keys():
        if not seen.has(sig):
            ships[sig]["node"].queue_free()
            ships.erase(sig)

    if player_node:
        _update_camera(delta, player_node,
                       ships[player_sig]["radius"])
        var vel: Vector3 = player_rec["vel"]
        var fv2: Vector3 = player_rec["fvec"]
        hud_right.text = "speed %6.1f\nengine %4d%%" \
            % [vel.dot(fv2), int(throttle * 100.0)]
        hud_left.text = "%s\n%d ships" % [mission_name, ships.size()]

# a new signature enters the world: its Ship if the assets carry the
# class's GLB, an honest gray box otherwise
func _spawn(sig: int, rec: Dictionary) -> void:
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
    match event.keycode:
        KEY_A:
            throttle = clampf(throttle + 0.1, -1.0, 1.0)
        KEY_Z:
            throttle = clampf(throttle - 0.1, -1.0, 1.0)
        KEY_0:
            throttle = 0.0
        KEY_BACKSLASH:
            throttle = 1.0
        KEY_BACKSPACE:
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

func _update_camera(delta: float, node: Node3D, radius: float) -> void:
    var r := maxf(radius, 1.0)
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

    ticker = _hud_label()
    ticker.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
    ticker.grow_horizontal = Control.GROW_DIRECTION_BEGIN
    ticker.grow_vertical = Control.GROW_DIRECTION_BEGIN
    ticker.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
    ticker.offset_right = -16
    ticker.offset_bottom = -12
    hud.add_child(ticker)

    var help := _hud_label()
    help.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
    help.grow_vertical = Control.GROW_DIRECTION_BEGIN
    help.offset_left = 16
    help.offset_bottom = -12
    help.text = "mouse steers, Q/E roll, A/Z throttle, \\ full, Tab burner, H hud, Esc quit"
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
