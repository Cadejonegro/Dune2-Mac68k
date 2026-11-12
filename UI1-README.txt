R3D8D-UI1 read-only sidebar, based on validated D1. No game logic changed.
Existing battlefield 320x200 remains at (20,20); sidebar occupies x348..438.
UI1 redraws only during existing update events; does not add CopyBits.
Basilisk II first; verify no overlapping diagnostics, palette issues, or IIci slowdown.
