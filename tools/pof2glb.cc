// -*- mode: c++; -*-
//
// pof2glb: convert a retail POF model into GLB + Godot .tres + manifest
// (docs/godot-migration-plan.md, "The asset pipeline"). Reading goes through
// libpof, whose parse is oracle-pinned byte-identical to retail's loader, so
// the risk this tool carries is confined to the mapping and the writing,
// never the reading.
//
//   pof2glb <model.pof> [<out.glb>]
//   pof2glb --summary <model.pof>
//
// Emits geometry, hierarchy and textured materials into the GLB (retail PCX
// maps transcoded to PNG beside it, in textures/), the FS2-specific ship data
// (weapon/thruster/dock/path points, turrets, subsystems, shield) into a
// Godot .tres (inspect/ship_data.gd is the schema), and a <stem>.manifest.json
// recording sources, digests, converter version and warnings.

#include <model/file.hh>

#include "pof2glb_version.hh"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

namespace geom = pof::geometry;

// ---- the axis map ----------------------------------------------------
//
// Three frames are in play. The FILE is FS2's: left-handed, +X starboard,
// +Y up, +Z nose. LIBPOF MEMORY mirrors X on parse (PCS2 inheritance, see
// libpof file.hh), which makes it right-handed with +X port. GODOT/glTF is
// right-handed, +Y up, -Z forward.
//
// We map memory -> glTF as (x, y, z) -> (-x, y, -z): undo the X mirror and
// flip Z, so the nose ends up at Godot's -Z (forward) and file starboard
// stays +X (chase view matches the game's). From the memory frame this is a
// pure 180-degree rotation about Y -- no handedness change, so triangle
// winding survives untouched. Every point, normal and offset goes through
// here; nothing else may touch coordinates.

geom::vec_t
to_godot(const geom::vec_t &v)
{
    return { -v[0], v[1], -v[2] };
}

// ---- JSON building ---------------------------------------------------
//
// The glTF document is one generated JSON string; printf-style appends keep
// the mechanism visible. %.9g round-trips a float exactly.

void
jf(std::string &s, const char *fmt, ...)
{
    char buf[512];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    s += buf;
}

// escape for a JSON string literal: quotes, backslashes, control characters
std::string
jstr(const std::string &s)
{
    std::string out;

    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c < 0x20) {
                char buf[8];
                snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            }
            else
                out += c;
        }
    }

    return out;
}

// drop a trailing comma so lists can be emitted append-only
void
close_list(std::string &s, char bracket)
{
    if (!s.empty() && s.back() == ',')
        s.pop_back();
    s += bracket;
}

// ---- manifest --------------------------------------------------------
//
// Every conversion writes a <stem>.manifest.json beside the GLB -- the
// pipeline's promise that generated assets are reproducibly derived from the
// user's own retail data (docs/godot-migration-plan.md, "The asset
// pipeline"). It records each source file with its SHA-256, the converter
// version (git describe, injected at build time by meson vcs_tag), every
// warning the run produced, and each output with its digest -- output paths
// relative to the manifest, so a converted tree relocates wholesale. No
// timestamp on purpose: the same inputs through the same converter yield a
// byte-identical manifest, so regeneration is diffable.
//
// SHA-256 is hand-rolled (FIPS 180-4), the PCX bargain again: a frozen
// algorithm is not worth a dependency, and the manifest-check gate recomputes
// every digest with python's hashlib -- an independent implementation -- so a
// mistake here cannot survive.

constexpr std::uint32_t sha_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

