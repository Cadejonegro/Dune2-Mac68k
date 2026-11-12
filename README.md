# Dune II for Macintosh 68k

An experimental native Macintosh 68k adaptation of Dune II, built using OpenDUNE and original DOS game data.

## Target hardware

- Macintosh IIci, 25 MHz Motorola 68030
- 20 MB RAM
- System 7.1
- Built-in 256-color video
- BlueSCSI

## Project goals

Create a responsive, enjoyable Macintosh-specific adaptation of Dune II rather than an exact DOS reproduction. Preserve recognizable missions, factions, construction, harvesting, combat, and AI while reducing unnecessary workloads.

## Current status

Experimental R4-P2 baseline. Tested in Basilisk II and on a physical Macintosh IIci. Infantry remain invisible on physical hardware, Carryall delivery animation is missing, and unit movement is visually choppy.

## Building

Requires Retro68 and CMake. Configure using Retro68's retro68.toolchain.cmake and build the OpenDUNE68k target.

Example commands:

    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/retro68.toolchain.cmake
    cmake --build build -j 4

Original DOS Dune II game data must be supplied separately.

## Contributing

Contributions are welcome, particularly for Macintosh 68k rendering, performance, QuickDraw, and OpenDUNE integration. See docs/KNOWN_ISSUES.md.

## Licensing and credits

Based on https://github.com/OpenDUNE/OpenDUNE at commit ebea79aeba025be2fe9004570e9255044aacc0be.

OpenDUNE is licensed under GNU GPL version 2. See LICENSE and third_party/OpenDUNE/COPYING. Preserve original copyright notices.

Commercial Dune II game assets are not included. This is an unofficial fan project.
