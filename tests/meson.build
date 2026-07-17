# Point-1 oracle and unit tests.  stubs.cpp carries temporary definitions for
# game globals the foundation references but whose owning subsystems are not
# ported yet; entries get deleted as the real code comes in.

# sexp.cpp's evaluator half references gameplay code that is not ported yet;
# gamestubs_gen.cpp fills those symbols with traps (see its header comment).
sexp_dump = executable('sexp_dump',
    ['sexp_dump.cpp', 'cfile_stub.cpp', 'stubs.cpp', 'gamestubs_gen.cpp'],
    include_directories: inc,
    link_with: foundation)

math_test = executable('math_test',
    ['math_test.cpp', 'stubs.cpp'],
    include_directories: inc,
    link_with: foundation)

test('math', math_test)
