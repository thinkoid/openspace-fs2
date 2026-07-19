// sexp_reader.cpp -- standalone SEXP reader (see sexp_reader.h).
//
// The reader functions below are lifted VERBATIM from code/parse/sexp.cpp
// (cited per function). The only adaptations, all marked "PROTOTYPE:", are:
//   1. the parselo cursor (Mp / ignore_white_space / is_white_space) is
//      implemented here instead of linked from parselo.cpp;
//   2. Operators[] carries only names (from sexp_ops.inc) -- the reader needs
//      the table solely to classify a token as operator-vs-number;
//   3. true/false singleton collapse keys on the token text, not OP_TRUE/FALSE;
//   4. @variable interning keeps the name (as Fred does) instead of rewriting
//      it to a Sexp_variables[] index (the game has no variable table here).
// None of these change the produced tree STRUCTURE. Assert -> assert.
#include "sexp_reader.h"
#include <cassert>
#include <cstring>
#include <cstdio>

#define EOF_CHAR '\0'   // PROTOTYPE: buffer is NUL-terminated; game uses parselo's EOF_CHAR

sexp_node Sexp_nodes[MAX_SEXP_NODES];
int Locked_sexp_true, Locked_sexp_false;

// PROTOTYPE: operator NAME table (real Operators[] also carries OP_* + min/max).
static const char *Operators[] = {
#include "sexp_ops.inc"
};
static const int Num_operators = sizeof(Operators) / sizeof(Operators[0]);

// ---- the cursor (parselo stand-in) -----------------------------------------
char *Mp = 0;

