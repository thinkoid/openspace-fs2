# -*- mode: gdscript -*-
#
# Ship -- the game-side consumer of a converted ship: assembles the GLB and
# its ShipData .tres into a runtime node the way RETAIL loads a POF, not the
# way the inspection scene displays one. Where inspect.gd deliberately spins
# every file-declared axis, this class replays retail's load-time movement
# reinterpretation (pofparse; libpof dump.cc loaded_movement), so a turret is
# ROT_SPECIAL (AI-driven, never free-spinning), a thruster's rotation is
# stripped, and a rotator without a subsystem goes inert. The replay is
# oracle-pinned: tests/check_ship_load.gd cross-checks it per submodel
# against pof_dump --model's loaded values over the whole corpus (2903
# submodels). HONEST GAP: no retail model carries $special=no_rotate (88 use
# $special=subsystem), so that branch is faithful to pofparse but pinned by
# no data.
#
# Movement types after the replay (retail's LOADED view):
#   -1 none    1 ROT (free runtime rotation: dishes, panels)
#    0 POS (runtime-inert)    2 ROT_SPECIAL (turret; subsystem/TGUN path)
class_name Ship
extends Node3D

# POF movement_axis -> Godot rotation axis; same double trap as the
# geometry: the encoding is -1/0 X/1 Z/2 Y (Y and Z swapped, model.hh:37)
# and the axes are POF-frame, crossing the (x,y,z) -> (-x,y,-z) map
# (tools/pof2glb.cc "the axis map"; inspect.gd carries the same table).
const AXIS_TO_GODOT := {
    0: Vector3(-1, 0, 0),
    1: Vector3(0, 0, -1),
    2: Vector3(0, 1, 0),
}

var model: Node3D                 # the GLB scene, LODs/debris/wrecks hidden
var data: Resource                # the ShipData .tres
var node_names: Array = []        # glTF node index (== submodel index) -> name
var node_extras: Array = []       # same index -> file-encoding extras
var loaded: Array = []            # same index -> Vector2i(type, axis), replayed
var thruster_nodes: Array = []    # is_thruster submodels, engine-gated
var _sub_nodes := {}

# Build the ship from a converted GLB (its .tres sibling is found by stem).
# Returns true on success; the node is inert on failure.
func load_ship(glb_path: String) -> bool:
    var doc := GLTFDocument.new()
    var state := GLTFState.new()
    if doc.append_from_file(glb_path, state) != OK:
        push_error("Ship: cannot load GLB: " + glb_path)
        return false
    model = doc.generate_scene(state) as Node3D
    if model == null:
        push_error("Ship: GLB produced no scene: " + glb_path)
        return false

    if not _read_glb_json(glb_path):
        push_error("Ship: cannot parse GLB JSON chunk: " + glb_path)
        return false

    data = ResourceLoader.load(glb_path.get_basename() + ".tres")
    if data == null or data.get_script() != preload("res://ship_data.gd"):
        push_error("Ship: no ShipData beside " + glb_path)
        return false

    add_child(model)

    for i in node_extras.size():
        var ex: Dictionary = node_extras[i]
        loaded.append(_loaded_movement(
            str(node_names[i]),
            int(ex.get("movement_type", -1)),
            int(ex.get("movement_axis", -1)),
            str(ex.get("properties", ""))))

    # Retail shows one detail level, no debris, and live turrets only
    # ("-destroyed" wrecks swap in on subsystem death; survey).
    for k in data.detail_levels.size():
        var n := sub_node(data.detail_levels[k])
        if n:
            n.visible = (k == 0)
    for i in data.debris_pieces:
        var n := sub_node(i)
        if n:
            n.visible = false
    for i in node_names.size():
        if str(node_names[i]).ends_with("-destroyed"):
            var n := sub_node(i)
            if n:
                n.visible = false

    # thruster submodels (is_thruster = name contains "thruster",
    # pofparse.cc:688) render only with the engines: retail skips them
    # entirely without MR_SHOW_THRUSTERS (modelinterp.cc:1192) and
    # scales the cone by forward_thrust. Hidden until someone throttles
    # up; the scale refinement can ride a later polish pass.
    for i in node_names.size():
        if str(node_names[i]).find("thruster") != -1:
            var n := sub_node(i)
            if n:
                n.visible = false
                thruster_nodes.append(n)

    return true

# engine output as visibility: on when thrusting, gone at zero
func set_thrusters(on: bool) -> void:
    for n in thruster_nodes:
        n.visible = on

# Retail's ID_SOBJ movement reinterpretation, replayed from the file values
# the GLB extras carry (libpof dump.cc loaded_movement, itself retail's
# pofparse verbatim): a type-1 submodel named turret*/gun*/cannon* becomes a
# turret (2); a thruster* loses its rotation; $special=no_rotate opts out;
# and a rotator that is not a subsystem goes inert. Name and property
# matches are case-SENSITIVE (std::string::find), the $special *value*
# compare is case-insensitive (retail stricmp) -- fidelity here is what the
# corpus oracle pins.
static func _loaded_movement(nm: String, type: int, axis: int,
                             properties: String) -> Vector2i:
    if type == 1:
        if nm.find("turret") != -1 or nm.find("gun") != -1 \
                or nm.find("cannon") != -1:
            type = 2
        elif nm.find("thruster") != -1:
            type = -1
            axis = -1

    var has_subsystem := type != 1

    if properties.find("$special") != -1:
        var value := _user_prop(properties, "$special")
        if value.nocasecmp_to("subsystem") == 0:
            has_subsystem = true
        elif value.nocasecmp_to("no_rotate") == 0:
            type = -1
            axis = -1

    if not has_subsystem:
        type = -1
        axis = -1

    return Vector2i(type, axis)

# Retail's get_user_prop_value: skip whitespace/'=' after the key (C
# isspace, so \r\n skip too), then take the value up to the first control
# character (properties are \r\n-separated).
static func _user_prop(props: String, key: String) -> String:
    var pos := props.find(key)
    if pos == -1:
        return ""
    var p := pos + key.length()
    while p < props.length():
        var c := props.unicode_at(p)
        if c == 61 or c == 32 or (c >= 9 and c <= 13):  # '=' + isspace
            p += 1
        else:
            break
    var value := ""
    while p < props.length():
        var c := props.unicode_at(p)
        if c < 32 or c == 127:                          # iscntrl
            break
        value += props[p]
        p += 1
    return value

# Submodel index -> the scene node generate_scene() built (matched by name;
# pof2glb writes one glTF node per subobject at the same index).
func sub_node(i: int) -> Node3D:
    if i < 0 or i >= node_names.size():
        return null
    if not _sub_nodes.has(i):
        _sub_nodes[i] = model.find_child(str(node_names[i]), true, false) as Node3D
    return _sub_nodes[i]

# The submodels retail would actually rotate freely at runtime (loaded type
# 1: dishes, panels), with their axes already in the Godot frame.
func rotators() -> Array:
    var out: Array = []
    for i in loaded.size():
        if loaded[i].x == 1 and AXIS_TO_GODOT.has(loaded[i].y):
            var n := sub_node(i)
            if n:
                out.append({ "node": n, "axis": AXIS_TO_GODOT[loaded[i].y] })
    return out

# The merged turrets with their scene nodes resolved (base yaws, arm
# pitches; motion belongs to the AI/subsystem layer, not this class).
func turrets() -> Array:
    var out: Array = []
    for t in data.turrets:
        out.append({
            "base": sub_node(t["base"]),
            "arm": sub_node(t["arm"]),
            "normal": t["normal"],
            "fire_points": t["fire_points"],
        })
    return out

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
