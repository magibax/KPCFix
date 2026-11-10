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

## Recommended next runtime instrumentation

1. Trace the server's existing collision/ground-query calls using the official supplied `server.original.dll` and `object.lto` and verify the method offset/signature against working original call sites.
2. Add a strictly read-only diagnostic pass to log candidate acceptance/rejection, initially on R1M2B against the three known good positions and at least one deliberately blocked location.
3. Only after that diagnostic has passed independently, replace the current three fixed positions with `coop_allocator_assign` using the verified engine probe.
4. Integrate authoritative disconnect/map-reset notifications, then regression-test ordinary deathmatch and the untouched server mode.