std::uint32_t
rotr(std::uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

void
sha_block(std::uint32_t h[8], const std::uint8_t *p)
{
    std::uint32_t w[64];

    for (int i = 0; i < 16; ++i)
        w[i] = std::uint32_t(p[4 * i]) << 24 |
            std::uint32_t(p[4 * i + 1]) << 16 |
            std::uint32_t(p[4 * i + 2]) << 8 | p[4 * i + 3];

    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 =
            rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 =
            rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

    for (int i = 0; i < 64; ++i) {
        const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = hh + s1 + ch + sha_k[i] + w[i];
        const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);

        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + s0 + maj;
    }

    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

// hex SHA-256 of a whole buffer (every file here is small enough to slurp)
std::string
sha256_hex(const std::uint8_t *p, std::size_t n)
{
    std::uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                           0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

    std::size_t off = 0;
    for (; off + 64 <= n; off += 64)
        sha_block(h, p + off);

    // pad: 0x80, zeros, the message bit length big-endian in the last 8
    std::uint8_t tail[128] = { };
    const std::size_t rest = n - off;
    std::memcpy(tail, p + off, rest);
    tail[rest] = 0x80;
    const std::size_t blocks = rest + 9 <= 64 ? 1 : 2;
    const std::uint64_t bits = std::uint64_t(n) * 8;
    for (int i = 0; i < 8; ++i)
        tail[blocks * 64 - 1 - i] = std::uint8_t(bits >> (8 * i));
    for (std::size_t i = 0; i < blocks; ++i)
        sha_block(h, tail + 64 * i);

    char hex[65];
    for (int i = 0; i < 8; ++i)
        snprintf(hex + 8 * i, 9, "%08x", h[i]);
    return hex;
}

// hex digest of a file's bytes; empty on read failure
std::string
sha256_file(const std::string &path)
{
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return "";

    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    std::vector< std::uint8_t > buf(sz > 0 ? sz : 0);
    const bool ok = sz >= 0 &&
        (buf.empty() ||
         std::fread(buf.data(), 1, buf.size(), f) == buf.size());
    std::fclose(f);

    return ok ? sha256_hex(buf.data(), buf.size()) : "";
}

// The manifest accumulates as the conversion runs: sources as they are read,
// outputs as they land, warnings as they happen.
struct manifest_t
{
    std::string sources;    // JSON objects: {"path":...,"sha256":...},
    std::string outputs;    // ditto, paths relative to the manifest
    std::string warnings;   // JSON strings
    int n_warnings = 0;
};

// report a non-fatal problem: stderr for the operator, the manifest for the
// record (the pipeline must be able to tell a clean conversion from a lossy
// one after the fact)
void
warn(manifest_t &m, const char *fmt, ...)
{
    char buf[512];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    std::fprintf(stderr, "pof2glb: %s\n", buf);
    jf(m.warnings, "\"%s\",", jstr(buf).c_str());
    ++m.n_warnings;
}

// record an input file, digested as it is on disk right now
void
add_source(manifest_t &m, const std::string &path)
{
    jf(m.sources, "{\"path\":\"%s\",\"sha256\":\"%s\"},", jstr(path).c_str(),
       sha256_file(path).c_str());
}

// record an output file: `rel` names it relative to the manifest (dir + rel
// is where it actually landed)
void
add_output(manifest_t &m, const std::string &dir, const std::string &rel)
{
    jf(m.outputs, "{\"path\":\"%s\",\"sha256\":\"%s\"},", jstr(rel).c_str(),
       sha256_file(dir + "/" + rel).c_str());
}

bool
write_manifest(manifest_t &m, const std::string &path)
{
    std::string s = "{\"tool\":\"pof2glb\",\"version\":\"" POF2GLB_VERSION
                    "\",\"sources\":[";
    s += m.sources;
    close_list(s, ']');
    s += ",\"outputs\":[";
    s += m.outputs;
    close_list(s, ']');
    s += ",\"warnings\":[";
    s += m.warnings;
    close_list(s, ']');
    s += "}\n";

    std::FILE *out = std::fopen(path.c_str(), "wb");
    if (!out) {
        std::fprintf(stderr, "pof2glb: cannot write %s\n", path.c_str());
        return false;
    }
    const bool ok = std::fwrite(s.data(), 1, s.size(), out) == s.size();
    return std::fclose(out) == 0 && ok;
}

// ---- binary buffer ---------------------------------------------------

struct bin_t
{
    std::vector< std::uint8_t > data;

    // append raw bytes, return the view's byte offset
    std::size_t append(const void *p, std::size_t n)
    {
        std::size_t off = data.size();
        data.insert(data.end(), static_cast< const std::uint8_t * >(p),
                    static_cast< const std::uint8_t * >(p) + n);
        return off;
    }
};

// ---- glTF document ---------------------------------------------------
//
// Append-only arrays of the document's JSON objects; indices into them are
// glTF's own cross-references. The working representation is the JSON text
// itself -- there is nothing here worth an object model.

struct gltf_t
{
    std::string nodes;      // one JSON object per POF subobject, same index
    std::string meshes;
    std::string materials;
    std::string accessors;
    std::string views;
    std::string images;     // one per transcoded PNG
    std::string gtextures;  // glTF texture: sampler + image source, 1:1 images

    bin_t bin;

    int n_meshes = 0;
    int n_accessors = 0;
    int n_views = 0;
    int n_images = 0;

    long n_triangles = 0;

    // texture_id -> material index (flat-colored polys keyed by -1-rgb below)
    std::map< int, int > material_of;

    // POF texture slot -> glTF image/texture index, -1 for keyword or missing
    // maps (filled by emit_textures before any material is built)
    std::vector< int > image_of;

    int view(std::size_t off, std::size_t len, int target)
    {
        jf(views, "{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu", off,
           len);
        if (target)
            jf(views, ",\"target\":%d", target);
        views += "},";
        return n_views++;
    }
};

// one draw call: the corners of every same-material triangle, deduplicated
struct prim_t
{
    std::vector< pof::model::vertex_t > verts;
    std::unordered_map< pof::model::vertex_t, std::uint32_t > index_of;
    std::vector< std::uint32_t > indices;

    std::uint32_t corner(const pof::model::vertex_t &v)
    {
        auto [it, fresh] = index_of.try_emplace(v, verts.size());
        if (fresh)
            verts.push_back(v);
        return it->second;
    }
};

constexpr int GL_ARRAY_BUFFER = 34962;
constexpr int GL_ELEMENT_ARRAY_BUFFER = 34963;
constexpr int GL_FLOAT = 5126;
constexpr int GL_UNSIGNED_INT = 5125;

// material for a texture slot: named after the texture and pointing at the
// transcoded PNG (emit_textures resolved the slot to g.image_of[key], or -1
// when the map is a keyword/missing, in which case the material stays a
// name-only stub). Flat-colored polys get a factor-only material keyed by
// -(0x1000000 | rgb) so distinct colors stay distinct.
int
material(gltf_t &g, const std::vector< std::string > &textures, int key,
         const pof::model::poly_t &poly)
{
    auto [it, fresh] = g.material_of.try_emplace(key, g.material_of.size());
    if (!fresh)
        return it->second;

    if (key >= 0) {
        const int img = key < int(g.image_of.size()) ? g.image_of[key] : -1;
        if (img >= 0)
            jf(g.materials,
               "{\"name\":\"%s\",\"doubleSided\":false,\"pbrMetallicRoughness\""
               ":{\"baseColorTexture\":{\"index\":%d}}},",
               jstr(textures[key]).c_str(), img);
        else
            jf(g.materials, "{\"name\":\"%s\",\"doubleSided\":false},",
               jstr(textures[key]).c_str());
    }
    else
        jf(g.materials,
           "{\"name\":\"flat-%02x%02x%02x\",\"pbrMetallicRoughness\":"
           "{\"baseColorFactor\":[%.9g,%.9g,%.9g,1]}},",
           poly.red, poly.green, poly.blue, poly.red / 255.0,
           poly.green / 255.0, poly.blue / 255.0);

    return it->second;
}

// emit one primitive: interleave nothing, one tightly packed view per
// attribute, indices as uint32
void
primitive(gltf_t &g, std::string &prims, const prim_t &p, int material_ix)
{
    // positions, mapped, with the min/max glTF requires of POSITION
    std::vector< float > pos, norm, uv;
    float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };

    for (const auto &v : p.verts) {
        const geom::vec_t gp = to_godot(v.point);
        const geom::vec_t gn = to_godot(v.norm);

        for (int i = 0; i < 3; ++i) {
            pos.push_back(gp[i]);
            norm.push_back(gn[i]);
            lo[i] = std::min(lo[i], gp[i]);
            hi[i] = std::max(hi[i], gp[i]);
        }

        uv.push_back(v.u);
        uv.push_back(v.v);
    }

    const std::size_t nv = p.verts.size();

    int pos_view = g.view(g.bin.append(pos.data(), pos.size() * 4),
                          pos.size() * 4, GL_ARRAY_BUFFER);
    int norm_view = g.view(g.bin.append(norm.data(), norm.size() * 4),
                           norm.size() * 4, GL_ARRAY_BUFFER);
    int uv_view = g.view(g.bin.append(uv.data(), uv.size() * 4),
                         uv.size() * 4, GL_ARRAY_BUFFER);
    int ix_view =
        g.view(g.bin.append(p.indices.data(), p.indices.size() * 4),
               p.indices.size() * 4, GL_ELEMENT_ARRAY_BUFFER);

    int pos_acc = g.n_accessors++;
    jf(g.accessors,
       "{\"bufferView\":%d,\"componentType\":%d,\"count\":%zu,"
       "\"type\":\"VEC3\",\"min\":[%.9g,%.9g,%.9g],\"max\":[%.9g,%.9g,%.9g]},",
       pos_view, GL_FLOAT, nv, lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);

    int norm_acc = g.n_accessors++;
    jf(g.accessors,
       "{\"bufferView\":%d,\"componentType\":%d,\"count\":%zu,"
       "\"type\":\"VEC3\"},",
       norm_view, GL_FLOAT, nv);

    int uv_acc = g.n_accessors++;
    jf(g.accessors,
       "{\"bufferView\":%d,\"componentType\":%d,\"count\":%zu,"
       "\"type\":\"VEC2\"},",
       uv_view, GL_FLOAT, nv);

    int ix_acc = g.n_accessors++;
    jf(g.accessors,
       "{\"bufferView\":%d,\"componentType\":%d,\"count\":%zu,"
       "\"type\":\"SCALAR\"},",
       ix_view, GL_UNSIGNED_INT, p.indices.size());

    jf(prims,
       "{\"attributes\":{\"POSITION\":%d,\"NORMAL\":%d,\"TEXCOORD_0\":%d},"
       "\"indices\":%d,\"material\":%d},",
       pos_acc, norm_acc, uv_acc, ix_acc, material_ix);

    g.n_triangles += p.indices.size() / 3;
}

