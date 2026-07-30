# -*- mode: gdscript -*-
#
# RETIRED TO SPEC (2026-07-30): the runtime is libfs2 -- retail's own
# eval_sexp/mission_eval_goals, the very code this file transcribed. The
# planned differential gate was overtaken by events: the original itself
# now runs. This file remains the annotated, readable specification of
# the evaluator's semantics (KNOWN_* caching, NAN rules, the timestamp
# overload) -- the commentary the C++ never carried.
#
# SexpVM -- retail's SEXP evaluator and mission event loop, ported
# function-for-function from sexp.cc (eval_sexp, eval_when, sexp_and/or/
# not) and missiongoals.cc (mission_process_event, mission_eval_goals).
# The formulas come from MissionData (mission2tres's canonical one-line
# text, itself oracle-gated against retail's parse); this class re-parses
# them into cons cells mirroring Sexp_nodes -- first/rest/value -- so
# retail's short-circuit caching (SEXP_KNOWN_*) ports verbatim.
#
# Predicates that touch the game world go through `world` (the mission
# scene). An operator the world can't answer yet logs itself ONCE and
# evaluates false -- the mission is its own TODO list; watch the output
# to see which verb it starves for (targeted, is-destroyed-delay...
# the targeting and weapons slices).
#
# Units, kept retail-exact: Missiontime is fix (1/65536 s) held in
# mt_fix; scheduling timestamps are a millisecond clock (timestamp()).
# mission_event.timestamp is BOTH, exactly as retail overloads it --
# a ms deadline while the event repeats, (int)Missiontime once done
# (missiongoals.cc:925 vs :953); is-event-true-delay does fix math on it
# (sexp.cc:6128), which is only coherent for completed events. Bug-compat.
class_name SexpVM
extends RefCounted

const TRUE_ := 1
const FALSE_ := 0
const KNOWN_FALSE := -1
const KNOWN_TRUE := -2
const UNKNOWN := -3
const NAN_ := -4
const NAN_FOREVER := -5
const CANT_EVAL := -6

var world: Object                # the mission scene: predicates + actions
var events: Array = []           # mission_event mirrors, .tres order
var goals: Array = []
var mt_fix: int = 0              # Missiontime, fix units
# the timer ms clock. Starts at an arbitrary nonzero epoch exactly like
# retail's (app uptime at mission start): timestamp_elapsed treats 0 as
# never-elapsed, so a zero clock would wedge Mission_goal_timestamp
var ms: int = 1000
var goal_ts: int = 0             # Mission_goal_timestamp (ms deadline)
var event_index: int = 0         # Event_index during eval
var _unknown_logged := {}

const GOAL_TIMESTAMP_TRAINING_MS := 500


# ---- sexp text -> cons cells (mirrors Sexp_nodes: text/first/rest/value) --

static func _cell(text: String, quoted: bool) -> Dictionary:
    return {"text": text, "str": quoted, "first": null, "rest": null,
            "value": UNKNOWN}

static func parse(text: String):
    var toks := []
    var re := RegEx.create_from_string('"[^"]*"|[()]|[^\\s()"]+')
    for m in re.search_all(text):
        toks.append(m.get_string())
    var pos := [0]
    return _parse_list(toks, pos)

static func _parse_list(toks: Array, pos: Array):
    # consumes past the opening '(' the caller saw; returns the head cell
    var head = null
    var tail = null
    while pos[0] < toks.size():
        var t: String = toks[pos[0]]
        pos[0] += 1
        var cell = null
        if t == "(":
            cell = _cell("", false)
            cell["first"] = _parse_list(toks, pos)
        elif t == ")":
            return head
        elif t.begins_with('"'):
            cell = _cell(t.substr(1, t.length() - 2), true)
        else:
            cell = _cell(t, false)
        if head == null:
            head = cell
        else:
            tail["rest"] = cell
        tail = cell
    return head

