cmake_minimum_required(VERSION 3.16)

project(OpenDUNE68k C)

set(OPENDUNE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/OpenDUNE"
)

add_application(OpenDUNE68k
    OpenDUNE68k.c
    r4_present.c
    r4_framebuffer.c
    r4_terrain_restore.c
    dirty_closure.c
    r4_partial_present.c
    dune_game_sound.c
    mac68k_support.c
    readdir_mac68k.c
    file_mac68k.c
    "${OPENDUNE_DIR}/src/os/endian.c"
    "${OPENDUNE_DIR}/src/codec/format80.c"
)

target_include_directories(OpenDUNE68k PRIVATE
    "${OPENDUNE_DIR}/include"
    "${OPENDUNE_DIR}/src/codec"
)

target_compile_options(OpenDUNE68k PRIVATE
    -std=gnu99
    -O2
    -include
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/mac68k_compat.h"
)

target_compile_definitions(OpenDUNE68k PRIVATE
    DUNE_DATA_DIR="."
    MAC68K_CLASSIC=1
)

# Stage 10G: Original OpenDUNE gameplay engine.
target_sources(OpenDUNE68k PRIVATE
    "${OPENDUNE_DIR}/src/map.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit_h1c.c"
    "${OPENDUNE_DIR}/src/scenario.c"
    "${OPENDUNE_DIR}/src/house.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/structure_r3d8d.c"
    "${OPENDUNE_DIR}/src/team.c"
    "${OPENDUNE_DIR}/src/tile.c"
    "${OPENDUNE_DIR}/src/tools.c"
    "${OPENDUNE_DIR}/src/pool/house.c"
    "${OPENDUNE_DIR}/src/pool/unit.c"
    "${OPENDUNE_DIR}/src/pool/structure.c"
    "${OPENDUNE_DIR}/src/pool/team.c"
    "${OPENDUNE_DIR}/src/table/houseinfo.c"
    "${OPENDUNE_DIR}/src/table/unitinfo.c"
    "${OPENDUNE_DIR}/src/table/structureinfo.c"
    "${OPENDUNE_DIR}/src/table/landscapeinfo.c"
    "${OPENDUNE_DIR}/src/table/tilediff.c"
    "${OPENDUNE_DIR}/src/animation.c"
    "${OPENDUNE_DIR}/src/explosion.c"
    "${OPENDUNE_DIR}/src/script/general.c"
    "${OPENDUNE_DIR}/src/script/script.c"
    "${OPENDUNE_DIR}/src/script/structure.c"
    "${OPENDUNE_DIR}/src/script/team.c"
    script_unit_r3d7s2.c
    "${OPENDUNE_DIR}/src/table/actioninfo.c"
    "${OPENDUNE_DIR}/src/table/animation.c"
    "${OPENDUNE_DIR}/src/object.c"
    sprites_mac68k.c
    "${OPENDUNE_DIR}/src/ini.c"
    "${OPENDUNE_DIR}/src/inifile.c"
    "${OPENDUNE_DIR}/src/string.c"
    "${OPENDUNE_DIR}/src/config.c"
    "${OPENDUNE_DIR}/src/gfx.c"
    "${OPENDUNE_DIR}/src/wsa.c"
    "${OPENDUNE_DIR}/src/gui/viewport.c"
    "${OPENDUNE_DIR}/src/input/mouse.c"
    "${OPENDUNE_DIR}/src/gui/widget.c"
    "${OPENDUNE_DIR}/src/gui/gui.c"
    "${OPENDUNE_DIR}/src/gui/font.c"
    "${OPENDUNE_DIR}/src/codec/format40.c"
    mac_game_state.c
    mac_video.c
    mac_timer.c
    "${OPENDUNE_DIR}/src/table/explosion.c"
    "${OPENDUNE_DIR}/src/table/movementtype.c"
    "${OPENDUNE_DIR}/src/table/teamaction.c"
)


# Stage 10H: Original OpenDUNE initialization diagnostic.
target_sources(OpenDUNE68k PRIVATE
    diagnostic/mac_mission_init.c
)

# Original OpenDUNE input system.
target_sources(OpenDUNE68k PRIVATE
    "${OPENDUNE_DIR}/src/input/input.c"
)

# Keep only engine functions needed by the application.
target_compile_options(OpenDUNE68k PRIVATE
    -ffunction-sections
    -fdata-sections
)
target_link_options(OpenDUNE68k PRIVATE
    -Wl,--gc-sections
)

target_sources(OpenDUNE68k PRIVATE "${OPENDUNE_DIR}/src/table/widgetinfo.c")

target_sources(OpenDUNE68k PRIVATE "${OPENDUNE_DIR}/src/table/selectiontype.c")

target_sources(OpenDUNE68k PRIVATE mac_mission_link_stubs.c)

# R3D7S2: include roots for relocated script translation unit.
target_compile_options(OpenDUNE68k PRIVATE "-iquote${OPENDUNE_DIR}/src" "-iquote${OPENDUNE_DIR}/src/script")