// fan-triangulate every polygon of one subobject into per-material
// primitives; returns the mesh index, or -1 for a geometry-less subobject
int
mesh(gltf_t &g, const pof::model::sobj_t &sobj,
     const std::vector< std::string > &textures)
{
    if (sobj.polygons.empty())
        return -1;

    // group by material key first, so each texture becomes one draw call
    std::map< int, prim_t > prims;
    const pof::model::poly_t *flat_sample = nullptr;

    for (const auto &poly : sobj.polygons) {
        int key = poly.texture_id;
        if (key < 0) {
            key = -(0x1000000 | (poly.red << 16) | (poly.green << 8) |
                    poly.blue);
            flat_sample = &poly;
        }

        prim_t &p = prims[key];

        // The map to Godot is a pure rotation of libpof's memory frame, so
        // it cannot flip winding -- but the stored corner order is already
        // clockwise against the stored normals there (measured over the
        // whole of fighter01 and science01: unanimous). glTF fronts are
        // counter-clockwise, so each fan is emitted mirrored.
        for (std::size_t i = 2; i < poly.verts.size(); ++i) {
            p.indices.push_back(p.corner(poly.verts[0]));
            p.indices.push_back(p.corner(poly.verts[i]));
            p.indices.push_back(p.corner(poly.verts[i - 1]));
        }
    }

    std::string body;
    for (auto &[key, p] : prims) {
        const pof::model::poly_t &sample =
            key >= 0 ? sobj.polygons.front() : *flat_sample;
        primitive(g, body, p, material(g, textures, key, sample));
    }
    close_list(body, ']');

    jf(g.meshes, "{\"name\":\"%s\",\"primitives\":[%s},",
       jstr(sobj.name).c_str(), body.c_str());

    return g.n_meshes++;
}

// one glTF node per subobject, same index; POF offsets are parent-relative
// already, so they map directly to node translations. The POF facts a viewer
// wants but glTF has no slot for ride in "extras", carried verbatim in
// retail's encoding (docs/pof-corpus-survey.txt):
//
//   movement_type  -1 none; 1 rotates at runtime (dishes, panels); 2 turret,
//                  which moves through the subsystem/TGUN path and NOT this
//                  field; 0 exists in the corpus but is runtime-inert.
//   movement_axis  -1 none, 0 X, 1 Z, 2 Y -- Y and Z are SWAPPED relative
//                  to the naive reading (model.hh:37). 2 is turret heading;
//                  decode it wrong and every turret pitches instead of yaws.
//
// These are POF-frame axes; whoever animates from extras must send the axis
// through the same map as the geometry (to_godot above).
void
node(gltf_t &g, const pof::model::sobj_t &sobj, int mesh_ix,
     const std::string &children)
{
    const geom::vec_t t = to_godot(sobj.offset);

    jf(g.nodes, "{\"name\":\"%s\",\"translation\":[%.9g,%.9g,%.9g]",
       jstr(sobj.name).c_str(), t[0], t[1], t[2]);

    if (mesh_ix >= 0)
        jf(g.nodes, ",\"mesh\":%d", mesh_ix);

    if (!children.empty()) {
        g.nodes += ",\"children\":[";
        g.nodes += children;
        close_list(g.nodes, ']');
    }

    jf(g.nodes, ",\"extras\":{\"movement_type\":%d,\"movement_axis\":%d",
       sobj.movement_type, sobj.movement_axis);
    if (!sobj.properties.empty())
        jf(g.nodes, ",\"properties\":\"%s\"", jstr(sobj.properties).c_str());
    g.nodes += "}},";
}

