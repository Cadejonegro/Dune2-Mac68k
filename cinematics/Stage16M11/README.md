# Stage16M11 — Macintosh Dune II cinematic

This is an **experimental native Macintosh 68k introduction/cinematic player**, separate from the [R4-P2 gameplay prototype](../../README.md). It is **not integrated into the gameplay application**.

## What is implemented

- QuickDraw scene playback and palette timing
- Optional timed subtitles
- Narration playback using an external PCM pack
- House-specific sound-effect scheduling
- Experimental profiling and timing controls

These are development features, not a guarantee of complete or accurate playback on physical hardware.

## Build the source

Install [Retro68](https://github.com/autc04/Retro68) and CMake. From the **repository root**, replace the toolchain placeholder with the path to your installed Retro68 toolchain file:

```sh
cmake -S cinematics/Stage16M11 -B build-cinematic -DCMAKE_TOOLCHAIN_FILE=/path/to/retro68.toolchain.cmake
cmake --build build-cinematic -j 4
```

The published source-only cinematic has compiled with Retro68. **The modified optional subtitle loader and complete playback have not yet been verified on a physical Macintosh IIci.** Compilation does not imply the required commercial assets are present.

## External game assets

**No original Westwood graphics, animation, palettes, narration, sound effects, or dialogue are included in this repository.** The player expects external, locally prepared files:

| Format | Purpose |
| --- | --- |
| `.FRM` | Converted introduction scene frames |
| `.PAL` | Scene palettes |
| `INTROVOX.DAT` | Raw unsigned 8-bit narration PCM, indexed by the source's `VoiceIndex.h` |
| `EFFECTS8.DAT` | Raw unsigned 8-bit effects PCM, indexed in `DuneEffects.c` |
| `INTROTXT.DAT` | Optional subtitles; see below |

The two audio packs are **custom concatenated PCM files**, not original DOS files that can simply be copied unchanged. The published repository does not yet include a tested, complete conversion pipeline from a DOS Dune II installation to all the required cinematic files. OpenDUNE's extraction utilities and experimental local development scripts are not a verified one-command solution. Users must prepare the assets themselves from game data they are authorized to use.

For reference, the Stage16M11 development index describes 43 narration segments totaling **839,275 bytes** and eight effect segments totaling **256,309 bytes**. These sizes describe the indexed development packs, not a promise that an arbitrary converter will produce byte-identical audio.

## Optional subtitles

`INTROTXT.DAT` is an optional plain-text file with **19 lines**, one per subtitle event, in the event order expected by the source. For a multiline caption, write a literal `\n` sequence within the line.

If the file is absent, the published player is intended to continue without subtitles. The original 19 commercial dialogue strings were removed from the source-only publication. **This modified loader still needs runtime verification**; do not assume the feature is confirmed solely because the code compiles.

## Current limitations

- Asset preparation is not yet reproducible end-to-end from public instructions.
- The optional subtitle loader has not been playback-verified after source-only publication.
- Narration and effect timing remain experimental.
- Effects can be delayed or dropped because of audio-channel contention.
- Complete cinematic playback and performance still need testing on a physical 25 MHz Macintosh IIci.
- This workstream is not part of R4-P2 gameplay.

## Legal and project notes

This is an unofficial fan project. Supply original game data locally; do not upload extracted commercial assets, generated packs, or original dialogue to issues or pull requests. Retain applicable OpenDUNE notices and licensing terms. Development has involved substantial assistance from OpenAI's ChatGPT; this does not imply OpenAI endorsement.

See [the main README](../../README.md) and [contribution guide](../../docs/CONTRIBUTING.md).
