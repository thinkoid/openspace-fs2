# -*- mode: gdscript -*-
#
# The inspection scene -- the eyeball half of the conversion gates
# (docs/godot-migration-plan.md, "Verification"): the automated checks pin the
# numbers, this pins what no diff can, that the ship *looks* right -- UV
# origin, materials, the axis map, articulation. Run it with
#
#     godot --path inspect -- /abs/path/to/ship.glb
#
# Loading is RUNTIME, deliberately: the GLB through GLTFDocument
# .append_from_file and the sibling .tres through ResourceLoader, absolute
# paths, no import pipeline -- so what renders is the converter's raw output,
# not Godot's reprocessing of it. The per-node POF facts (movement type/axis)
# are read straight out of the GLB's JSON chunk, not from importer metadata.
#
# Keys:  1 guns   2 missiles  3 turrets     4 thrusters  5 docks
#        6 eyes   7 paths     8 subsystems  9 shield     0 bounds/axes
#        D detail level   M spin   R reset movables   H help   Esc quit
# Mouse: left-drag orbits, wheel zooms.
extends Node3D

# POF movement_axis -> Godot rotation axis. Two traps compound here
# (docs/pof-corpus-survey.txt): the encoding itself is -1 none / 0 X / 1 Z /
# 2 Y -- Y and Z SWAPPED relative to the naive reading (model.hh:37) -- and
# the axes are POF-frame, so each must cross the same (x,y,z) -> (-x,y,-z)
# map as the geometry (tools/pof2glb.cc, "the axis map"). Decode either wrong
# and the Faustus dish tumbles instead of turning.
const AXIS_TO_GODOT := {
    0: Vector3(-1, 0, 0), # POF X
    1: Vector3(0, 0, -1), # POF Z (encoding value 1!)
    2: Vector3(0, 1, 0),  # POF Y (encoding value 2!)
}

const SPIN_RATE := 0.8 # rad/s, inspection speed -- not a retail value

var ship: Node3D
var data: Resource                # the ShipData .tres
var node_names: Array = []        # glTF node index (== submodel index) -> name
var node_extras: Array = []       # same index -> extras dict
var sub_nodes := {}               # submodel index -> scene Node3D (cache)
var movables: Array = []          # { "node": Node3D, "axis": Vector3, "rest": Transform3D }
var overlays := {}                # key -> [Node3D, ...] to toggle together
var spin := true
var detail_ix := 0
var marker_r := 0.02

var cam_pivot: Node3D
var cam: Camera3D
var cam_yaw := 0.6
var cam_pitch := -0.3
var cam_dist := 10.0
var dragging := false

var help_label: Label

func _ready() -> void:
    var args := OS.get_cmdline_user_args()
    if args.is_empty():
        _fatal("usage: godot --path inspect -- /abs/path/to/ship.glb")
        return
    var glb_path: String = args[0]
    var tres_path := glb_path.get_basename() + ".tres"

    # -- the GLB, at runtime --
    var doc := GLTFDocument.new()
    var state := GLTFState.new()
    if doc.append_from_file(glb_path, state) != OK:
        _fatal("cannot load GLB: " + glb_path)
        return
    ship = doc.generate_scene(state) as Node3D
    if ship == null:
        _fatal("GLB produced no scene: " + glb_path)
        return
    add_child(ship)

    # -- the POF extras, straight from the GLB's JSON chunk --
    if not _read_glb_json(glb_path):
        _fatal("cannot parse GLB JSON chunk: " + glb_path)
        return

    # -- the ship data --
    data = ResourceLoader.load(tres_path)
    if data == null:
        _fatal("cannot load ship data: " + tres_path)
        return
    if data.get_script() != preload("res://ship_data.gd"):
        _fatal(".tres did not load as ShipData: " + tres_path)
        return

    marker_r = maxf(data.radius * 0.008, 0.02)

    # Detail levels are alternates of one another (LOD chain), not parts:
    # show only one, and hide debris. D cycles.
    for i in data.debris_pieces:
        var n := _sub_node(i)
        if n:
            n.visible = false
    _set_detail(0)

    _collect_movables()
    _build_overlays()
    _setup_camera()
    _setup_lights()
    _setup_help()

    print("inspect: %s -- %d submodels, %d movables, detail levels %d, debris %d"
        % [glb_path.get_file(), node_names.size(), movables.size(),
           data.detail_levels.size(), data.debris_pieces.size()])