// ---- textures: PCX -> PNG --------------------------------------------
//
// Retail texture maps are 256-colour RLE PCX under data/maps (cfile.cc:45),
// referenced from the POF TXTR chunk by bare, extension-less basename. Godot
// cannot read PCX, so each is transcoded to a PNG in a textures/ dir beside
// the GLB, and the material points at it. PNG and not TGA because the target
// is RUNTIME loading (the inspection scene, later the game): Godot's editor
// import pipeline reads TGA, but GLTFDocument at runtime parses external
// images by mimetype and accepts only PNG/JPEG (gltf_document.cpp:2186) --
// measured, a .tga uri loads under import and errors at runtime.
//
// The decode is hand-rolled: pof2glb stays libpof + stb and does NOT link the
// port's foundation/cfile. PCX is a frozen format, so the only risk that buys
// is a decode bug -- pinned shut by tests/pcx_dump, which reads the same maps
// through retail's authoritative pcx_read_bitmap_8bpp (pcxutils.cc:87) and the
// tex-check gate compares the two pixel-for-pixel. The one FS2 semantic, the
// green colour-key, is retail's and is replicated in the expand loop below.

std::string
lower(std::string s)
{
    for (char &c : s)
        c = char(std::tolower((unsigned char) c));
    return s;
}

std::uint16_t
le16(const std::uint8_t *p)
{
    return std::uint16_t(p[0] | (p[1] << 8));
}

// Decode a 256-colour RLE PCX into top-down RGBA. Mirrors retail's
// pcx_read_bitmap_8bpp row/col/count structure (pcxutils.cc:141-169) so the
// index stream is read identically -- padding columns beyond the image width
// are still consumed, and RLE runs carry across row boundaries -- then expands
// the indices through the tail palette. Returns false on a malformed header.
bool
decode_pcx(const std::string &path, int &w, int &h,
           std::vector< std::uint8_t > &rgba)
{
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;

    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz < 128 + 768) {
        std::fclose(f);
        return false;
    }

    std::vector< std::uint8_t > buf(sz);
    const bool read = std::fread(buf.data(), 1, sz, f) == std::size_t(sz);
    std::fclose(f);
    if (!read)
        return false;

    const std::uint8_t *hd = buf.data();
    // 256-colour RLE PCX only: Manufacturer 10, Encoding 1, 8bpp, 1 plane
    // (the same gate retail applies, pcxutils.cc:115-117).
    if (hd[0] != 10 || hd[2] != 1 || hd[3] != 8 || hd[65] != 1)
        return false;

    w = le16(hd + 8) - le16(hd + 4) + 1;   // Xmax - Xmin + 1
    h = le16(hd + 10) - le16(hd + 6) + 1;  // Ymax - Ymin + 1
    const int bpl = le16(hd + 66);         // BytesPerLine
    if (w <= 0 || h <= 0 || bpl < w)
        return false;

    // Palette: the last 768 bytes, retail's seek of -768 from EOF
    // (pcxutils.cc:129 -- it trusts the layout, not the 0x0C marker byte).
    const std::uint8_t *pal = buf.data() + sz - 768;

    std::vector< std::uint8_t > idx(std::size_t(w) * h);
    const std::uint8_t *in = buf.data() + 128;
    const std::uint8_t *end = buf.data() + sz;
    int count = 0;
    std::uint8_t data = 0;

    for (int row = 0; row < h; ++row) {
        std::uint8_t *out = idx.data() + std::size_t(row) * w;
        for (int col = 0; col < bpl; ++col) {
            if (count == 0) {
                if (in >= end)
                    return false;
                data = *in++;
                if ((data & 0xC0) == 0xC0) {
                    count = data & 0x3F;
                    if (in >= end)
                        return false;
                    data = *in++;
                }
                else
                    count = 1;
            }
            if (col < w)
                *out++ = data;
            --count;
        }
    }

    // Expand to RGBA with retail's green colour-key: a palette entry of
    // exactly (0,255,0) is transparent (alpha 0), everything else opaque
    // (pcxutils.cc:266-274). No magic index -- it is purely the palette RGB.
    rgba.resize(std::size_t(w) * h * 4);
    for (std::size_t i = 0; i < std::size_t(w) * h; ++i) {
        const std::uint8_t *c = pal + std::size_t(idx[i]) * 3;
        const bool key = c[0] == 0 && c[1] == 255 && c[2] == 0;
        rgba[i * 4 + 0] = c[0];
        rgba[i * 4 + 1] = c[1];
        rgba[i * 4 + 2] = c[2];
        rgba[i * 4 + 3] = key ? 0 : 255;
    }
    return true;
}

