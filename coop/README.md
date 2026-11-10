# Co-op campaign — research and roadmap

Status: three-player R1M2B separation **live-tested successfully**; general fail-closed spawn allocator implemented and tested as a standalone policy (**not yet connected to game physics**). The map-specific prototype stays off by default. Continue experimental co-op development on `feature/coop-campaign`. The current `main` history also contains these changes following the earlier branch merge; this does not make the tagged stable Server Tools release co-op-ready.

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

**Tracking:** Maintain outstanding work in the checklist below. Keep detailed engine evidence in the linked research documents rather than expanding a GitHub issue.

## Next diagnostic: loaded WorldModel resource survey

The new opt-in `[Coop] TraceModelRegistry=1` diagnostic reads only the
original 1.13 server's named WorldModel resource registry, reports how
many resource entries and plausible BSP roots are reachable, and logs
the first root pointer. No engine method is called and no model transform,
game object or spawning is modified. The resource registry does **not**
prove which model objects are live, solid or transformed. See
[BSP-VOLUME-ABI.md](BSP-VOLUME-ABI.md) for the original ABI evidence and
the Windows test controls.

The follow-up `registry-backrefs-v3` build additionally verifies
`model+0xdc` against the reciprocal `object+0x140` resource pointer,
reporting matched, mismatched and unreadable counts separately.
Even a matching link is not evidence of current solidity or safe
world-space hull clearance.

The optional `TraceModelState=1` (requiring `TraceModelRegistry=1`)
now surveys raw native WorldModel type, object flag-bit counts and
creation-time vector equality for reciprocally verified associations.
These are diagnostics, not validated solidity flags or geometry
transforms. See [BSP-VOLUME-ABI.md](BSP-VOLUME-ABI.md).

**2026-11-10 correction:** Original engine WorldModel-specific paths
test `object+0x76 == 2`; the v4 diagnostic's type-one label
was incorrect. The updated `model-types-v5` survey reports both
type 2 and type 1 and captures at most two read-only examples
of mismatched model/source position vectors. See
[BSP-VOLUME-ABI.md](BSP-VOLUME-ABI.md).

The earlier `model-geometry-survey-v6` diagnostic added a type histogram,
type-2/other flag cross-tabs and raw adjacent/bounds values for the
two changed-vector objects. The current `spawn-list-control-v8` build
continues the read-only investigation. These are *read-only research data*,
not a verified collision transform, live-solid roster, or spawn gate.

## Outstanding work — co-op roadmap

- [ ] **Authoritative clearance:** Verify world BSP and live solid WorldModel collision, coordinate transforms, actor/player occupancy and hazards using open, blocked and enclosed-obstacle controls. Keep unknown results fail-closed. See [allocator research](GENERAL-ALLOCATOR.md) and [BSP evidence](BSP-VOLUME-ABI.md).
- [ ] **Automatic spawning:** Connect the general allocator only after the collision/grounding contract is verified. Spawn two or more clients safely near campaign entry anchors on R1M2B and at least one additional map.
- [ ] **Player lifecycle:** Verify departure, death, respawn, reconnect, world restart and reservation reuse; reject occupied positions and prevent telefrags.
- [ ] **Campaign startup and synchronisation:** Establish original script execution timing with zero, one and multiple players; coordinate intro presentation, joining mid-sequence, reliable state replication and scripted gameplay effects. See [campaign sync design](CAMPAIGN-SYNC-DESIGN.md).
- [ ] **Map progression:** Make right-click mirror exits and corridor triggers transition all clients exactly once, with consistent objectives, pickups and player state. Demonstrate a complete two-level playthrough before extending across realms.
- [ ] **Windows regressions and release gate:** Test multiplayer deathmatch, disabled passthrough, supported regional 1.13 profiles, installation/upgrade/rollback, unchanged original assets and safe defaults. Document results before publishing an experimental build.

Checked items should be backed by reproducible tests or linked Windows observations; keep intermediate reverse-engineering logs in the relevant research Markdown files.

### Campaign activation and first exit

The [campaign startup and scripted-sequence synchronisation design](CAMPAIGN-SYNC-DESIGN.md) records the empty-server question, proposed server-authoritative state machine, client presentation versus game effects, late joining/reconnect policy, and required read-only engine traces. This is a **proposal**, not a working co-op sequence implementation.


In the dedicated Windows trial the two fixed spawn positions worked, but the usual intro did not start and walking through a known transition did nothing. The developer browser's client-hosted map load provides a working intro comparison. R1M2B has WorldProperties.starting_cinematic pointing at CS2START.SCR, which also loads level scripts. CExitTrigger0.NextWorld and WORLDORDER.TXT independently identify R1M2C as the next map.

The original object module checks the game-type field at RVA 0x2d455 before the exit handler can call 0x2d510. A nonzero value skips that call. The opt-in TraceCampaign switch logs this decision and the starting_cinematic lookup at 0x1045b1, preserving the native decisions. If the trace is armed but the hallway produces no exit line, trace the upstream event source and object activation. Shared transitions and per-client cinematic delivery still need separate verification.
