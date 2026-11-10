# Gate 1: campaign spawn-point census (static analysis)

Date: 2026-11-10
Branch: feature/coop-campaign
The static survey covers 59 campaign and multiplayer maps. Subsequent live spawn and ray trials cover three R1M2B positions.

## Inputs and reproducibility

The user provided MAPDATA.zip for local examination. It contains 59 original compiled world DAT files and associated scripts. The ZIP, maps, world geometry, sounds and scripts must not be committed or bundled with KPCFix.

Reproduce the read-only inspection using the analyzer in this branch:

    python coop/tools/analyze_spawn_points.py /path/to/MAPDATA.zip
    python coop/tools/analyze_spawn_points.py /path/to/MAPDATA.zip --map R1M2B --map PM01 --positions
    python coop/tools/analyze_spawn_points.py /path/to/MAPDATA.zip --json
    python -m unittest discover -s coop/tools -p 'test_*.py' -v

The inspector parses DAT version 127 object tables with typed properties; it validates that all object records end exactly at the next world-section offset declared in the header. All **59/59** world files in this upload passed that boundary check. The synthetic regression suite passed four tests locally.

The PlayerStart.GameType field has property type 6 and is serialized as a 32-bit float. Values 0.0 and 1.0 are called SP and MP below based on comparative usage; the exact engine-side interpretation still needs disassembly or runtime verification. Objects of type GameStartPoint are counted separately.

## Census

- 59 world DAT files examined: 10 deathmatch, 49 non-deathmatch.
- 47/49 non-deathmatch maps have zero PlayerStart objects with GameType=1.
- Only non-deathmatch maps R1M1A (9) and R1M1B (16) have GameType=1 player starts.
- All 10 deathmatch maps include GameType=1 player starts.
- Totals across 59 maps: 58 GameType=0 PlayerStarts, 132 GameType=1 PlayerStarts, two separate GameStartPoint objects, 32,398 total world objects.

| Map | PlayerStart Type 0 | PlayerStart Type 1 | GameStartPoint | Notes |
| --- | ---: | ---: | ---: | --- |
| R1M2B | 1 | 0 | 0 | Matches user-reported multiplayer start fallback |
| R1M1A | 2 | 9 | 0 | Existing MP starts are geographically dispersed |
| R1M1B | 1 | 16 | 0 | Existing MP starts are geographically dispersed |
| PM01 | 1 | 8 | 0 | Deathmatch control |
| PM02 | 1 | 6 | 1 | Deathmatch control with additional GameStartPoint |
| R2M3A | 1 | 0 | 1 | Extra GameStartPoint is not a PlayerStart |

R1M2B has one PlayerStart named PlayerStart0 at (1600, -256, -2944) with GameType=0. Its SHA-256 is 42f29784548eee4f... (prefix only). PM01 has one type-0 start and eight type-1 starts. This strongly explains the reported warning:

    Error!! Could not find any multiplayer start points. Looking for a singleplayer start point.

It does not establish a validated runtime hook location, nor whether a fallback first respawns and later map transitions are handled identically.

## Critical gameplay caveat

GameType=1 positions should not be used as co-op starts merely because they already work for deathmatch. Their positions in R1M1A are roughly 141–3207 world units from the first type-0 start, and in R1M1B roughly 544–4103 units away. They appear distributed throughout the map, not clustered around the campaign entrance.

Also, original CExitTrigger objects refer to NextWorld and sometimes StartPointName. For example, the exit from R1M1A into R1M1B names PlayerStart0. A co-op allocator must preserve the intended entry anchor, not just choose the first start object or a random multiplayer position.

## Engine disassembly follow-up

Matching official European-family 1.13 dedicated.exe, server.original.dll, and original object.lto were provided and SHA-256 verified. The start selector, explicit single-player fallback, and spawn-position setter callsite have now been traced. See [SPAWN-ENGINE-TRACE.md](SPAWN-ENGINE-TRACE.md) for exact addresses, the optional tracing hook, and the next Windows runtime test.

## Runtime follow-up

Three players spawned separately and moved normally at the tested R1M2B positions. The ground-ray diagnostic recorded nine hits and three control misses. Next verify player-volume clearance, departure and world reset before connecting the general allocator. See [spawn trials](SPAWN-ENGINE-TRACE.md) and [collision results](INTERSECT-ABI.md).
