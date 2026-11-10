# Co-op campaign: spawn-point investigation

Status: research / **no game patch installed**. All changes for this experiment belong on `feature/coop-campaign`, not `main`.

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

## Inputs required for evidence-based implementation

Provide **local original world files for analysis only** (do not commit game assets):

1. `worlds/realm1/R1M2B.dat` — known single-player fallback.
2. `worlds/deathmatch/PM01.dat` — multiplayer start-point control.
3. Optional: `worlds/realm1/R1M1A.dat` — second campaign-world control.
4. Optional: corresponding editable DEdit `.ed` files, if available.

These files may need extracting from the user's own `PSYCHO.REZ` with their installed KPC tools. If extracted files have different extensions or paths, retain the original names and supply the relevant world entries instead.

Do not import or package original game maps, models, textures, sound, or artwork in KPCFix releases. Derived, non-copyright-protected coordinate metadata may be considered only after verification.

## Investigation sequence

1. **Static world comparison.** Identify DAT version and structure; locate object class tables and property records. Compare player-start object classes and counts in R1M2B and PM01. Confirm whether the absence of multiplayer starts accounts for the observed fallback.
2. **Runtime call path.** In matching official 1.13 `object.lto` / `server.dll` Ghidra analysis, find xrefs to the exact fallback message. Trace multiplayer-start lookup, single-player fallback, player initialization and spawn placement. Record function signatures and calling conventions, not only absolute addresses.
3. **Patch design.** Prefer a server-authoritative, co-op-only spawn allocator activated **after** the engine's world objects load. First reuse valid real spawn objects; where campaign worlds have only a single-player start, select collision-validated offsets or a per-map table.
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

The exact class names, object table, player-coordinate layout, hook sites, and collision API are **not yet known**. Do not infer these from the fallback error or implement hard-coded position patches until the supplied world data and corresponding original engine paths are inspected.
