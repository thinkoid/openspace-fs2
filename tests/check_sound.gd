# -*- mode: gdscript -*-
#
# The sound gate: inspect/sound.gd's case-insensitive resolution over
# the install's search dirs and real AudioStreamWAV loads of the wavs
# the missions actually name -- a training voice line, the Subach's
# launch and impact, both explosion booms. FS2_GAME_ROOT points at the
# unpacked install (the driver skips without it).
#
#   FS2_GAME_ROOT=<root> godot --headless --path <repo>/inspect \
#       --script check_sound.gd
extends SceneTree

var failed := 0

func check(what: String, got, want) -> void:
    if got != want:
        printerr("FAIL %s: got %s, want %s" % [what, got, want])
        failed += 1

func _init() -> void:
    var root := OS.get_environment("FS2_GAME_ROOT")
    var S := preload("res://sound.gd")
    var s = S.new()
    get_root().add_child(s)
    s.setup(root)

    check("index sees the voice tree",
          s.index.has("dtm_in_01.wav"), true)
    check("index sees the effects dir",
          s.index.has("l_sidearm.wav"), true)

    # case-insensitive like cfile: tables and missions disagree with
    # the on-disk case throughout the install
    for name in ["DTM_IN_01.wav", "dtm_in_01.WAV", "L_Sidearm.wav",
                 "l_sidearm.wav", "hit_1.wav", "boom_3.wav",
                 "boom_1.wav"]:
        var st = s._stream(name)
        var length: float = st.get_length() if st != null else 0.0
        check("loads %s" % name, st != null and length > 0.05, true)

    # the polite failure modes: unknown stays silent (and logs once),
    # the empty name never even logs
    check("unknown resolves null", s._stream("no_such_sound.wav"), null)
    check("empty name is silent", s._stream(""), null)
    s.play_voice("no_such_sound.wav")   # must not crash
    s.play_effect("")

    if failed == 0:
        print("OK sound: resolution, loads, silence modes")
    quit(1 if failed > 0 else 0)
