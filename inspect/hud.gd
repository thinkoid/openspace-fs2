# -*- mode: gdscript -*-
#
# The HUD: every 2D pixel -- the flight symbology overlay (boresight,
# velocity vector, speed tape, target box + subsystem frame, lead
# intercept, edge chevron, weapon gauge), the text surfaces (status,
# directives, training message, radio chatter, ticker, target monitor,
# help line), the radar embed, and the debrief overlay. Split from
# world.gd 2026-08-01 (the god-script split); bodies verbatim.
#
# Passive module, the radar.gd pattern writ large: world pushes state in
# (fields + the per-frame update calls) and this layer draws it; nothing
# reaches back into the sim. Every number is the sim's; only the drawing
# lives here.
extends CanvasLayer

const RadarClass := preload("res://radar.gd")

# the flight HUD, jet symbology (docs/hud-design.md): boresight where
# the guns POINT, the velocity vector where the ship GOES, a speed tape
# with the commanded caret, the target box with range and closure AT the
# box, an edge chevron when the target leaves the view, and the lead
# intercept.
const HUD_LINE := Color(0.4, 1.0, 0.5, 0.9)
const HUD_DIM := Color(0.4, 1.0, 0.5, 0.45)
const HUD_MATCH := Color(0.45, 0.8, 1.0, 0.95)

var cam: Camera3D             # set by world; unprojection for the overlay
var sounds                    # SoundBank; chatter + training voice

var hud_left: Label
var ticker: Label
var ticker_lines: Array[String] = []
var directives: Label
var training_msg: Label
var chatter: Label
var chatter_lines: Array[Dictionary] = []   # {line, deadline}
var radar: Control                          # the retired file's art, live
var overlay: Control                        # the HUD symbology, drawn 2d
var target_monitor: Label
var hud_font: SystemFont                    # overlay text (Iosevka)

# presentation state, pushed by world each frame
var throttle := 0.0
var match_target := false
var target_sig := -1                        # hud_state's target_signature
var target_rec := {}                        # its snapshot record this frame
var target_subsys := ""                     # targeted subsystem name on it
var target_subsys_pos := Vector3.ZERO       # its world pos (FS2 frame)
var weapon_banks_p: Array = []              # weapon gauge: mounted banks,
var weapon_banks_s: Array = []              # {name, armed, shots}
var lead_speed := 0.0                       # the primary's muzzle speed
var player_vel := Vector3.ZERO              # FS2 frame, lead solution input
var player_speed := 0.0                     # forward speed, the tape's needle
var player_energy_frac := 1.0               # gun reserve fraction
var player_burner_frac := 1.0               # afterburner fuel fraction
var player_shield: Array = []
var player_shield_max := 0.0
var player_hull_frac := 1.0
var player_pos := Vector3.ZERO              # FS2 frame
var player_team := 0                        # for hostile/friendly coloring
var player_max_speed := 0.0                 # match-speed's denominator
var aim_from := Vector3.ZERO                # boresight ray for the reticle,
var aim_dir := Vector3.FORWARD              # godot frame
var target_range := 0.0                     # to the target, meters
var target_closure := 0.0                   # smoothed d(range)/dt, m/s
var last_text := ""
var msg_deadline := 0

var debrief_panel: Control

