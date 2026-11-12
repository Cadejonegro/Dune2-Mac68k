R3D8D-H1E: guarded recovery + lightweight harvest indicator

SOURCE: H1D after successful compilation. Apply only to a new copy.

Changes:
- If the player's empty Harvester is harvesting on non-spice and its script explicitly requests STOP, search within radius 20 for another spice tile. If found, issue MOVE and assign that tile. No spice found => original STOP remains.
- R7 diagnostic marks guarded recovery, preserving H1D logging.
- Draw a tiny alternating three-pixel dust indicator on spice while ACTION_HARVEST is active. No new assets, timers, or allocation. Animation depends on the existing redraw cadence and may not be perceptible at low redraw rates.
- Correct H1C_TraceGet prototype to 13 arguments.

LIMITS: This is a targeted mitigation of the observed failure path, not proof that every Harvester stall is resolved. It intentionally does not alter full-cargo RETURN, enemy Harvester AI, or user STOP orders. If no spice is found within radius 20, STOP still occurs.

Installation: see paste-ready commands in chat. Compile locally with Retro68 before transfer.