// Texture names containing "thruster" or "invisible" are engine keywords, not
// files -- retail never loads them (modelread.cc:270): thrusters get the
// animated glow, invisible gets no texture. The match is case-sensitive, as
// retail's strstr is: a capitalised name is not a keyword there, and shipped
// behavior wins (docs/godot-migration-plan.md).
bool
is_keyword_texture(const std::string &name)
{
    return name.find("thruster") != std::string::npos ||
        name.find("invisible") != std::string::npos;
}

// Lowercased-name -> on-disk-name index of a directory, so a POF's mixed-case
// TXTR basename resolves to the actual file the way retail's case-insensitive
// cfile does on a case-sensitive filesystem. Empty if the dir won't open.
std::map< std::string, std::string >
index_dir(const std::string &dir)
{
    std::map< std::string, std::string > m;
    if (DIR *d = opendir(dir.c_str())) {
        while (dirent *e = readdir(d))
            m.emplace(lower(e->d_name), e->d_name);
        closedir(d);
    }
    return m;
}

// "a/b/c.pof" -> "a/b"; no slash -> "."
std::string
dir_of(const std::string &p)
{
    const std::size_t s = p.find_last_of('/');
    return s == std::string::npos ? "." : p.substr(0, s);
}

// "a/b/c.pof" -> "c.pof"
std::string
base_of(const std::string &p)
{
    const std::size_t s = p.find_last_of('/');
    return s == std::string::npos ? p : p.substr(s + 1);
}

// Transcode every referenced map to <out_dir>/textures/<name>.png and record,
// per POF texture slot, the glTF image it resolved to (or -1 for a keyword or
// a map missing from disk -- those slots keep a name-only material). Duplicate
// names collapse to one image. maps_dir is the model's sibling data/maps.
// Each map consumed goes into the manifest's sources, each PNG written into
// its outputs; a map that fails to resolve or decode becomes a warning there.
void
emit_textures(gltf_t &g, manifest_t &m,
              const std::vector< std::string > &textures,
              const std::string &maps_dir, const std::string &out_dir)
{
    g.image_of.assign(textures.size(), -1);

    const auto maps = index_dir(maps_dir);
    std::map< std::string, int > tex_ix;   // lowercased basename -> image index
    bool made_dir = false;

    for (std::size_t slot = 0; slot < textures.size(); ++slot) {
        const std::string &nm = textures[slot];
        if (is_keyword_texture(nm))
            continue;

        const std::string key = lower(nm);
        if (auto it = tex_ix.find(key); it != tex_ix.end()) {
            g.image_of[slot] = it->second;   // duplicate slot, same image
            continue;
        }

        const auto hit = maps.find(key + ".pcx");
        if (hit == maps.end()) {
            warn(m, "texture '%s' not found in %s", nm.c_str(),
                 maps_dir.c_str());
            continue;
        }

        const std::string pcx_path = maps_dir + "/" + hit->second;
        int w, h;
        std::vector< std::uint8_t > rgba;
        if (!decode_pcx(pcx_path, w, h, rgba)) {
            warn(m, "cannot decode %s", hit->second.c_str());
            continue;
        }
        add_source(m, pcx_path);

        if (!made_dir) {
            mkdir((out_dir + "/textures").c_str(), 0755);   // EEXIST is fine
            made_dir = true;
        }

        const std::string png = key + ".png";
        if (!stbi_write_png((out_dir + "/textures/" + png).c_str(), w, h, 4,
                            rgba.data(), w * 4)) {
            warn(m, "cannot write %s", png.c_str());
            continue;
        }
        add_output(m, out_dir, "textures/" + png);

        const int ix = g.n_images++;
        jf(g.images, "{\"uri\":\"textures/%s\"},", jstr(png).c_str());
        jf(g.gtextures, "{\"sampler\":0,\"source\":%d},", ix);
        tex_ix[key] = ix;
        g.image_of[slot] = ix;
        std::printf("  %s -> textures/%s (%dx%d)\n", nm.c_str(), png.c_str(), w,
                    h);
    }
}

// ---- Godot .tres ship data -------------------------------------------
//
// The FS2-specific half of the model -- weapon muzzles, turrets, thrusters,
// docks, eyes, paths, subsystems, shield -- that glTF has no slot for, written
// as a Godot text resource beside the GLB. inspect/ship_data.gd is its schema;
// the pipeline shape is docs/godot-migration-plan.md ("POF is not merely a
// mesh"). Every coordinate goes through to_godot(), the same axis map as the
// geometry (docs/pof-corpus-survey.txt) -- nothing here re-derives coordinates.

// one Godot Vector3 literal, mapped to Godot's frame
void
gv3(std::string &s, const geom::vec_t &v)
{
    const geom::vec_t g = to_godot(v);
    jf(s, "Vector3(%.9g, %.9g, %.9g)", g[0], g[1], g[2]);
}

// PackedVector3Array literal from a run of points, each mapped
void
packed_points(std::string &s, const geom::vec_array_t &pts)
{
    s += "PackedVector3Array(";
    for (std::size_t i = 0; i < pts.size(); ++i) {
        const geom::vec_t g = to_godot(pts[i]);
        jf(s, "%s%.9g, %.9g, %.9g", i ? ", " : "", g[0], g[1], g[2]);
    }
    s += ")";
}

void
packed_int(std::string &s, const std::vector< int > &xs)
{
    s += "PackedInt32Array(";
    for (std::size_t i = 0; i < xs.size(); ++i)
        jf(s, "%s%d", i ? ", " : "", xs[i]);
    s += ")";
}

void
packed_float(std::string &s, const std::vector< float > &xs)
{
    s += "PackedFloat32Array(";
    for (std::size_t i = 0; i < xs.size(); ++i)
        jf(s, "%s%.9g", i ? ", " : "", xs[i]);
    s += ")";
}