func setup(cam_: Camera3D, sounds_) -> void:
    cam = cam_
    sounds = sounds_

    hud_left = _hud_label()
    hud_left.position = Vector2(16, 12)
    add_child(hud_left)

    directives = _hud_label()
    directives.position = Vector2(16, 120)
    add_child(directives)

    training_msg = _hud_label()
    training_msg.set_anchors_preset(Control.PRESET_CENTER_TOP)
    training_msg.grow_horizontal = Control.GROW_DIRECTION_BOTH
    training_msg.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    training_msg.offset_top = 60
    training_msg.autowrap_mode = TextServer.AUTOWRAP_WORD
    training_msg.custom_minimum_size = Vector2(900, 0)
    add_child(training_msg)

    # the radio: mid-left, clear of the directives gauge above and the
    # help line below
    chatter = _hud_label()
    chatter.position = Vector2(16, 560)
    chatter.autowrap_mode = TextServer.AUTOWRAP_WORD
    chatter.custom_minimum_size = Vector2(760, 0)
    add_child(chatter)

    # the combat gauges: the retired radar art bottom-center, the target
    # monitor beside it, the bracket overlay across the whole view
    radar = RadarClass.new()
    radar.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
    radar.offset_left = -110
    radar.offset_right = 110
    radar.offset_top = -232
    radar.offset_bottom = -12
    radar.mouse_filter = Control.MOUSE_FILTER_IGNORE
    add_child(radar)

    # the target text block, positioned each frame at the HUD
    # rectangle's lower-left corner (under the speed tape) -- see
    # update_combat
    target_monitor = _hud_label()
    add_child(target_monitor)

    hud_font = SystemFont.new()
    hud_font.font_names = ["Iosevka"]

    overlay = Control.new()
    overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
    overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
    overlay.draw.connect(_draw_hud)
    add_child(overlay)

    # the ticker sits ABOVE the help line's row -- at wide help texts the
    # two used to meet in the bottom corner (field-reported overlap)
    ticker = _hud_label()
    ticker.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
    ticker.grow_horizontal = Control.GROW_DIRECTION_BEGIN
    ticker.grow_vertical = Control.GROW_DIRECTION_BEGIN
    ticker.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
    ticker.offset_right = -16
    ticker.offset_bottom = -56
    add_child(ticker)

    var help := _hud_label()
    help.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
    help.grow_vertical = Control.GROW_DIRECTION_BEGIN
    help.offset_left = 16
    help.offset_bottom = -12
    help.add_theme_font_size_override("font_size", 24)
    help.text = "mouse steers + fires (RMB missile), A/Z throttle, \\ full, Tab burner, T target, H hostile, E escort, M match, V view, Shift-Super-J end mission, Esc quit"
    add_child(help)

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

func _ui(k: float) -> float:
    return k * overlay.size.y / 1080.0

func _tick(line: String) -> void:
    ticker_lines.append(line)
    while ticker_lines.size() > 5:
        ticker_lines.pop_front()
    if ticker:
        ticker.text = "\n".join(ticker_lines)

# radio chatter: the line joins the window for retail's text-length
# formula; the voice takes the one-speaker channel (a new message cuts
# the old, retail's rule)
func add_chatter(ev: Dictionary) -> void:
    var who: String = (ev["who"] as String).trim_prefix("#")
    var text: String = (ev["text"] as String).replace("$", "")
    chatter_lines.append({
        "line": "%s: %s" % [who, text],
        "deadline": Time.get_ticks_msec() + 1000 + 150 * text.length(),
    })
    if chatter_lines.size() > 4:
        chatter_lines.pop_front()
    if not (ev["wave"] as String).is_empty():
        sounds.play_voice(ev["wave"])

# a HUD ticker line (the boundary's hud_text seam): retail's scrolling
# feedback -- warp notices, warnings -- joins the chatter window,
# speakerless
func add_hud_line(text: String) -> void:
    chatter_lines.append({
        "line": text,
        "deadline": Time.get_ticks_msec() + 1000 + 150 * text.length(),
    })
    if chatter_lines.size() > 4:
        chatter_lines.pop_front()

# the chatter window: recent radio lines, each shown for its own
# text-length window (the voice may run longer -- lines scroll off,
# the voice plays out)
func update_chatter() -> void:
    var now := Time.get_ticks_msec()
    var kept: Array[Dictionary] = []
    var lines: Array[String] = []
    for c in chatter_lines:
        if c["deadline"] >= now:
            kept.append(c)
            lines.append(c["line"])
    chatter_lines = kept
    chatter.text = "\n".join(lines)