func load_mission(data: Resource) -> void:
    goal_ts = timestamp(0)   # missiongoals.cc:394 -- eval from the start
    for e in data.events:
        var f = parse(e["formula"])
        events.append({
            "name": e["name"], "formula": f,
            "repeat_count": int(e["repeat_count"]),
            "interval": int(e["interval"]),
            "chain_delay": int(e["chain_delay"]),
            "objective_text": e["objective_text"],
            "objective_key_text": e["objective_key_text"],
            "result": 0, "timestamp": -1, "satisfied_time": 0,
            "current": false,
        })
    for g in data.goals:
        goals.append({
            "name": g["name"], "type": int(g["type"]),
            "message": g["message"], "formula": parse(g["formula"]),
            "satisfied": 0,   # GOAL_INCOMPLETE
        })

# ---- the timer module, minimally (timer.cc semantics) ----

func timestamp(delta_ms: int) -> int:
    return ms + delta_ms

static func timestamp_valid(t: int) -> bool:
    return t != -1

func timestamp_elapsed(t: int) -> bool:
    return t != 0 and ms >= t

func timestamp_has_time_elapsed(stamp_ms: int, delta_ms: int) -> bool:
    return ms >= stamp_ms + delta_ms   # timer.cc: time since `stamp` >= delta

static func i2f(x: int) -> int:
    return x * 65536

static func f2i(x: int) -> int:
    return x / 65536

# ---- mission_eval_goals / mission_process_event (missiongoals.cc) ----

func frame(delta: float) -> void:
    mt_fix += int(delta * 65536.0)
    ms += int(delta * 1000.0)

    # repeating events whose interval popped re-eval on their own schedule
    for i in events.size():
        var e = events[i]
        if e["formula"] != null and timestamp_valid(e["timestamp"]) \
                and timestamp_elapsed(e["timestamp"]):
            process_event(i)

    if not timestamp_elapsed(goal_ts):
        return

    for g in goals:
        if g["type"] & (1 << 16):   # INVALID_GOAL
            continue
        if g["satisfied"] == 0:
            var result := eval_sexp(g["formula"])
            if g["formula"]["value"] == KNOWN_FALSE:
                g["satisfied"] = 2    # GOAL_FAILED
                world.goal_changed(g)
            elif result != 0:
                g["satisfied"] = 1    # GOAL_COMPLETE
                world.goal_changed(g)

    for i in events.size():
        var e = events[i]
        if e["formula"] != null and not timestamp_valid(e["timestamp"]):
            process_event(i)

    # retail training cadence (GOAL_TIMESTAMP_TRAINING); the campaign value
    # is 0 = every frame, training throttles to 2 Hz
    goal_ts = timestamp(GOAL_TIMESTAMP_TRAINING_MS)

func process_event(i: int) -> void:
    var e = events[i]
    var formula = e["formula"]
    var result: int = e["result"]

    # chained: previous event must be true (plus delay), next not yet true
    if e["chain_delay"] >= 0:
        if i > 0:
            var prev = events[i - 1]
            if prev["result"] == 0 \
                    or prev["timestamp"] + i2f(e["chain_delay"]) > mt_fix:
                formula = null
        if i < events.size() - 1 and events[i + 1]["result"] != 0 \
                and events[i + 1]["chain_delay"] >= 0:
            formula = null

    if formula != null:
        event_index = i
        result = eval_sexp(formula)
        e["current"] = true
        event_index = 0

    e["result"] = result

    if formula != null and formula["value"] == KNOWN_FALSE:
        e["timestamp"] = mt_fix     # (int)Missiontime, the fix-unit stamp
        e["satisfied_time"] = mt_fix
        e["repeat_count"] = -1
        e["formula"] = null
        return

    if result != 0 and e["satisfied_time"] == 0:
        e["satisfied_time"] = mt_fix
        if e["objective_text"] != "":
            world.directive_satisfied(e)

    if result != 0 or timestamp_valid(e["timestamp"]):
        e["repeat_count"] -= 1
        if e["repeat_count"] <= 0:
            e["timestamp"] = mt_fix
            e["formula"] = null
        else:
            e["timestamp"] = timestamp(e["interval"] * 1000)

