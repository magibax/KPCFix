# Native 1.13 server: read-only 3D AABB overlap helper

**Stage: static function/ABI checked; not wired into gameplay.** AABB-vs-AABB is only a box broadphase. It cannot identify walls or solid BSP polygons within a world-model bounding box, and a clear result cannot approve a campaign spawn.

## Verified original engine evidence

Original `server.original.dll` SHA-256: `83b7bce809948b6f70ed3f35a77a771b6087f1104d90d06a62ac3dc0d4bd37d5`. Its x86 function at RVA `0x18991` is 114 bytes and has SHA-256 `1e3021c4b29680f9f06f43424b61d937f32aa33b7749c32c62abc0472588fa1f`. An independent disassembly of this function shows six axis-bound comparisons, a 0/1 result and no indirect calls or object/engine writes. The original entrypoint is **read-only for its supplied arrays**.

At callsite RVAs `0x186b9`, `0x1953f` and `0x1a915`, five pushed 32-bit arguments are followed by a relative call to `0x18991`, `add esp,0x14`, and `test eax,eax`. That supports a five-argument, caller-cleaned `cdecl` convention and Boolean EAX result. The inferred signature, for static research only, is:

```c
int __cdecl overlap_aabb(const float a_min[3], const float a_max[3],
                         const float b_min[3], const float b_max[3],
                         float tolerance);
```

At zero tolerance the boxes overlap only when `a_min < b_max` and `a_max > b_min` on every axis. **A positive tolerance makes it easier to miss a shallow overlap; it is not a requested clearance margin.** Zero tolerance is appropriate for conservative spawn occupancy rejection. Face touching remains `DISJOINT` under the original helper's strict comparison, so additional separation remains necessary for spawn safety.

## Reproduction

On a locally owned original DLL (never included in this repository):

```sh
python coop/tools/check_server_aabb_site.py /path/to/server.original.dll
python -m unittest discover -s coop/tools -p 'test_*.py' -v
gcc -std=c11 -Wall -Wextra -Werror -I server-tools/server-tools-source/src \
  server-tools/server-tools-source/tests/test_coop_aabb_overlap.c -o /tmp/kpc-aabb-test
/tmp/kpc-aabb-test
```

The checker verifies the **complete original DLL hash**, function digest and three independently identified callsite/stack signatures. It never executes or modifies the DLL. Python mutation fixtures are synthetic and asset-free. The portable C AABB contract tests the 40 × 92 × 40 player box, 96-unit Z-separated players, touching faces, invalid fields, the X=1664 wall-control *represented as an obstacle box*, and an **obstacle entirely inside the player hull** that sparse ray samples would miss.

## Hard limit

The portable AABB comparator only checks **boxes supplied to it**; no native BSP geometry traversal, complete world collision data, current actor enumeration, or owner exclusion has yet been validated. `ServerDE::GetWorldBox` is a world envelope, not a set of wall obstacles. A `DISJOINT` result is not `COOP_VOLUME_VERIFIED_CLEAR` unless a separately proven complete obstruction inventory and native world-hull query exist.

Both native world-hull and active-actor fields **remain `UNKNOWN`**, and `coop_allocator_assign` remains disconnected from the runtime DLL. The original nearby-object handler at RVA `0x0f10c0` has touch/telefrag side effects and is unsuitable for a read-only adapter.

**Next milestone:** find and verify a query-only native world-BSP box/hull collision method, including its calling convention, side effects and control cases. Then independently verify live actor enumeration/box filtering before setting either full-clear field.
