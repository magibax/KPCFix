# General co-op spawn allocator — staged integration

Status: **policy implemented and unit-tested; physics adapter NOT yet verified; not connected to live game**.

The existing R1M2B three-player test was confirmed in-game to be stable, with all three players surviving and moving normally. That validates exactly three specific map coordinates; it does not imply that adjacent points, or points in other maps, are safe.

## What is now implemented

The platform-independent `server-tools/server-tools-source/src/coop_allocator.h` provides:

- A deterministic expanding X/Z candidate grid around a supplied original campaign PlayerStart.
- Distance-based exclusions from already-reserved spawn positions.
- A **required read-only engine collision probe** callback; the position is reserved only if the callback explicitly returns `COOP_PROBE_SAFE` and supplies a validated, potentially floor-snapped position.
- Explicit owner-slot reservation, stable idempotent repeat placement, release on confirmed player departure, bounded capacity, and full-world reset.
- Fail-closed behaviour: an unknown/unsupported probe, blocked candidate, invalid/out-of-range position, floor-snap overlap, or exhausted search does not return the occupied original PlayerStart.

`test_coop_allocator.c` exercises synthetic clear/blocked/unknown geometry, deterministic order, reconnect after an explicit release, and full capacity. This does **not** emulate the LithTech physics engine. Build under GitHub Actions and test locally using the documented GCC host build.

## Engine-specific integration still required

The legacy `IntersectSegment` line-query ABI has now been established by original object.lto static disassembly and an opt-in runtime diagnostic has been added (see [INTERSECT-ABI.md](INTERSECT-ABI.md)). **Do not mistake a segment hit for verified full-body clearance.**

**Do not connect a guessed server API or issue live placement based on these tests alone.** A safe callback must establish the exact official 1.13 server interface and calling convention, then verify:

1. World bounds and a floor surface beneath a proposed player-sized volume.
2. Sufficient 3D clearance for the actual current player collision dimensions (not only a ray or point).
3. Correct grounding/surface normal and rejection of void/blocked/hazardous locations.
4. Sufficient separation from already-reserved co-op positions and currently active players.
5. Handling of lifts, teleporters and scripted spawn-entry anchors; no synthetic location can be assumed safe for all maps.
6. A player-departure event with a verified owner identity, to call `coop_allocator_release`. The present R1M2B trial stores object pointers without an authoritative disconnect callback.
7. A new-world or restart event to reset reservations. The loaded object module and server process can persist across maps.

The original `object.lto` spawn routine at RVA `0x0f12a0` calls the nearby-object procedure at `0x0f10c0` (from `0x0f13a4`) **before** the engine position setter at `0x0f13c4`. `0x0f10c0` enumerates nearby objects and can cause damage/telefragging. **It is not a full geometry or floor validator.** There is a second spawn path at `0x0f15d0` with another call to `0x0f10c0` from `0x0f167a` and a different placement sequence. Both must eventually be handled before claiming universal respawn support. The asset-free `check_spawn_site.py` now hash-guards both original paths (16 byte/sequence signatures) before any experimental hook can be considered.

Some published LithTech SDKs expose `IntersectSegment` and `GetStandingOn`, but **those interfaces and virtual-table offsets cannot be copied into the year-2000 KPC build without disassembly/ABI verification**. A single ground ray also cannot prove the entire player body fits.

## Current live build

The shipped experimental three-player R1M2B DLL remains intentionally restricted to the three manually play-tested coordinates. It is **off by default**. The new generic policy is **not** wired into that DLL until the engine collision probe contract has been met.

## Player bounds recovered from the original 1.13 spawn paths

The default spawn overlap box extends from (−20, −46, −20) to (+20, +46, +20) relative to the spawn position. Both original spawn paths pass the same lower and upper vectors to the overlap routine. The following addresses are RVAs in the original object.lto identified by check_spawn_site.py.

| RVA | Observation |
| --- | --- |
| 0xEE750, 0xEE780 | Initializers write the lower and upper vectors respectively. |
| 0xF138E–0xF13A4, 0xF1669–0xF167A | Each caller supplies those vectors and the candidate position to 0xF10C0. |
| 0xF10D9–0xF111C | The overlap routine adds the position to both vectors component by component. |
| 0xF1150–0xF115C | A virtual call at slot +0x104 takes an existing object's handle and two output vector addresses. The following code adds those vectors to the object's position for overlap comparisons. |
| 0xF2A85–0xF2AD9 | During a player state change, virtual slot +0x108 receives the same lower vector and switches between the standing upper vector and (+20, +14, +20), initialized at 0xEE7B0. |

