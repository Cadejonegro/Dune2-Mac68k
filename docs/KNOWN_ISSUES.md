# Known issues and testing status

This page describes the **published R4-P2 gameplay baseline**, not every local experimental build. The separate Stage16M11 cinematic is documented in [its README](../cinematics/Stage16M11/README.md).

## Physical Macintosh IIci issues

### Infantry rendering
Infantry appear in Basilisk II but are invisible on the physical Macintosh IIci, including after the R4-P2 redraw changes. Emulator success does not resolve this bug.

### Carryall delivery
The Harvester appears after delivery, but the Carryall delivery animation is missing on physical hardware.

### Unit movement smoothness
R4-P1 improved perceived unit responsiveness on the physical IIci, but movement became visibly choppy. R4-P2 retains that tradeoff. Faster movement alone should not be mistaken for smoother gameplay.

### Overall performance
The 25 MHz IIci remains a demanding target. Future optimization should reduce total rendering and simulation workload, and should be evaluated through actual scrolling, command response, and playability on the real machine. Isolated QuickDraw benchmarks are not sufficient.

## Feature-completeness caveat

The published baseline is experimental. Construction, harvesting, commands, and combat have been observed during development, but **not all missions, houses, AI behavior, and gameplay paths have been validated end-to-end**. Do not interpret a successful build or an emulator pass as a claim that the game is finished.

## Build and original-data limitations

- The repository does not distribute original commercial Dune II game data.
- Building the source and preparing the required runtime data are separate tasks.
- A complete, reproducible public asset-preparation pipeline is not yet provided. Existing upstream extraction utilities do not by themselves establish a validated end-to-end workflow.

## Reporting a useful test

Include the tested commit, whether you used Basilisk II or a physical Macintosh, OS and memory/video configuration, the action that reproduced the issue, and what happened. For performance reports, describe scrolling, unit-command response, and animation smoothness separately.

See [CONTRIBUTING.md](CONTRIBUTING.md) for build commands and contribution guidance.