# ---- eval_sexp (sexp.cc:7855) over cons cells ----

func ctext(n) -> String:
    return n["text"]

static func num(n) -> int:
    return int(n["text"])

func eval_sexp(n) -> int:
    if n == null:
        return 0
    if n["value"] == KNOWN_TRUE:
        return 1
    if n["value"] == KNOWN_FALSE:
        return 0

    if n["first"] != null:
        var v := eval_sexp(n["first"])
        n["value"] = n["first"]["value"]
        return v

    var args = n["rest"]
    var sexp_val := _dispatch(n["text"], args)

    if sexp_val == KNOWN_TRUE:
        n["value"] = KNOWN_TRUE
        return 1
    if sexp_val == KNOWN_FALSE:
        n["value"] = KNOWN_FALSE
        return 0
    if sexp_val == NAN_:
        n["value"] = NAN_
        return 0
    if sexp_val == NAN_FOREVER:
        n["value"] = NAN_FOREVER
        return sexp_val
    if sexp_val == CANT_EVAL:
        n["value"] = CANT_EVAL
        return 0
    n["value"] = TRUE_ if sexp_val != 0 else FALSE_
    return sexp_val

func _dispatch(op: String, n) -> int:
    match op:
        "true":
            return KNOWN_TRUE
        "false":
            return KNOWN_FALSE
        "and":
            return _and(n)
        "or":
            return _or(n)
        "not":
            return _not(n)
        "when":
            return _when(n)
        "has-time-elapsed":
            return KNOWN_TRUE if f2i(mt_fix) >= num(n) else 0
        "is-event-true-delay":
            return _event_delay_status(n, true)
        "is-event-false-delay":
            return _event_delay_status(n, false)
        "is-goal-true-delay":
            return _goal_delay_status(n, true)
        "is-goal-false-delay":
            return _goal_delay_status(n, false)
        "do-nothing":
            return TRUE_
        "+":
            return _arith(n, func(a, b): return a + b)
        "-":
            return _arith(n, func(a, b): return a - b)
        "*":
            return _arith(n, func(a, b): return a * b)
        "/":
            return _arith(n, func(a, b): return a / b if b != 0 else 0)
        "mod":
            return _arith(n, func(a, b): return a % b if b != 0 else 0)
        ">":
            return _compare(n, func(a, b): return a > b)
        "<":
            return _compare(n, func(a, b): return a < b)
        "=":
            return _compare(n, func(a, b): return a == b)
        _:
            # everything else touches the world; the scene answers what it
            # can and logs what it can't
            return world.sexp_op(op, n, self)

# sexp_and (sexp.cc:2814): no short-circuit, KNOWN propagation
func _and(n) -> int:
    var result := -1   # all-bits, C's `result &= ...`
    var all_true := true
    while n != null:
        var sub = n["first"]
        if sub != null:
            result &= eval_sexp(sub)
            if sub["value"] == KNOWN_FALSE:
                return KNOWN_FALSE
            if sub["value"] != KNOWN_TRUE:
                all_true = false
        else:
            result &= _atom_bool(n)
        n = n["rest"]
    return KNOWN_TRUE if all_true else result

func _or(n) -> int:
    var result := 0
    var all_false := true
    while n != null:
        var sub = n["first"]
        if sub != null:
            result |= eval_sexp(sub)
            if sub["value"] == KNOWN_TRUE:
                return KNOWN_TRUE
            if sub["value"] != KNOWN_FALSE:
                all_false = false
        else:
            result |= _atom_bool(n)
        n = n["rest"]
    return KNOWN_FALSE if all_false else result

