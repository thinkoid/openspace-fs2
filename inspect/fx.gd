# -*- mode: gdscript -*-
#
# The transient-art shop: everything short-lived and per-record that the
# reconciler hangs in the world -- bolts, muzzle flashes, explosions,
# shockwave rings, warp discs, debris chunks, shield shimmer, thruster
# dress -- plus the baked-flipbook cache (ani2png atlases) they all draw
# from. Split from world.gd 2026-08-01 (the god-script split); function
# bodies are the reconciler's own, moved verbatim. world.gd owns WHEN
# art appears (the reconciler); this file owns WHAT it looks like.
#
# Passive module, the radar.gd pattern: world calls in, nothing calls
# back. Nodes parent under ships_root so a mission reset sweeps them
# with the ships; flash lifetimes reap on world's clock via reap().
extends RefCounted

const ShipClass := preload("res://ship.gd")

var assets_dir := ""
var ships_root: Node3D
var fx_cache := {}            # ani stem -> {tex, cols, rows, frames, fps},
                              # or null where the bake is missing
var flashes: Array[Dictionary] = []   # transient art: {node, deadline}

func setup(dir: String, root: Node3D) -> void:
    assets_dir = dir
    ships_root = root

# transient art expires on its own clock; a parent freed by the
# reconciler takes its flash with it
func reap(now_ms: int) -> void:
    var live_flashes: Array[Dictionary] = []
    for f: Dictionary in flashes:
        var fn: Node = f["node"]
        if not is_instance_valid(fn):
            continue
        if now_ms >= int(f["deadline"]):
            fn.queue_free()
            continue
        # a shield patch dims over its life instead of blinking out
        if f.has("fade_mat"):
            var born := int(f["born"])
            var span := maxf(float(int(f["deadline"]) - born), 1.0)
            var k := 1.0 - clampf((now_ms - born) / span, 0.0, 1.0)
            (f["fade_mat"] as StandardMaterial3D).albedo_color.a = \
                float(f["alpha0"]) * k
        live_flashes.append(f)
    flashes = live_flashes

# the mission-reset half: the nodes die with ships_root's children (the
# reconciler frees those); only the deadline list needs clearing. The
# art cache survives -- it is mission-independent.
func reset() -> void:
    for f in flashes:
        if is_instance_valid(f["node"]):
            (f["node"] as Node).queue_free()
    flashes.clear()

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

# the shield taking a hit: retail lights the struck section of the ship's
# OWN shield mesh (the POF's SHLD chunk, which pof2glb carries into the
# .tres), not a bubble around the hull. We bake that mesh into four
# quadrant meshes once per class and light the struck one additively.
#
# The quadrant test is retail's, shield.cc get_quadrant -- two halfplanes
# in x and z, giving 0 right / 1 front / 2 rear / 3 left. Retail runs it
# in FILE space; pof2glb's axis map leaves x alone and flips z
# (godot.x = file.x, godot.z = -file.z), so `x < z` becomes `x < -z` and
# `x < -z` becomes `x < z`. Nothing else about the test changes.
# A whisper, not a slab. Two things made the first cut blinding: the alpha
# was half-opaque, and CULL_DISABLED drew the far side of the patch too, so
# every pixel got the additive contribution TWICE. Culling to the near
# surface alone and dropping the alpha to a twentieth leaves a faint glow
# that reads as energy on the hull rather than a pane of blue glass.
# (0.55 -> 0.10 -> 0.05, both steps field-reported as still too bright.)
const SHIELD_COLOR := Color(0.40, 0.68, 1.0, 0.05)
const SHIELD_MS := 260

var shield_meshes: Dictionary = {}   # class stem -> Array[4] of ArrayMesh
var shield_patches: Dictionary = {}  # "<ship id>:<quadrant>" -> flash entry


func _shield_quadrants(stem: String, data) -> Array:
    if shield_meshes.has(stem):
        return shield_meshes[stem]

    var out: Array = [null, null, null, null]
    if data != null and not data.shield_tris.is_empty():
        var bucket := [PackedVector3Array(), PackedVector3Array(),
                       PackedVector3Array(), PackedVector3Array()]
        for t in data.shield_tris:
            var vix: PackedInt32Array = t["verts"]
            var a: Vector3 = data.shield_verts[vix[0]]
            var b: Vector3 = data.shield_verts[vix[1]]
            var c: Vector3 = data.shield_verts[vix[2]]
            var mid := (a + b + c) / 3.0
            var q := 0
            if mid.x < -mid.z:
                q |= 1
            if mid.x < mid.z:
                q |= 2
            bucket[q].append_array(PackedVector3Array([a, b, c]))
        for q in 4:
            if bucket[q].is_empty():
                continue
            var arrays := []
            arrays.resize(Mesh.ARRAY_MAX)
            arrays[Mesh.ARRAY_VERTEX] = bucket[q]
            var am := ArrayMesh.new()
            am.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
            out[q] = am

    shield_meshes[stem] = out
    return out


# one patch per ship and quadrant, REFRESHED rather than appended -- the
# old whole-bubble flash stacked a new translucent shell per hit, so
# sustained fire compounded into an opaque ball (field-reported)
func shield_hit(node: Node3D, stem: String, data, quad: int) -> void:
    var meshes := _shield_quadrants(stem, data)
    if meshes[quad] == null:
        return

    var now := Time.get_ticks_msec()
    var key := "%d:%d" % [node.get_instance_id(), quad]
    var f = shield_patches.get(key)
    if f != null and is_instance_valid(f["node"]):
        f["born"] = now
        f["deadline"] = now + SHIELD_MS
        return

    var mi := MeshInstance3D.new()
    mi.mesh = meshes[quad]
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
    mat.cull_mode = BaseMaterial3D.CULL_BACK     # near surface only
    mat.albedo_color = SHIELD_COLOR
    mi.material_override = mat
    node.add_child(mi)

    f = { "node": mi, "born": now, "deadline": now + SHIELD_MS,
          "fade_mat": mat, "alpha0": SHIELD_COLOR.a }
    flashes.append(f)
    shield_patches[key] = f

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