# the lesson gauges, fed by the boundary's hud_state: the directives list
# (status-marked, key lines indented) and the training message, with
# Sensky's voice played once per message through the SoundBank
func update_lesson(h: Dictionary) -> void:
    lead_speed = float(h.get("primary_speed", 0.0))
    target_subsys = h.get("target_subsys", "")
    target_subsys_pos = h.get("target_subsys_pos", Vector3.ZERO)
    weapon_banks_p = h.get("primary_banks", [])
    weapon_banks_s = h.get("secondary_banks", [])

    var lines: Array[String] = []
    for d in h["directives"]:
        if d["key"]:
            lines.append("      " + (d["text"] as String).replace("$", ""))
            continue
        var mark := "  "
        match int(d["state"]):
            1: mark = "> "     # EVENT_CURRENT
            2: mark = "+ "     # EVENT_SATISFIED
            3: mark = "x "     # EVENT_FAILED
        lines.append(mark + (d["text"] as String).replace("$", ""))
    if lines.is_empty():
        directives.text = ""
    else:
        directives.text = "directives\n" + "\n".join(lines)

    # The message window is PRESENTATION-owned: headless retail can't time
    # a voice it cannot play (the sim exposes the text for as long as the
    # message is current), so the scene shows it while its own playback
    # runs, with retail's text-length fallback (missiontraining.cc:787)
    # when there is no wave.
    var text: String = h["training_text"]
    var voice: String = h["training_voice"]
    if text != last_text:
        last_text = text
        msg_deadline = Time.get_ticks_msec() + 1000 + 150 * text.length()
        if not voice.is_empty():
            sounds.play_voice(voice)

    var visible_now: bool = not text.is_empty() \
        and (sounds.voice.playing or Time.get_ticks_msec() < msg_deadline)
    training_msg.text = text.replace("$", "") if visible_now else ""

# the combat gauges: radar blips through the retired file's projection
# (the data path -- radar_plot_object -- runs natively; this is the art),
# the target monitor's readout, and a redraw request for the bracket
func update_combat(prec: Dictionary, ship_recs: Array, target_sig_: int,
                   target_rec_: Dictionary, target_radius: float) -> void:
    target_sig = target_sig_
    target_rec = target_rec_
    _target_radius = target_radius

    var ppos: Vector3 = prec["pos"]
    var rv: Vector3 = prec["rvec"]
    var uv: Vector3 = prec["uvec"]
    var fv: Vector3 = prec["fvec"]
    var pteam: int = prec["team"]
    player_team = pteam
    aim_from = Vector3(ppos.x, ppos.y, -ppos.z)
    aim_dir = Vector3(fv.x, fv.y, -fv.z)
    player_max_speed = prec.get("max_speed", 0.0)
    player_vel = prec["vel"]
    player_shield = prec.get("shield", [])
    player_shield_max = prec.get("shield_max", 0.0)
    player_hull_frac = prec["hull"] / maxf(prec["hull_max"], 1.0)

    var blips := []
    for rec in ship_recs:
        if rec["player"] or rec["dying"]:
            continue
        var d: Vector3 = (rec["pos"] as Vector3) - ppos
        blips.append({
            "disc": RadarClass.blip_disc(
                Vector3(rv.dot(d), uv.dot(d), fv.dot(d))),
            "dim": d.length() > 1500.0,      # retail's fallback range
            "hostile": rec["team"] != pteam,
            "target": rec["signature"] == target_sig,
        })
    radar.blips = blips
    radar.queue_redraw()

    # the monitor sits at the HUD rectangle's lower-left: under the
    # speed tape's foot, on its x (the tape is the rectangle's left
    # edge), top-aligned with the weapon gauge across the rectangle --
    # same font, same small size (field-calibrated)
    target_monitor.position = Vector2(overlay.size.x * 0.16,
                                      overlay.size.y * 0.5 + _ui(226.0))
    var mfsz := int(_ui(13.0))
    if target_monitor.get_theme_font_size("font_size") != mfsz:
        target_monitor.add_theme_font_size_override("font_size", mfsz)

    if target_rec.is_empty():
        target_monitor.text = ""
        target_range = 0.0
        target_closure = 0.0
    else:
        # a dying ship's record carries the overkill (hull below zero)
        target_monitor.text = "%s\n%s\nhull %3d%%" % [
            target_rec["name"], target_rec["class"],
            maxi(0, int(100.0 * target_rec["hull"]
                        / maxf(target_rec["hull_max"], 1.0)))]
        if not target_subsys.is_empty():
            target_monitor.text += "\nsys: %s" % target_subsys
        # range and closure (positive = closing), smoothed for the box
        var rng := ((target_rec["pos"] as Vector3) - ppos).length()
        var dt := get_physics_process_delta_time()
        if dt > 0.0 and target_range > 0.0:
            target_closure = lerpf(target_closure,
                                   (target_range - rng) / dt, 0.15)
        target_range = rng

    overlay.queue_redraw()