On R1M2B the traced spawn centre is at Y=−256. The default box bottom is therefore −302, **18 units above** the observed floor at −320. Preserve the measured spawn height as a baseline when testing floor snapping. The box size alone cannot establish whether world geometry intrudes into it.

For a live dimensions trace, observe the original +0x104 call's outputs on return and correlate the queried handle with a known player. Its receiver is the server interface instance (ECX), with dispatch through its vtable. The direct +0x90 segment API uses a different calling convention. Slot +0x108 changes object bounds and should stay out of a read-only probe. A separate world-geometry clearance query still needs verification against a deliberately obstructed candidate.

## Live R1M2B wall control (2026-11-10)

The Windows run of diagnostic 291ecb4 returned player-relative bounds (−20, −46, −20) to (+20, +46, +20). At X=1600, Y=−256, the three candidate Z coordinates −2944, −2848 and −3040 each produced a floor hit at Y=−320 with normal (0, 1, 0) and no sampled body hit.

The positive-X ray reached a wall at X=1664. At the control centre (1654, −256, −2944), the measured box extended to X=1674, ten units through the wall plane. The floor query still hit the same level surface. The body sample reported an obstruction.

Two distinct player handles were placed at the original anchor. The diagnostic ran once and queried hypothetical candidates. Player-to-player clearance and full world-volume clearance remain unverified. The next engine investigation is a read-only box-overlap query, using these four positions as regression controls before enabling automatic placement.

## Recommended next runtime instrumentation

1. Use the [verified R1M2B segment-query results](INTERSECT-ABI.md) as the ground reference. Trace a player-volume clearance method and verify its calling convention against original call sites.
2. Add a strictly read-only diagnostic pass to log candidate acceptance/rejection, initially on R1M2B against the three known good positions and at least one deliberately blocked location.
3. Only after that diagnostic has passed independently, replace the current three fixed positions with `coop_allocator_assign` using the verified engine probe.
4. Integrate authoritative disconnect/map-reset notifications, then regression-test ordinary deathmatch and the untouched server mode.

## Read-only player-volume evidence gate

`server-tools/server-tools-source/src/coop_volume_probe.h` defines a fail-closed evidence policy, exercised by `test_coop_volume_probe.c` and the CI-built Windows diagnostic. With `TracePlayerClearance=1`, the server logs `[KPCFix Co-op] volume gate=...` for three known navigable R1M2B positions and an obstructed wall control. **No position is changed or approved for automatic spawning.**

| Inputs | Result |
| --- | --- |
| Positive body ray hit or verified native overlap | `BLOCKED` |
| Ground hit and ray misses, but native hull/active-actor results unknown | `UNSUPPORTED` |
| Valid ground/bounds plus *independently verified native full-volume AND active-actor clearance* | `SAFE` (synthetic policy test only, not available in this game adapter) |

Currently the native hull and actor fields remain explicitly UNKNOWN. The three previously live-tested positions should therefore log `UNSUPPORTED`, not `SAFE`; the wall-crossing control should log `BLOCKED`. Floor-gap and slope constraints are provisional (32 world units, normal Y 0.70) and do not prove absence of hazards or scripted conflicts.

**Next research:** Use matching original 1.13 binaries to find a *read-only* box/hull world intersection or sweep. Establish calling convention, ECX receiver, return layout, flags and two original callsites before invoking any newly identified API. Test positive controls (wall at X=1664 and a blocker fully enclosed in the body that the sampling rays miss), three open controls, and active-player overlap. The proven `IntersectSegment` +0x90 API performs line tests; +0x104 returns object bounds; the nearby-object routine at RVA 0xf10c0 handles telefrag contacts, not general world-hull clearance. The general allocator remains disconnected.

## Windows live test: volume evidence gate (2026-11-10)

The user supplied the complete progression of Windows logs from the fixed three-player
spawn trial, the original all-miss segment ABI diagnostic, the corrected 0x6003
ground-ray differential, the player-bounds/wall-control diagnostic, and the
current `91c58dc` read-only evidence-gate build. **These are separate server runs,
not a single simultaneous configuration.**

In the final run, the server reported its runtime hooks installed without
changing the original object file. The `TracePlayerClearance` diagnostic
queried these four positions:

