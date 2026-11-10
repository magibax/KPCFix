# BSP world-volume classification: original server engine

Status: **original native BSP point walker verified by disassembly; conservative AABB traversal implemented and unit-tested on synthetic trees; real map volumes NOT yet live-validated**. No spawn allocator integration.

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

The hash checker never executes/patches or redistributes original game code; its unit tests use synthetic PE fixtures. Neither the native runtime nor current R1M2B fixed-position trial has been modified by this stage.

## Next runtime gate

1. Confirm original 1.13 live server world-model root provenance without relying on later LithTech SDK layouts. Verify all pointers and callback constraints.
2. Run **read-only** full-box BSP traversal at three known navigable R1M2B positions and wall-control (1654, -256, -2944), with a strict pointer/visit budget. Report `INTERIOR_ONLY`, `TOUCHES_CLASS_ZERO` or `UNKNOWN`; these labels are **not** a gameplay SAFE verdict.
3. Cross-check native `GetPointStatus` class mapping and whether the known wall is in the *primary BSP* versus a separate world model. Test an enclosed BSP obstruction if a safe, grounded control is available.
4. Establish coverage of *all* solid WorldModels and active players and relevant collision flags before setting either `world_hull` or `other_actors` evidence to verified clear.