// a hardpoint array (muzzle/dock slots) as the paired "points"/"normals" keys
void
hardpoints(std::string &s, const pof::model::hardpoint_array_t &hp)
{
    geom::vec_array_t pts, norms;
    for (const auto &h : hp) {
        pts.push_back(h.point);
        norms.push_back(h.norm);
    }
    s += "\"points\": ";
    packed_points(s, pts);
    s += ", \"normals\": ";
    packed_points(s, norms);
}

bool
iequals(const std::string &a, const std::string &b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower((unsigned char) a[i]) !=
            std::tolower((unsigned char) b[i]))
            return false;
    return true;
}

// A property string's value for `key`, up to end of line. Retail's own reader
// (get_user_prop_value) is fussier; this is only the human dock label, which
// nothing keys on, so the simple form is deliberate.
std::string
prop_value(const std::string &props, const std::string &key)
{
    std::size_t p = props.find(key);
    if (p == std::string::npos)
        return "";
    p += key.size();
    while (p < props.size() && (props[p] == '=' || props[p] == ' '))
        ++p;
    const std::size_t e = props.find_first_of("\r\n", p);
    return props.substr(p, e == std::string::npos ? std::string::npos : e - p);
}

bool
write_tres(pof::model::model_t &model, const std::string &name,
           const std::string &path)
{
    const auto &hdr = model.get_header();
    const auto &subs = model.get_subobjects();

    std::string s;
    s += "[gd_resource type=\"Resource\" script_class=\"ShipData\" "
         "load_steps=2 format=3]\n\n";
    s += "[ext_resource type=\"Script\" path=\"res://ship_data.gd\" "
         "id=\"1_shipdata\"]\n\n";
    s += "[resource]\n";
    s += "script = ExtResource(\"1_shipdata\")\n";

    jf(s, "source_pof = \"%s\"\n", jstr(name).c_str());
    jf(s, "pof_version = %d\n", model.GetVersion());
    jf(s, "radius = %.9g\n", hdr.max_radius);
    jf(s, "mass = %.9g\n", hdr.mass);
    s += "mass_center = ";
    gv3(s, hdr.mass_center);
    s += "\n";

    // The axis map crosses the file's min/max corners on X, so the transformed
    // corners are no longer the frame's min/max -- recompute component-wise.
    {
        const geom::vec_t a = to_godot(hdr.min_bounding);
        const geom::vec_t b = to_godot(hdr.max_bounding);
        jf(s, "bbox_min = Vector3(%.9g, %.9g, %.9g)\n", std::min(a[0], b[0]),
           std::min(a[1], b[1]), std::min(a[2], b[2]));
        jf(s, "bbox_max = Vector3(%.9g, %.9g, %.9g)\n", std::max(a[0], b[0]),
           std::max(a[1], b[1]), std::max(a[2], b[2]));
    }

    s += "detail_levels = ";
    packed_int(s, hdr.detail_levels);
    s += "\ndebris_pieces = ";
    packed_int(s, hdr.debris_pieces);
    s += "\n";

    // Weapon muzzles: gun banks then missile banks, retail's own split.
    for (int wtype = 0; wtype < 2; ++wtype) {
        jf(s, "%s = [", wtype ? "missile_banks" : "gun_banks");
        bool first = true;
        for (const auto &bank : model.get_weapons()) {
            if (bank.type != (wtype ? MISSILE : GUN))
                continue;
            s += first ? "{" : ", {";
            first = false;
            hardpoints(s, bank.muzzles);
            s += "}";
        }
        s += "]\n";
    }

    // Turrets, one per base submodel: retail merges the gun/missile banks
    // last-wins and drops the chunk provenance (dump.cc; survey), so the .tres
    // carries the merged form -- the thing the game actually turns.
    {
        struct merged_t
        {
            int arm;
            geom::vec_t norm;
            geom::vec_array_t points;
        };
        std::map< int, merged_t > merged;
        for (const auto &t : model.get_turrets())
            merged[t.parent] = { t.physical_parent, t.norm, t.fire_points };

        s += "turrets = [";
        bool first = true;
        for (const auto &[base, t] : merged) {
            jf(s, "%s{\"base\": %d, \"arm\": %d, \"normal\": ",
               first ? "" : ", ", base, t.arm);
            first = false;
            gv3(s, t.norm);
            s += ", \"fire_points\": ";
            packed_points(s, t.points);
            s += "}";
        }
        s += "]\n";
    }

    s += "thrusters = [";
    {
        bool first = true;
        for (const auto &bank : model.get_thrusters()) {
            geom::vec_array_t pts, norms;
            std::vector< float > radii;
            for (const auto &gp : bank.glow_points) {
                pts.push_back(gp.pos);
                norms.push_back(gp.norm);
                radii.push_back(gp.radius);
            }
            s += first ? "{\"points\": " : ", {\"points\": ";
            first = false;
            packed_points(s, pts);
            s += ", \"normals\": ";
            packed_points(s, norms);
            s += ", \"radii\": ";
            packed_float(s, radii);
            jf(s, ", \"properties\": \"%s\"}", jstr(bank.properties).c_str());
        }
    }
    s += "]\n";

    s += "docks = [";
    {
        bool first = true;
        for (const auto &d : model.get_docking()) {
            const std::string nm = prop_value(d.properties, "$name");
            jf(s, "%s{\"name\": \"%s\", \"paths\": ", first ? "" : ", ",
               jstr(nm).c_str());
            first = false;
            packed_int(s, d.paths);
            s += ", ";
            hardpoints(s, d.dockpoints);
            s += "}";
        }
    }
    s += "]\n";

    s += "eyes = [";
    {
        bool first = true;
        for (const auto &e : model.get_eyes()) {
            jf(s, "%s{\"parent\": %d, \"point\": ", first ? "" : ", ",
               e.sobj_number);
            first = false;
            gv3(s, e.sobj_offset);
            s += ", \"normal\": ";
            gv3(s, e.norm);
            s += "}";
        }
    }
    s += "]\n";

    // AI paths: parent is a submodel *name*; resolve it to an index the way
    // retail does (drop a leading '$', last case-insensitive name match, -1
    // if none) so the .tres carries the resolved `sub` the game uses.
    s += "paths = [";
    {
        bool first = true;
        for (const auto &pth : model.get_paths()) {
            std::string parent = pth.parent;
            if (!parent.empty() && parent[0] == '$')
                parent.erase(0, 1);
            int sub = -1;
            for (std::size_t j = 0; j < subs.size(); ++j)
                if (iequals(subs[j].name, parent))
                    sub = int(j);

            geom::vec_array_t pts;
            std::vector< float > radii;
            for (const auto &vert : pth.verts) {
                pts.push_back(vert.pos);
                radii.push_back(vert.radius);
            }

            jf(s, "%s{\"name\": \"%s\", \"parent\": \"%s\", \"sub\": %d, "
                  "\"points\": ",
               first ? "" : ", ", jstr(pth.name).c_str(),
               jstr(parent).c_str(), sub);
            first = false;
            packed_points(s, pts);
            s += ", \"radii\": ";
            packed_float(s, radii);
            s += "}";
        }
    }
    s += "]\n";

    // Subsystem/special points: emitted straight from the SPCL chunk. Retail
    // resolves these against ships.tbl and the pof_dump oracle drops them
    // (dump.cc), so this field alone has no oracle -- tests/check_tres.py says
    // as much rather than pretend coverage.
    s += "subsystems = [";
    {
        bool first = true;
        for (const auto &sp : model.get_specials()) {
            jf(s, "%s{\"name\": \"%s\", \"properties\": \"%s\", \"point\": ",
               first ? "" : ", ", jstr(sp.name).c_str(),
               jstr(sp.properties).c_str());
            first = false;
            gv3(s, sp.point);
            jf(s, ", \"radius\": %.9g}", sp.radius);
        }
    }
    s += "]\n";

    // Shield: a flat vertex table and triangles indexing it, both file data
    // (no BSP walk). Vertex/normal coordinates map; the index triples do not.
    const auto &shield = model.get_shield();
    s += "shield_verts = ";
    packed_points(s, shield.verts);
    s += "\nshield_tris = [";
    {
        bool first = true;
        for (const auto &tri : shield.tris) {
            s += first ? "{\"normal\": " : ", {\"normal\": ";
            first = false;
            gv3(s, tri.norm);
            jf(s, ", \"verts\": PackedInt32Array(%d, %d, %d), "
                  "\"neighbors\": PackedInt32Array(%d, %d, %d)}",
               tri.verts[0], tri.verts[1], tri.verts[2], tri.neighbors[0],
               tri.neighbors[1], tri.neighbors[2]);
        }
    }
    s += "]\n";

    std::FILE *out = std::fopen(path.c_str(), "wb");
    if (!out) {
        std::fprintf(stderr, "pof2glb: cannot write %s\n", path.c_str());
        return false;
    }
    const bool ok = std::fwrite(s.data(), 1, s.size(), out) == s.size();
    return std::fclose(out) == 0 && ok;
}