| Position (X,Y,Z) | Ground | Sampled body | Volume verdict |
| --- | --- | --- | --- |
| (1600, -256, -2944) | Y=-320, normal Y=1 | No sampled hit | `UNSUPPORTED` |
| (1600, -256, -2848) | Y=-320, normal Y=1 | No sampled hit | `UNSUPPORTED` |
| (1600, -256, -3040) | Y=-320, normal Y=1 | No sampled hit | `UNSUPPORTED` |
| (1654, -256, -2944), wall at X=1664 | Y=-320, normal Y=1 | Obstructed | `BLOCKED` |

This matches all four predicted classifications. It validates the *read-only
evidence gate's response to ray observations* in the original Windows game,
but **does not establish full player-volume clearance**, validate actual
player-versus-player overlap, or demonstrate a native box/hull sweep.
The three `UNSUPPORTED` results are intentional and must not be renamed
`SAFE` based on successful floor rays or earlier three-player movement.

The final trace includes one ordinary spawn-position setter event at the
original campaign anchor. It provides no evidence for disconnect/reconnect,
map reset, PM01 deathmatch or altered engine movement. The separate older
trace with three trial slots remains the evidence for the fixed-position
three-client test only.

### Focused static-research follow-up

Independent read-only disassembly of the same SHA-verified original
`object.lto` confirms that the known nearby-object handler at RVA
`0x0f10c0` invokes the game's object-list enumerator at RVA `0x07d220`,
reads object position through the engine interface at `+0x1d0`,
reads relative object bounds through its virtual slot `+0x104`, and
tests AABB overlap before calling a target object's `+0x50` virtual
method. The latter path appears capable of touch/telefrag side effects,
so it is **not** a safe read-only world-hull probe.

The matching original `server.original.dll` contains identifiable
`PhysicsLT::GetStandingOn` and `ServerDE::GetWorldBox` diagnostic strings.
Their presence alone says nothing about the required box-sweep ABI:
`GetStandingOn` describes existing standing state and `GetWorldBox`
may expose world extents, neither of which proves that an arbitrary
candidate hull is free of solid geometry. Do not call a guessed interface
offset based on these names.

**Next gate:** find the original full-volume world-geometry collision
routine, establish its call/return ABI and absence of mutations, then run
a separate opt-in differential test on the four positions above **and**
an interior obstacle missed by the current ray samples. Keep both native
world-hull and actor-clearance fields UNKNOWN in the current runtime.

### Original server.dll API candidates eliminated by disassembly

The verified original `server.original.dll` (SHA-256
`83b7bce809948b6f70ed3f35a77a771b6087f1104d90d06a62ac3dc0d4bd37d5`)
has two informative routines, but neither is the required world-volume query:

- **RVA `0x2477b`: `ServerDE::GetWorldBox`**. The success path copies
  three floats from world-data offsets `+0xf8` and `+0x104` into two
  caller output buffers and returns zero; it performs no collision
  traversal. This exposes stored world-box endpoints, not occupancy
  of an arbitrary player AABB.
- **RVA `0x104fb`: `PhysicsLT::GetStandingOn`**. The routine requires
  an existing object and output record, reads that object's standing
  relationship at `+0xa0`/`+0xa4`, and writes current standing
  information to the caller. It does not take an arbitrary candidate
  position and cannot establish candidate world clearance.

These RVAs are relative to the original PE base `0x10000000`. The
identifications are grounded in the original binary's corresponding
diagnostic strings and the disassembled function bodies. They are
**static findings only**, not runtime-approved calling conventions for
a new hook. No new game-engine function has been invoked in this pass.

## Original native AABB comparator: static milestone

The original 1.13 server DLL contains a small read-only 3D AABB-overlap helper at RVA `0x18991`. Three original callsites independently verify its five 32-bit arguments and 20-byte caller stack cleanup. The exact function/body digests and test procedure are documented in [BOX-OVERLAP-ABI.md](BOX-OVERLAP-ABI.md). An isolated C implementation and regression tests detect both the known wall-control overlap (when supplied a wall obstacle box) and a fully enclosed obstacle missed by sparse rays.

**Do not connect this box broadphase to live spawn approval.** It has neither authoritative BSP polygon coverage nor a complete list of live solid actors. The native `world_hull` and `other_actors` evidence inputs remain UNKNOWN, and the next task is the separate world-geometry collision traversal and active-player enumeration ABIs.
