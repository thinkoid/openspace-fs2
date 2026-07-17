# Point-1 oracle and unit tests.  stubs.cpp carries temporary definitions for
# game globals the foundation references but whose owning subsystems are not
# ported yet; entries get deleted as the real code comes in.

# sexp.cpp's evaluator half references gameplay code that is not ported yet;
# gamestubs_gen.cpp fills those symbols with traps (see its header comment).
sexp_dump = executable('sexp_dump',
    ['sexp_dump.cpp', 'stubs.cpp', 'gamestubs_gen.cpp', 'freespacestubs_gen.cpp'],
    include_directories: inc,
    link_with: foundation,
    dependencies: sdl2_dep)

vp_ls = executable('vp_ls',
    ['vp_ls.cpp', 'stubs.cpp', 'gamestubs_gen.cpp', 'freespacestubs_gen.cpp'],
    include_directories: inc,
    link_with: foundation,
    dependencies: sdl2_dep)

pof_dump = executable('pof_dump',
    ['pof_dump.cpp', 'stubs.cpp', 'gamestubs_gen.cpp', 'freespacestubs_gen.cpp'],
    include_directories: inc,
    link_with: foundation,
    dependencies: sdl2_dep)

math_test = executable('math_test',
    ['math_test.cpp', 'stubs.cpp', 'gamestubs_gen.cpp', 'freespacestubs_gen.cpp'],
    include_directories: inc,
    link_with: foundation,
    dependencies: sdl2_dep)

first_pixels = executable('first_pixels',
    ['first_pixels.cpp', 'stubs.cpp', 'gamestubs_gen.cpp', 'freespacestubs_gen.cpp'],
    include_directories: inc,
    link_with: foundation,
    dependencies: sdl2_dep)

test('math', math_test)
