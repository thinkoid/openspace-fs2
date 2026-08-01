# equivalence: frame() rows vs snapshot() records, same frame, exact
extends SceneTree

func _initialize() -> void:
    GDExtensionManager.load_extension(OS.get_environment("FS2_GDEXT"))
    var sim = ClassDB.instantiate("FS2")
    if not sim.load(OS.get_environment("FS2_GAME_ROOT"), "SM1-01.fs2", 42):
        printerr("load failed")
        quit(1)
        return
    var ci := {}
    var bad := 0
    for w in 300:
        sim.step(1.0 / 60.0, ci)
    for probe_i in 10:
        for w in 60:
            sim.step(1.0 / 60.0, ci)
        var snap: Array = sim.snapshot()
        var fr: Dictionary = sim.frame()
        var sig: PackedInt32Array = fr["sig"]
        if snap.size() != sig.size():
            printerr("count mismatch: ", snap.size(), " vs ", sig.size())
            bad += 1
            continue
        var by_sig := {}
        for rec in snap:
            by_sig[rec["signature"]] = rec
        for j in sig.size():
            var rec: Dictionary = by_sig.get(sig[j], {})
            if rec.is_empty():
                printerr("sig ", sig[j], " missing from snapshot")
                bad += 1
                continue
            if (fr["pos"] as PackedVector3Array)[j] != (rec["pos"] as Vector3) \
                    or (fr["vel"] as PackedVector3Array)[j] != (rec["vel"] as Vector3) \
                    or (fr["rvec"] as PackedVector3Array)[j] != (rec["rvec"] as Vector3) \
                    or (fr["hull"] as PackedFloat32Array)[j] != float(rec["hull"]) \
                    or (fr["radius"] as PackedFloat32Array)[j] != float(rec["radius"]):
                printerr("field mismatch at sig ", sig[j])
                bad += 1
            var fl: int = (fr["flags"] as PackedInt32Array)[j]
            if bool(fl & 1) != bool(rec["dying"]) \
                    or bool(fl & 2) != bool(rec["afterburner"]) \
                    or bool(fl & 4) != bool(rec["player"]):
                printerr("flags mismatch at sig ", sig[j])
                bad += 1
            var sh: Array = rec["shield"]
            for k in 4:
                if (fr["shield"] as PackedFloat32Array)[j * 4 + k] != float(sh[k]):
                    printerr("shield mismatch at sig ", sig[j])
                    bad += 1
                    break
    print("frame-eq: " + ("OK" if bad == 0 else "%d MISMATCHES" % bad))
    quit(0 if bad == 0 else 1)
