# Engine spawn trace: official 1.13 binaries

Status: **two-client spawn separation live-verified** on R1M2B (no telefragging; in-bounds movement), with repeat same-pointer placement observed after reconnects. A **three-client map-specific trial** is in the feature branch; slot 2 has not yet been live-tested.

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

## Live observation and experimental offset trial

On 2026-11-10 the user tested the trace build on R1M2B. Two distinct player object handles requested precisely the same float32 position:

    obj=0x02d08284 x=0x44c80000 y=0xc3800000 z=0xc5380000
    obj=0x02d080d8 x=0x44c80000 y=0xc3800000 z=0xc5380000

The values decode to (1600, -256, -2944). The user observed the second spawn telefragging the first player.

Further disassembly shows the world-object handling function at RVA 0x0f10c0 is called at RVA 0x0f13a4 **before** the final engine position setter at RVA 0x0f13c4. It receives the proposed LTVector by pointer, plus two additional vector pointers, with the player instance in ECX; it ends in ret 12. Therefore late changes at f13c4 would not necessarily prevent telefragging, and the trial hooks the earlier f13a4 site instead.

The trial:
- is **disabled by default** and requires [Coop] TrialSeparateSpawns=1, with ServerTools.Enabled=1;
- only acts on exact R1M2B single-player start coordinate bits;
- reserves first three distinct player *object pointers* for the lifetime of the object module;
- leaves the first at (1600,-256,-2944), moves the second to (1600,-256,-2848), and proposes (1600,-256,-3040) for the third, **before** calling the unmodified original f10c0 routine;
- leaves the original engine setter and telefrag handling intact, logs the chosen slot/coordinates, and keeps the existing post-position trace enabled;
- does **not** validate walls, floor, body size, or other actors, and does **not** release slots after disconnect. Set maxclients=3 and do not test a fourth distinct player pointer.

The first two positions have now been tested by the user in-game. Both players survived, no telefragging occurred, and their spawns were sensible and navigable. The trial logged three placements for slot 0 because of disconnect/rejoin testing; in those records the engine returned the **same player pointer**. That does not prove safe reuse when reconnecting produces a new pointer. The third candidate, -96 Z relative to the entry anchor, lies near corridor light and script-manager objects, but compiled object proximity does not certify floor or wall geometry. It remains untested.

**Three-client test:** Set server maxclients=3, connect three clients to R1M2B once, confirm every client survives and can move normally, and provide KPCServerTools.log. Do not test a fourth player or repeated disconnect/reconnect after three distinct pointer slots are reserved.

This remains a research-only, non-collision-validating allocator. Existing engine telefrag logic is not disabled.

## Three-client trace: three unique position requests

The user's subsequent three-player R1M2B server log shows the opt-in trial assigned three distinct player pointers to three distinct positions:

| Slot | Player pointer | Engine object handle | X | Y | Z |
| --- | --- | --- | ---: | ---: | ---: |
| 0 | `0x02ecb428` | `0x02e68284` | 1600 | -256 | -2944 |
| 1 | `0x02ecb170` | `0x02e680d8` | 1600 | -256 | -2848 |
| 2 | `0x02ecaeb8` | `0x02e67f2c` | 1600 | -256 | -3040 |

The log reported successful runtime hook installation and emitted all three modified positions at the engine position setter; no allocation-capacity warning appears. **This confirms three-slot assignment and forwarding, not actual player survival, wall/floor safety, or absence of telefragging.** Await direct in-game confirmation for all three, especially slot 2.

Do not enable more than three players, assume safe reconnect replacement, or generalize this hard-coded coordinate trial to other levels.

## Next gate

After the third-player test, the next engineering gate is robust disconnect/reconnect slot reclamation, then collision-validated nearby spawn selection, all-map entry anchors, and regressions against PM01 and normal server mode.
