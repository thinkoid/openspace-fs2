// GENERATED trap stubs for game symbols the foundation references but
// whose subsystems are not ported yet.  Calling one aborts with its name;
// data symbols read as zeros.  Entries disappear as subsystems port.
// Regenerate: link without this file, feed the demangled undefined-symbol
// list to gen_gamestubs.py (see notes.txt).

#include <stdio.h>
#include <stdlib.h>

#include "pstypes.h"


static void oracle_trap(const char *sym)
{
	fprintf(stderr, "oracle strayed into unported code: %s\n", sym);
	abort();
}

void debug_console(void (*)())
{
}

// data symbols, zero-backed
