# Stage 16M11 — Macintosh Dune II Cinematic

Experimental native Macintosh 68k Dune II introduction player.

This cinematic is a separate development milestone from the R4-P2
gameplay prototype.

## Features

- QuickDraw cinematic playback
- Scene and palette timing
- Optional timed subtitles
- Narration playback
- House-specific sound-effect scheduling
- Performance profiling

## Building

Requires Retro68 and CMake.

    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/retro68.toolchain.cmake
    cmake --build build -j 4

## Required original game assets

Original Westwood graphics, narration, music, and sound effects
are not included.

The program expects locally prepared FRM/PAL scene files,
INTROVOX.DAT, and EFFECTS8.DAT.

Users must prepare these separately from game data they are
authorized to use.

## Optional subtitles

INTROTXT.DAT is an optional plain-text file containing
19 lines, one per subtitle event.

Use literal backslash-n sequences for multiline captions.

If the file is missing, the cinematic continues without subtitles.

Original game dialogue is not distributed in this repository.

## Known limitations

- Audio and effects timing remain experimental.
- Effects may be delayed or dropped due to channel contention.
- Physical Macintosh IIci testing remains necessary.
- A successful source-only build does not mean the cinematic
  can play without its required external game assets.

## Attribution

Unofficial fan project developed with substantial assistance
from OpenAI's ChatGPT.

Original Dune II assets belong to their respective rights holders.
OpenDUNE-derived code remains subject to its applicable license.
