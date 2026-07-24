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
// Current state: emits geometry, hierarchy and named materials; textures,
// the ship-data .tres and the manifest land next.

#include <model/file.hh>

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

    bin_t bin;

    int n_meshes = 0;
    int n_accessors = 0;
    int n_views = 0;

    long n_triangles = 0;

    // texture_id -> material index (flat-colored polys keyed by -1-rgb below)
    std::map< int, int > material_of;

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

// material for a texture slot: named after the texture, no image yet (the
// texture pipeline is a later phase); flat-colored polys get a factor-only
// material keyed by -(0x1000000 | rgb) so distinct colors stay distinct
int
material(gltf_t &g, const std::vector< std::string > &textures, int key,
         const pof::model::poly_t &poly)
{
    auto [it, fresh] = g.material_of.try_emplace(key, g.material_of.size());
    if (!fresh)
        return it->second;

    if (key >= 0)
        jf(g.materials, "{\"name\":\"%s\",\"doubleSided\":false},",
           jstr(textures[key]).c_str());
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
// wants but glTF has no slot for ride in "extras".
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
        const std::string &out_path)
{
    const auto &subobjects = model.get_subobjects();
    const auto &textures = model.get_textures();

    gltf_t g;

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

    std::printf("%s: %zu nodes, %d meshes, %zu materials, %ld triangles\n",
                out_path.c_str(), subobjects.size(), g.n_meshes,
                g.material_of.size(), g.n_triangles);
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

    return convert(model, name, out_path) ? 0 : 1;
}
