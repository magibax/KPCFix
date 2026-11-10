# Co-op campaign: spawn-point investigation

Status: three-player R1M2B separation **live-tested successfully**; general fail-closed spawn allocator implemented and tested as a standalone policy (**not yet connected to game physics**). The map-specific prototype stays off by default. All changes for this experiment belong on `feature/coop-campaign`, not `main`.

## Question

Can official 1.13 campaign worlds spawn two or more multiplayer clients at safe, distinct positions without editing or redistributing the original world archives?

## Known baseline

- `worlds\\realm1\\R1M2B` loads under the dedicated server, but reports:

  ```text
  Error!! Could not find any multiplayer start points. Looking for a singleplayer start point.
  ```

- The same message was reported with `[ServerTools] Enabled=0`. It therefore cannot be attributed to the current Server Tools runtime hooks alone.
- The launcher indexes 59 maps in `server-tools/maps.json`, including campaign and deathmatch maps. Exposing a map in the launcher does **not** establish co-op compatibility.
- Stable Server Tools hooks are restricted to cheats/chat/player disconnect handling. They do **not** implement campaign spawn selection.

## Evidence now available

The user supplied MAPDATA.zip (59 compiled v127 DAT worlds plus scripts) for **local, read-only analysis**. All world object tables parsed and validated. The corresponding results are in [SPAWN-FINDINGS.md](SPAWN-FINDINGS.md), and the reusable asset-free analyzer is [tools/analyze_spawn_points.py](tools/analyze_spawn_points.py).

The user also supplied matching, original 1.13 dedicated.exe, server.original.dll and object.lto. Their checksums were verified and the server spawn-selection call graph traced. See [SPAWN-ENGINE-TRACE.md](SPAWN-ENGINE-TRACE.md) for the confirmed runtime addresses, the **opt-in, non-relocating** spawn logging prototype, how to build it, and how to decode its logs.

The [native BSP world-volume investigation](BSP-VOLUME-ABI.md) includes an opt-in loaded-world R1M2B whole-box traversal (TraceBspVolume=1), **live-validated on three navigable spawn boxes and one wall-overlapping control**. It does not cover other solid WorldModels or actors, so general spawn approval remains disabled. The [native AABB overlap research](BOX-OVERLAP-ABI.md) documents a verified read-only box-vs-box comparator, but not world BSP clearance. The [R1M2B collision diagnostic](INTERSECT-ABI.md) recorded nine grounded ray hits and three control misses. Player-volume clearance, disconnect and map-reset handling remain open.

Do not import or package original game maps, models, textures, sound, or artwork in KPCFix releases.

## Investigation sequence

1. **Static world comparison.** Identify DAT version and structure; locate object class tables and property records. Compare player-start object classes and counts in R1M2B and PM01. Confirm whether the absence of multiplayer starts accounts for the observed fallback.
2. **Runtime call path.** In matching official 1.13 `object.lto` / `server.dll` Ghidra analysis, find xrefs to the exact fallback message. Trace multiplayer-start lookup, single-player fallback, player initialization and spawn placement. Record function signatures and calling conventions, not only absolute addresses.
3. **Patch design.** Prefer a server-authoritative, co-op-only spawn allocator activated **after** the engine's world objects load. Use the intended campaign entry anchor and select collision-validated, nearby offsets; existing deathmatch-type starts may be spread throughout the level, so they are not automatically valid co-op candidates.
4. **Isolation.** Retain strict official-build verification and runtime-only patching. Keep the original server behaviour intact when co-op is disabled; abort safely if a hook's expected instruction bytes/layout differ.
5. **2-client proof.** Run R1M2B with two real game clients. Both must spawn at separately validated positions, stand on floor, move and survive independently; neither should spawn inside geometry or each other.
6. **Regression.** Verify DM map PM01, ordinary server mode, Enabled=0 passthrough, disconnect/reconnect, restart, map change, EU/US server profiles, and unchanged `object.lto` on disk.

## Acceptance criteria (gate 1)

- Two distinct initial spawn transforms are assigned deterministically.
- Both transforms are collision-safe and valid for the loaded world.
- Existing deathmatch spawning is unchanged.
- The co-op spawn mechanism is off by default until explicitly selected.
- Logging reports map ID, player ID, chosen spawn source and validation result.
- No original assets or modified official binaries appear in source or release archives.

## Explicitly out of scope for gate 1

Lobby/ready-up UI, synchronized films, scripted triggers, shared pickups, saves, campaign progression, and complete co-op playability. These require separate gates after basic player spawning is proven.

## Next live test

The volume evidence gate passed its four Windows controls. The box-vs-box overlap primitive is now statically verified; next identify and verify **native full-hull against world BSP**, followed by active-player enumeration. Do not infer safe spawns from clear rays or broadphase box tests. Keep the trial at three clients and restart before reusing slots until authoritative departure and world-reset events are verified. See [allocator integration](GENERAL-ALLOCATOR.md).

[Issue #8](https://github.com/magibax/KPCFix/issues/8) tracks clearance, reconnect, map transitions and Windows regression tests for the integrated build.
