# -*- mode: gdscript -*-
#
# Radar -- retail's radar gauge (radar.cc radar_plot_object), the data
# path exact, the art lean Iosevka-era vectors instead of bitmap blips.
# A contact's position in the PLAYER frame projects to a polar blip:
# radial fraction = acos(z/dist)/pi -- dead ahead is the center, the
# beam is the half-radius ring, dead astern the rim (radar.cc:327) --
# direction = the x/y bearing, clipped just inside the rim
# (radar.cc:351). Screen y is inverted at draw (radar.cc:360). Retail
# quirks kept: a contact EXACTLY astern has indeterminate bearing and
# plots at the center (the zdist < 0.01 arm, radar.cc:335); blips
# beyond the player's farthest weapon range draw dim (radar.cc:376,
# fallback 1500 m); the range filter defaults to RR_INFINITY
# (hudconfig.cc:1687), so everything shows.
#
# The scene feeds `blips` each frame ({disc: Vector2, dim, hostile,
# target}); this control only draws. The lead indicator's aim point
# lives here too -- HUD math in one place.
class_name Radar
extends Control

const RIM_MARGIN := 5.0           # retail clips to radius - 5 (radar.cc:349)

var blips := []                   # [{disc, dim, hostile, target}]

# retail's blip projection: relative position in the player frame
# (FS2: x starboard, y up, z forward) -> offset in the unit disc
# (x right, y up)
static func blip_disc(rel: Vector3) -> Vector2:
    var dist := rel.length()
    if dist < 1e-6:
        return Vector2.ZERO
    var rscale := 0.0
    if dist >= rel.z:               # float-noise guard, radar.cc:323
        rscale = acos(clampf(rel.z / dist, -1.0, 1.0)) / 3.14159
    var zdist := sqrt(rel.x * rel.x + rel.y * rel.y)
    if zdist < 0.01:                # dead ahead OR dead astern: center
        return Vector2.ZERO
    return Vector2(rel.x / zdist, rel.y / zdist) * rscale

# the lead indicator's aim point: where the bolt and the target meet,
# one iteration of time-of-flight (retail refines against the solved
# range; at fighter speeds the second pass moves the point < 1 m)
static func lead_point(tpos: Vector3, tvel: Vector3, ppos: Vector3,
                       bolt_speed: float) -> Vector3:
    if bolt_speed <= 0.0:
        return tpos
    return tpos + tvel * (tpos.distance_to(ppos) / bolt_speed)

func _draw() -> void:
    var c := size / 2.0
    var r: float = minf(c.x, c.y)

    # the scope: rim, the half-radius beam ring, crosshair
    var line := Color(0.35, 0.6, 0.4, 0.8)
    draw_arc(c, r - 1.0, 0.0, TAU, 64, line, 2.0)
    draw_arc(c, r * 0.5, 0.0, TAU, 48, Color(line, 0.35), 1.0)
    draw_line(c - Vector2(r, 0), c + Vector2(r, 0), Color(line, 0.35))
    draw_line(c - Vector2(0, r), c + Vector2(0, r), Color(line, 0.35))

    for b in blips:
        var v: Vector2 = b["disc"] * r
        if v.length() > r - RIM_MARGIN:   # retail's rim clip
            v = v.normalized() * (r - RIM_MARGIN)
        var p := c + Vector2(v.x, -v.y)   # screen y inverted
        var col := Color(1.0, 0.35, 0.3) if b["hostile"] \
            else Color(0.35, 1.0, 0.4)
        if b["dim"]:
            col = Color(col, 0.4)
        var half := 4.0
        if b["target"]:                   # the current target sits proud
            half = 6.0
            draw_rect(Rect2(p - Vector2(half + 3, half + 3),
                            Vector2.ONE * (2 * half + 6)),
                      Color(1, 1, 1, 0.9), false, 1.5)
        draw_rect(Rect2(p - Vector2(half, half), Vector2.ONE * 2 * half),
                  col)
