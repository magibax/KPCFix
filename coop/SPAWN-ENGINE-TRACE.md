# Engine spawn trace: official 1.13 binaries

Status: **static reverse engineering verified**, optional instrumentation added in source, **no multiplayer runtime test and no spawn relocation patch**.

## Uploaded binary provenance

| Filename | SHA-256 | Identity |
| --- | --- | --- |
| dedicated.exe | b940fd6d3fb7ef91c965631b72b0856e17ed14d91f11cda7c7d50cf8c4668ad8 | European-family 1.13 host |
| server.original.dll | 83b7bce809948b6f70ed3f35a77a771b6087f1104d90d06a62ac3dc0d4bd37d5 | Matching original 1.13 engine |
| object(3).lto | 3a03f8796c482b1a7ad72755df33ee8f1f8056bb10957f6f700e64bd006b3e5b | Original, unmodified object.lto |

All three SHA-256 digests match server-tools/SERVER_TOOLS_SUPPORTED_BUILDS.json.
No uploaded binaries or original assets are included in this repository commit.

## Verified call graph in object.lto

The PE32 image has a preferred base of 0x10000000. Addresses below are RVAs so runtime relocation does not invalidate the description.

1. RVA 0x0f12a0 begins the player spawn-initialization routine. RVA 0x0f12be calls 0x0f1070, a spawn-strategy dispatcher. Its dispatch depends on game state at global+0x1f8.
2. When no start is returned, RVA 0x0f12f1 logs the fallback warning from RVA 0x148a7c.
3. RVA 0x0f132b addresses the global game-type selector at global+0x1d0; RVA 0x0f1333 temporarily **zeroes it**, 0x0f1339 calls 0x0f08a0, then 0x0f1340 restores the original selector. This is an intentional single-player fallback.
4. RVA 0x0f08a0 enumerates PlayerStart objects and compares each object's field +0x1d4 to the current game-type selector. This agrees with the GameType distinction in the compiled world maps.
5. RVA 0x0f1355 calls engine API offset +0x1d0 to read the selected start position into a local vector.
6. RVA 0x0f13a4 calls 0x0f10c0 to prepare or adjust the proposed spawn position. The geometry/collision implications remain unverified.
7. RVA 0x0f13c4 calls the engine API function pointer at +0x1e0 with (HOBJECT, LTVector*), followed at RVA 0x0f13ca by eight bytes of caller stack cleanup. This is the **position-setting observation point**.

**Caution:** RVA 0x0f13b0 calls a *different* player routine with three scalar values and a flag. Do not displace those parameters.

Use the read-only check before testing a build:

    python coop/tools/check_spawn_site.py /path/to/object.lto

The checker verifies the complete original object hash and 11 instruction/data signatures.

## Optional spawn trace (research build only)

The feature branch adds a conditional server-side tracing redirect at RVA 0x0f13c4 in server-tools-source/src/proxy.c.

- Disabled by default.
- Activated only in an approved dedicated-server host with [ServerTools] Enabled=1 and [Coop] TraceSpawns=1 in KPCServerTools.ini; restart required.
- The official object SHA-256, PE image layout and original six-byte instruction are checked before the additional redirect is installed **in memory only**.
- The wrapper preserves the original (HOBJECT, LTVector*) arguments, invokes the engine's original +0x1e0 function, then logs the requested position vector (float32 bit patterns) and object handle to KPCServerTools.log. The first 64 events are logged.
- No coordinates are modified in this phase; ordinary gameplay is expected to remain unchanged. This still requires in-game verification.
- object.lto remains unchanged on disk. No map or artwork is redistributed.

The existing committed server-tools/KPCServerTools.dll in this experimental branch is **not rebuilt** from the new source. To test the new hook, build the server DLL from this branch using the documented 32-bit GCC/binutils toolchain, then run Setup-KPCServerTools.cmd to deploy it. Do not create a release until the rebuilt DLL, manifests, and runtime tests have been validated.

Example KPCServerTools.ini:

    [ServerTools]
    Enabled=1
    AllowCheats=0

    [Coop]
    TraceSpawns=1

Suggested test: start R1M2B, join with two clients, then inspect KPCServerTools.log. Decode the logged float bits:

    python coop/tools/decode_spawn_log.py /path/to/KPCServerTools.log

Run synthetic tests:

    python -m unittest discover -s coop/tools -p 'test_*.py' -v

**Trace records observe requested coordinates, not proven collision-validated or final player transforms.**

## Next gate

Only after verifying this tracing hook in-game should we add co-op-only, server-authoritative spawn assignment. That requires known player collision dimensions, safe nearby world positions, distinct reservations, and regression against PM01 and normal server mode.