func _fatal(msg: String) -> void:
    printerr("inspect: " + msg)
    get_tree().quit(1)

# ---- GLB JSON chunk ---------------------------------------------------
#
# Node extras survive glTF -> scene only as importer metadata, and only under
# the *import pipeline*; loading at runtime, the deterministic source is the
# file itself. GLB layout (glTF 2.0 spec 4.4): 12-byte header (magic
# 0x46546C67, version, length), then the JSON chunk (length, type 'JSON',
# payload). pof2glb writes one glTF node per subobject at the same index, so
# nodes[i] here IS submodel i.

func _read_glb_json(path: String) -> bool:
    var f := FileAccess.open(path, FileAccess.READ)
    if f == null:
        return false
    if f.get_32() != 0x46546C67:
        return false
    f.get_32() # container version
    f.get_32() # total length
    var jlen := f.get_32()
    if f.get_32() != 0x4E4F534A: # 'JSON'
        return false
    var j: Variant = JSON.parse_string(f.get_buffer(jlen).get_string_from_utf8())
    if typeof(j) != TYPE_DICTIONARY or not j.has("nodes"):
        return false
    for n in j["nodes"]:
        node_names.append(n.get("name", ""))
        node_extras.append(n.get("extras", {}))
    return true

# Submodel index -> the node generate_scene() built for it, matched by name
# (generate_scene keeps glTF node names; POF names are plain identifiers, so
# nothing gets sanitized away).
func _sub_node(i: int) -> Node3D:
    if i < 0 or i >= node_names.size():
        return null
    if not sub_nodes.has(i):
        sub_nodes[i] = ship.find_child(str(node_names[i]), true, false) as Node3D
    return sub_nodes[i]

func _set_detail(ix: int) -> void:
    if data.detail_levels.is_empty():
        return
    detail_ix = ix % data.detail_levels.size()
    for k in data.detail_levels.size():
        var n := _sub_node(data.detail_levels[k])
        if n:
            n.visible = (k == detail_ix)

# ---- movables ---------------------------------------------------------
#
# movement_type 1 = rotates at runtime (dishes, panels; the Faustus solar
# panel is the slice's specimen). Type 2 (turret) moves through the
# subsystem/TGUN path, NOT this field, and 0 is runtime-inert -- both stay
# still here, same as retail (docs/pof-corpus-survey.txt).

func _collect_movables() -> void:
    for i in node_extras.size():
        var ex: Dictionary = node_extras[i]
        if int(ex.get("movement_type", -1)) != 1:
            continue
        var axis_code := int(ex.get("movement_axis", -1))
        if not AXIS_TO_GODOT.has(axis_code):
            continue
        var n := _sub_node(i)
        if n:
            movables.append({
                "node": n,
                "axis": AXIS_TO_GODOT[axis_code],
                "rest": n.transform,
            })

func _process(delta: float) -> void:
    if not spin:
        return
    for m in movables:
        m["node"].rotate_object_local(m["axis"], SPIN_RATE * delta)

# ---- overlays ---------------------------------------------------------
#
# Every point below comes from the .tres, already in the Godot frame
# (tests/check_tres.py pins the coordinates against the retail oracle) -- so
# markers landing on the wrong side of the hull would indict the axis map or
# the emitter, not this scene. All model-frame except where noted.

func _build_overlays() -> void:
    _banks_overlay("guns", data.gun_banks, Color.RED)
    _banks_overlay("missiles", data.missile_banks, Color.ORANGE)
    _turrets_overlay()
    _thrusters_overlay()
    _docks_overlay()
    _eyes_overlay()
    _paths_overlay()
    _subsystems_overlay()
    _shield_overlay()
    _bounds_overlay()

func _overlay(key: String) -> Node3D:
    var root := Node3D.new()
    root.name = "overlay_" + key
    root.visible = false
    add_child(root)
    overlays[key] = [root]
    return root

func _toggle(key: String) -> void:
    for n in overlays[key]:
        n.visible = not n.visible

