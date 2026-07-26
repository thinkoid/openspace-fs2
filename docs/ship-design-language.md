# A ship design language for openspace

Direction note, 2026-07-26. The retail campaign keeps retail's fleet —
this is the design bible seed for openspace's *own* ship line, the
replacement designs that come after the port. The premise: every hull
visibly answers the one question space combat games politely ignore —
**where does the heat go?**

## The physics that shapes the hulls

Space is the worst place in the universe to dissipate heat. Vacuum
kills convection and conduction dead; radiation is the only exit, and
Stefan-Boltzmann is the whole law:

    P = ε σ A T⁴        σ = 5.67e-8 W/m²K⁴

At 300 K a perfect black surface sheds ~460 W/m². A fighter throwing
gigawatt-class beam shots at a charitable 20% wall-plug efficiency must
reject hundreds of megawatts — at room temperature that is a square
*kilometer* of panel. The T⁴ term is the escape hatch: radiators run
hot on purpose. At 1000 K a panel sheds ~57 kW/m²; at 1500 K, ~290.
Real radiators glow dull red to orange, and they are still large.

Consequences, each one a design feature:

- **Heat is ammunition.** No combat ship tows fragile square
  kilometers into a knife fight. The realistic combat loop is a
  *thermal capacitor* — a mass of lithium or salt that melts during
  the engagement and eats the heat — plus deployable radiators that
  pay the debt down after the fight, over hours. Combat endurance is a
  heat budget: you disengage when the sink is full, not when the
  magazine is empty. (Children of a Dead Earth models exactly this;
  the Atomic Rockets site carries the full liturgy.)
- **Radiators cannot be armored.** Armor is insulation — the one thing
  a radiator must not be. Radiators are the soft kill on every hull,
  the reason a disabled ship drifts with its panels shredded rather
  than its engines melted.
- **Open-cycle is the emergency.** Boiling coolant overboard buys
  minutes at the cost of mass. A ship venting glowing vapor is a ship
  losing the thermal fight.

## The visual vocabulary

No thermal simulation is required for ships to *read* real — they need
fictional consistency, not physics. Every design answers the question
visibly:

- **Panels.** Flat, thin, large, arranged edge-on to likely threat
  axes, conspicuously unarmored against the hull's armored core.
  Retracted/deployed states tell the ship's posture: panels folded =
  cleared for action, running on the sink; panels spread = cruising,
  radiating the debt.
- **Glow.** Radiator temperature is state made visible: dull red in
  cruise, angry orange after a long burn or a beam duel. Engine bells
  and weapon spines glow after use and cool visibly.
- **The armored core vs. the fragile fringe.** The silhouette
  contrast IS the doctrine: a compact greebled body wearing large
  delicate geometry it cannot afford to lose.
- **Droplet variants for capitals.** Sheets of falling coolant between
  boom arms — spectacular, unarmorable, and instantly readable as
  "capital ship at cruise."

## What FS2 already owns that this maps onto

The machinery is sitting in the engine, most of it already touched by
the migration:

- **POF subsystems** are named, placed, destroyable machinery. A
  `radiator01` subsystem per panel makes "shoot out his radiators" a
  real tactic with zero new engine concepts.
- **Subsystem targeting and `is-subsystem-destroyed-delay`** exist in
  retail's SEXP vocabulary (both currently in the migration's deferred
  pile — this ship line is the reason to lift them).
- **`$Rotation`/movement submodels** (the inspection scene already
  drives them) cover deployable panel articulation.
- **Disable-by-subsystem** is retail doctrine already (`ai-disable`,
  engine subsystems); radiators extend the same grammar to heat.

A heat *mechanic* (heat-as-ammo gauges, sink capacity, forced
disengage) is post-port modernization and explicitly out of scope for
the retail-forward port. The design language costs nothing now and
leaves the mechanic's door open.

## The asset route for generated hulls

Proven today with a Meshy-generated gunship (twin spinal cannons,
radiator panels, single engine bell — the design language by
instinct): generation delivers ~1M triangles; the entire GTF Ulysses
is 830. Two routes into the cockpit:

1. **Godot-native** (second-class): Meshy GLB straight into the
   inspect scene; synthesize the ShipData .tres (radius/bbox from the
   mesh, eye point by hand) and borrow flight params from a comparable
   retail ship. No POF, no oracle, no retail life. Fast, honest for
   previews.
2. **Full citizenship via POF**: decimate to retail fighter budget
   (~1000×), bake the lost detail into textures, write POF with the
   culverin writer (the only POF writer in the workspace; its traps
   are mapped — flat BSP, the ~1100-vertex-per-submodel cap,
   cumulative normnum), then ride the ENTIRE existing pipeline:
   pof2glb, oracle, ships.tbl entry, the works.

Route 2 is the real one for a ship line. Note the convergence:
radiator panels — big, flat, low-poly — are exactly the geometry that
survives decimation and the POF vertex caps gracefully. The expensive
part of shrinking a generated hull is the greebled core; the design
language's signature feature is nearly free.

## Where this lives

- Retail campaign: retail fleet, untouched ("cousins, not siblings").
- openspace's own missions and any future campaign: this design
  language, one hull at a time, starting whenever a generated model is
  worth the decimation pass.
- First candidate: the Meshy gunship, as a target drone or flyable on
  the weapons range.