var _target_radius := 10.0    # the target's model radius, for box sizing

func _draw_hud() -> void:
    if cam == null:
        return
    var vp := overlay.size
    var fsz := int(_ui(26.0))

    # boresight: the gun line
    var aim := aim_from + aim_dir * 1000.0
    if not cam.is_position_behind(aim):
        var rp := cam.unproject_position(aim)
        overlay.draw_arc(rp, _ui(12.0), 0.0, TAU, 32, HUD_LINE, 1.5)
        for d: Vector2 in [Vector2.RIGHT, Vector2.LEFT, Vector2.UP,
                           Vector2.DOWN]:
            overlay.draw_line(rp + d * _ui(7.0), rp + d * _ui(16.0),
                              HUD_LINE, 1.5)

    # the flight-path marker: where the ship actually GOES -- the one
    # symbol a real HUD is built around; with inertia it rarely agrees
    # with the boresight
    var vel_g := Vector3(player_vel.x, player_vel.y, -player_vel.z)
    if vel_g.length() > 0.5:
        var vv := aim_from + vel_g.normalized() * 1000.0
        if not cam.is_position_behind(vv):
            var vc := cam.unproject_position(vv)
            overlay.draw_arc(vc, _ui(6.0), 0.0, TAU, 24, HUD_LINE, 1.5)
            overlay.draw_line(vc + Vector2(-_ui(15.0), 0),
                              vc + Vector2(-_ui(6.0), 0), HUD_LINE, 1.5)
            overlay.draw_line(vc + Vector2(_ui(6.0), 0),
                              vc + Vector2(_ui(15.0), 0), HUD_LINE, 1.5)
            overlay.draw_line(vc + Vector2(0, -_ui(13.0)),
                              vc + Vector2(0, -_ui(6.0)), HUD_LINE, 1.5)

    _draw_speed_tape(vp, fsz)
    _draw_weapon_gauge(vp, fsz)

    if target_rec.is_empty():
        return

    var col := Color(1.0, 0.35, 0.3) if target_rec["team"] != player_team \
        else Color(0.35, 1.0, 0.4)

    var p3: Vector3 = target_rec["pos"]
    var v := Vector3(p3.x, p3.y, -p3.z)

    # off the view: an edge chevron pointing the way, range beside it
    var onscreen := not cam.is_position_behind(v)
    var p := Vector2.ZERO
    if onscreen:
        p = cam.unproject_position(v)
        onscreen = Rect2(Vector2.ZERO, vp).has_point(p)
    if not onscreen:
        var local: Vector3 = cam.global_transform.affine_inverse() * v
        var dir2 := Vector2(local.x, -local.y)
        if dir2.length() < 0.001:
            dir2 = Vector2.DOWN
        dir2 = dir2.normalized()
        var centre := vp * 0.5
        var edge_pt := centre + dir2 * (minf(vp.x, vp.y) * 0.5 - _ui(80.0))
        var perp := Vector2(-dir2.y, dir2.x)
        overlay.draw_colored_polygon(PackedVector2Array([
            edge_pt + dir2 * _ui(18.0), edge_pt + perp * _ui(9.0),
            edge_pt - perp * _ui(9.0)]), col)
        overlay.draw_string(hud_font,
                            edge_pt - dir2 * _ui(26.0) + Vector2(-_ui(36.0),
                                                                 _ui(9.0)),
                            "%.0f m" % target_range,
                            HORIZONTAL_ALIGNMENT_LEFT, -1, fsz, col)
        return

    # the box, sized by radius at distance; range + closure ride its side
    var edge := cam.unproject_position(
        v + cam.global_basis.x * _target_radius)
    var half := clampf((edge - p).length(), _ui(24.0), _ui(300.0))
    var arm := half * 0.5
    for corner: Vector2 in [Vector2(-1, -1), Vector2(1, -1), Vector2(-1, 1),
                            Vector2(1, 1)]:
        var c := p + corner * half
        overlay.draw_line(c, c - Vector2(corner.x * arm, 0), col, 2.0)
        overlay.draw_line(c, c - Vector2(0, corner.y * arm), col, 2.0)
    overlay.draw_string(hud_font, p + Vector2(half + _ui(10.0), 0),
                        "%.0f m" % target_range,
                        HORIZONTAL_ALIGNMENT_LEFT, -1, fsz, col)
    overlay.draw_string(hud_font,
                        p + Vector2(half + _ui(10.0), fsz + _ui(4.0)),
                        "%+.0f m/s" % target_closure,
                        HORIZONTAL_ALIGNMENT_LEFT, -1, fsz, col)

    # the subsystem frame: a second, smaller bracket on the targeted
    # subsystem (S cycles it) -- absent when the hull itself is the
    # target (no targeted_subsys crosses)
    if not target_subsys.is_empty():
        var sv := Vector3(target_subsys_pos.x, target_subsys_pos.y,
                          -target_subsys_pos.z)
        if not cam.is_position_behind(sv):
            var sc := cam.unproject_position(sv)
            var sh := _ui(16.0)
            for corner: Vector2 in [Vector2(-1, -1), Vector2(1, -1),
                                    Vector2(-1, 1), Vector2(1, 1)]:
                var cc3 := sc + corner * sh
                overlay.draw_line(cc3, cc3 - Vector2(corner.x * sh * 0.6, 0),
                                  col, 1.5)
                overlay.draw_line(cc3, cc3 - Vector2(0, corner.y * sh * 0.6),
                                  col, 1.5)

    # the lead intercept: put the boresight on the dot and the bolts
    # meet the target -- solved on RELATIVE motion, bolts inherit the
    # shooter velocity (|R + Vt| = s*t, smallest t > 0)
    if lead_speed <= 0.0:
        return
    var rp3 := (target_rec["pos"] as Vector3) - player_pos
    var rv3 := (target_rec["vel"] as Vector3) - player_vel
    var a := rv3.dot(rv3) - lead_speed * lead_speed
    var b := 2.0 * rp3.dot(rv3)
    var cc := rp3.dot(rp3)
    var t := -1.0
    if absf(a) < 1e-3:
        if absf(b) > 1e-6:
            t = -cc / b
    else:
        var disc := b * b - 4.0 * a * cc
        if disc >= 0.0:
            var sq := sqrt(disc)
            var t1 := (-b - sq) / (2.0 * a)
            var t2 := (-b + sq) / (2.0 * a)
            t = t1 if t1 > 0.0 else t2
    if t > 0.0 and t < 10.0:
        var lead: Vector3 = (target_rec["pos"] as Vector3) \
            + (target_rec["vel"] as Vector3) * t
        var lv := Vector3(lead.x, lead.y, -lead.z)
        if not cam.is_position_behind(lv):
            var lp := cam.unproject_position(lv)
            overlay.draw_circle(lp, _ui(3.5), col)
            overlay.draw_arc(lp, _ui(8.0), 0.0, TAU, 24, col, 1.5)

