project('fs2', 'cpp',
    version: '0.1',
    default_options: ['cpp_std=c++17', 'warning_level=1', 'buildtype=debug'])

# Retail members shadowing their type names (angles angles; etc.) — renames
# happen when each subsystem is ported, not wholesale.
add_project_arguments('-Wno-changes-meaning', language: 'cpp')

sdl2_dep = dependency('sdl2')

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
    'code/graphics/2d.cpp',
    'code/graphics/aaline.cpp',
    'code/graphics/bitblt.cpp',
    'code/graphics/circle.cpp',
    'code/graphics/colors.cpp',
    'code/graphics/font.cpp',
    'code/graphics/gradient.cpp',
    'code/graphics/grsoft.cpp',
    'code/graphics/grzbuffer.cpp',
    'code/graphics/line.cpp',
    'code/graphics/pixel.cpp',
    'code/graphics/rect.cpp',
    'code/graphics/scaler.cpp',
    'code/graphics/shade.cpp',
    'code/graphics/tmapper.cpp',
    'code/graphics/tmapscanline.cpp',
    'code/graphics/tmapscantiled16x16.cpp',
    'code/graphics/tmapscantiled32x32.cpp',
    'code/graphics/tmapscantiled64x64.cpp',
    'code/graphics/tmapscantiled128x128.cpp',
    'code/graphics/tmapscantiled256x256.cpp',
    'code/io/joy.cpp',
    'code/io/key.cpp',
    'code/io/mouse.cpp',
    'code/io/timer.cpp',
    'code/osapi/osapi.cpp',
    'code/osapi/osregistry.cpp',
    'code/anim/animplay.cpp',
    'code/anim/packunpack.cpp',
    'code/bmpman/bmpman.cpp',
    'code/model/modelcollide.cpp',
    'code/model/modelinterp.cpp',
    'code/model/modeloctant.cpp',
    'code/model/modelread.cpp',
    'code/palman/palman.cpp',
    'code/pcxutils/pcxutils.cpp',
    'code/tgautils/tgautils.cpp',
    'code/cfile/cfile.cpp',
    'code/cfile/cfilearchive.cpp',
    'code/cfile/cfilelist.cpp',
    'code/cfile/cfilesystem.cpp',
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
    include_directories: inc,
    dependencies: sdl2_dep)

cfilearchiver = executable('cfilearchiver',
    'code/cfilearchiver/cfilearchiver.cpp',
    include_directories: inc)

subdir('tests')
