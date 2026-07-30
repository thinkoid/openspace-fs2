// fs2_t is the boundary object of libfs2: the retail simulation behind a
// narrow, engine-agnostic API (docs/godot-migration-plan.md, "Second step").
// No Godot header may appear here or in fs2.cc -- the Godot marshalling
// lives in extension.cc, the one file that speaks both languages. Oracle
// tools (sim_dump) link fs2_t directly, engine absent.

#pragma once

struct fs2_t {
    // the vcs_tag build version -- slice 0's round-trip payload
    const char *version() const;
};
