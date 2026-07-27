/*
 * sexp_reader.cpp -- the SEXP text->node-tree reader, carved out of sexp.cpp.
 *
 * This is the parser half of the SEXP VM: it turns mission-file s-expression
 * text into the internal Sexp_nodes[] cons tree and touches NO ship / object /
 * game state.  The evaluator, operator table, and variable machinery stay in
 * sexp.cpp.  The seam is documented in docs/sexp-vm.md; the reader was proven
 * game-state-free by the standalone tools/sexp_parse oracle before this split.
 *
 * The function bodies below were moved VERBATIM from sexp.cpp -- this is a pure
 * relocation, not a rewrite.  They call back into sexp.cpp for operator lookup
 * (identify_operator/find_operator), variable interning (get_sexp_text_for_
 * variable), and the variable table reset (init_sexp_vars); those calls already
 * crossed a function boundary, so the move adds no indirection.
 */

#include <parse/parselo.hh> // Mp, ignore_white_space, is_white_space, EOF_CHAR
#include <parse/sexp.hh>

// defined in sexp.cpp; only the reader references them
void init_sexp_vars();
void get_sexp_text_for_variable(char *text, char *token);

// forward decl for the recursive reader (not part of the public sexp.h surface)
int get_sexp(char *token);

// the node pool and the two locked boolean singletons (declared extern in sexp.h)
int Locked_sexp_true, Locked_sexp_false;
sexp_node Sexp_nodes[MAX_SEXP_NODES];

void
init_sexp()
{
    int i;

    for (i = 0; i < MAX_SEXP_NODES; i++) {
        if (!(Sexp_nodes[i].type & SEXP_FLAG_PERSISTENT)) {
            Sexp_nodes[i].type = SEXP_NOT_USED;
        }
    }

    init_sexp_vars();

    Locked_sexp_false = Locked_sexp_true = -1;
    Locked_sexp_false = alloc_sexp("false", SEXP_LIST, SEXP_ATOM_OPERATOR, -1,
                                   -1);
    Assert(Locked_sexp_false != -1);
    Sexp_nodes[Locked_sexp_false].type = SEXP_ATOM; // fix bypassing value
    Locked_sexp_true = alloc_sexp("true", SEXP_LIST, SEXP_ATOM_OPERATOR, -1, -1);
    Assert(Locked_sexp_true != -1);
    Sexp_nodes[Locked_sexp_true].type = SEXP_ATOM; // fix bypassing value
}

// allocates an sexp node.
int
alloc_sexp(char *text, int type, int subtype, int first, int rest)
{
    int i;

    i = find_operator(text);
    if ((i == OP_TRUE) && (type == SEXP_ATOM) &&
        (subtype == SEXP_ATOM_OPERATOR)) {
        return Locked_sexp_true;
    }
    else if ((i == OP_FALSE) && (type == SEXP_ATOM) &&
             (subtype == SEXP_ATOM_OPERATOR)) {
        return Locked_sexp_false;
    }

    i = find_free_sexp();
    Assert(i != Locked_sexp_true);
    Assert(i != Locked_sexp_false);
    if (i == MAX_SEXP_NODES) {
        return -1;
    }

    Assert(strlen(text) < TOKEN_LENGTH);
    strcpy(Sexp_nodes[i].text, text);
    Assert(type >= 0);
    Sexp_nodes[i].type = type;
    Sexp_nodes[i].subtype = subtype;
    Sexp_nodes[i].first = first;
    Sexp_nodes[i].rest = rest;
    Sexp_nodes[i].value = SEXP_UNKNOWN;
    return i;
}

// find the next free sexp and return it's index.
int
find_free_sexp()
{
    int i;

    for (i = 0; i < MAX_SEXP_NODES; i++) {
        if (Sexp_nodes[i].type == SEXP_NOT_USED) {
            break;
        }
    }

#ifndef NDEBUG
    //count_free_sexp_nodes();
#endif

    Assert(i != MAX_SEXP_NODES); // time to raise the limit..
    if (i == MAX_SEXP_NODES) {
        return -1;
    }

    return i;
}