func _atom_bool(n) -> int:
    # a bare atom in boolean position: retail atoi()s it, but an OPERATOR
    # atom ((and (true)(false)) parses ops as sublists, so this is numbers
    # only) -- except our cons cells keep ops as atoms too when they have
    # no args wrapper; evaluate those as operators
    if n["str"] or n["text"].is_valid_int():
        return int(n["text"])
    return eval_sexp(n)

func _not(n) -> int:
    var result := 0
    if n != null:
        var sub = n["first"]
        if sub != null:
            result = eval_sexp(sub)
            if sub["value"] == KNOWN_FALSE:
                return KNOWN_TRUE
            if sub["value"] == KNOWN_TRUE:
                return KNOWN_FALSE
            if sub["value"] == NAN_ or sub["value"] == NAN_FOREVER:
                return 1
        else:
            result = _atom_bool(n)
    return 1 if result == 0 else 0

# eval_when (sexp.cc:4725): actions eval only for side effects
func _when(n) -> int:
    var cond = n["first"] if n["first"] != null else n
    var val := eval_sexp(cond)
    if val != 0:
        var actions = n["rest"]
        while actions != null:
            if actions["first"] != null:
                val = eval_sexp(actions["first"])
            actions = actions["rest"]
    if cond["value"] == KNOWN_FALSE:
        return KNOWN_FALSE
    if val == KNOWN_FALSE:
        return 0
    return val

# an argument in numeric position: a literal, or a nested op (num_eval)
func _num_arg(n) -> int:
    if n["first"] != null:
        return eval_sexp(n["first"])
    if n["str"] or n["text"].is_valid_int():
        return int(n["text"])
    return eval_sexp(n)

# add_sexps and kin (sexp.cc:2598): fold left over the argument list
func _arith(n, f: Callable) -> int:
    if n == null:
        return 0
    var acc := _num_arg(n)
    n = n["rest"]
    while n != null:
        acc = f.call(acc, _num_arg(n))
        n = n["rest"]
    return acc

# sexp_gt/lt/equal (sexp.cc:2946): NAN makes it false, NAN_FOREVER known
func _compare(n, f: Callable) -> int:
    var a := _num_arg(n)
    var b := _num_arg(n["rest"])
    var va: int = n["first"]["value"] if n["first"] != null else UNKNOWN
    var vb: int = n["rest"]["first"]["value"] \
        if n["rest"]["first"] != null else UNKNOWN
    if va == NAN_ or vb == NAN_:
        return 0
    if va == NAN_FOREVER or vb == NAN_FOREVER:
        return KNOWN_FALSE
    return 1 if f.call(a, b) else 0

# sexp_event_delay_status (sexp.cc:6117), fix math on the overloaded stamp
func _event_delay_status(n, want_true: bool) -> int:
    var ename := ctext(n)
    var delay := i2f(num(n["rest"]))
    for e in events:
        if e["name"].nocasecmp_to(ename) == 0:
            if e["timestamp"] + delay >= mt_fix:
                return FALSE_
            var result: bool = e["result"] != 0
            if e["formula"] == null:
                if want_true == result:
                    return KNOWN_TRUE
                return KNOWN_FALSE
            if want_true and result:
                return TRUE_
            return FALSE_
    return 0

func _goal_delay_status(n, want_true: bool) -> int:
    var gname := ctext(n)
    for g in goals:
        if g["name"].nocasecmp_to(gname) == 0:
            if want_true and g["satisfied"] == 1:
                return KNOWN_TRUE
            if not want_true and g["satisfied"] == 2:
                return KNOWN_TRUE
            if g["satisfied"] == 0:
                return FALSE_
            return KNOWN_FALSE
    return 0

func log_unknown(op: String) -> void:
    log_stub(op, "op UNKNOWN, evaluates false")

func log_stub(op: String, why: String) -> void:
    if not _unknown_logged.has(op):
        _unknown_logged[op] = true
        print("sexp: %s: %s" % [op, why])
