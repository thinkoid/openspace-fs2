// savejson -- the pilot-save codec: retail's .plr / .csg / .css decoded to
// JSON and encoded back. The contract is byte fidelity: decode | encode
// must reproduce the original file exactly (the gate cmps it), so a save
// can be JSON-ified, hand-edited, and re-encoded without disturbing
// anything but the edited field.
//
//   savejson <save-file>              decode: JSON on stdout (format by magic)
//   savejson <edited.json> <outfile>  encode: binary written to outfile
//
// The formats are the writers' own byte streams (managepilot.cc's
// write_pilot_file_core, missioncampaign.cc's mission_campaign_savefile_save
// -- single-version: .plr 140, .csg 12, .css 1), mirrored here primitive by
// primitive. The raw-struct blocks (color, detail_levels, scoring_struct)
// come from the game's own headers, so the codec always matches the layout
// THIS build reads and writes -- the padding a struct dump carries is
// assumed zero (static storage in retail), and the round-trip gate would
// expose any surprise. Nothing links; headers only.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <controlconfig/controlsconfig.hh>  // CCFG_MAX
#include <globalincs/pstypes.hh>            // ubyte/ushort/fix, fourcc
#include <globalincs/systemvars.hh>         // detail_levels
#include <graphics/2d.hh>                   // color (the gauge dump)
#include <hud/hudgauges.hh>                 // NUM_HUD_GAUGES
#include <missionui/missionscreencommon.hh> // MAX_WSS_SLOTS, MAX_WL_WEAPONS
#include <model/model.hh>                   // SUBSYSTEM_MAX
#include <stats/scoring.hh>                 // scoring_struct, NUM_MEDALS

// file-scope in redalert.cc:46, not exported by any header
#define MAX_RED_ALERT_SUBSYSTEMS 64

// managepilot.cc:32 / missioncampaign.cc:75..84 -- the ids and versions
#define PLR_MAGIC uint(fourcc("FSPF"))
#define PLR_VERSION 140
#define CSG_MAGIC uint(0xbeefcafe)
#define CSG_VERSION 12
#define CSG_SINGLE_SIG int(0xddddeeee)
#define CSS_MAGIC uint(0xabbadaad)
#define CSS_VERSION 1

