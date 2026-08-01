# -*- mode: gdscript -*-
#
# The sky: lights, the deterministic starfield, and the mission's
# authored backdrop -- suns (each driving a directional light in its
# RGBI) and background patches on a far camera-riding shell. Split from
# world.gd 2026-08-01 (the god-script split); bodies verbatim. Rebuilt
# per mission (setup_backdrop after every load), reset() strips the
# authored half and keeps the key light for reuse.
#
# Passive module: world calls in -- setup once, setup_backdrop per
# mission, follow(cam) per frame -- nothing calls back.
extends Node3D

# the mission's authored sky, retail's own numbers: each element sits
# along its instance matrix's uvec at retail's angular scale (a sun's
# radius is 0.05*scale rad, a patch spans 10 deg * scale), painted on a
# far shell that follows the camera (no parallax). stars.tbl decides the
# blend -- $BitmapX green-key art by alpha, $Bitmap intensity art
# additive -- and gives each sun the RGBI that takes over the key light.
const SKY_R := 3200.0

var sim                       # FS2 (libfs2) instance, backdrop()/num_stars()
var fx                        # the art cache (fx.gd), sun/patch bakes
var sky_root: Node3D          # the authored backdrop, riding the camera
var key_light: DirectionalLight3D

func setup(sim_, fx_) -> void:
    sim = sim_
    fx = fx_

func setup_lights() -> void:
    key_light = DirectionalLight3D.new()
    key_light.rotation_degrees = Vector3(-35.0, 40.0, 0.0)
    add_child(key_light)
    var env := Environment.new()
    env.background_mode = Environment.BG_COLOR
    env.background_color = Color(0.02, 0.02, 0.04)
    env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
    env.ambient_light_color = Color(0.25, 0.25, 0.3)
    var we := WorldEnvironment.new()
    we.environment = env
    add_child(we)

func setup_starfield() -> void:
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
    # the mission's own star count ($Num stars), retail's dial
    mm.instance_count = maxi(int(sim.num_stars()), 200)
    var rng := RandomNumberGenerator.new()
    rng.seed = 0x46533200  # deterministic sky ("FS2\0")
    for i in mm.instance_count:
        var dir := Vector3(rng.randfn(), rng.randfn(), rng.randfn()).normalized()
        var t := Transform3D(Basis(), dir * rng.randf_range(2500.0, 4000.0))
        mm.set_instance_transform(i, t)
    var mmi := MultiMeshInstance3D.new()
    mmi.multimesh = mm
    add_child(mmi)

func setup_backdrop() -> void:
    sky_root = Node3D.new()
    add_child(sky_root)

    var suns := 0
    for e in sim.backdrop():
        var u: Vector3 = e["uvec"]
        var r3: Vector3 = e["rvec"]
        var f3: Vector3 = e["fvec"]
        var dir := Vector3(u.x, u.y, -u.z)
        var ax := Vector3(r3.x, r3.y, -r3.z)
        var ay := Vector3(f3.x, f3.y, -f3.z)
        var e_fx = fx._fx((e["name"] as String).to_lower())

        if e["sun"]:
            suns += 1
            var li := key_light if suns == 1 and key_light \
                else DirectionalLight3D.new()
            if li.get_parent() == null:
                add_child(li)
            li.light_color = e["color"]
            li.light_energy = maxf(e["intensity"], 0.1)
            li.basis = Basis.looking_at(-dir)      # shines FROM the sun

            var size: float = 2.0 * SKY_R * tan(0.05 * e["scale_x"])
            var glow = fx._fx((e["glow"] as String).to_lower())
            if glow != null:
                sky_root.add_child(_sky_quad(glow["tex"], dir, ax, ay,
                                             size * 2.2, size * 2.2,
                                             false, true))
            if e_fx != null:
                sky_root.add_child(_sky_quad(e_fx["tex"], dir, ax, ay,
                                             size, size, false, true))
        elif e_fx != null:
            var sx: float = 2.0 * SKY_R * tan(deg_to_rad(5.0 * e["scale_x"]))
            var sy: float = 2.0 * SKY_R * tan(deg_to_rad(5.0 * e["scale_y"]))
            sky_root.add_child(_sky_quad(e_fx["tex"], dir, ax, ay, sx, sy,
                                         e["xparent"], false))

# the sky rides the camera: infinitely far, parallax-free
func follow(cam_pos: Vector3) -> void:
    if sky_root:
        sky_root.position = cam_pos

# the mission-reset half: the authored shell and any secondary-sun
# lights go; the key light (and starfield, environment) stay for reuse
func reset() -> void:
    if sky_root:
        sky_root.queue_free()
        sky_root = null
    for child in get_children():
        if child is DirectionalLight3D and child != key_light:
            child.queue_free()

func _sky_quad(tex: Texture2D, dir: Vector3, ax: Vector3, ay: Vector3,
               sx: float, sy: float, alpha: bool,
               billboard: bool) -> MeshInstance3D:
    var mi := MeshInstance3D.new()
    var qm := QuadMesh.new()
    qm.size = Vector2(sx, sy)
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    if alpha:
        mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    else:
        mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
    mat.albedo_texture = tex
    mat.cull_mode = BaseMaterial3D.CULL_DISABLED
    # depth-tested, NOT no_depth_test: the shell sits at SKY_R and every
    # ship is nearer, so hulls occlude the sky the ordinary way --
    # no_depth_test painted the nebula OVER the Instructor
    # (field-reported). Priority still sorts it first among transparents
    # (bolts and explosions draw over the sky).
    mat.render_priority = -100
    if billboard:
        mat.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
    qm.material = mat
    mi.mesh = qm
    mi.position = dir * SKY_R
    if not billboard:
        mi.basis = Basis(ax, ay, dir)
    return mi
