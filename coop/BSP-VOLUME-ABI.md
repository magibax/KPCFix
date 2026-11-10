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

## Live Windows named-model registry validation (2026-11-10)

**Passed the initial read-only resource traversal on R1M2B** using
`[Coop] TraceModelRegistry=1`. The observed log was:

```text
[KPCFix Co-op] model registry=TRAVERSED entries=0x00000073 roots=0x00000073 attached=0x0000006d
[KPCFix Co-op] model registry first root=0x03f36950 object=0x03d74e58
```

Decimal interpretation: **115** linked resource records successfully
visited, **115** readable non-null candidate BSP-root pointers and
**109** non-null associated-object pointers (six records with no
associated-object pointer). These counts are evidence of the original
1.13 registry's pointer-chain layout, **not** of 115 independently solid
WorldModels or 109 currently live collision objects. The pointers do
not establish transforms, object types, collision flags or liveness.

The previous base-BSP four-control differential repeated unchanged:
three `INTERIOR_ONLY` readings (27, 24 and 25 visits), and the known
wall control `TOUCHES_CLASS_ZERO` (17 visits). All four shared loaded
base-tree root `0x0330a020`. The overall gate remained three
`UNSUPPORTED` plus one `BLOCKED` as intended.

The shift in absolute root addresses between runs is expected for
process-local allocations and does not itself imply a geometry change.

### Next test contract

Static original-server instructions at `se_InitWorldModel` RVA `0x22958`
write `object+0x140 = resource_record` and
`model+0xdc = object`. Validate this **reciprocal association** for
all 109 candidate associations using guarded reads, and count absent,
unreadable and mismatched back-references separately. Read-only and
non-invasive; a reciprocal match corroborates the association but
still does not establish current solidity or a valid world-space
transform. After that, identify object collision and transform state
from original engine callsites before attempting any additional BSP
collision checks or enabling the allocator.

## Next controlled probe — resource/object reciprocal back-reference

From the original 1.13 `se_InitWorldModel` disassembly,
`0x22990` stores `object+0x140 = resource_record`, while
`0x22998` stores `model+0xdc = object`. An unverified,
non-null `model+0xdc` pointer is therefore an opportunity for an
additional **read-only reciprocal consistency check**, not proof of
liveness. The registry reader now checks each associated object's
`+0x140` DWORD with `VirtualQuery` before reading and reports:

- `model refs matched`: `object+0x140 == resource_record`.
- `mismatch`: readable object+0x140 but points to another record.
- `unreadable`: missing/unreadable back-reference memory.

The three counters must sum to `attached` from the same registry
snapshot when traversal succeeds. An unreadable back-reference
**does not** change registry `TRAVERSED` to `UNKNOWN`, because
those statuses describe different kinds of evidence. No game object
is written or called.

This new build reports
`[KPCFix Co-op] diagnostics build=registry-backrefs-v3` and a
`model refs matched=... mismatch=... unreadable=...` line. The
first R1M2B spawn invokes one bounded scan with
`[Coop] TraceModelRegistry=1` as before. Use one client,
`TrialSeparateSpawns=0`, and capture both lines. The previously
observed R1M2B counts (115 resources, 109 associations) are the
regression baseline; the reciprocal match count is intentionally
unpredicted until live-tested. **Never use these reference checks
to set a spawn-clearance verdict.**

## Live Windows reciprocal WorldModel validation — PASS (2026-11-10)

The subsequent `registry-backrefs-v3` opt-in R1M2B runtime produced:

```text
[KPCFix Co-op] model registry=TRAVERSED entries=0x00000073 roots=0x00000073 attached=0x0000006d
[KPCFix Co-op] model refs matched=0x0000006d mismatch=0x00000000 unreadable=0x00000000
[KPCFix Co-op] model registry first root=0x04006950 object=0x03e44e58
```

The 115 resource records and 115 readable candidate BSP roots are
consistent with the prior Windows run. **All 109 non-null object
associations have readable reciprocal `object+0x140` references
matching their original resource records.** No mismatches or unreadable
back-references were observed. Six resources still have no associated
object. The reported pointers are process-local; changed address
values are not by themselves a failure.

