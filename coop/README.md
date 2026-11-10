# Co-op campaign: spawn-point investigation

Status: static spawn-selector analysis complete; optional read-only spawn-trace hook added in source (**off by default; no client spawn separation yet**). All changes for this experiment belong on `feature/coop-campaign`, not `main`.

## Question

Can official 1.13 campaign worlds spawn two or more multiplayer clients at safe, distinct positions without editing or redistributing the original world archives?

## Known baseline

- `worlds\\realm1\\R1M2B` loads under the dedicated server, but reports:

  ```text
  Error!! Could not find any multiplayer start points. Looking for a singleplayer start point.
  ```

- The same message was reported with `[ServerTools] Enabled=0`. It therefore cannot be attributed to the current Server Tools runtime hooks alone.
- The launcher indexes 59 maps in `server-tools/maps.json`, including campaign and deathmatch maps. Exposing a map in the launcher does **not** establish co-op compatibility.
- Current Server Tools hooks are restricted to cheats/chat/player disconnect handling. They do **not** implement campaign spawn selection.

## Evidence now available

The user supplied MAPDATA.zip (59 compiled v127 DAT worlds plus scripts) for **local, read-only analysis**. All world object tables parsed and validated. The corresponding results are in [SPAWN-FINDINGS.md](SPAWN-FINDINGS.md), and the reusable asset-free analyzer is [tools/analyze_spawn_points.py](tools/analyze_spawn_points.py).

The user also supplied matching, original 1.13 dedicated.exe, server.original.dll and object.lto. Their checksums were verified and the server spawn-selection call graph traced. See [SPAWN-ENGINE-TRACE.md](SPAWN-ENGINE-TRACE.md) for the confirmed runtime addresses, the **opt-in, non-relocating** spawn logging prototype, how to build it, and how to decode its logs.

**Remaining prerequisite:** two-client Windows runtime testing of the tracing prototype, followed by identification of safe, collision-validated co-op positions. Optional DEdit source maps could help research geometry but are not required for the current stage.

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

## Research notes

The DAT object-table layout, PlayerStart class, GameType property and sample world coordinates have been confirmed for 59 maps. Static reverse engineering now confirms the game-type fallback and identifies a guarded position-setting call site. The tracing wrapper is **not yet live-tested** and no collision-validating co-op offset mechanism exists. Do not enable automatic relocation until the in-game ABI and collision behavior are confirmed.

**Next action:** Build the experimental Server Tools DLL from this branch, enable [Coop] TraceSpawns=1 for a disposable two-client R1M2B test, inspect KPCServerTools.log, and verify pass-through with TraceSpawns=0 and Enabled=0. Use PM01 as a control.
