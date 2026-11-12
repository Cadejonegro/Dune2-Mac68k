# Dune II for Macintosh 68k

**An experimental, native Macintosh 68k adaptation of Westwood Studios' Dune II, built around OpenDUNE and the original DOS game data.**

**Status:** Work in progress — R4-P2 is the latest published, physical-hardware-tested baseline. This is not yet a complete or polished port.

## The goal

The goal is to make **Dune II genuinely enjoyable on a stock-speed 25 MHz Macintosh IIci**, running Classic Mac OS as a native Motorola 68030 application — **not through DOS emulation**.

This is a **Macintosh-specific adaptation**, not a pixel-perfect DOS clone. Responsiveness on the real IIci matters more than preserving every original graphic, animation frame, or timing detail. We are willing to simplify rendering, animation, user-interface details, and nonessential simulation workload where doing so improves the experience.

At the same time, the game should still *feel like Dune II*. The intended experience includes the three playable houses (Atreides, Harkonnen, and Ordos), recognizable missions and campaigns, units and structures, construction, spice harvesting and economy, combat, commands, and enemy AI. OpenDUNE's existing systems and original DOS data are the foundation; the aim is to adapt them rather than reinvent them unnecessarily.

**Success is judged on physical Macintosh IIci hardware:** smoother battlefield scrolling, quicker response to unit commands, and noticeably better overall gameplay. Merely dropping animation frames or slowing down the game is not a performance win.

## Target hardware

- Macintosh IIci with a 25 MHz Motorola 68030
- 20 MB RAM
- System 7.1
- Built-in 256-color video
- BlueSCSI storage

## How this project was developed

I started this project as a retro-computing enthusiast, **not as a programmer**. Most of the implementation, code generation, debugging suggestions, build scripts, and technical guidance so far have been produced with the help of **OpenAI's ChatGPT**, with me directing the goals, compiling builds, running tests in Basilisk II and on my physical Macintosh IIci, reporting failures, and deciding what felt playable.

This repository is being shared both to document how far that collaboration got and to invite experienced developers to inspect, correct, improve, and continue the work. **AI-generated code should be reviewed critically**: successful compilation and emulator testing do not guarantee correctness or acceptable performance on real hardware.

## Progress so far

The project has advanced beyond a simple proof of concept:

- A native Macintosh 68k application can be built with **Retro68 and CMake**.
- Original Dune II/OpenDUNE terrain and game assets have been brought into a Macintosh display pipeline using user-supplied DOS data.
- Battlefield scrolling, mouse selection, and unit commands have been implemented and tested.
- Original OpenDUNE gameplay code has been integrated for substantial parts of the mission, unit, structure, combat, and game-state systems.
- Construction and Wind Trap placement have worked in testing; Harvester deployment, harvesting, and docking have also been observed.
- A succession of renderer and workload-reduction experiments led to the **R4-P1/R4-P2** baseline. R4-P1 improved perceived unit responsiveness on the physical IIci, although movement became choppier; R4-P2 retained that behavior.
- **R4-P2 builds cleanly** from the standalone publication source tree and has been tested in Basilisk II and on physical hardware.

These milestones do **not** mean all missions, factions, AI behaviors, or gameplay systems are complete or fully validated. Some features work in the emulator but fail on the physical machine.

## Current limitations and help wanted

1. **Infantry rendering:** infantry appear in Basilisk II but remain invisible on the physical IIci.
2. **Carryall delivery:** the Harvester appears, but the Carryall delivery animation is missing on physical hardware.
3. **Movement smoothness:** responsiveness improved with reduced workload, but movement became visibly choppy.
4. **Overall performance:** further architecture-level reductions in drawing and simulation work are needed; isolated QuickDraw micro-optimizations have not reliably translated into gameplay improvements on the IIci.

See [Known Issues](docs/KNOWN_ISSUES.md) and the [GitHub issue tracker](https://github.com/Cadejonegro/Dune2-Mac68k/issues) for details.

**Contributors are especially welcome** if they have experience with Classic Mac OS, Motorola 680x0 C or assembly, QuickDraw, Retro68, game engine architecture, or OpenDUNE. Help with code review, hardware-specific bugs, performance profiling, and build reproducibility would be valuable. See [Contributing](docs/CONTRIBUTING.md).

## Building

Requires Retro68 and CMake. Configure with your Retro68 toolchain file:

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/retro68.toolchain.cmake
cmake --build build -j 4
```

The build produces the OpenDUNE68k application artifacts. Running the game requires **original DOS Dune II game data supplied separately**; commercial game assets are not included in this repository. Build success alone is not proof of hardware compatibility.

## Licensing and credits

This project incorporates source from [OpenDUNE](https://github.com/OpenDUNE/OpenDUNE), based on upstream commit `ebea79aeba025be2fe9004570e9255044aacc0be`. OpenDUNE is licensed under the GNU General Public License version 2; see [LICENSE](LICENSE) and [third_party/OpenDUNE/COPYING](third_party/OpenDUNE/COPYING). Original copyright and attribution notices must be retained.

The Macintosh adaptation has been developed by the repository owner with substantial AI-assisted coding and guidance from OpenAI's ChatGPT. That assistance does not imply OpenAI endorsement, maintenance, or official affiliation.

Dune II and its commercial game assets belong to their respective rights holders. This is an unofficial fan project.
