# BSP world-volume classification: original server engine

Status: **original native BSP point walker and conservative whole-AABB traversal verified; live R1M2B primary-BSP differential passed all four controls on 2026-11-10**. Additional world models and dynamic actors are unverified. No spawn allocator integration.

## Original 1.13 evidence (binary-specific)

The user-supplied official `server.original.dll` SHA-256 is `83b7bce809948b6f70ed3f35a77a771b6087f1104d90d06a62ac3dc0d4bd37d5`. A short 75-byte read-only routine at **RVA 0x1068f** (SHA-256 `8f9f9e17e82c130e85704585e66c8fff62b7de782e0c79d8eb29419eba64b35f`) implements point-to-BSP-leaf classification. It uses two stack pointers, returns 0/1 in EAX, and has no stores to engine state. Five original relative calls: `0x24426`, `0x28fdc`, `0x2e2f`, `0x2e3d`, `0x2e4a`.

At each native BSP node:

- `+0x00`: node flag bits; bit `0x2` returns class zero, bit `0x1` returns class one, with bit 0x2 checked first.
- `+0x08`: pointer to plane (normal X/Y/Z at +0/+4/+8 and distance at +0x0c).
- `+0x2c`: signed-distance ≤0 branch; `+0x30`: signed-distance >0 branch. Original branch threshold at `0x411f0` is float zero.
- `GetPointStatus` wraps this routine at RVA `0x2440a`, using the world root accessed via server world-data and mapping the walker class-one result to status 0x40, class zero to 0x41. Its vtable starts at RVA `0x416f4`, with the method at offset +0x1c.
- Root pointer provenance from the original wrapper: server manager +0x19c → world structure +0x08 → root +0x44. This is **static** layout evidence, not a verified arbitrary runtime pointer access contract.

The game binary also exposes a separate routine at `0x3145` which superficially resembles a box test because its server wrapper (`0x24388`) supplies two vectors. However it computes **the difference between those vectors**, normalizes that direction, then calls the segment-tree routine `0x30b1`. It is another **line** query; it cannot certify player-volume clearance. Do not call it a box collider.

## Portable conservative full-box traversal

