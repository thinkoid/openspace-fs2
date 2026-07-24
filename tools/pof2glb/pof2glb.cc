// -*- mode: c++; -*-
//
// pof2glb: convert a retail POF model into GLB + Godot .tres + manifest
// (docs/godot-migration-plan.md, "The asset pipeline"). Reading goes through
// libpof, whose parse is oracle-pinned byte-identical to retail's loader, so
// the risk this tool carries is confined to the mapping and the writing,
// never the reading.
//
//   pof2glb <model.pof>
//
// Current state: loads the model and prints the structural summary the
// conversion will be driven by. The GLB and .tres emitters land next.

#include <model/file.hh>

#include <cstdio>
#include <string>

namespace {

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

    const auto textures = model.get_textures();
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

} // namespace

int
main(int argc, char **argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: pof2glb <model.pof>\n");
        return 2;
    }

    const std::string path = argv[1];
    pof::model::model_t model;

    if (model.LoadFromPOF(path) != 0) {
        std::fprintf(stderr, "pof2glb: cannot load %s\n", path.c_str());
        return 1;
    }

    print_summary(model, path);
    return 0;
}
