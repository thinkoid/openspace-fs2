// sexp_reader.h -- standalone SEXP text->node-tree reader.
//
// A game-free extraction of the FreeSpace 2 SEXP *reader* (parser), lifted
// verbatim from code/parse/sexp.cpp to prove the carve seam documented in
// docs/sexp-vm.md: the reader touches no ship/object/game state. This unit
// links with NO game object files. See README.md for the (small, deliberate)
// prototype adaptations vs the in-game reader.
//
// Stage 1 of the sexp-vm.md carve plan. Stage 2 replaces this copy with a
// shared translation unit compiled into both the game and this tool.
#pragma once

// ---- data model (mirrors code/parse/sexp.h) --------------------------------
#define TOKEN_LENGTH        32
#define MAX_SEXP_NODES      2200

// sexp_node.type
#define SEXP_NOT_USED       0
#define SEXP_LIST           1
#define SEXP_ATOM           2
#define SEXP_FLAG_PERSISTENT (1<<31)
#define SEXP_FLAG_VARIABLE   (1<<30)

// sexp_node.subtype (atom kinds)
#define SEXP_ATOM_LIST      0
#define SEXP_ATOM_OPERATOR  1
#define SEXP_ATOM_NUMBER    2
#define SEXP_ATOM_STRING    3

// sexp_node.value sentinels (only SEXP_UNKNOWN is set by the reader)
#define SEXP_UNKNOWN        (-3)

#define SEXP_VARIABLE_CHAR  '@'

struct sexp_node {
    char text[TOKEN_LENGTH];
    int  type;      // SEXP_ATOM | SEXP_LIST | SEXP_NOT_USED (+ flag bits)
    int  subtype;   // SEXP_ATOM_*
    int  first;     // CAR: child list index, or -1
    int  rest;      // CDR: next sibling index, or -1
    int  value;     // result cache (reader writes SEXP_UNKNOWN)
};

extern sexp_node Sexp_nodes[MAX_SEXP_NODES];

#define CAR(n)  (Sexp_nodes[n].first)
#define CDR(n)  (Sexp_nodes[n].rest)
#define CTEXT(n) (Sexp_nodes[n].text)

// ---- the parselo cursor (self-contained; game parselo not linked) ----------
extern char *Mp;              // read cursor, walks the mission text buffer
void  ignore_white_space();
int   is_white_space(char ch);

// ---- reader API (the carved surface) ---------------------------------------
void  init_sexp();            // reset pool + (re)create the true/false singletons
int   get_sexp(char *token);  // recursive reader: reads a '('..')' body
int   get_sexp_main();        // reads ONE top-level expression from Mp

int   identify_operator(char *token);  // Operators[] name lookup -> index or -1

// ---- oracle helpers --------------------------------------------------------
int   sexp_nodes_used();       // count of non-free nodes (pool high-water)
int   sexp_tree_depth(int n);  // max depth from node n
void  sexp_round_trip(int n, char *out, int outsz);  // node tree -> s-expr text
