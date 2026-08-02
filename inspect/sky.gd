# -*- mode: gdscript -*-
#
# The sky: lights, the panorama background, and the mission's authored
# suns (each driving a directional light in its RGBI) on a far
# camera-riding shell. Split from world.gd 2026-08-01 (the god-script
# split). Rebuilt per mission (setup_backdrop after every load), reset()
# strips the authored half and keeps the key light for reuse.
#
# The deep background is NASA's Deep Star Maps 2020 (SVS 4851, public
# domain; Gaia DR2 / ESA/Gaia/DPAC) as a panorama Sky -- it replaced the
# procedural starfield AND retail's 1998 nebula/planet backdrop bitmaps
# (2026-08-02, deliberate aesthetic call: every mission shares one
# galaxy; the suns still carry the per-mission lighting mood, and the
# volumetric neb2 fog is a separate, untouched system). The sky also
# feeds ambient light, so the scene brightness rides the Environment
# knobs below, not a flat constant.
#
# Passive module: world calls in -- setup once, setup_backdrop per
# mission, follow(cam) per frame -- nothing calls back.
extends Node3D

# the mission's authored suns, retail's own numbers: each sits along its
# instance matrix's uvec at retail's angular scale (a sun's radius is
# 0.05*scale rad), painted on a far shell that follows the camera (no
# parallax). stars.tbl gives each sun the RGBI that takes over the key
# light.
const SKY_R := 3200.0

const STARMAP := "res://assets/starmap_2020_4k_gal.exr"

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

    # runtime-loaded like all art here (no import pipeline); mipmaps
    # tame star shimmer once the panorama is baked into the radiance map
    var img := Image.load_from_file(STARMAP)
    img.generate_mipmaps()
    var pano := PanoramaSkyMaterial.new()
    pano.panorama = ImageTexture.create_from_image(img)
    var sky_res := Sky.new()
    sky_res.sky_material = pano

    var env := Environment.new()
    env.background_mode = Environment.BG_SKY
    env.sky = sky_res
    # the map is physical radiance and reads dim raw; lift it
    env.background_energy_multiplier = 2.0
    # ambient: mostly a lifted constant, tinted by the sky -- a pure-sky
    # ambient would be DARKER than the old flat gray, not brighter
    env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
    env.ambient_light_sky_contribution = 0.3
    env.ambient_light_color = Color(0.30, 0.31, 0.38)
    env.ambient_light_energy = 1.2
    env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
    env.tonemap_exposure = 1.1
    # additive sun quads stack past 1.0 and bloom; the sky band stays under
    env.glow_enabled = true
    var we := WorldEnvironment.new()
    we.environment = env
    add_child(we)

func setup_backdrop() -> void:
    sky_root = Node3D.new()
    add_child(sky_root)

    var suns := 0
    for e in sim.backdrop():
        # non-sun bitmaps (nebulae, planets) retired -- the panorama
        # sky IS the deep background now
        if not e["sun"]:
            continue

        var u: Vector3 = e["uvec"]
        var dir := Vector3(u.x, u.y, -u.z)

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
            sky_root.add_child(_sky_quad(glow["tex"], dir, size * 2.2))
        var e_fx = fx._fx((e["name"] as String).to_lower())
        if e_fx != null:
            sky_root.add_child(_sky_quad(e_fx["tex"], dir, size))

# the sky rides the camera: infinitely far, parallax-free
func follow(cam_pos: Vector3) -> void:
    if sky_root:
        sky_root.position = cam_pos

# the mission-reset half: the authored shell and any secondary-sun
# lights go; the key light (and panorama, environment) stay for reuse
func reset() -> void:
    if sky_root:
        sky_root.queue_free()
        sky_root = null
    for child in get_children():
        if child is DirectionalLight3D and child != key_light:
            child.queue_free()

# a sun billboard: additive intensity art on the far shell
func _sky_quad(tex: Texture2D, dir: Vector3, size: float) -> MeshInstance3D:
    var mi := MeshInstance3D.new()
    var qm := QuadMesh.new()
    qm.size = Vector2(size, size)
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
    mat.albedo_texture = tex
    mat.cull_mode = BaseMaterial3D.CULL_DISABLED
    # depth-tested, NOT no_depth_test: the shell sits at SKY_R and every
    # ship is nearer, so hulls occlude the sun the ordinary way --
    # no_depth_test painted backdrop art OVER the Instructor
    # (field-reported). Priority still sorts it first among transparents
    # (bolts and explosions draw over it).
    mat.render_priority = -100
    mat.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
    qm.material = mat
    mi.mesh = qm
    mi.position = dir * SKY_R
    return mi