The base BSP still returns `INTERIOR_ONLY` for the three previously
navigable R1M2B trial boxes (27/24/25 nodes) and
`TOUCHES_CLASS_ZERO` for the wall-overlapping box (17 nodes).
The base BSP root in this run was `0x03057020`. The existing full
volume gate correctly remains **3 × `UNSUPPORTED`, 1 × `BLOCKED`**,
since active solidity and WorldModel coordinate transforms are
not yet established.

### Next required engine work

This confirmation upgrades the candidate associations to
**reciprocally consistent references**, not verified active solid
objects. Never infer collidability, collision flags or world-space
location merely from an object pointer.

Relevant additional original-1.13 static observations from
`server.original.dll` disassembly:
- At RVA `0x2299e..0x229b6`, the `se_InitWorldModel` initializer
  copies three DWORDs from `model+0x11c/0x120/0x124` to
  `object+0x40/0x44/0x48`. This verifies a value-transfer
  relationship but **not yet** its geometric semantics.
- The code at `0x22af1..0x22b33` accesses the object's
  `+0x13c` linked state and, conditionally for
  `object+0x76 == 1`, copies 60 bytes beginning at
  `object+0x14c` into a caller-provided buffer. The paired
  `0x22b65..0x22ba6` path writes values back into model-linked
  state and is therefore **not safe to invoke in a diagnostic**.
  Neither path is yet confirmed as a collision-transform API.

The next investigation is to identify the engine's **actual live
object list, collision flags and world-to-model transform** from
independent original call sites. Only once those contracts are
verified can a read-only, transformed multi-BSP hull check be
considered. Respawn and actor occupancy remain separate blockers.

## Static object type/flags survey — experimental original 1.13 milestone

Independent original-server disassembly verifies that the engine object's
constructor at **RVA 0x1d403** copies creation flags to
`object+0x28` (RVA 0x1d486), and the creation-object type
into byte `object+0x76` (RVA 0x1d509). The `se_InitWorldModel`
path at RVA 0x2299e copies the three DWORDs at
`model+0x11c..0x124` into `object+0x40..0x48` at creation.
The original physics spatial-tree visitor at RVA 0x19d0c
performs `test dword [object+0x28],0x12000`, then checks
more type, masks and callback conditions before AABB overlap.
This is evidence that these bits influence a collision-related
path, **not** that a particular bit alone means fully solid.

`coop_model_state.h` now adds a **bounded, pure read-only
collector** for every model/object association that passes its
existing reciprocal back-reference check. It uses the same guarded
reader as the resource registry, and never invokes a virtual method,
mutates the object or turns a classification into a `SAFE` spawn.

With `[Coop] TraceModelRegistry=1` and
`[Coop] TraceModelState=1`, the Windows tracer records:

- `model state=TRAVERSED/UNKNOWN matched=... skipped=...`:
  count of reciprocally verified associations inspected versus
  candidates excluded for broken/unreadable backlinks.
- `model types type1=... other=... unread=...`:
  This v4 diagnostic incorrectly labelled type one as the WorldModel path; the corrected v5 diagnostic below supersedes that classification.
- `model flags 2000=... 10000=... any12000=... unread=...`:
  raw object+0x28 mask test counts, **not a solid roster**.
  These counts overlap: `any12000` means either tested bit.
- `model vectors equal=... changed=... unread=...`:
  bit-for-bit comparison of the creation-time model vector with
  the currently stored object vector. A difference may indicate
  changed placement or something else; equality does **not**
  prove the model is stationary, unrotated or safe to collide.

The three outcome sets (types, flags-readable/unreadable,
vector-readable/unreadable) should each account for the number of
reciprocally matched references if the registry is `TRAVERSED`.
The earlier R1M2B baseline was 115 resource entries, 109/109
reciprocal matches and stable primary-BSP positive/negative controls.

The current blocker remains **collision semantics and true
world-to-local transforms**, including the distinction between
associated WorldModel resources and actively collidable instances.
The model's `+0x11c` triple is only a creation-time copy
relationship; do not use it to translate or rotate BSP queries.
No dynamic BSP sweep or automatic spawn has been enabled.