int is_white_space(char ch)          // parselo.cpp: is_white_space()
{
    return (ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r');
}

void ignore_white_space()            // parselo.cpp: ignore_white_space()
{
    while ((*Mp != EOF_CHAR) && is_white_space(*Mp))
        Mp++;
}

// ---- operator lookup (sexp.cpp: identify_operator, line ~754) ---------------
int identify_operator(char *token)
{
    int i;
    for (i = 0; i < Num_operators; i++)
        if (!strcasecmp(token, Operators[i]))   // game: stricmp
            return i;
    return -1;
}

// PROTOTYPE: name-keyed true/false detection (game: find_operator == OP_TRUE/FALSE)
static int is_true_token(const char *t)  { return !strcasecmp(t, "true"); }
static int is_false_token(const char *t) { return !strcasecmp(t, "false"); }

// ---- node pool (sexp.cpp: find_free_sexp / alloc_sexp / init_sexp) ----------
int find_free_sexp()                 // sexp.cpp: find_free_sexp(), line ~451
{
    int i;
    for (i = 0; i < MAX_SEXP_NODES; i++)
        if (Sexp_nodes[i].type == SEXP_NOT_USED)
            break;
    assert(i != MAX_SEXP_NODES);     // time to raise the limit..
    if (i == MAX_SEXP_NODES)
        return -1;
    return i;
}

int alloc_sexp(char *text, int type, int subtype, int first, int rest)  // sexp.cpp: ~399
{
    // PROTOTYPE: collapse true/false operator atoms to the two locked singletons.
    if ((type == SEXP_ATOM) && (subtype == SEXP_ATOM_OPERATOR)) {
        if (is_true_token(text))  return Locked_sexp_true;
        if (is_false_token(text)) return Locked_sexp_false;
    }

    int i = find_free_sexp();
    if (i == MAX_SEXP_NODES || i == -1)
        return -1;

    assert(strlen(text) < TOKEN_LENGTH);
    strcpy(Sexp_nodes[i].text, text);
    assert(type >= 0);
    Sexp_nodes[i].type = type;
    Sexp_nodes[i].subtype = subtype;
    Sexp_nodes[i].first = first;
    Sexp_nodes[i].rest = rest;
    Sexp_nodes[i].value = SEXP_UNKNOWN;
    return i;
}

void init_sexp()                     // sexp.cpp: init_sexp(), line ~377
{
    for (int i = 0; i < MAX_SEXP_NODES; i++)
        if (!(Sexp_nodes[i].type & SEXP_FLAG_PERSISTENT))
            Sexp_nodes[i].type = SEXP_NOT_USED;

    Locked_sexp_false = Locked_sexp_true = -1;
    Locked_sexp_false = alloc_sexp((char *)"false", SEXP_LIST, SEXP_ATOM_OPERATOR, -1, -1);
    assert(Locked_sexp_false != -1);
    Sexp_nodes[Locked_sexp_false].type = SEXP_ATOM;   // fix bypassing value
    Locked_sexp_true = alloc_sexp((char *)"true", SEXP_LIST, SEXP_ATOM_OPERATOR, -1, -1);
    assert(Locked_sexp_true != -1);
    Sexp_nodes[Locked_sexp_true].type = SEXP_ATOM;
}

// PROTOTYPE: game's get_sexp_text_for_variable rewrites to a Sexp_variables[]
// index; here we just keep the bare variable name.
static void get_sexp_text_for_variable(char *dest, char *src) { strcpy(dest, src); }

// ---- the recursive reader (sexp.cpp: get_sexp, line ~1613) ------------------
int get_sexp(char *token)
{
    int start, node, last, len, op, count;
    char variable_text[TOKEN_LENGTH];

    start = last = -1;
    count = 0;

    ignore_white_space();
    while (*Mp != ')') {
        assert(*Mp != EOF_CHAR);
        if (*Mp == '(') {
            Mp++;
            node = alloc_sexp((char *)"", SEXP_LIST, SEXP_ATOM_LIST, get_sexp(token), -1);

        } else if (*Mp == '\"') {
            len = strcspn(Mp + 1, "\"");
            assert(Mp[len + 1] == '\"');   // unterminated string
            assert(len < TOKEN_LENGTH);

            if (*(Mp + 1) == SEXP_VARIABLE_CHAR) {
                int length = len - 1;
                strncpy(token, Mp + 2, length);
                token[length] = 0;
                get_sexp_text_for_variable(variable_text, token);
                node = alloc_sexp(variable_text, (SEXP_ATOM | SEXP_FLAG_VARIABLE), SEXP_ATOM_STRING, -1, -1);
            } else {
                strncpy(token, Mp + 1, len);
                token[len] = 0;
                node = alloc_sexp(token, SEXP_ATOM, SEXP_ATOM_STRING, -1, -1);
            }
            Mp += len + 2;

        } else {
            len = 0;
            bool variable = false;
            while (*Mp != ')' && !is_white_space(*Mp)) {
                if ((len == 0) && (*Mp == SEXP_VARIABLE_CHAR)) {
                    variable = true;
                    Mp++;
                    continue;
                }
                assert(*Mp != EOF_CHAR);
                assert(len < TOKEN_LENGTH - 1);
                token[len++] = *Mp++;
            }
            token[len] = 0;
            len = 0;
            op = identify_operator(token);
            if (op != -1) {
                node = alloc_sexp(token, SEXP_ATOM, SEXP_ATOM_OPERATOR, -1, -1);
            } else {
                if (variable) {
                    get_sexp_text_for_variable(variable_text, token);
                    node = alloc_sexp(variable_text, (SEXP_ATOM | SEXP_FLAG_VARIABLE), SEXP_ATOM_NUMBER, -1, -1);
                } else {
                    node = alloc_sexp(token, SEXP_ATOM, SEXP_ATOM_NUMBER, -1, -1);
                }
            }
        }

        if (count++) {
            assert(last != -1);
            Sexp_nodes[last].rest = node;
        } else {
            start = node;
        }
        assert(node != -1);   // ran out of nodes
        last = node;
        ignore_white_space();
    }

    Mp++;   // skip past the ')'
    return start;
}

// ---- top-level entry (sexp.cpp: get_sexp_main, line ~7651) ------------------
// The game version has a `Fred_running || ...` branch here that, in-game, does a
// single root-operator identify_operator() Error check. That is the ONLY game
// coupling in the reader path -- and it is omitted here, which is the whole
// point of the carve.
int get_sexp_main()
{
    int start_node;
    char token[TOKEN_LENGTH];

    ignore_white_space();
    char *savep = Mp;
    if (!strncmp(Mp, "( )", 3))
        savep++;

    assert(*Mp == '(');
    Mp++;
    start_node = get_sexp(token);
    (void)savep;
    return start_node;
}

// ---- oracle helpers --------------------------------------------------------
int sexp_nodes_used()
{
    int n = 0;
    for (int i = 0; i < MAX_SEXP_NODES; i++)
        if (Sexp_nodes[i].type != SEXP_NOT_USED)
            n++;
    return n;
}

int sexp_tree_depth(int n)
{
    if (n < 0) return 0;
    int d_first = Sexp_nodes[n].first >= 0 ? sexp_tree_depth(Sexp_nodes[n].first) + 1 : 0;
    int d_rest  = Sexp_nodes[n].rest  >= 0 ? sexp_tree_depth(Sexp_nodes[n].rest)      : 0;
    return d_first > d_rest ? d_first : d_rest;
}

// re-emit the tree as s-expr text (round-trip fidelity check)
void sexp_round_trip(int n, char *out, int outsz)
{
    if (n < 0 || outsz <= 1) return;
    int used = strlen(out);
    if (Sexp_nodes[n].subtype == SEXP_ATOM_LIST || Sexp_nodes[n].type == SEXP_LIST) {
        snprintf(out + used, outsz - used, "( ");
        sexp_round_trip(Sexp_nodes[n].first, out, outsz);
        used = strlen(out);
        snprintf(out + used, outsz - used, ") ");
    } else {
        const char *q = (Sexp_nodes[n].subtype == SEXP_ATOM_STRING) ? "\"" : "";
        snprintf(out + used, outsz - used, "%s%s%s ", q, Sexp_nodes[n].text, q);
    }
    if (Sexp_nodes[n].rest >= 0)
        sexp_round_trip(Sexp_nodes[n].rest, out, outsz);
}
