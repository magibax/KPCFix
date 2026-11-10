# KPC 1.13 LithTech IntersectSegment ABI

**Scope:** original, user-provided official 1.13 object.lto, SHA-256
`3a03f8796c482b1a7ad72755df33ee8f1f8056bb10957f6f700e64bd006b3e5b`.

**Status:** original calling convention is statically supported by independent callsites. The first wrapper build ran on Windows with three `hit=0` results, but produced no positive-hit evidence. Further disassembly identified a 16-bit query field at +0x30 that our initial wrapper omitted; a differential test now compares both forms. The info layout beyond fields actually observed remains tentative. This API is a *segment intersection*, not a player-size sweep or full collision validation.

No original commercial binary or assets are committed.

## Original call sites

- At RVA **0x0F2FAE** (`ff 90 90 00 00 00`), the game calls **the pointer located at offset +0x90 in the server function-pointer table** that is itself loaded from RVA `0x16de80`.
- At RVA `0x0F2FB4`, `add esp,8` proves two 32-bit stack pointer parameters are cleaned by the *caller*.
- `cmp al,1` at `0x0F2FB7` proves the boolean result is read through `AL`.
- An independent original call at RVA `0x00EBC7` repeats the same +0x90 call and stack cleanup.
- The interface function pointer is **not a virtual C++ method slot**; no implicit `this` argument is passed by either caller.
- Consequently, the tested function type is `uint8_t __cdecl IntersectSegment(const KpcIntersectQuery*,KpcIntersectInfo*)` on 32-bit Windows.

These source addresses are RVAs with the PE preferred image base `0x10000000`.

## Original vintage query ABI

The game allocates an `IntersectQuery`-like structure on stack at `esp+0xac` and initializes it with original helper RVA `0x03A60`, then passes its pointer to the API.

| Offset | Width | Original use |
| --- | --- | --- |
| 0x00 | 12 bytes | From: three 32-bit floats |
| 0x0C | 12 bytes | To: three 32-bit floats |
| 0x18 | 12 bytes | Direction vector; relevant to a CastRay-style call, not normally IntersectSegment |
| 0x24 | 4 bytes | Flags; original caller sets 5 at RVA `0xF2F5F` |
| 0x28 | 4 bytes | Object filter function pointer; caller writes RVA `0x102730` at `0xF2F6A` |
| 0x2C | 4 bytes | Filter callback user-data object; caller writes ESI at `0xF2F75` |
| **0x30** | **2 bytes** | **Additional query field; caller writes `0x6003` at RVA `0xF2F7C`. Meaning unverified. Initial diagnostic left it zero.** |

The original callback at RVA `0x102730` is a `__cdecl` two-argument function. It rejects a null hit object and otherwise returns 1 for objects unequal to user-data. The diagnostic reuses this exact original callback instead of introducing a new filter ABI.

Flags 5 correspond to `INTERSECT_OBJECTS` (=1) and `INTERSECT_HPOLY` (=4); the same symbolic names exist in public LithTech SDK headers, **but their ABI structure is from a later engine and not byte-compatible with this KPC version**. The 1.13 original caller uses `+0x2C` as user data. Later SDK structures have additional function fields in this area; do not copy their layout uncritically.

The compiler-facing C definition, with explicit safety padding beyond the **confirmed first 50 bytes**, is `server-tools/server-tools-source/src/coop_intersect_abi.h`. The extra zeroed padding is *not claimed as the actual full engine structure size*. The prior claim that only 48 bytes of the input footprint were established was incomplete.

## Original result information

The original caller supplies a second local result record at `esp+0x78`. At return it reads its hit point at offsets 0x00/0x04/0x08, plus several plane/result fields. Tentative result layout:

- +0x00: hit point, x/y/z (12 bytes);
- +0x0C: hit plane normal, x/y/z (12 bytes);
- +0x18: plane distance (float);
- +0x1C: HOBJECT of intersected object (32-bit handle);
- +0x20: HPOLY;
- +0x24: surface flags;
- +0x28: model node.

