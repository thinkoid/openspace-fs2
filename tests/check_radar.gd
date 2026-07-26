# -*- mode: gdscript -*-
#
# The radar gate: inspect/radar.gd's blip projection pinned against
# retail's radar_plot_object math (radar.cc:323-341 -- radial fraction
# acos(z/dist)/pi, bearing x/y, the dead-astern-plots-center quirk) and
# the lead indicator's time-of-flight aim point. Pure math, no scene.
#
#   godot --headless --path <repo>/inspect --script check_radar.gd
extends SceneTree

var failed := 0

func check(what: String, got, want) -> void:
    if got != want:
        printerr("FAIL %s: got %s, want %s" % [what, got, want])
        failed += 1

func close(what: String, got: Vector2, want: Vector2) -> void:
    if (got - want).length() > 1e-4:
        printerr("FAIL %s: got %s, want %s" % [what, got, want])
        failed += 1

func _init() -> void:
    var R := preload("res://radar.gd")

    # the cardinal contract: ahead center, beam at half radius in its
    # bearing, astern near the rim -- and EXACTLY astern back at the
    # center (indeterminate bearing, radar.cc:335, retail's own quirk)
    close("dead ahead centers", R.blip_disc(Vector3(0, 0, 100)),
          Vector2.ZERO)
    close("dead astern centers (retail quirk)",
          R.blip_disc(Vector3(0, 0, -100)), Vector2.ZERO)
    close("starboard beam: right, half radius",
          R.blip_disc(Vector3(100, 0, 0)), Vector2(0.5, 0))
    close("port beam: left, half radius",
          R.blip_disc(Vector3(-100, 0, 0)), Vector2(-0.5, 0))
    close("overhead: up, half radius",
          R.blip_disc(Vector3(0, 100, 0)), Vector2(0, 0.5))

    # 45 degrees off the nose = quarter radius; 45 past the beam =
    # three quarters -- the acos fraction, not linear distance
    close("45 ahead-starboard: quarter radius",
          R.blip_disc(Vector3(100, 0, 100)), Vector2(0.25, 0))
    close("45 astern-starboard: three-quarter radius",
          R.blip_disc(Vector3(100, 0, -100)), Vector2(0.75, 0))

    # bearing splits by components: equal x and y shares the offset
    var d := R.blip_disc(Vector3(100, 100, 0)) as Vector2
    close("diagonal bearing", d, Vector2(0.5, 0.5).normalized() * 0.5)

    # range plays no part in the position -- only the angles do
    close("distance invariant", R.blip_disc(Vector3(7, 0, 7)),
          R.blip_disc(Vector3(7000, 0, 7000)))

    # the lead point: still target needs no lead; a crossing target
    # leads by velocity times time-of-flight (dist / bolt speed)
    check("still target: no lead",
          R.lead_point(Vector3(0, 0, 900), Vector3.ZERO,
                       Vector3.ZERO, 450.0), Vector3(0, 0, 900))
    check("crossing target leads by flight time",
          R.lead_point(Vector3(0, 0, 900), Vector3(70, 0, 0),
                       Vector3.ZERO, 450.0), Vector3(140, 0, 900))
    check("degenerate bolt speed: aim at the hull",
          R.lead_point(Vector3(0, 0, 900), Vector3(70, 0, 0),
                       Vector3.ZERO, 0.0), Vector3(0, 0, 900))

    if failed == 0:
        print("OK radar: projection, quirks, lead point")
    quit(1 if failed > 0 else 0)
