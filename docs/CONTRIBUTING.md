# Contributing to Dune2-Mac68k

Contributions, reproducible bug reports, code reviews, and hardware test results are welcome. This is an experimental **native Classic Mac OS 68k adaptation** of Dune II, not a DOS emulator or a finished port.

## Two independent targets

- **Gameplay R4-P2:** the repository-root CMake project builds `OpenDUNE68k`. R4-P2 is the latest published baseline tested on a physical Macintosh IIci.
- **Stage16M11 cinematic:** `cinematics/Stage16M11/` has its own CMake project. Its source-only build has compiled with Retro68, but the modified optional subtitle loader and complete cinematic playback have **not** yet been verified on a physical IIci. It is not linked into R4-P2.

## Build the source

Install CMake and a working [Retro68](https://github.com/autc04/Retro68) toolchain. Replace the placeholder below with the path to your actual Retro68 toolchain file.

Gameplay (from repository root):

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/retro68.toolchain.cmake
cmake --build build -j 4
```

Cinematic (from repository root):

```sh
cmake -S cinematics/Stage16M11 -B build-cinematic -DCMAKE_TOOLCHAIN_FILE=/path/to/retro68.toolchain.cmake
cmake --build build-cinematic -j 4
```

These commands compile the source; they do **not** supply the original commercial data needed for runtime. Build outputs include Retro68 Classic Mac application artifacts. A successful compile does not establish correct gameplay or cinematic playback.

## Original game data and asset conversion

Do **not** commit, attach to issues, or submit in pull requests original Dune II commercial data, extracted audio, animation frames, palettes, subtitle dialogue, generated game-data packs, or binaries embedding those assets.

- The gameplay prototype expects original DOS Dune II data supplied separately. The root source references files including `DUNEMAP.CPS` and `SCENA002.INI`.
- The cinematic expects converted `.FRM` scene frames and `.PAL` palettes, plus `INTROVOX.DAT` narration and `EFFECTS8.DAT` effects. Optional `INTROTXT.DAT` contains 19 subtitle entries, one line per event; the published source does not include the original dialogue.
- The repository contains upstream OpenDUNE extraction utilities under `third_party/OpenDUNE/tools/`, but these are **not** a verified end-to-end Stage16M11 conversion pipeline.
- Local experimental conversion scripts exist in earlier development milestones. Some have machine-specific paths or depend on files not in this repository. The reproducible public asset-preparation workflow is **not yet complete**.

If you contribute an asset converter, it should read user-supplied files locally, have documented inputs and outputs, validate malformed input, and never include extracted copyrighted data in source or test fixtures.

## Testing and performance

Target: 25 MHz Motorola 68030 Macintosh IIci, 20 MB RAM, System 7.1, built-in 256-color video. Basilisk II tests are useful but are **not a substitute** for real IIci testing.

When reporting a bug or performance change, include the commit SHA, emulator or hardware configuration, original-data version if relevant, reproduction steps, expected and observed behavior, and whether the issue also occurs on the physical IIci. For performance changes, distinguish command responsiveness, battlefield scrolling, simulation rate, and animation smoothness.

Known physical-hardware issues include invisible infantry, missing Carryall delivery animation, choppy movement after responsiveness improvements, and limited overall performance. See [known issues](KNOWN_ISSUES.md) and the [issue tracker](https://github.com/Cadejonegro/Dune2-Mac68k/issues).

## Development approach

- Preserve a known-working baseline; prefer focused changes with clear test instructions.
- Reuse/adapt original OpenDUNE gameplay mechanics where practical rather than rewriting movement, pathfinding, construction, harvesting, combat, or AI unnecessarily.
- Prioritize architectural workload reductions and actual IIci playability over isolated emulator or QuickDraw microbenchmarks.
- Retain upstream notices and applicable GPLv2 licensing requirements.
- Explain any untested assumptions. AI-generated code should be reviewed like any other code.

The repository owner directs the project and performs hardware testing, with substantial coding assistance from ChatGPT. Contributions from experienced Classic Mac and OpenDUNE developers are especially appreciated.