This layout fits the vintage LithTech `IntersectInfo`/plane format, but **the exact fields after the hit point and plane require Windows runtime confirmation before making gameplay decisions from them.** A segment hit/no-hit result alone is not a proof of physical fitness of a spawning character.

## Proof gates already completed

- Confirmed original object file SHA-256 and eight additional byte signatures (two independent interface calls, caller cleanup, initializer, filter callback, and flags/filter/userdata writes). `coop/tools/check_spawn_site.py` now covers **25** total signature guards and rejects modified/mismatched originals.
- Added `coop_intersect_abi.h` with compile-time field offsets and padding checks.
- Native test `tests/test_coop_intersect_abi.c` is built with GCC -Werror; CI also runs spawn-site signatures, generic allocator policy, and the full Server Tools DLL build.
- The original tested three-player fixed-spawn hook remains unchanged. The general allocator still **does not** call these diagnostics to approve candidate positions.

## Live result: three zero-intersection returns

On 2026-11-10 the user tested the initial collision diagnostic at three previously confirmed navigable R1M2B spawn points. Each returned `[KPCFix Co-op] ABI segment hit=0` with no crash reported. All three rays used flags=5, the original object-filter callback, start Y+64 and end Y−256. **This does not establish missing floors or an unusable spawn.**

Further disassembly exposed a concrete difference in the original engine caller: at RVA `0xF2F7C` the original writes `WORD 0x6003` into its query structure at offset **+0x30**. The old diagnostic zeroed this field. The original write is guarded by exact bytes `66c78424dc0000000360` and synthetic tests. Another original callsite (`0xEBC7`) also uses this field within its call-site structure.

The semantic name of this 16-bit field is not yet established. In particular, no claim is made that `0x6003` is definitively a floor/world collision mask. We need direct differential evidence before treating it as a necessary setting.

## Four-mode runtime diagnostic (read-only)

`[Coop] TraceCollisionABI=1` now performs four segment queries per known spawn position, with a maximum of three observed positions (12 total calls).

| Mode | Query flags | Word +0x30 | Vertical ray extent | Purpose |
| --- | --- | --- | --- | --- |
| 0 | 5 | `0` | Y+64 to Y−256 | Reproduce previous all-zero-field control |
| 1 | 5 | `0x6003` | Y+64 to Y−256 | Change only the newly discovered query field |
| 2 | 4 | `0x6003` | Y+64 to Y−256 | Compare geometry-only flags |
| 3 | 5 | `0x6003` | Y+192 to Y−2048 | Test whether the shorter segment misses the floor |

The diagnostic is called **after** the original placement setter and only when the original selected coordinates match one of the three already verified R1M2B test positions. It neither changes coordinates nor disables collisions.

Example output:

```text
[KPCFix Co-op] ray m=0 flags=5 ext=0x00000000 z=0xc5380000 hit=0
[KPCFix Co-op] ray m=1 flags=5 ext=0x00006003 z=0xc5380000 hit=1
[KPCFix Co-op] ray+ m=1 y=0x... ny=0x... obj=0x... surf=0x...
```

The example is illustrative and **not an observed hit**. Result fields beyond the hit point remain tentative until a positive-hit run. If the server misbehaves, disable `TraceCollisionABI`, restart, and restore the previous DLL if needed. Do not use any mode as automatic spawn approval; a raycast cannot prove character body clearance.

## Next ABI research — player-volume clearance

`IntersectSegment` supplies a **line** hit; the general allocator's `CoopProbe` contract requires **player-sized clearance**, valid world geometry, and grounding. Next disassemble the original engine's world-model queries, player hull physics and `GetStandingOn`/object dimensions or collision-sweep APIs, verify their method offsets and call conventions separately, and then implement a tested read-only 3D volume probe. Also trace authoritative disconnect/map lifecycle before connecting the general allocator. No automatic world-map spawn placement should be activated before this.