# the weapon gauge at the HUD rectangle's lower-right, retail's bank
# list: one line per mounted bank, a box per shot the next trigger pull
# fires -- filled when the bank is armed (linked primaries fill every
# line, dual-fire missiles draw two boxes on the selected one). Same
# small font as the target monitor, both blocks top-aligned under the
# rectangle's bottom edge (field-calibrated).
func _draw_weapon_gauge(vp: Vector2, _fsz: int) -> void:
    var lines: Array = weapon_banks_p + weapon_banks_s
    if lines.is_empty():
        return

    var fsz := int(_ui(13.0))
    var x := vp.x * 0.84                       # the rectangle's right edge
    var lh := fsz + _ui(5.0)
    var y := vp.y * 0.5 + _ui(226.0) + fsz     # first baseline, top-aligned
                                               # with the monitor label
    var side := fsz * 0.55

    for b in lines:
        var col: Color = HUD_LINE if b["armed"] else HUD_DIM
        overlay.draw_string(hud_font, Vector2(x - _ui(260.0), y),
                            b["name"], HORIZONTAL_ALIGNMENT_RIGHT,
                            int(_ui(250.0)), fsz, col)
        var bx := x + _ui(8.0)
        for s in range(int(b["shots"])):
            var r := Rect2(bx, y - side * 0.85, side, side)
            if b["armed"]:
                overlay.draw_rect(r, col)
            else:
                overlay.draw_rect(r, col, false, 1.5)
            bx += side + _ui(4.0)
        y += lh