// ---- GLB container ---------------------------------------------------

bool
write_glb(const std::string &path, const std::string &json, bin_t &bin)
{
    std::string js = json;
    while (js.size() % 4)
        js += ' ';
    while (bin.data.size() % 4)
        bin.data.push_back(0);

    const std::uint32_t magic = 0x46546C67; // "glTF"
    const std::uint32_t version = 2;
    const std::uint32_t total = 12 + 8 + js.size() + 8 + bin.data.size();
    const std::uint32_t jlen = js.size(), jtag = 0x4E4F534A; // "JSON"
    const std::uint32_t blen = bin.data.size(), btag = 0x004E4942; // "BIN"

    std::FILE *out = std::fopen(path.c_str(), "wb");
    if (!out)
        return false;

    bool ok = std::fwrite(&magic, 4, 1, out) == 1 &&
        std::fwrite(&version, 4, 1, out) == 1 &&
        std::fwrite(&total, 4, 1, out) == 1 &&
        std::fwrite(&jlen, 4, 1, out) == 1 &&
        std::fwrite(&jtag, 4, 1, out) == 1 &&
        std::fwrite(js.data(), 1, js.size(), out) == js.size() &&
        std::fwrite(&blen, 4, 1, out) == 1 &&
        std::fwrite(&btag, 4, 1, out) == 1 &&
        std::fwrite(bin.data.data(), 1, bin.data.size(), out) ==
            bin.data.size();

    return std::fclose(out) == 0 && ok;
}

// ---- conversion ------------------------------------------------------