## Live 1.13 R1M2B type/flags/vector survey and correction (2026-11-10)

The user's opt-in `registry-object-state-v4` log demonstrated:
- The resource registry still traverses **115** resources, with **115**
  readable model-root pointers and **109/109** reciprocal object
  back-references; no unreadable fields.
- The old diagnostic reported `type1=0, other=109`.
  That did **not** mean no WorldModels were instantiated. A fresh
  check of the *original DLL* shows the object constructor at RVA
  `0x1d509` writes type to `object+0x76`, while independent
  WorldModel-specific engine paths at RVAs `0x18435` and
  `0x20719` explicitly branch on `type == 2`.
  **The original v4 type-1 label was incorrect.**
  The v5 collector now reports `type2`, `type1`, `other`
  and `unread` independently. The type-2 count must be measured
  live rather than inferred from the earlier `other` count.
- Raw object flag counts: `0x2000` set on 49 references,
  `0x10000` set on 18, either bit on 67, neither on 42.
  Because 49+18=67, no associated object in this snapshot had
  **both** tested bits. No single flag has yet been proven
  to imply active solidity or collision-blocking.
- Object's currently stored vector `+0x40..48` matched
  model `+0x11c..124` bit-for-bit for **107** references and
  differed for **2**. This is not, by itself, proof
  of actual motion, world transform or physics participation.
- Primary BSP controls passed again (three interior-only, one
  wall-crossing class-zero); overall volume verdict correctly
  remained three UNSUPPORTED and one BLOCKED.

The corrected v5 read-only snapshot additionally captures up to
**two** differing-vector examples (object and resource-record
pointers, raw object type and flags, and bit-exact three-component
model source and current object vectors). This is designed to
identify what actually differed without guessing motion or
applying an unverified transform. It adds no native engine calls,
does not alter objects, and does not approve any spawn.

The next objective remains verifying current solidity and the
world-to-model transformation matrix before checking additional
BSP roots. Type 2 alone and raw 0x12000 flags are not
complete proof of a current collidable object.

## Live Windows v5 — type-2 and vector-delta validation (2026-11-10)

The opt-in `model-types-v5` log fully traversed the R1M2B model
registry, repeating **115** entries, **115** readable candidate BSP
roots and **109/109** matching reciprocal object references. No
unreadable or skipped state was reported. Type values now decode:

| Object-type result | Count |
| --- | ---: |
| Native `object+0x76 == 2` | **57** |
| Native type 1 | **0** |
| Other readable types | **52** |
| Unreadable | **0** |

The corrected type-2 count verifies the old v4 type-1-only label
was misleading; the 52 remaining associations require an actual
type histogram, not an assumption about their nature.

The raw `object+0x28` flag distribution repeated exactly:
49 with `0x2000`, 18 with `0x10000`, 67 with
either (`0x12000`), 42 with neither; no overlap of the two
tested bits in this snapshot. These masks are **not verified
full-solidity flags**. The 109 object/source vectors repeated
107 equal, 2 different, with no unreadable values.

The two changed entries were **both type 2, with identical raw
flags `0x00204001`** (and neither `0x2000` nor
`0x10000`):

| | Source position (X, Y, Z) | Current object position (X, Y, Z) | Difference |
| --- | --- | --- | --- |
| Object `0x03be438c`, record `0x03f1b430` | (537, 852, -1445) | (501.377716, 895.350525, -1409.377808) | (-35.622284, +43.350525, +35.622192) |
| Object `0x03be4550`, record `0x03f1a240` | (504.5, 886, -1415.5) | (481.073486, 844.358276, -1392.073486) | (-23.426514, -41.641724, +23.426514) |

This is strong evidence of *changed stored model-object position
state* after the initial copy. The similar diagonally opposed X/Z
components are consistent with scripted repositioning or a
movable WorldModel, but a single timestamp does not establish
time-dependent motion or rotation. Do **not** use this to
calculate transformed BSP collision planes without independently
verified local/world matrix semantics.