# the speed tape: 10 m/s ticks scrolling past the fixed needle, the
# commanded caret (throttle x max; match-mode blue when it drives)
func _draw_speed_tape(vp: Vector2, fsz: int) -> void:
    var x := vp.x * 0.16
    var half_h := _ui(220.0)
    var cy := vp.y * 0.5
    var span := 60.0
    # the tape floors at zero -- there is no reverse (retail's set-speed
    # never goes negative; the throttle is already clipped 0..1)
    var y_floor := minf(cy + player_speed / span * half_h, cy + half_h)
    overlay.draw_line(Vector2(x, cy - half_h), Vector2(x, y_floor),
                      HUD_DIM, 1.5)
    var m := maxi(0, int(floor((player_speed - span) / 10.0)) * 10)
    while m <= int(ceil((player_speed + span) / 10.0)) * 10:
        var y := cy + (player_speed - m) / span * half_h
        if y >= cy - half_h and y <= cy + half_h:
            var major := m % 30 == 0
            var w := _ui(16.0) if major else _ui(9.0)
            overlay.draw_line(Vector2(x - w, y), Vector2(x, y), HUD_DIM, 1.5)
            if major:
                overlay.draw_string(hud_font,
                                    Vector2(x - w - _ui(76.0),
                                            y + fsz * 0.35),
                                    "%d" % m, HORIZONTAL_ALIGNMENT_RIGHT,
                                    int(_ui(68.0)), fsz, HUD_DIM)
        m += 10
    overlay.draw_line(Vector2(x - _ui(20.0), cy), Vector2(x - _ui(2.0), cy),
                      HUD_LINE, 3.0)
    overlay.draw_string(hud_font, Vector2(x + _ui(12.0), cy + fsz * 0.35),
                        "%.0f" % player_speed, HORIZONTAL_ALIGNMENT_LEFT,
                        -1, fsz, HUD_LINE)
    var cmd := throttle * player_max_speed
    var cy2 := clampf(cy + (player_speed - cmd) / span * half_h,
                      cy - half_h, cy + half_h)
    var cc2 := HUD_MATCH if match_target else HUD_LINE
    overlay.draw_colored_polygon(PackedVector2Array([
        Vector2(x + _ui(2.0), cy2), Vector2(x + _ui(11.0), cy2 - _ui(6.0)),
        Vector2(x + _ui(11.0), cy2 + _ui(6.0))]), cc2)

# ----------------------------------------------------------------------
# the debrief overlay (visual half; world owns the campaign logic)

