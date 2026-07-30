# -*- mode: gdscript -*-
#
# SoundBank -- the install's own wavs played straight: message voice on
# one dedicated channel (a new line cuts the old, retail's one-speaker
# rule), effects round-robin over a small player pool. Files resolve
# from the unpacked game root the way cfile would -- case-insensitively
# against the directories retail searches (voice trees + the 16-bit
# sounds dir; 8b22k is the low-quality fallback set) -- because table
# and mission names disagree with on-disk case throughout the install.
# Every stream is a plain 8/16-bit PCM RIFF (corpus census: the entire
# voice + effects set is Microsoft PCM), which AudioStreamWAV loads
# directly; a name that resolves nowhere logs once and stays silent.
class_name SoundBank
extends Node

const DIRS := [
    "data/voice/special", "data/voice/personas", "data/voice/briefing",
    "data/voice/command_briefings", "data/voice/debriefing",
    "data/sounds/16b11k", "data/sounds/8b22k",
]
const POOL := 6

var index := {}                  # lowercased wav name -> absolute path
var cache := {}                  # lowercased wav name -> AudioStreamWAV
var voice: AudioStreamPlayer
var pool := []
var pool_next := 0
var _missing := {}

func setup(root: String) -> void:
    for d in DIRS:
        var dir := DirAccess.open(root + "/" + d)
        if dir == null:
            continue
        for f in dir.get_files():
            if f.get_extension().to_lower() == "wav":
                index[f.to_lower()] = root + "/" + d + "/" + f

    voice = AudioStreamPlayer.new()
    add_child(voice)
    for i in POOL:
        var p := AudioStreamPlayer.new()
        add_child(p)
        pool.append(p)

func _stream(name: String):
    if name == "":
        return null
    var key := name.to_lower()
    if cache.has(key):
        return cache[key]
    if not index.has(key):
        if not _missing.has(key):
            _missing[key] = true
            print("sound: %s resolves nowhere, silent" % name)
        return null
    var s := AudioStreamWAV.load_from_file(index[key])
    cache[key] = s
    return s

func play_voice(name: String) -> void:
    var s = _stream(name)
    if s == null:
        return
    voice.stop()
    voice.stream = s
    voice.play()

func play_effect(name: String, volume_db: float = 0.0) -> void:
    var s = _stream(name)
    if s == null:
        return
    var p: AudioStreamPlayer = pool[pool_next]
    pool_next = (pool_next + 1) % POOL
    p.stop()
    p.stream = s
    p.volume_db = volume_db
    p.play()

# a resolved stream for callers running their own player (engine loops)
func stream_of(name: String):
    return _stream(name)
