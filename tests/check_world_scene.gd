# -*- mode: gdscript -*-
#
# The reconciler gate's engine half: boot world.tscn headless (the args
# after `--` are world.gd's own -- world <mission> <assets> <root>; the
# extension comes from FS2_GDEXT), run ~6 sim-seconds of physics frames,
# and assert the reconciliation happened: the Instructor and Alpha 1 exist
# as scene nodes under /root/World/Ships, and the Instructor's node
# MEASURABLY MOVED between two probes -- retail AI flying a Godot node.
#
# Exit 0 clean, 1 on any failure.
extends SceneTree

var frames := 0
var probe := Vector3.ZERO
var world: Node3D

func _initialize() -> void:
    world = (load("res://world.tscn") as PackedScene).instantiate()
    root.add_child(world)

func _process(_delta: float) -> bool:
    frames += 1

    if frames == 60:
        var n := world.get_node_or_null("Ships/Instructor") as Node3D
        if n == null:
            printerr("FAIL: no Instructor node at frame 60")
            quit(1)
            return true
        probe = n.global_position

    if frames >= 360:
        var inst := world.get_node_or_null("Ships/Instructor") as Node3D
        var alpha := world.get_node_or_null("Ships/Alpha 1") as Node3D
        if inst == null or alpha == null:
            printerr("FAIL: ships missing at frame 360")
            quit(1)
            return true
        var moved := (inst.global_position - probe).length()
        if moved < 10.0:
            printerr("FAIL: Instructor node moved only %f units" % moved)
            quit(1)
            return true
        print("OK: reconciled world -- Instructor's node flew %.1f units in %d frames"
              % [moved, frames - 60])
        quit(0)
        return true

    return false