func _banks_overlay(key: String, banks: Array, color: Color) -> void:
    var root := _overlay(key)
    var mat := _unshaded(color)
    var lines := PackedVector3Array()
    for bank in banks:
        var pts: PackedVector3Array = bank["points"]
        var nrm: PackedVector3Array = bank["normals"]
        for i in pts.size():
            _sphere(root, pts[i], marker_r, mat)
            lines.append(pts[i])
            lines.append(pts[i] + nrm[i] * marker_r * 6.0)
    _lines(root, lines, mat)

# Turret fire points ride the arm submodel (they must turn with it), so the
# markers anchor under its scene node. HONEST GAP: the slice pair has no
# turrets (fighters/science don't; survey: turrets live on capships), so this
# branch has never drawn -- first capship converted is its trial.
func _turrets_overlay() -> void:
    _overlay("turrets")
    var mat := _unshaded(Color.CRIMSON)
    for t in data.turrets:
        var host: Node3D = _sub_node(t["arm"])
        if host == null:
            host = ship
        var anchor := Node3D.new()
        anchor.visible = false
        host.add_child(anchor)
        overlays["turrets"].append(anchor)
        var lines := PackedVector3Array()
        for p in t["fire_points"]:
            _sphere(anchor, p, marker_r, mat)
            lines.append(p)
            lines.append(p + t["normal"] * marker_r * 6.0)
        _lines(anchor, lines, mat)

func _thrusters_overlay() -> void:
    var root := _overlay("thrusters")
    var mat := _unshaded(Color.DODGER_BLUE)
    var glow := _unshaded(Color(Color.DODGER_BLUE, 0.25))
    var lines := PackedVector3Array()
    for bank in data.thrusters:
        var pts: PackedVector3Array = bank["points"]
        var nrm: PackedVector3Array = bank["normals"]
        var radii: PackedFloat32Array = bank["radii"]
        for i in pts.size():
            _sphere(root, pts[i], marker_r, mat)
            _sphere(root, pts[i], maxf(radii[i], marker_r), glow)
            lines.append(pts[i])
            lines.append(pts[i] + nrm[i] * marker_r * 6.0)
    _lines(root, lines, mat)

func _docks_overlay() -> void:
    var root := _overlay("docks")
    var mat := _unshaded(Color.LIME_GREEN)
    var lines := PackedVector3Array()
    for dock in data.docks:
        var pts: PackedVector3Array = dock["points"]
        var nrm: PackedVector3Array = dock["normals"]
        var centroid := Vector3.ZERO
        for i in pts.size():
            _sphere(root, pts[i], marker_r, mat)
            lines.append(pts[i])
            lines.append(pts[i] + nrm[i] * marker_r * 6.0)
            centroid += pts[i]
        if pts.size():
            centroid /= pts.size()
        _label(root, centroid + Vector3.UP * marker_r * 4.0, dock["name"],
               Color.LIME_GREEN)
    _lines(root, lines, mat)

# Eye offsets are parent-submodel-relative -- the ONE field in the .tres that
# is not model-frame (dump.cc prints them raw; check_tres.py pins it) -- so
# the marker anchors under the parent's node and rides its motion.
func _eyes_overlay() -> void:
    _overlay("eyes")
    var mat := _unshaded(Color.YELLOW)
    for e in data.eyes:
        var host: Node3D = _sub_node(e["parent"])
        if host == null:
            host = ship
        var anchor := Node3D.new()
        anchor.visible = false
        host.add_child(anchor)
        overlays["eyes"].append(anchor)
        _sphere(anchor, e["point"], marker_r, mat)
        var lines := PackedVector3Array()
        lines.append(e["point"])
        lines.append(e["point"] + e["normal"] * marker_r * 6.0)
        _lines(anchor, lines, mat)

func _paths_overlay() -> void:
    var root := _overlay("paths")
    var mat := _unshaded(Color.CYAN)
    var tube := _unshaded(Color(Color.CYAN, 0.12))
    var lines := PackedVector3Array()
    for p in data.paths:
        var pts: PackedVector3Array = p["points"]
        var radii: PackedFloat32Array = p["radii"]
        for i in pts.size():
            _sphere(root, pts[i], maxf(radii[i], marker_r), tube)
            if i > 0:
                lines.append(pts[i - 1])
                lines.append(pts[i])
    _lines(root, lines, mat)