static void
die(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "savejson: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

// ----------------------------------------------------------------------
// the JSON value -- one tree type for both directions; objects keep
// insertion order (the decode side IS the documentation of the format)

struct json_t {
    enum kind_t { object, array, string, number, boolean, null };

    kind_t kind = null;
    std::vector<std::pair<std::string, json_t>> fields;   // object
    std::vector<json_t> items;                            // array
    std::string str;
    double num = 0.0;
    bool flag = false;

    static json_t obj() { json_t j; j.kind = object; return j; }
    static json_t arr() { json_t j; j.kind = array; return j; }
    static json_t of(double v) { json_t j; j.kind = number; j.num = v; return j; }
    static json_t of(const std::string &s)
    {
        json_t j;
        j.kind = string;
        j.str = s;
        return j;
    }

    json_t &add(const char *key, json_t v)
    {
        fields.push_back({ key, std::move(v) });
        return fields.back().second;
    }
    void add(const char *key, double v) { add(key, of(v)); }
    void add(const char *key, const std::string &s) { add(key, of(s)); }

    const json_t &get(const char *key) const
    {
        for (const auto &f : fields)
            if (f.first == key)
                return f.second;
        die("missing field '%s'", key);
        abort();
    }

    long long i(const char *key) const { return (long long)get(key).num; }
    double f(const char *key) const { return get(key).num; }
    const std::string &s(const char *key) const { return get(key).str; }
    const std::vector<json_t> &a(const char *key) const
    {
        return get(key).items;
    }
};

// ----------------------------------------------------------------------
// serializer: scalar arrays inline (the 130/200-slot pools stay one line
// each), structured arrays and objects one entry per line

static bool
scalar(const json_t &j)
{
    return j.kind == json_t::number || j.kind == json_t::string ||
           j.kind == json_t::boolean || j.kind == json_t::null;
}

static void
print_string(const std::string &s, FILE *out)
{
    fputc('"', out);
    for (unsigned char c : s) {
        switch (c) {
        case '"': fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n", out); break;
        case '\r': fputs("\\r", out); break;
        case '\t': fputs("\\t", out); break;
        default:
            if (c < 0x20)
                fprintf(out, "\\u%04x", c);
            else
                fputc(c, out);
        }
    }
    fputc('"', out);
}

static void
print_number(double v, FILE *out)
{
    if (v == (long long)v)
        fprintf(out, "%lld", (long long)v);
    else
        fprintf(out, "%.9g", v);
}

static void
print_json(const json_t &j, FILE *out, int indent)
{
    switch (j.kind) {
    case json_t::string:
        print_string(j.str, out);
        break;
    case json_t::number:
        print_number(j.num, out);
        break;
    case json_t::boolean:
        fputs(j.flag ? "true" : "false", out);
        break;
    case json_t::null:
        fputs("null", out);
        break;

    case json_t::array: {
        bool inl = true;
        for (const json_t &item : j.items)
            if (!scalar(item))
                inl = false;

        if (j.items.empty()) {
            fputs("[]", out);
        }
        else if (inl) {
            fputc('[', out);
            for (size_t i = 0; i < j.items.size(); i++) {
                if (i)
                    fputs(", ", out);
                print_json(j.items[i], out, indent);
            }
            fputc(']', out);
        }
        else {
            fputs("[\n", out);
            for (size_t i = 0; i < j.items.size(); i++) {
                fprintf(out, "%*s", (indent + 1) * 4, "");
                print_json(j.items[i], out, indent + 1);
                fputs(i + 1 < j.items.size() ? ",\n" : "\n", out);
            }
            fprintf(out, "%*s]", indent * 4, "");
        }
        break;
    }

    case json_t::object:
        if (j.fields.empty()) {
            fputs("{}", out);
            break;
        }
        fputs("{\n", out);
        for (size_t i = 0; i < j.fields.size(); i++) {
            fprintf(out, "%*s", (indent + 1) * 4, "");
            print_string(j.fields[i].first, out);
            fputs(": ", out);
            print_json(j.fields[i].second, out, indent + 1);
            fputs(i + 1 < j.fields.size() ? ",\n" : "\n", out);
        }
        fprintf(out, "%*s}", indent * 4, "");
        break;
    }
}

// ----------------------------------------------------------------------
// parser: recursive descent over the whole text, positions in errors

struct json_parser_t {
    const char *p, *begin, *end;

    json_parser_t(const char *text, size_t n)
        : p(text), begin(text), end(text + n)
    {
    }

    void fail(const char *what)
    {
        die("JSON parse error at offset %zu: %s", size_t(p - begin), what);
    }

    void skip_ws()
    {
        while (p < end &&
               (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            p++;
    }

    bool eat(char c)
    {
        skip_ws();
        if (p < end && *p == c) {
            p++;
            return true;
        }
        return false;
    }

    void expect(char c)
    {
        if (!eat(c)) {
            char msg[64];
            snprintf(msg, sizeof(msg), "expected '%c'", c);
            fail(msg);
        }
    }

    std::string parse_string()
    {
        expect('"');
        std::string s;
        while (p < end && *p != '"') {
            char c = *p++;
            if (c == '\\') {
                if (p >= end)
                    fail("dangling escape");
                char e = *p++;
                switch (e) {
                case '"': s += '"'; break;
                case '\\': s += '\\'; break;
                case '/': s += '/'; break;
                case 'n': s += '\n'; break;
                case 'r': s += '\r'; break;
                case 't': s += '\t'; break;
                case 'b': s += '\b'; break;
                case 'f': s += '\f'; break;
                case 'u': {
                    if (end - p < 4)
                        fail("short \\u escape");
                    char hex[5] = { p[0], p[1], p[2], p[3], 0 };
                    long v = strtol(hex, NULL, 16);
                    if (v > 0xff)
                        fail("non-latin \\u escape (saves are byte strings)");
                    s += char(v);
                    p += 4;
                    break;
                }
                default:
                    fail("unknown escape");
                }
            }
            else {
                s += c;
            }
        }
        expect('"');
        return s;
    }

    json_t parse_value()
    {
        skip_ws();
        if (p >= end)
            fail("unexpected end of input");

        if (*p == '{') {
            p++;
            json_t j = json_t::obj();
            skip_ws();
            if (eat('}'))
                return j;
            for (;;) {
                std::string key = parse_string();
                expect(':');
                j.fields.push_back({ key, parse_value() });
                if (eat(','))
                    continue;
                expect('}');
                return j;
            }
        }
        if (*p == '[') {
            p++;
            json_t j = json_t::arr();
            skip_ws();
            if (eat(']'))
                return j;
            for (;;) {
                j.items.push_back(parse_value());
                if (eat(','))
                    continue;
                expect(']');
                return j;
            }
        }
        if (*p == '"') {
            json_t j;
            j.kind = json_t::string;
            j.str = parse_string();
            return j;
        }
        if (!strncmp(p, "true", 4) && end - p >= 4) {
            p += 4;
            json_t j;
            j.kind = json_t::boolean;
            j.flag = true;
            return j;
        }
        if (!strncmp(p, "false", 5) && end - p >= 5) {
            p += 5;
            json_t j;
            j.kind = json_t::boolean;
            return j;
        }
        if (!strncmp(p, "null", 4) && end - p >= 4) {
            p += 4;
            return json_t();
        }

        char *stop = NULL;
        double v = strtod(p, &stop);
        if (stop == p)
            fail("not a value");
        p = stop;
        return json_t::of(v);
    }
};

// ----------------------------------------------------------------------
// the byte stream, cfread/cfwrite's own primitives (native little-endian,
// INTEL_* are no-ops here); reads are bounds-checked with the offset

struct reader_t {
    const unsigned char *p, *begin, *end;

    reader_t(const unsigned char *data, size_t n)
        : p(data), begin(data), end(data + n)
    {
    }

    size_t off() const { return size_t(p - begin); }

    void need(size_t n)
    {
        if (size_t(end - p) < n)
            die("truncated file at offset %zu (need %zu more bytes)", off(),
                n);
    }

    void raw(void *dst, size_t n)
    {
        need(n);
        memcpy(dst, p, n);
        p += n;
    }

    ubyte u8() { ubyte v; raw(&v, 1); return v; }
    short i16() { short v; raw(&v, 2); return v; }
    ushort u16() { ushort v; raw(&v, 2); return v; }
    int i32() { int v; raw(&v, 4); return v; }
    long long i64() { long long v; raw(&v, 8); return v; }
    uint u32() { uint v; raw(&v, 4); return v; }
    float f32() { float v; raw(&v, 4); return v; }

    // cfwrite_string_len: int length, then the bytes, no terminator
    std::string string_len()
    {
        int len = i32();
        if (len < 0 || len > 4096)
            die("implausible string length %d at offset %zu", len, off() - 4);
        std::string s(len, '\0');
        if (len)
            raw(&s[0], len);
        return s;
    }

    // cfwrite_string: the bytes, then a NUL (empty = the NUL alone)
    std::string string_z()
    {
        std::string s;
        for (;;) {
            char c = (char)u8();
            if (!c)
                return s;
            s += c;
        }
    }
};

struct writer_t {
    std::vector<unsigned char> buf;

    void raw(const void *src, size_t n)
    {
        const unsigned char *b = (const unsigned char *)src;
        buf.insert(buf.end(), b, b + n);
    }

    void u8(ubyte v) { raw(&v, 1); }
    void i16(short v) { raw(&v, 2); }
    void u16(ushort v) { raw(&v, 2); }
    void i32(int v) { raw(&v, 4); }
    void i64(long long v) { raw(&v, 8); }
    void u32(uint v) { raw(&v, 4); }
    void f32(float v) { raw(&v, 4); }

    void string_len(const std::string &s)
    {
        i32((int)s.size());
        raw(s.data(), s.size());
    }

    void string_z(const std::string &s)
    {
        raw(s.data(), s.size());
        u8(0);
    }
};

// ----------------------------------------------------------------------
// the raw-struct blocks, field by field (padding zeroed on encode)

static json_t
color_json(const color &c)
{
    json_t j = json_t::obj();
    j.add("screen_sig", (double)c.screen_sig);
    j.add("red", c.red);
    j.add("green", c.green);
    j.add("blue", c.blue);
    j.add("alpha", c.alpha);
    j.add("ac_type", c.ac_type);
    j.add("is_alphacolor", c.is_alphacolor);
    j.add("raw8", c.raw8);
    j.add("alphacolor", c.alphacolor);
    j.add("magic", c.magic);
    return j;
}

static color
color_from(const json_t &j)
{
    color c;
    memset(&c, 0, sizeof(c));
    c.screen_sig = (uint)j.i("screen_sig");
    c.red = (ubyte)j.i("red");
    c.green = (ubyte)j.i("green");
    c.blue = (ubyte)j.i("blue");
    c.alpha = (ubyte)j.i("alpha");
    c.ac_type = (ubyte)j.i("ac_type");
    c.is_alphacolor = (int)j.i("is_alphacolor");
    c.raw8 = (ubyte)j.i("raw8");
    c.alphacolor = (int)j.i("alphacolor");
    c.magic = (int)j.i("magic");
    return c;
}

static json_t
detail_json(const detail_levels &d)
{
    json_t j = json_t::obj();
    j.add("setting", d.setting);
    j.add("nebula_detail", d.nebula_detail);
    j.add("detail_distance", d.detail_distance);
    j.add("hardware_textures", d.hardware_textures);
    j.add("num_small_debris", d.num_small_debris);
    j.add("num_particles", d.num_particles);
    j.add("num_stars", d.num_stars);
    j.add("shield_effects", d.shield_effects);
    j.add("lighting", d.lighting);
    j.add("targetview_model", d.targetview_model);
    j.add("planets_suns", d.planets_suns);
    j.add("weapon_extras", d.weapon_extras);
    return j;
}

static detail_levels
detail_from(const json_t &j)
{
    detail_levels d;
    memset(&d, 0, sizeof(d));
    d.setting = (int)j.i("setting");
    d.nebula_detail = (int)j.i("nebula_detail");
    d.detail_distance = (int)j.i("detail_distance");
    d.hardware_textures = (int)j.i("hardware_textures");
    d.num_small_debris = (int)j.i("num_small_debris");
    d.num_particles = (int)j.i("num_particles");
    d.num_stars = (int)j.i("num_stars");
    d.shield_effects = (int)j.i("shield_effects");
    d.lighting = (int)j.i("lighting");
    d.targetview_model = (int)j.i("targetview_model");
    d.planets_suns = (int)j.i("planets_suns");
    d.weapon_extras = (int)j.i("weapon_extras");
    return d;
}

static json_t
ushort_array_json(const ushort *v, int n)
{
    json_t a = json_t::arr();
    for (int i = 0; i < n; i++)
        a.items.push_back(json_t::of(v[i]));
    return a;
}

// the whole scoring_struct -- the .css dump carries every field, the
// mission-total half included
static json_t
scoring_json(const scoring_struct &s)
{
    json_t j = json_t::obj();
    j.add("flags", s.flags);
    j.add("score", s.score);
    j.add("rank", s.rank);

    json_t medals = json_t::arr();
    for (int i = 0; i < NUM_MEDALS; i++)
        medals.items.push_back(json_t::of(s.medals[i]));
    j.add("medals", std::move(medals));

    j.add("kills", ushort_array_json(s.kills, MAX_SHIP_TYPES));
    j.add("assists", s.assists);
    j.add("kill_count", s.kill_count);
    j.add("kill_count_ok", s.kill_count_ok);
    j.add("p_shots_fired", (double)s.p_shots_fired);
    j.add("s_shots_fired", (double)s.s_shots_fired);
    j.add("p_shots_hit", (double)s.p_shots_hit);
    j.add("s_shots_hit", (double)s.s_shots_hit);
    j.add("p_bonehead_hits", (double)s.p_bonehead_hits);
    j.add("s_bonehead_hits", (double)s.s_bonehead_hits);
    j.add("bonehead_kills", s.bonehead_kills);
    j.add("missions_flown", (double)s.missions_flown);
    j.add("flight_time", (double)s.flight_time);
    j.add("last_flown", (double)s.last_flown);
    j.add("last_backup", (double)s.last_backup);
    j.add("m_medal_earned", s.m_medal_earned);
    j.add("m_badge_earned", s.m_badge_earned);
    j.add("m_promotion_earned", s.m_promotion_earned);
    j.add("m_score", s.m_score);
    j.add("m_kills", ushort_array_json(s.m_kills, MAX_SHIP_TYPES));
    j.add("m_okKills", ushort_array_json(s.m_okKills, MAX_SHIP_TYPES));
    j.add("m_kill_count", s.m_kill_count);
    j.add("m_kill_count_ok", s.m_kill_count_ok);
    j.add("m_assists", s.m_assists);
    j.add("mp_shots_fired", (double)s.mp_shots_fired);
    j.add("ms_shots_fired", (double)s.ms_shots_fired);
    j.add("mp_shots_hit", (double)s.mp_shots_hit);
    j.add("ms_shots_hit", (double)s.ms_shots_hit);
    j.add("mp_bonehead_hits", (double)s.mp_bonehead_hits);
    j.add("ms_bonehead_hits", (double)s.ms_bonehead_hits);
    j.add("m_bonehead_kills", s.m_bonehead_kills);
    j.add("m_player_deaths", s.m_player_deaths);
    j.add("m_dogfight_kills", ushort_array_json(s.m_dogfight_kills,
                                                MAX_PLAYERS));
    return j;
}

static void
ushort_array_from(const json_t &a, ushort *v, int n, const char *what)
{
    if ((int)a.items.size() != n)
        die("%s: expected %d entries, got %zu", what, n, a.items.size());
    for (int i = 0; i < n; i++)
        v[i] = (ushort)a.items[i].num;
}

static scoring_struct
scoring_from(const json_t &j)
{
    scoring_struct s;
    memset(&s, 0, sizeof(s));
    s.flags = (int)j.i("flags");
    s.score = (int)j.i("score");
    s.rank = (int)j.i("rank");

    const auto &medals = j.a("medals");
    if ((int)medals.size() != NUM_MEDALS)
        die("medals: expected %d entries, got %zu", NUM_MEDALS, medals.size());
    for (int i = 0; i < NUM_MEDALS; i++)
        s.medals[i] = (int)medals[i].num;

    ushort_array_from(j.get("kills"), s.kills, MAX_SHIP_TYPES, "kills");
    s.assists = (int)j.i("assists");
    s.kill_count = (int)j.i("kill_count");
    s.kill_count_ok = (int)j.i("kill_count_ok");
    s.p_shots_fired = (uint)j.f("p_shots_fired");
    s.s_shots_fired = (uint)j.f("s_shots_fired");
    s.p_shots_hit = (uint)j.f("p_shots_hit");
    s.s_shots_hit = (uint)j.f("s_shots_hit");
    s.p_bonehead_hits = (uint)j.f("p_bonehead_hits");
    s.s_bonehead_hits = (uint)j.f("s_bonehead_hits");
    s.bonehead_kills = (int)j.i("bonehead_kills");
    s.missions_flown = (uint)j.f("missions_flown");
    s.flight_time = (uint)j.f("flight_time");
    s.last_flown = (time_t)j.f("last_flown");
    s.last_backup = (time_t)j.f("last_backup");
    s.m_medal_earned = (int)j.i("m_medal_earned");
    s.m_badge_earned = (int)j.i("m_badge_earned");
    s.m_promotion_earned = (int)j.i("m_promotion_earned");
    s.m_score = (int)j.i("m_score");
    ushort_array_from(j.get("m_kills"), s.m_kills, MAX_SHIP_TYPES, "m_kills");
    ushort_array_from(j.get("m_okKills"), s.m_okKills, MAX_SHIP_TYPES,
                      "m_okKills");
    s.m_kill_count = (int)j.i("m_kill_count");
    s.m_kill_count_ok = (int)j.i("m_kill_count_ok");
    s.m_assists = (int)j.i("m_assists");
    s.mp_shots_fired = (uint)j.f("mp_shots_fired");
    s.ms_shots_fired = (uint)j.f("ms_shots_fired");
    s.mp_shots_hit = (uint)j.f("mp_shots_hit");
    s.ms_shots_hit = (uint)j.f("ms_shots_hit");
    s.mp_bonehead_hits = (uint)j.f("mp_bonehead_hits");
    s.ms_bonehead_hits = (uint)j.f("ms_bonehead_hits");
    s.m_bonehead_kills = (int)j.i("m_bonehead_kills");
    s.m_player_deaths = (int)j.i("m_player_deaths");
    ushort_array_from(j.get("m_dogfight_kills"), s.m_dogfight_kills,
                      MAX_PLAYERS, "m_dogfight_kills");
    return s;
}

// ----------------------------------------------------------------------
// .plr -- write_pilot_file_core's stream (managepilot.cc:659), v140

// the variable-length stats block (write_stats_block, managepilot.cc:865):
// the kills array is truncated at its last nonzero element in the file --
// decoded as-is, so the round trip reproduces the truncation exactly
static json_t
stats_block_decode(reader_t &rd)
{
    json_t j = json_t::obj();
    j.add("score", rd.i32());
    j.add("rank", rd.i32());
    j.add("assists", rd.i32());

    json_t medals = json_t::arr();
    for (int i = 0; i < NUM_MEDALS; i++)
        medals.items.push_back(json_t::of(rd.i32()));
    j.add("medals", std::move(medals));

    int nkills = rd.i32();
    if (nkills < 0 || nkills > MAX_SHIP_TYPES)
        die("implausible kills count %d", nkills);
    json_t kills = json_t::arr();
    for (int i = 0; i < nkills; i++)
        kills.items.push_back(json_t::of(rd.u16()));
    j.add("kills", std::move(kills));

    j.add("kill_count", rd.i32());
    j.add("kill_count_ok", rd.i32());
    j.add("p_shots_fired", (double)rd.u32());
    j.add("s_shots_fired", (double)rd.u32());
    j.add("p_shots_hit", (double)rd.u32());
    j.add("s_shots_hit", (double)rd.u32());
    j.add("p_bonehead_hits", (double)rd.u32());
    j.add("s_bonehead_hits", (double)rd.u32());
    j.add("bonehead_kills", (int)rd.u32());
    return j;
}

static void
stats_block_encode(const json_t &j, writer_t &wr)
{
    wr.i32((int)j.i("score"));
    wr.i32((int)j.i("rank"));
    wr.i32((int)j.i("assists"));

    const auto &medals = j.a("medals");
    if ((int)medals.size() != NUM_MEDALS)
        die("stats.medals: expected %d entries, got %zu", NUM_MEDALS,
            medals.size());
    for (const json_t &m : medals)
        wr.i32((int)m.num);

    const auto &kills = j.a("kills");
    wr.i32((int)kills.size());
    for (const json_t &k : kills)
        wr.u16((ushort)k.num);

    wr.i32((int)j.i("kill_count"));
    wr.i32((int)j.i("kill_count_ok"));
    wr.u32((uint)j.f("p_shots_fired"));
    wr.u32((uint)j.f("s_shots_fired"));
    wr.u32((uint)j.f("p_shots_hit"));
    wr.u32((uint)j.f("s_shots_hit"));
    wr.u32((uint)j.f("p_bonehead_hits"));
    wr.u32((uint)j.f("s_bonehead_hits"));
    wr.u32((uint)j.i("bonehead_kills"));
}

// SAVEJSON_TRACE=1 prints each section's start offset -- the bisection
// tool when the stream and the codec disagree
static void
trace(const char *section, const reader_t &rd)
{
    static int on = -1;
    if (on < 0)
        on = getenv("SAVEJSON_TRACE") != NULL;
    if (on)
        fprintf(stderr, "savejson: %6zu %s\n", rd.off(), section);
}

static json_t
plr_decode(reader_t &rd)
{
    json_t j = json_t::obj();
    j.add("format", std::string("plr"));

    if (rd.u32() != PLR_MAGIC)
        die("not a .plr file (bad magic)");
    int version = (int)rd.u32();
    if (version != PLR_VERSION)
        die(".plr version %d (this codec speaks %d only)", version,
            PLR_VERSION);
    j.add("version", version);

    trace("header", rd);
    j.add("is_multi", rd.u8());
    j.add("rank", rd.i32());          // stats.rank's copy; reader discards it
    j.add("on_bastion", rd.u8());
    j.add("tips", rd.i32());

    j.add("image_filename", rd.string_len());
    j.add("squad_name", rd.string_len());
    j.add("squad_filename", rd.string_len());
    j.add("current_campaign", rd.string_len());
    j.add("last_ship_flown", rd.string_len());

    trace("controls", rd);
    int nctrl = rd.u8();              // CCFG_MAX when this build wrote it
    json_t controls = json_t::arr();
    for (int i = 0; i < nctrl; i++) {
        json_t c = json_t::obj();
        c.add("key", rd.i16());
        c.add("joy", rd.i16());
        controls.items.push_back(std::move(c));
    }
    j.add("controls", std::move(controls));

    trace("hud", rd);
    json_t hud = json_t::obj();
    hud.add("show_flags", rd.i32());
    hud.add("show_flags2", rd.i32());
    hud.add("popup_flags", rd.i32());
    hud.add("popup_flags2", rd.i32());
    hud.add("num_msg_window_lines", rd.u8());
    hud.add("rp_flags", rd.i32());
    hud.add("rp_dist", rd.i32());
    json_t colors = json_t::arr();
    for (int i = 0; i < NUM_HUD_GAUGES; i++) {
        color c;
        rd.raw(&c, sizeof(c));
        colors.items.push_back(color_json(c));
    }
    hud.add("gauge_colors", std::move(colors));
    j.add("hud", std::move(hud));

    trace("cutscenes+volumes", rd);
    j.add("cutscenes_viewable", rd.i32());
    j.add("sound_volume", rd.f32());
    j.add("music_volume", rd.f32());
    j.add("voice_volume", rd.f32());

    trace("detail", rd);
    detail_levels d;
    rd.raw(&d, sizeof(d));
    j.add("detail", detail_json(d));

    trace("recent", rd);
    int nrecent = rd.i32();
    if (nrecent < 0 || nrecent > 64)
        die("implausible recent-mission count %d", nrecent);
    json_t recent = json_t::arr();
    for (int i = 0; i < nrecent; i++)
        recent.items.push_back(json_t::of(rd.string_len()));
    j.add("recent_missions", std::move(recent));

    trace("stats", rd);
    j.add("stats", stats_block_decode(rd));
    j.add("skill_level", rd.i32());

    trace("axis", rd);
    json_t axes = json_t::arr();
    for (int i = 0; i < NUM_JOY_AXIS_ACTIONS; i++) {
        json_t a = json_t::obj();
        a.add("map", rd.i32());
        a.add("invert", rd.i32());
        axes.items.push_back(std::move(a));
    }
    j.add("axis", std::move(axes));

    j.add("save_flags", rd.i32());

    // the loadout (pilot_write_loadout, managepilot.cc:260)
    trace("loadout", rd);
    json_t loadout = json_t::obj();
    loadout.add("filename", rd.string_len());
    loadout.add("last_modified", rd.string_len());
    int nships = rd.i32();
    int nweapons = rd.i32();
    if (nships < 0 || nships > MAX_SHIP_TYPES || nweapons < 0 ||
        nweapons > MAX_WEAPON_TYPES)
        die("implausible loadout pool sizes %d/%d", nships, nweapons);
    json_t spool = json_t::arr(), wpool = json_t::arr();
    for (int i = 0; i < nships; i++)
        spool.items.push_back(json_t::of(rd.i32()));
    for (int i = 0; i < nweapons; i++)
        wpool.items.push_back(json_t::of(rd.i32()));
    loadout.add("ship_pool", std::move(spool));
    loadout.add("weapon_pool", std::move(wpool));
    json_t slots = json_t::arr();
    for (int i = 0; i < MAX_WSS_SLOTS; i++) {
        json_t slot = json_t::obj();
        slot.add("ship_class", rd.i32());
        json_t weps = json_t::arr();
        for (int k = 0; k < MAX_WL_WEAPONS; k++) {
            json_t w = json_t::obj();
            w.add("index", rd.i32());
            w.add("count", rd.i32());
            weps.items.push_back(std::move(w));
        }
        slot.add("weapons", std::move(weps));
        slots.items.push_back(std::move(slot));
    }
    loadout.add("slots", std::move(slots));
    j.add("loadout", std::move(loadout));

    // multiplayer options (write_multiplayer_options, managepilot.cc:901)
    trace("multi", rd);
    json_t multi = json_t::obj();
    multi.add("squad_set", rd.u8());
    multi.add("endgame_set", rd.u8());
    multi.add("flags", rd.i32());
    multi.add("respawn", (double)rd.u32());
    multi.add("max_observers", rd.u8());
    multi.add("skill_level", rd.u8());
    multi.add("voice_qos", rd.u8());
    multi.add("voice_token_wait", rd.i32());
    multi.add("voice_record_time", rd.i32());
    multi.add("mission_time_limit", (double)rd.i64());   // fix -- typedef
                                                 // long, 8 raw bytes in
                                                 // this build (pstypes:92)
    multi.add("kill_limit", rd.i32());
    multi.add("local_flags", rd.i32());
    multi.add("obj_update_level", rd.i32());
    j.add("multi", std::move(multi));

    trace("readyroom", rd);
    j.add("readyroom_listing_mode", rd.i32());
    j.add("briefing_voice_enabled", rd.i32());
    j.add("net_protocol", rd.i32());

    // red-alert wingman status (red_alert_write_wingman_status,
    // redalert.cc:701); the precursor string only exists when occupied
    trace("red_alert", rd);
    json_t red = json_t::obj();
    int nslots = rd.i32();
    if (nslots < 0 || nslots > 64)
        die("implausible red-alert slot count %d", nslots);
    if (nslots > 0)
        red.add("precursor_mission", rd.string_z());
    json_t wingmen = json_t::arr();
    for (int i = 0; i < nslots; i++) {
        json_t ras = json_t::obj();
        ras.add("name", rd.string_z());
        ras.add("hull", rd.f32());
        ras.add("ship_class", rd.i32());
        json_t sub = json_t::arr(), agg = json_t::arr();
        for (int k = 0; k < MAX_RED_ALERT_SUBSYSTEMS; k++)
            sub.items.push_back(json_t::of(rd.f32()));
        for (int k = 0; k < SUBSYSTEM_MAX; k++)
            agg.items.push_back(json_t::of(rd.f32()));
        ras.add("subsys_hits", std::move(sub));
        ras.add("subsys_aggregate", std::move(agg));
        json_t weps = json_t::arr();
        for (int k = 0; k < MAX_WL_WEAPONS; k++) {
            json_t w = json_t::obj();
            w.add("index", rd.i32());
            w.add("count", rd.i32());
            weps.items.push_back(std::move(w));
        }
        ras.add("weapons", std::move(weps));
        wingmen.items.push_back(std::move(ras));
    }
    red.add("wingmen", std::move(wingmen));
    j.add("red_alert", std::move(red));

    // techroom flags (pilot_write_techroom_data, managepilot.cc:167)
    trace("techroom", rd);
    json_t tech = json_t::obj();
    int tships = rd.i32();
    int tweapons = rd.i32();
    int tintel = rd.i32();
    if (tships < 0 || tships > MAX_SHIP_TYPES || tweapons < 0 ||
        tweapons > MAX_WEAPON_TYPES || tintel < 0 || tintel > 64)
        die("implausible techroom counts %d/%d/%d", tships, tweapons, tintel);
    json_t ts = json_t::arr(), tw = json_t::arr(), ti = json_t::arr();
    for (int i = 0; i < tships; i++)
        ts.items.push_back(json_t::of(rd.u8()));
    for (int i = 0; i < tweapons; i++)
        tw.items.push_back(json_t::of(rd.u8()));
    for (int i = 0; i < tintel; i++)
        ti.items.push_back(json_t::of(rd.u8()));
    tech.add("ships", std::move(ts));
    tech.add("weapons", std::move(tw));
    tech.add("intel", std::move(ti));
    j.add("techroom", std::move(tech));

    trace("tail", rd);
    j.add("auto_advance", rd.i32());
    j.add("use_mouse_to_fly", rd.i32());
    j.add("mouse_sensitivity", rd.i32());
    j.add("joy_sensitivity", rd.i32());
    j.add("dead_zone_size", rd.i32());

    return j;
}

static void
plr_encode(const json_t &j, writer_t &wr)
{
    if ((int)j.i("version") != PLR_VERSION)
        die("plr version must be %d", PLR_VERSION);

    wr.u32(PLR_MAGIC);
    wr.u32(PLR_VERSION);

    wr.u8((ubyte)j.i("is_multi"));
    wr.i32((int)j.i("rank"));
    wr.u8((ubyte)j.i("on_bastion"));
    wr.i32((int)j.i("tips"));

    wr.string_len(j.s("image_filename"));
    wr.string_len(j.s("squad_name"));
    wr.string_len(j.s("squad_filename"));
    wr.string_len(j.s("current_campaign"));
    wr.string_len(j.s("last_ship_flown"));

    const auto &controls = j.a("controls");
    if (controls.size() > 255)
        die("controls: more than 255 entries");
    wr.u8((ubyte)controls.size());
    for (const json_t &c : controls) {
        wr.i16((short)c.i("key"));
        wr.i16((short)c.i("joy"));
    }

    const json_t &hud = j.get("hud");
    wr.i32((int)hud.i("show_flags"));
    wr.i32((int)hud.i("show_flags2"));
    wr.i32((int)hud.i("popup_flags"));
    wr.i32((int)hud.i("popup_flags2"));
    wr.u8((ubyte)hud.i("num_msg_window_lines"));
    wr.i32((int)hud.i("rp_flags"));
    wr.i32((int)hud.i("rp_dist"));
    const auto &colors = hud.a("gauge_colors");
    if ((int)colors.size() != NUM_HUD_GAUGES)
        die("gauge_colors: expected %d entries, got %zu (retail reads "
            "exactly that many)",
            NUM_HUD_GAUGES, colors.size());
    for (const json_t &c : colors) {
        color cc = color_from(c);
        wr.raw(&cc, sizeof(cc));
    }

    wr.i32((int)j.i("cutscenes_viewable"));
    wr.f32((float)j.f("sound_volume"));
    wr.f32((float)j.f("music_volume"));
    wr.f32((float)j.f("voice_volume"));

    detail_levels d = detail_from(j.get("detail"));
    wr.raw(&d, sizeof(d));

    const auto &recent = j.a("recent_missions");
    wr.i32((int)recent.size());
    for (const json_t &m : recent)
        wr.string_len(m.str);

    stats_block_encode(j.get("stats"), wr);
    wr.i32((int)j.i("skill_level"));

    const auto &axes = j.a("axis");
    if ((int)axes.size() != NUM_JOY_AXIS_ACTIONS)
        die("axis: expected %d entries, got %zu", NUM_JOY_AXIS_ACTIONS,
            axes.size());
    for (const json_t &a : axes) {
        wr.i32((int)a.i("map"));
        wr.i32((int)a.i("invert"));
    }

    wr.i32((int)j.i("save_flags"));

    const json_t &loadout = j.get("loadout");
    wr.string_len(loadout.s("filename"));
    wr.string_len(loadout.s("last_modified"));
    const auto &spool = loadout.a("ship_pool");
    const auto &wpool = loadout.a("weapon_pool");
    wr.i32((int)spool.size());
    wr.i32((int)wpool.size());
    for (const json_t &v : spool)
        wr.i32((int)v.num);
    for (const json_t &v : wpool)
        wr.i32((int)v.num);
    const auto &slots = loadout.a("slots");
    if ((int)slots.size() != MAX_WSS_SLOTS)
        die("loadout.slots: expected %d entries, got %zu", MAX_WSS_SLOTS,
            slots.size());
    for (const json_t &slot : slots) {
        wr.i32((int)slot.i("ship_class"));
        const auto &weps = slot.a("weapons");
        if ((int)weps.size() != MAX_WL_WEAPONS)
            die("loadout slot weapons: expected %d entries, got %zu",
                MAX_WL_WEAPONS, weps.size());
        for (const json_t &w : weps) {
            wr.i32((int)w.i("index"));
            wr.i32((int)w.i("count"));
        }
    }

    const json_t &multi = j.get("multi");
    wr.u8((ubyte)multi.i("squad_set"));
    wr.u8((ubyte)multi.i("endgame_set"));
    wr.i32((int)multi.i("flags"));
    wr.u32((uint)multi.f("respawn"));
    wr.u8((ubyte)multi.i("max_observers"));
    wr.u8((ubyte)multi.i("skill_level"));
    wr.u8((ubyte)multi.i("voice_qos"));
    wr.i32((int)multi.i("voice_token_wait"));
    wr.i32((int)multi.i("voice_record_time"));
    wr.i64(multi.i("mission_time_limit"));    // fix: 8 raw bytes here
    wr.i32((int)multi.i("kill_limit"));
    wr.i32((int)multi.i("local_flags"));
    wr.i32((int)multi.i("obj_update_level"));

    wr.i32((int)j.i("readyroom_listing_mode"));
    wr.i32((int)j.i("briefing_voice_enabled"));
    wr.i32((int)j.i("net_protocol"));

    const json_t &red = j.get("red_alert");
    const auto &wingmen = red.a("wingmen");
    wr.i32((int)wingmen.size());
    if (!wingmen.empty()) {
        wr.string_z(red.s("precursor_mission"));
        for (const json_t &ras : wingmen) {
            wr.string_z(ras.s("name"));
            wr.f32((float)ras.f("hull"));
            wr.i32((int)ras.i("ship_class"));
            const auto &sub = ras.a("subsys_hits");
            const auto &agg = ras.a("subsys_aggregate");
            if ((int)sub.size() != MAX_RED_ALERT_SUBSYSTEMS ||
                (int)agg.size() != SUBSYSTEM_MAX)
                die("red-alert subsystem arrays: expected %d/%d entries, "
                    "got %zu/%zu",
                    MAX_RED_ALERT_SUBSYSTEMS, SUBSYSTEM_MAX, sub.size(),
                    agg.size());
            for (const json_t &v : sub)
                wr.f32((float)v.num);
            for (const json_t &v : agg)
                wr.f32((float)v.num);
            const auto &weps = ras.a("weapons");
            if ((int)weps.size() != MAX_WL_WEAPONS)
                die("red-alert weapons: expected %d entries, got %zu",
                    MAX_WL_WEAPONS, weps.size());
            for (const json_t &w : weps) {
                wr.i32((int)w.i("index"));
                wr.i32((int)w.i("count"));
            }
        }
    }

    const json_t &tech = j.get("techroom");
    const auto &ts = tech.a("ships");
    const auto &tw = tech.a("weapons");
    const auto &ti = tech.a("intel");
    wr.i32((int)ts.size());
    wr.i32((int)tw.size());
    wr.i32((int)ti.size());
    for (const json_t &v : ts)
        wr.u8((ubyte)v.num);
    for (const json_t &v : tw)
        wr.u8((ubyte)v.num);
    for (const json_t &v : ti)
        wr.u8((ubyte)v.num);

    wr.i32((int)j.i("auto_advance"));
    wr.i32((int)j.i("use_mouse_to_fly"));
    wr.i32((int)j.i("mouse_sensitivity"));
    wr.i32((int)j.i("joy_sensitivity"));
    wr.i32((int)j.i("dead_zone_size"));
}

// ----------------------------------------------------------------------
// .csg -- mission_campaign_savefile_save's stream (missioncampaign.cc:557)

static json_t
csg_decode(reader_t &rd)
{
    json_t j = json_t::obj();
    j.add("format", std::string("csg"));

    if (rd.u32() != CSG_MAGIC)
        die("not a .csg file (bad magic)");
    int version = rd.i32();
    if (version != CSG_VERSION)
        die(".csg version %d (this codec speaks %d only)", version,
            CSG_VERSION);
    j.add("version", version);

    int sig = rd.i32();
    if (sig != CSG_SINGLE_SIG)
        die("not a single-player .csg (sig 0x%x)", (uint)sig);

    j.add("campaign_filename", rd.string_len());
    j.add("prev_mission", rd.i32());
    j.add("next_mission", rd.i32());
    j.add("loop_reentry", rd.i32());
    j.add("loop_enabled", rd.i32());

    int nships = rd.i32();
    int nweapons = rd.i32();
    if (nships < 0 || nships > MAX_SHIP_TYPES || nweapons < 0 ||
        nweapons > MAX_WEAPON_TYPES)
        die("implausible allowed-list sizes %d/%d", nships, nweapons);
    json_t sa = json_t::arr(), wa = json_t::arr();
    for (int i = 0; i < nships; i++)
        sa.items.push_back(json_t::of((signed char)rd.u8()));
    for (int i = 0; i < nweapons; i++)
        wa.items.push_back(json_t::of((signed char)rd.u8()));
    j.add("ships_allowed", std::move(sa));
    j.add("weapons_allowed", std::move(wa));

    int ncompleted = rd.i32();
    if (ncompleted < 0 || ncompleted > 1024)
        die("implausible completed-mission count %d", ncompleted);
    json_t missions = json_t::arr();
    for (int i = 0; i < ncompleted; i++) {
        json_t m = json_t::obj();
        m.add("index", rd.i32());

        int ngoals = rd.i32();
        if (ngoals < 0 || ngoals > 256)
            die("implausible goal count %d", ngoals);
        json_t goals = json_t::arr();
        for (int k = 0; k < ngoals; k++) {
            json_t g = json_t::obj();
            g.add("name", rd.string_len());
            g.add("status", (signed char)rd.u8());
            goals.items.push_back(std::move(g));
        }
        m.add("goals", std::move(goals));

        int nevents = rd.i32();
        if (nevents < 0 || nevents > 1024)
            die("implausible event count %d", nevents);
        json_t events = json_t::arr();
        for (int k = 0; k < nevents; k++) {
            json_t e = json_t::obj();
            e.add("name", rd.string_len());
            e.add("status", (signed char)rd.u8());
            events.items.push_back(std::move(e));
        }
        m.add("events", std::move(events));

        m.add("flags", rd.i32());
        missions.items.push_back(std::move(m));
    }
    j.add("missions_completed", std::move(missions));

    return j;
}

static void
csg_encode(const json_t &j, writer_t &wr)
{
    if ((int)j.i("version") != CSG_VERSION)
        die("csg version must be %d", CSG_VERSION);

    wr.u32(CSG_MAGIC);
    wr.i32(CSG_VERSION);
    wr.i32(CSG_SINGLE_SIG);

    wr.string_len(j.s("campaign_filename"));
    wr.i32((int)j.i("prev_mission"));
    wr.i32((int)j.i("next_mission"));
    wr.i32((int)j.i("loop_reentry"));
    wr.i32((int)j.i("loop_enabled"));

    const auto &sa = j.a("ships_allowed");
    const auto &wa = j.a("weapons_allowed");
    wr.i32((int)sa.size());
    wr.i32((int)wa.size());
    for (const json_t &v : sa)
        wr.u8((ubyte)(signed char)v.num);
    for (const json_t &v : wa)
        wr.u8((ubyte)(signed char)v.num);

    const auto &missions = j.a("missions_completed");
    wr.i32((int)missions.size());
    for (const json_t &m : missions) {
        wr.i32((int)m.i("index"));

        const auto &goals = m.a("goals");
        wr.i32((int)goals.size());
        for (const json_t &g : goals) {
            wr.string_len(g.s("name"));
            wr.u8((ubyte)(signed char)g.i("status"));
        }

        const auto &events = m.a("events");
        wr.i32((int)events.size());
        for (const json_t &e : events) {
            wr.string_len(e.s("name"));
            wr.u8((ubyte)(signed char)e.i("status"));
        }

        wr.i32((int)m.i("flags"));
    }
}

// ----------------------------------------------------------------------
// .css -- the stats sidecar (missioncampaign.cc:629): per completed
// mission, the raw scoring_struct as THIS build lays it out

static json_t
css_decode(reader_t &rd)
{
    json_t j = json_t::obj();
    j.add("format", std::string("css"));

    if (rd.u32() != CSS_MAGIC)
        die("not a .css file (bad magic)");
    int version = rd.i32();
    if (version != CSS_VERSION)
        die(".css version %d (this codec speaks %d only)", version,
            CSS_VERSION);
    j.add("version", version);

    int count = rd.i32();
    if (count < 0 || count > 1024)
        die("implausible mission count %d", count);
    json_t missions = json_t::arr();
    for (int i = 0; i < count; i++) {
        json_t m = json_t::obj();
        m.add("index", rd.i32());
        scoring_struct s;
        rd.raw(&s, sizeof(s));
        m.add("stats", scoring_json(s));
        missions.items.push_back(std::move(m));
    }
    j.add("missions", std::move(missions));

    return j;
}

static void
css_encode(const json_t &j, writer_t &wr)
{
    if ((int)j.i("version") != CSS_VERSION)
        die("css version must be %d", CSS_VERSION);

    wr.u32(CSS_MAGIC);
    wr.i32(CSS_VERSION);

    const auto &missions = j.a("missions");
    wr.i32((int)missions.size());
    for (const json_t &m : missions) {
        wr.i32((int)m.i("index"));
        scoring_struct s = scoring_from(m.get("stats"));
        wr.raw(&s, sizeof(s));
    }
}

// ----------------------------------------------------------------------

static std::vector<unsigned char>
slurp(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        die("cannot open %s", path);
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<unsigned char> data(n);
    if (n && fread(data.data(), 1, n, fp) != size_t(n))
        die("short read on %s", path);
    fclose(fp);
    return data;
}

int
main(int argc, char *argv[])
{
    if (argc != 2 && argc != 3) {
        fprintf(stderr,
                "usage: savejson <save-file>             decode to JSON on stdout\n"
                "       savejson <in.json> <save-file>   encode JSON back to a save\n");
        return 2;
    }

    if (argc == 2) {
        // decode: format by magic
        std::vector<unsigned char> data = slurp(argv[1]);
        reader_t rd(data.data(), data.size());
        if (data.size() < 4)
            die("%s: too short to carry a magic", argv[1]);

        uint magic;
        memcpy(&magic, data.data(), 4);

        json_t j;
        if (magic == PLR_MAGIC)
            j = plr_decode(rd);
        else if (magic == CSG_MAGIC)
            j = csg_decode(rd);
        else if (magic == CSS_MAGIC)
            j = css_decode(rd);
        else
            die("%s: unrecognized magic 0x%08x", argv[1], magic);

        if (rd.p != rd.end)
            die("%s: %zu stray bytes after the decoded stream", argv[1],
                size_t(rd.end - rd.p));

        print_json(j, stdout, 0);
        fputc('\n', stdout);
        return 0;
    }

    // encode: format from the JSON's own "format" field
    std::vector<unsigned char> text = slurp(argv[1]);
    json_parser_t parser((const char *)text.data(), text.size());
    json_t j = parser.parse_value();
    parser.skip_ws();
    if (parser.p != parser.end)
        parser.fail("trailing content");

    writer_t wr;
    const std::string &format = j.s("format");
    if (format == "plr")
        plr_encode(j, wr);
    else if (format == "csg")
        csg_encode(j, wr);
    else if (format == "css")
        css_encode(j, wr);
    else
        die("unknown format '%s' (plr, csg, or css)", format.c_str());

    FILE *fp = fopen(argv[2], "wb");
    if (!fp)
        die("cannot create %s", argv[2]);
    if (wr.buf.size() &&
        fwrite(wr.buf.data(), 1, wr.buf.size(), fp) != wr.buf.size())
        die("short write on %s", argv[2]);
    fclose(fp);

    return 0;
}
