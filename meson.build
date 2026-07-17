project('fs2', 'cpp',
    version: '0.1',
    default_options: ['cpp_std=c++17', 'warning_level=1', 'buildtype=debug'])

# Retail members shadowing their type names (angles angles; etc.) — renames
# happen when each subsystem is ported, not wholesale.
add_project_arguments('-Wno-changes-meaning', language: 'cpp')

# Retail sources use flat includes (#include "parselo.h"); the include path
# supplies the directories, as the original MSVC project files did.
inc = include_directories(
    'code/anim', 'code/asteroid', 'code/bmpman', 'code/cfile',
    'code/cmdline', 'code/cmeasure', 'code/controlconfig', 'code/cryptstring',
    'code/cutscene', 'code/debris', 'code/debugconsole', 'code/fireball',
    'code/freespace2', 'code/gamehelp', 'code/gamesequence', 'code/gamesnd',
    'code/globalincs', 'code/graphics', 'code/hud', 'code/io',
    'code/jumpnode', 'code/lighting', 'code/localization', 'code/math',
    'code/menuui', 'code/mission', 'code/missionui', 'code/model',
    'code/nebula', 'code/network', 'code/object', 'code/observer',
    'code/osapi', 'code/palman', 'code/parse', 'code/particle',
    'code/pcxutils', 'code/physics', 'code/playerman', 'code/popup',
    'code/radar', 'code/render', 'code/ship', 'code/sound',
    'code/starfield', 'code/stats', 'code/tgautils', 'code/ui',
    'code/weapon')

foundation_src = files(
    'code/globalincs/alphacolors.cpp',
    'code/globalincs/crypt.cpp',
    'code/globalincs/debug.cpp',
    'code/globalincs/systemvars.cpp',
    'code/globalincs/version.cpp',
    'code/math/fix.cpp',
    'code/math/floating.cpp',
    'code/math/fvi.cpp',
    'code/math/spline.cpp',
    'code/math/staticrand.cpp',
    'code/math/vecmat.cpp',
    'code/localization/fhash.cpp',
    'code/localization/localize.cpp',
    'code/parse/encrypt.cpp',
    'code/parse/parselo.cpp',
    'code/parse/sexp.cpp')

foundation = static_library('foundation', foundation_src,
    include_directories: inc)

subdir('tests')