func _subsystems_overlay() -> void:
    var root := _overlay("subsystems")
    var mat := _unshaded(Color.MAGENTA)
    var vol := _unshaded(Color(Color.MAGENTA, 0.15))
    for s in data.subsystems:
        _sphere(root, s["point"], marker_r, mat)
        _sphere(root, s["point"], maxf(s["radius"], marker_r), vol)

func _shield_overlay() -> void:
    var root := _overlay("shield")
    if data.shield_tris.is_empty():
        return
    var verts := PackedVector3Array()
    for t in data.shield_tris:
        var vix: PackedInt32Array = t["verts"]
        for k in 3:
            verts.append(data.shield_verts[vix[k]])
    var arrays := []
    arrays.resize(Mesh.ARRAY_MAX)
    arrays[Mesh.ARRAY_VERTEX] = verts
    var am := ArrayMesh.new()
    am.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
    var mi := MeshInstance3D.new()
    mi.mesh = am
    var mat := _unshaded(Color(0.3, 0.6, 1.0, 0.2))
    mat.cull_mode = BaseMaterial3D.CULL_DISABLED
    mi.material_override = mat
    root.add_child(mi)

# Bbox wireframe, mass center, and an origin tripod -- the tripod plus its
# "nose -Z" tag is the axis-map eyeball aid: a converted ship must point its
# nose down -Z (Godot forward). If it doesn't, the map is wrong end to end.
func _bounds_overlay() -> void:
    var root := _overlay("bounds")
    var white := _unshaded(Color(1, 1, 1, 0.6))
    _lines(root, _box_edges(data.bbox_min, data.bbox_max), white)
    _sphere(root, data.mass_center, marker_r * 1.5, _unshaded(Color.WHITE))
    _label(root, data.mass_center + Vector3.UP * marker_r * 5.0, "mass center",
           Color.WHITE)
    var arm: float = data.radius * 0.5
    for axis in [[Vector3.RIGHT, Color.RED], [Vector3.UP, Color.GREEN],
                 [Vector3.BACK, Color.BLUE]]:
        var pts := PackedVector3Array([Vector3.ZERO, axis[0] * arm])
        _lines(root, pts, _unshaded(axis[1]))
    _label(root, Vector3.FORWARD * arm * 1.15, "nose -Z", Color.WHITE)

# ---- drawing helpers --------------------------------------------------

# The 12 edges of the axis-aligned box [a, b], as line pairs.
func _box_edges(a: Vector3, b: Vector3) -> PackedVector3Array:
    var c := [
        Vector3(a.x, a.y, a.z), Vector3(b.x, a.y, a.z),
        Vector3(a.x, b.y, a.z), Vector3(b.x, b.y, a.z),
        Vector3(a.x, a.y, b.z), Vector3(b.x, a.y, b.z),
        Vector3(a.x, b.y, b.z), Vector3(b.x, b.y, b.z),
    ]
    var e := PackedVector3Array()
    for pair in [[0, 1], [1, 3], [3, 2], [2, 0], [4, 5], [5, 7], [7, 6],
                 [6, 4], [0, 4], [1, 5], [2, 6], [3, 7]]:
        e.append(c[pair[0]])
        e.append(c[pair[1]])
    return e

func _unshaded(color: Color) -> StandardMaterial3D:
    var m := StandardMaterial3D.new()
    m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    m.albedo_color = color
    if color.a < 1.0:
        m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    return m

func _sphere(parent: Node3D, pos: Vector3, r: float,
             mat: StandardMaterial3D) -> void:
    var s := SphereMesh.new()
    s.radius = r
    s.height = 2.0 * r
    s.radial_segments = 12
    s.rings = 6
    var mi := MeshInstance3D.new()
    mi.mesh = s
    mi.material_override = mat
    mi.position = pos
    parent.add_child(mi)