`server-tools/server-tools-source/src/coop_bsp_box.h` walks a **decoded BSP** through a bounded read callback. For every partition plane it calculates the minimum and maximum signed distances over the *entire AABB* (using the normal's sign to select support vertices). If the box straddles or merely touches a plane, it visits **both children**. A class-zero leaf anywhere in the visited partition set yields `COOP_BSP_TOUCHES_CLASS_ZERO`; all visited leaves class one yield `COOP_BSP_INTERIOR_ONLY`. Unreadable nodes, impossible fields, cycles, overflow and search exhaustion produce `COOP_BSP_UNKNOWN`.

This is a conservative topology classifier, not a full physics adapter. The per-node callback is intentionally separate from reading the game's raw pointers, which must be independently validated before calling it in a Windows server. A tree may have other world models, moving platforms, dynamic objects, and triggers not represented by its base BSP. Even `INTERIOR_ONLY` **must not** be promoted to `COOP_VOLUME_VERIFIED_CLEAR` or used for spawn selection.

The synthetic tests include a *thin excluded slab completely enclosed within the player box*, whose eight AABB corners would miss the obstruction. No corner-ray or sampled-line method can substitute for traversing every BSP partition intersecting the box.

## Reproduction

```sh
python coop/tools/check_server_bsp_site.py /path/to/server.original.dll
python -m unittest discover -s coop/tools -p 'test_*.py' -v
gcc -std=c11 -Wall -Wextra -Werror -I server-tools/server-tools-source/src \
    server-tools/server-tools-source/tests/test_coop_bsp_box.c -o /tmp/kpc-coop-bsp-test
/tmp/kpc-coop-bsp-test
```

The hash checker never executes/patches or redistributes original game code; its unit tests use synthetic PE fixtures. A subsequent opt-in Windows runtime tracer reads the original loaded BSP without changing player transforms or spawn allocation.

## Next runtime gate

1. Confirm original 1.13 live server world-model root provenance without relying on later LithTech SDK layouts. Verify all pointers and callback constraints.
2. Run **read-only** full-box BSP traversal at three known navigable R1M2B positions and wall-control (1654, -256, -2944), with a strict pointer/visit budget. Report `INTERIOR_ONLY`, `TOUCHES_CLASS_ZERO` or `UNKNOWN`; these labels are **not** a gameplay SAFE verdict.
3. Cross-check native `GetPointStatus` class mapping and whether the known wall is in the *primary BSP* versus a separate world model. Test an enclosed BSP obstruction if a safe, grounded control is available.
4. Establish coverage of *all* solid WorldModels and active players and relevant collision flags before setting either `world_hull` or `other_actors` evidence to verified clear.

## Opt-in Windows loaded-world differential (experimental)

Commit after the static synthetic tests adds `[Coop] TraceBspVolume=1`
(default off). It uses the exact **supported original 1.13 server DLL SHA-256**
as a compatibility gate. In the loaded, original engine, the tracer follows
the same root chain used by `GetPointStatus`:

```text
server.original.dll + 0x4a250 (live server manager pointer)
  -> manager + 0x19c (world state)
  -> state + 0x08 (base world BSP)
  -> tree + 0x44 (root BSP node)
```

The memory reader validates **every** node and plane via `VirtualQuery`,
rejects non-committed, guarded, unreadable or short regions, and caps
visit count, traversal depth and explicit stack. It never changes
engine state, tests no unknown native method offset, and only runs in
the existing first-anchor R1M2B clearance trace. A failed pointer
chain reports `UNKNOWN`; it does not crash intentionally, retry
forever or approve a candidate.

Each candidate logs `[KPCFix Co-op] base BSP box=INTERIOR_ONLY`,
`TOUCHES_CLASS_ZERO` or `UNKNOWN` plus visited count and native
root pointer. **Neither INTERIOR_ONLY nor a clear ray is a spawn-safe
result.** Neither `world_hull` nor `other_actors` is promoted to
VERIFIED_CLEAR. The main campaign map may include additional solid
WorldModel BSPs, doors, lifts and live players outside this base-tree
query. This is a *diagnostic* to establish whether the conservative
box traversal agrees with the original map's geometry at the known
open/control coordinates.

### Windows live-test instructions

Back up `server.dll`, `KPCServerTools.dll`, `SERVER_TOOLS_BUILD.json`
and `KPCServerTools.ini`; stop the dedicated server before replacing.
Install the exact DLL and manifest from this branch's CI artifact using
the existing `Setup-KPCServerTools.cmd`. Configure:

```ini
[ServerTools]
Enabled=1
AllowCheats=0

[Coop]
TrialSeparateSpawns=0
TraceCollisionABI=0
TracePlayerClearance=1
TraceBspVolume=1
```

Run `worlds\\realm1\\R1M2B` with `maxclients=2`, connect
**only one client**, and paste the `candidate`, `body sample`,
`base BSP box` and `volume gate` lines. The existing three
`UNSUPPORTED` and one `BLOCKED` evidence-gate results are expected;
the base-BSP classifications were later observed on Windows as described below. If all report `UNKNOWN` in another build, inspect the root
diagnostic and revisit the native pointer-chain inference. Stop and
restore the backups if anything crashes or misbehaves. Disable the
switch and restart to remove diagnostic traversal.

## Windows validation — four R1M2B controls (2026-11-10)

**Result: all four predicted control distinctions passed** using the opt-in
`TraceBspVolume=1` experimental build. The user supplied these live
dedicated-server log results after the prior synthetic tests and CI build.

| Candidate centre (X, Y, Z) | Base BSP classification | Nodes visited | Sampled body hit | Overall volume gate |
| --- | --- | ---: | --- | --- |
| (1600, -256, -2944) original anchor | `INTERIOR_ONLY` | 27 (`0x1b`) | No | `UNSUPPORTED` |
| (1600, -256, -2848) | `INTERIOR_ONLY` | 24 (`0x18`) | No | `UNSUPPORTED` |
| (1600, -256, -3040) | `INTERIOR_ONLY` | 25 (`0x19`) | No | `UNSUPPORTED` |
| (1654, -256, -2944) wall-crossing control | `TOUCHES_CLASS_ZERO` | 17 (`0x11`) | Yes | `BLOCKED` |

All four traversals used the same loaded primary-world BSP root pointer
`0x03a0c020`. Player-relative bounds were
(-20, -46, -20) to (+20, +46, +20).
Each candidate independently yielded a floor hit at Y=-320 with normal
(0, 1, 0). The wall reference was at (1664, -256, -2944).
The wall-control box spans X=1634..1674, crossing the wall plane.
The server logged successful runtime hook installation with the original
object file unchanged; the submitted trace contains no crashes.

**Interpretation:** On this specific official 1.13 `R1M2B` map, whole-AABB
traversal through the loaded **primary BSP** differentiates all three
previously navigable spawn volumes from the known obstructed wall control.
This is strong runtime evidence that the control wall obstruction is
represented by the same primary BSP leaf partition, though the log does not
identify the exact blocking polygon or prove arbitrary-world coverage.
Importantly, the `TOUCHES_CLASS_ZERO` result is obtained for the
full 3D player box, not solely a ground or body-ray sample.

**Boundary of proof:** The classification is conservative and applies
to one map, one player-box dimension set and four controls. A result of
`INTERIOR_ONLY` does **not** certify a usable spawn across all solid
WorldModels, destructible/moving geometry, actor bodies, hazardous surfaces,
trigger semantics or temporal changes. These are not in the primary-tree
query. Consequently the overall evidence gate correctly remains
`UNSUPPORTED` on all three clear base-BSP controls and `BLOCKED` at
the obstructed control. Do not set `world_hull` or `other_actors` to
`VERIFIED_CLEAR` or enable `coop_allocator_assign`.

### Next validation required

1. Inventory all solid WorldModels and determine their BSP roots,
   transformations, collision flags and live lifecycle. Establish whether
   a read-only traversal of every relevant model can be done safely.
2. Independently verify live-actor overlap, including self-filtering
   and authoritative active-player identity.
3. Differential-test against a wall/door belonging to a separate
   WorldModel, moving-world geometry and additional campaign maps.
4. Only after comprehensive coverage, integrate complete physical
   clearance into the fail-closed spawn probe; hazard/script checks and
   disconnect/map-reset lifecycle remain additional gates.

## Next research: named WorldModel resource list (opt-in read-only survey)

The original 1.13 server DLL `se_InitWorldModel` path at RVA
`0x22958` looks up the model name at RVA `0x22978`, calling a
read-only `cdecl` name lookup at RVA `0x08f69` with two arguments.
The caller uses the server manager's `+0x19c` world-state pointer
and its list sentinel at `state + 0xb0`. The lookup iterates
`sentinel+4` as the first link, `node+4` as next, and
`node+8` as a resource record. The record's first DWORD is
the model-data pointer; the lookup compares the model name at
`model+0x70`. On successful object initialisation,
`se_InitWorldModel` stores the resource record at
`object+0x140`, associates `model+0xdc` with the object,
and copies `model+0x11c..0x124` into `object+0x40..0x48`.

The new `coop_model_registry.h` uses only a user-supplied, read-only
32-bit-word callback. Its native adapter checks memory readability
before each pointer read. It caps traversal at 256 entries and treats
bad links, unreadable model pointers, a cycle or overflow as
`UNKNOWN`. Unit tests cover two records, an empty list,
invalid pointers, a loop, a missing root, and partial read failure.

The log prefix is `[KPCFix Co-op] model registry=` followed by
`TRAVERSED` or `UNKNOWN`, `entries`, `roots`, and `attached`
candidate counts, plus a second line with the first plausible root.
**TRAVERSED certifies only that the registry's linked pointers were
read within the budget; it is never a claim of complete solid-object
enumeration**. The `root` field at model+0x44 is a *candidate*
pointer based on primary-model layout. An `attached` pointer at
model+0xdc is a *candidate* object association, not authoritative
liveness. Nothing in this pass applies a model transform or BSP
collision query. No spawn verdict is changed.

### Windows test

Back up the same original server files as for the prior BSP
diagnostic, install the branch CI's test artifact, use
`TraceBspVolume=1` and `TraceModelRegistry=1`, with
`TrialSeparateSpawns=0`. Run R1M2B with one client, and provide
`model registry=`, `model registry first root=` and
`base BSP box=` lines. The four existing BSP controls should
remain three `INTERIOR_ONLY`, one `TOUCHES_CLASS_ZERO`.
If registry returns `UNKNOWN`, the native model-list assumption
requires re-investigation; do **not** enable automatic spawning.

**Next after live confirmation:** prove the model transform/lifecycle
and object flags, then enumerate the authoritative list of solid live
WorldModels; only then add a read-only multi-BSP hull differential.

## Diagnosing a missing registry log (registry-status-v2)

A Windows test of the first WorldModel registry build returned all four
expected base-BSP classifications but **no `model registry=` line**.
That means the registry read was not invoked, rather than that an
empty registry was successfully traversed. The first survey build
silently skipped its call when `model_registry_enabled=0`.

The revised experimental build emits:

```text
[KPCFix Co-op] diagnostics build=registry-status-v2 (reads KPCServerTools.ini)
[KPCFix Co-op] effective TraceModelRegistry=1
```

If enabled, one `model registry=TRAVERSED` or
`model registry=UNKNOWN` line follows the bounds observation.
If disabled, the build instead emits `effective TraceModelRegistry=0`
at startup and `model registry=DISABLED` after the bounds. If **neither
new status line appears**, the wrong/old proxy DLL may be in use.
Verify the actual `KPCServerTools.ini` beside the active
`server.dll` (not only `KPCServerTools-default.ini`), stop the
dedicated process completely and rerun `Setup-KPCServerTools.cmd`
after replacing the new DLL and matching manifest. The known BSP
classification lines alone cannot distinguish new from old builds.