// returns the first sexp index of data this function allocates. (start of this sexp)
// recursive function - always sets first and then rest
int
get_sexp(char *token)
{
    int start, node, last, len, op, count;
    char variable_text[TOKEN_LENGTH];

    // start - the node allocated in first instance of fuction
    // node - the node allocated in current instance of function
    // count - number of nodes allocated this instance of function [do we set last.rest or first]
    // variable - whether string or number is a variable referencing Sexp_variables

    // initialization
    start = last = -1;
    count = 0;

    ignore_white_space();
    while (*Mp != ')') {
        Assert(*Mp != EOF_CHAR);
        if (*Mp == '(') {
            // Sexp list
            Mp++;
            node = alloc_sexp("", SEXP_LIST, SEXP_ATOM_LIST, get_sexp(token), -1);
        }
        else if (*Mp == '\"') {
            // Sexp string
            len = strcspn(Mp + 1, "\"");

            Assert(Mp[len + 1] == '\"'); // hit EOF first (unterminated string)
            Assert(len < TOKEN_LENGTH); // token is too long.

            // check if string variable
            if (*(Mp + 1) == SEXP_VARIABLE_CHAR) {
                // reduce length by 1 for end \"
                int length = len - 1;
                Assert(length < 2 * TOKEN_LENGTH + 2);

                // start copying after skipping 1st char
                strncpy(token, Mp + 2, length);
                token[length] = 0;

                get_sexp_text_for_variable(variable_text, token);
                node = alloc_sexp(variable_text, (SEXP_ATOM | SEXP_FLAG_VARIABLE),
                                  SEXP_ATOM_STRING, -1, -1);
            }
            else {
                strncpy(token, Mp + 1, len);
                token[len] = 0;
                node = alloc_sexp(token, SEXP_ATOM, SEXP_ATOM_STRING, -1, -1);
            }

            // bump past closing \" by 1 char
            Mp += len + 2;
        }
        else {
            // Sexp operator or number
            len = 0;
            bool variable = false;
            while (*Mp != ')' && !is_white_space(*Mp)) {
                if ((len == 0) && (*Mp == SEXP_VARIABLE_CHAR)) {
                    variable = true;
                    Mp++;
                    continue;
                }
                Assert(*Mp != EOF_CHAR);
                Assert(len < TOKEN_LENGTH - 1);
                token[len++] = *Mp++;
            }

            token[len] = 0;
            len = 0;
            op = identify_operator(token);
            if (op != -1) {
                node = alloc_sexp(token, SEXP_ATOM, SEXP_ATOM_OPERATOR, -1, -1);
            }
            else {
                if (variable) {
                    // convert token text for variable
                    get_sexp_text_for_variable(variable_text, token);

                    node = alloc_sexp(variable_text,
                                      (SEXP_ATOM | SEXP_FLAG_VARIABLE),
                                      SEXP_ATOM_NUMBER, -1, -1);
                }
                else {
                    node = alloc_sexp(token, SEXP_ATOM, SEXP_ATOM_NUMBER, -1, -1);
                }
            }
        }

        // update links
        if (count++) {
            Assert(last != -1);
            Sexp_nodes[last].rest = node;
        }
        else {
            start = node;
        }

        Assert(node != -1); // ran out of nodes.  Time to raise the MAX!
        last = node;
        ignore_white_space();
    }

    Mp++; // skip past the ')'
    return start;
}

//	Still a debug-level system.
//	get_sexp_main reads and builds the internal representation for a
//	symbolic expression.
//	On entry:
//		Mp points at first character in expression.
//	The symbolic expression is built in Sexp_nodes beginning at node 0.
int
get_sexp_main()
{
    int start_node, op;
    char token[TOKEN_LENGTH];
    char *savep, ch;

    ignore_white_space();

    savep = Mp;
    if (!strncmp(Mp, "( )", 3))
        savep++;

    Assert(*Mp == '(');
    Mp++;
    start_node = get_sexp(token);
    // only need to check syntax if we have a operator
    if (/*Sexp_nodes[start_node].subtype != SEXP_ATOM_OPERATOR  ||*/ Fred_running ||
        (start_node == -1))
        return start_node;

    ch = *Mp;
    *Mp = '\0';

    op = identify_operator(CTEXT(start_node));
    if (op == -1)
        Error(LOCATION, "Can't find operator %s in operator list\n.",
              CTEXT(start_node));

    *Mp = ch;

    return start_node;
}