# pts is pairs: (from, to), (from, to), ...
func _lines(parent: Node3D, pts: PackedVector3Array,
            mat: StandardMaterial3D) -> void:
    if pts.is_empty():
        return
    var arrays := []
    arrays.resize(Mesh.ARRAY_MAX)
    arrays[Mesh.ARRAY_VERTEX] = pts
    var am := ArrayMesh.new()
    am.add_surface_from_arrays(Mesh.PRIMITIVE_LINES, arrays)
    var mi := MeshInstance3D.new()
    mi.mesh = am
    mi.material_override = mat
    parent.add_child(mi)

func _label(parent: Node3D, pos: Vector3, text: String, color: Color) -> void:
    var l := Label3D.new()
    l.text = text
    l.modulate = color
    l.billboard = BaseMaterial3D.BILLBOARD_ENABLED
    l.no_depth_test = true
    l.pixel_size = maxf(data.radius, 1.0) * 0.0015
    l.position = pos
    parent.add_child(l)

# ---- camera, lights, help ---------------------------------------------

func _setup_camera() -> void:
    cam_pivot = Node3D.new()
    cam_pivot.position = (data.bbox_min + data.bbox_max) * 0.5
    add_child(cam_pivot)
    cam = Camera3D.new()
    cam_dist = maxf(data.radius, 1.0) * 2.5
    cam.far = maxf(cam_dist * 40.0, 4000.0)
    cam_pivot.add_child(cam)
    _update_camera()

func _update_camera() -> void:
    cam_pivot.rotation = Vector3(cam_pitch, cam_yaw, 0.0)
    cam.position = Vector3(0.0, 0.0, cam_dist)

func _setup_lights() -> void:
    var key := DirectionalLight3D.new()
    key.rotation_degrees = Vector3(-40.0, 35.0, 0.0)
    add_child(key)
    var fill := DirectionalLight3D.new()
    fill.rotation_degrees = Vector3(-15.0, -140.0, 0.0)
    fill.light_energy = 0.4
    add_child(fill)
    var env := Environment.new()
    env.background_mode = Environment.BG_COLOR
    env.background_color = Color(0.07, 0.08, 0.10)
    env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
    env.ambient_light_color = Color(0.4, 0.4, 0.45)
    var we := WorldEnvironment.new()
    we.environment = env
    add_child(we)

func _setup_help() -> void:
    var layer := CanvasLayer.new()
    add_child(layer)
    help_label = Label.new()
    help_label.text = "\n".join([
        "  1 guns   2 missiles  3 turrets     4 thrusters  5 docks",
        "  6 eyes   7 paths     8 subsystems  9 shield     0 bounds/axes",
        "  D detail level   M spin   R reset   H help   Esc quit",
        "  left-drag orbit, wheel zoom",
    ])
    help_label.position = Vector2(12, 12)
    layer.add_child(help_label)

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventMouseButton:
        match event.button_index:
            MOUSE_BUTTON_LEFT:
                dragging = event.pressed
            MOUSE_BUTTON_WHEEL_UP:
                if event.pressed:
                    cam_dist = maxf(cam_dist * 0.9, marker_r * 4.0)
                    _update_camera()
            MOUSE_BUTTON_WHEEL_DOWN:
                if event.pressed:
                    cam_dist *= 1.1
                    _update_camera()
    elif event is InputEventMouseMotion and dragging:
        cam_yaw -= event.relative.x * 0.01
        cam_pitch = clampf(cam_pitch - event.relative.y * 0.01, -1.4, 1.4)
        _update_camera()
    elif event is InputEventKey and event.pressed and not event.echo:
        match event.keycode:
            KEY_1: _toggle("guns")
            KEY_2: _toggle("missiles")
            KEY_3: _toggle("turrets")
            KEY_4: _toggle("thrusters")
            KEY_5: _toggle("docks")
            KEY_6: _toggle("eyes")
            KEY_7: _toggle("paths")
            KEY_8: _toggle("subsystems")
            KEY_9: _toggle("shield")
            KEY_0: _toggle("bounds")
            KEY_D: _set_detail(detail_ix + 1)
            KEY_M: spin = not spin
            KEY_R:
                for m in movables:
                    m["node"].transform = m["rest"]
            KEY_H: help_label.visible = not help_label.visible
            KEY_ESCAPE: get_tree().quit()