func build_debrief(mission_name: String, debrief_data: Dictionary) -> void:
    debrief_panel = Control.new()
    debrief_panel.set_anchors_preset(Control.PRESET_FULL_RECT)
    add_child(debrief_panel)

    var dim := ColorRect.new()
    dim.color = Color(0.0, 0.02, 0.0, 0.85)
    dim.set_anchors_preset(Control.PRESET_FULL_RECT)
    debrief_panel.add_child(dim)

    var margin := MarginContainer.new()
    margin.set_anchors_preset(Control.PRESET_FULL_RECT)
    for side in ["left", "top", "right", "bottom"]:
        margin.add_theme_constant_override("margin_" + side, int(_ui(90.0)))
    debrief_panel.add_child(margin)

    var col := VBoxContainer.new()
    col.add_theme_constant_override("separation", int(_ui(14.0)))
    margin.add_child(col)

    var fsz := int(_ui(26.0))

    var title := Label.new()
    title.text = "DEBRIEFING -- %s" % mission_name
    _debrief_style(title, int(fsz * 1.3), HUD_LINE)
    col.add_child(title)

    # the goals, status spelled out; retail's colors by verdict
    for g in debrief_data.get("goals", []):
        var line := Label.new()
        var status: String = ["FAILED", "COMPLETE", "INCOMPLETE"][g["status"]]
        line.text = "  %-11s %s" % [status, g["text"]]
        var c := HUD_DIM
        if not g["invalid"]:
            c = Color(0.35, 1.0, 0.4) if g["status"] == 1 \
                else Color(1.0, 0.35, 0.3) if g["status"] == 0 else HUD_DIM
        _debrief_style(line, fsz, c)
        col.add_child(line)

    # the selected stages, scrollable prose
    var scroll := ScrollContainer.new()
    scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
    col.add_child(scroll)

    var stages := VBoxContainer.new()
    stages.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    stages.add_theme_constant_override("separation", int(_ui(18.0)))
    scroll.add_child(stages)

    for s in debrief_data.get("stages", []):
        var para := Label.new()
        para.text = s["text"]
        para.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
        para.size_flags_horizontal = Control.SIZE_EXPAND_FILL
        _debrief_style(para, fsz, HUD_LINE)
        stages.add_child(para)
        if not (s["recommendation"] as String).is_empty():
            var rec := Label.new()
            rec.text = s["recommendation"]
            rec.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
            rec.size_flags_horizontal = Control.SIZE_EXPAND_FILL
            _debrief_style(rec, fsz, HUD_DIM)
            stages.add_child(rec)

    # the verdict and the keys
    var verdict := Label.new()
    var next: String = debrief_data.get("next_mission", "")
    if next.is_empty():
        verdict.text = "CAMPAIGN COMPLETE      [Enter] finish"
    else:
        verdict.text = "next: %s      [Enter] accept" % next
        if debrief_data.get("loop_offer", false):
            verdict.text += "      [L] optional mission: %s" \
                % debrief_data.get("loop_desc", "")
    _debrief_style(verdict, fsz, HUD_LINE)
    col.add_child(verdict)

func drop_debrief() -> void:
    if debrief_panel:
        debrief_panel.queue_free()
        debrief_panel = null

func _debrief_style(l: Label, size: int, color: Color) -> void:
    l.add_theme_font_override("font", hud_font)
    l.add_theme_font_size_override("font_size", size)
    l.add_theme_color_override("font_color", color)

# the mission-reset half: per-mission text and target state; the layer
# itself (labels, radar, overlay) survives
func reset() -> void:
    target_sig = -1
    target_rec = {}
    target_range = 0.0
    target_closure = 0.0
    player_shield = []
    player_hull_frac = 1.0

    ticker_lines.clear()
    chatter_lines.clear()
    ticker.text = ""
    chatter.text = ""
    directives.text = ""
    training_msg.text = ""
    last_text = ""
    msg_deadline = 0
    radar.blips = []