The base-BSP volume differential remained identical: the three
navigable controls returned `INTERIOR_ONLY` with 27/24/25
visits, and the known wall control returned
`TOUCHES_CLASS_ZERO` in 17 visits (base root
`0x0399f020`). The overall evidence gate correctly remained
3× `UNSUPPORTED` and 1× `BLOCKED`.

### Additional static original-server findings

The original `se_InitWorldModel` at
RVA `0x229bf..0x22a57` derives candidate min/max bounds
from model fields `+0xe0..+0xf4` and passes these into
native routine RVA `0x19e06` for spatial-bound indexing.
Disassembly of `0x19e06` shows it **updates live
object-indexing bounds and spatial-tree structures**,
including writes at `object+0xc4..+0xd8`. It is
therefore *not* a side-effect-free collision query and
must **not** be invoked from experimental instrumentation.

The constructor at `0x1d403..0x1d509` also initializes
object fields `+0x4c..+0x64` from the creation record,
but the geometry/rotation semantics of those fields have
not yet been corroborated across independent transform
call sites. Copying them, or simply subtracting the
current object's position from BSP plane distances, would
not establish a sound collision transform.

**Next engineering target:** verify native type distribution
and the full collision-related flags/masks, establish
rotation and world/local transform layouts, and identify the
actual live-solid object set. Only then can the current
read-only BSP-box classifier be applied to additional
models without silently omitting obstacles.

## V6 follow-up: type histogram and raw geometry field samples

The passing v5 experiment established 57 type-2 links and 52 links
of as-yet-unclassified types. To avoid treating **all attached
resources as 57 distinct solid WorldModels**, the v6 read-only
collector now logs nonzero values in the full byte-type histogram
(`0..15`, with an overflow bucket for types `16..255`) and
cross-tabulates `0x12000` and `0x4000` masks for type-2
versus non-type-2 objects. Unknown type ranges remain unclassified.

For the two proven changed-vector cases it also records:
- raw DWORDs at `object+0x4c..0x58`; the object constructor
  copies these from creation data (`0x1d4bd..0x1d4d2`)
  but a rotation interpretation is **not yet independently
  verified**;
- raw source-model bounds at `model+0xe0..0xf4`; the original
  `se_InitWorldModel` initial-bound derivation references these
  fields (`0x229bf..0x229e3`) but does not prove they are in
  the same coordinate frame as the player AABB.

Each optional sample is individually validity-checked and
read-only. Invalid samples are tagged `valid=0` rather than
used. No transform is applied; no new engine method is invoked.
The original spatial-indexing procedure RVA `0x19e06`
continues to be forbidden for our diagnostic because it writes
live index state. Nothing changes the `world_hull`,
`other_actors`, or automatic-allocator evidence.

## R1M2B v6 result and linked-object box survey

The Windows run had 115 entries, 109 matched associations, 57 type 2 and 52 type 9. Of the type-2 entries, 49 had some bit in 0x12000 and 8 had neither. Of the type-9 entries, 18 had some bit in the mask and 34 had neither. The EU spatial visitor explicitly considers type 9 even when the mask is absent (RVAs 0x19d0c through 0x19d1f). Both differing position samples were type 2 with flags 0x00204001. The named GetObjectRotation path copies object offsets 0x4c through 0x58 to the result (0x28439 through 0x28451).

For a candidate's first comparison with linked geometry, v7 uses the same pair of bounds the native visitor supplies to its overlap comparator at 0x19d73 through 0x19d81. It reads the six values at object+0xac through +0xc0 for every reciprocal association and counts overlap, disjoint and unknown. An incomplete registry scan is logged separately. This broadphase can include false positives and excludes actors outside the registry. The result is observational and cannot authorize automatic placement.

V7 Windows R1M2B: all 109 linked boxes were readable and disjoint for all four candidates. The wall control was blocked by the main-world tests. The v8 diagnostic additionally compares the joining player's engine spatial box with each candidate. It traverses selector zero from the original spawn-preparation routine, counts self references, and compares other handles separately. A private bounded cursor preserves the game's iterator state. The list's coverage of other actors remains under investigation.