bool
convert(pof::model::model_t &model, const std::string &name,
        const std::string &src_path, const std::string &out_path)
{
    const auto &subobjects = model.get_subobjects();
    const auto &textures = model.get_textures();

    gltf_t g;
    manifest_t m;
    add_source(m, src_path);

    // Transcode the maps first: this fills g.image_of, which material() reads
    // as it builds each draw call below. data/maps is the model's sibling
    // (cfile.cc:45/48), the PNGs land beside the GLB we are about to write.
    emit_textures(g, m, textures, dir_of(src_path) + "/../maps",
                  dir_of(out_path));

    // children lists first: parent -> "i,j,k," in subobject order
    std::vector< std::string > children(subobjects.size());
    std::string roots;

    for (std::size_t i = 0; i < subobjects.size(); ++i) {
        const int parent = subobjects[i].parent_sobj;
        char buf[24];
        snprintf(buf, sizeof buf, "%zu,", i);
        if (parent < 0)
            roots += buf;
        else
            children[parent] += buf;
    }

    for (std::size_t i = 0; i < subobjects.size(); ++i)
        node(g, subobjects[i], mesh(g, subobjects[i], textures), children[i]);

    std::string json = "{\"asset\":{\"version\":\"2.0\","
                       "\"generator\":\"pof2glb\"},\"scene\":0,";

    jf(json, "\"scenes\":[{\"name\":\"%s\",\"nodes\":[", jstr(name).c_str());
    json += roots;
    close_list(json, ']');
    json += "}],";

    json += "\"nodes\":[";
    json += g.nodes;
    close_list(json, ']');

    json += ",\"meshes\":[";
    json += g.meshes;
    close_list(json, ']');

    json += ",\"materials\":[";
    json += g.materials;
    close_list(json, ']');

    // Textures share one default sampler; each glTF texture is 1:1 with an
    // image (its external PNG uri). Omitted entirely when no map resolved.
    if (g.n_images > 0) {
        json += ",\"samplers\":[{}]";
        json += ",\"textures\":[";
        json += g.gtextures;
        close_list(json, ']');
        json += ",\"images\":[";
        json += g.images;
        close_list(json, ']');
    }

    json += ",\"accessors\":[";
    json += g.accessors;
    close_list(json, ']');

    json += ",\"bufferViews\":[";
    json += g.views;
    close_list(json, ']');

    jf(json, ",\"buffers\":[{\"byteLength\":%zu}]}", g.bin.data.size());

    if (!write_glb(out_path, json, g.bin)) {
        std::fprintf(stderr, "pof2glb: cannot write %s\n", out_path.c_str());
        return false;
    }
    add_output(m, dir_of(out_path), base_of(out_path));

    std::printf("%s: %zu nodes, %d meshes, %zu materials, %ld triangles\n",
                out_path.c_str(), subobjects.size(), g.n_meshes,
                g.material_of.size(), g.n_triangles);

    // The ship-data .tres rides beside the GLB, same stem (the plan's tree:
    // Ulysses.glb + UlyssesShipData.tres). Derive its path from the GLB's.
    std::string tres_path = out_path;
    if (tres_path.size() >= 4 && tres_path.substr(tres_path.size() - 4) == ".glb")
        tres_path.replace(tres_path.size() - 4, 4, ".tres");
    else
        tres_path += ".tres";

    if (!write_tres(model, name, tres_path))
        return false;
    add_output(m, dir_of(out_path), base_of(tres_path));

    std::printf("%s: ship data\n", tres_path.c_str());

    // The manifest closes the conversion, same stem again: only after it is
    // written is the output tree accounted for.
    const std::string man_path =
        tres_path.substr(0, tres_path.size() - 5) + ".manifest.json";
    if (!write_manifest(m, man_path))
        return false;

    std::printf("%s: manifest%s\n", man_path.c_str(),
                m.n_warnings ? " (WITH WARNINGS)" : "");
    return true;
}

// ---- the structural summary -------------------------------------------
//
// One line per subobject: index, name, parent, and whatever the file says
// about motion. Mirrors the survey's reading of the corpus so the two can be
// eyeballed against each other (docs/pof-corpus-survey.txt).

void
print_summary(pof::model::model_t &model, const std::string &path)
{
    const auto &hdr = model.get_header();

    std::printf("%s version %d\n", path.c_str(), model.GetVersion());
    std::printf("radius %g mass %g\n", hdr.max_radius, hdr.mass);

    std::printf("details %zu:", hdr.detail_levels.size());
    for (int ix : hdr.detail_levels)
        std::printf(" %d", ix);
    std::printf("\n");

    std::printf("debris %zu:", hdr.debris_pieces.size());
    for (int ix : hdr.debris_pieces)
        std::printf(" %d", ix);
    std::printf("\n");

    const auto &textures = model.get_textures();
    std::printf("textures %zu:", textures.size());
    for (const auto &name : textures)
        std::printf(" \"%s\"", name.c_str());
    std::printf("\n");

    std::printf("subobjects %d:\n", model.GetSOBJCount());

    for (int i = 0; i < model.GetSOBJCount(); ++i) {
        const auto &sobj = model.SOBJ(i);

        std::printf("  %3d \"%s\" parent %d polys %zu", i, sobj.name.c_str(),
                    sobj.parent_sobj, sobj.polygons.size());

        if (sobj.movement_type != -1)
            std::printf(" movement type %d axis %d", sobj.movement_type,
                        sobj.movement_axis);

        std::printf("\n");
    }
}

// "path/fighter01.pof" -> "fighter01"
std::string
model_name(const std::string &path)
{
    const std::size_t slash = path.find_last_of('/');
    const std::size_t base = slash == std::string::npos ? 0 : slash + 1;
    const std::size_t dot = path.find_last_of('.');

    return path.substr(base, dot > base ? dot - base : std::string::npos);
}

} // namespace

int
main(int argc, char **argv)
{
    bool summary = false;
    int arg = 1;

    if (arg < argc && !std::strcmp(argv[arg], "--summary")) {
        summary = true;
        ++arg;
    }

    if (argc - arg < 1 || argc - arg > 2) {
        std::fprintf(stderr,
                     "usage: pof2glb [--summary] <model.pof> [<out.glb>]\n");
        return 2;
    }

    const std::string path = argv[arg];
    pof::model::model_t model;

    if (model.LoadFromPOF(path) != 0) {
        std::fprintf(stderr, "pof2glb: cannot load %s\n", path.c_str());
        return 1;
    }

    if (summary) {
        print_summary(model, path);
        return 0;
    }

    const std::string name = model_name(path);
    const std::string out_path =
        argc - arg == 2 ? argv[arg + 1] : name + ".glb";

    return convert(model, name, path, out_path) ? 0 : 1;
}
