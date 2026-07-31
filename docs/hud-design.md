# The HUD: functional, not archaeological

Decision (2026-07-31, after flying the retail-skin pass): stop
replicating retail's HUD. It is period art from a 640x480 world — the
rotating target model is ornament, the reticle arch is composition.
The port's HUD should serve a pilot, in the idiom of real fighter HUDs,
adapted to 6DOF space. The retail-skin work stays in the tree (the
`retail_hud` path in world.gd) but is parked, not polished further.

What survives from the skin pass regardless: the ani2png `--aa` bake
(interface art as alpha masks), the boundary freight (shield quadrants,
weapon energy, burner fuel, species, shield-icon names, key bindings,
primary muzzle speed), and the window-anchored layout scheme.

## The design

Jet-HUD symbology, one element per real question:

- **Velocity vector marker** — where the ship is GOING (unprojected
  vel), distinct from the boresight where it POINTS. The most valuable
  symbol in a real HUD; more so in space (inertia, no drag).
- **Boresight** — small fixed cross/chevron at gun line. Lead indicator
  stays as-is (relative-motion intercept, dot-in-ring).
- **Target box + closure** — the bracket keeps its job, and gains range
  + closure rate (d|rel_pos|/dt, sim-side data already crossing) drawn
  AT the box, not in a corner.
- **Off-screen target locator** — edge chevron toward the target when
  it leaves the view, with range. Retail had it (hudtarget's offscreen
  indicator); the port never did. The most-missed element in a fight.
- **Speed tape** (left): actual speed, commanded-throttle caret, match
  marker. **Energy/burner bars** (right): thin verticals.
- **Shield/hull glyph** (bottom): quadrant diamond + hull digits —
  data-dense, no ship portrait. Target's twin beside the target text.
- **Radar** — kept; the one retail instrument that earns its place.
  Blip scope as today (radar.gd), small, bottom-center.
- **Text**: directives, chatter, target name/class/hull/dist, ticker —
  as today.

Retired: the target-view subviewport well, the reticle arch cluster,
the shield-icon anis on the HUD (the anis remain baked; the monitor
frames remain for the parked retail path).

## Notes

- All data crosses the boundary already; this is a world.gd-only
  redesign. Closure rate = per-frame delta of |target_pos - player_pos|
  (scene-side derivative is fine; it is presentation).
- Color language: friend/foe by hue as today; the HUD green for
  neutral chrome.
