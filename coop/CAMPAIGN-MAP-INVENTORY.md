# KISS Psycho Circus 1.13: campaign map inventory

**Status (2026-11-11):** read-only, evidence-backed inventory of the shipped single-player
campaign, used for the co-op campaign graph (`CoopCampaign=1`,
[CAMPAIGN-ARCHITECTURE.md](CAMPAIGN-ARCHITECTURE.md)).

* The compiled-in table `server-tools/server-tools-source/src/coop_campaign.h` matches the
  installed `WORLDORDER.TXT`. Re-check it with
  `python coop/tools/check_campaign_table.py C:\Games\Psycho\PSYCHO.REZ`.
* **Implementation status per world:** every world is in the graph and selectable. Natural
  exits are accepted only to the WORLDORDER successor, which all 48 stock exits are.
  * Intros allowed: R1M3A (user-tested) and R1M0–R4M0 (implemented, **not client-tested**).
    Other worlds' `starting_cinematic` scripts are skipped unless the test switch
    `CoopIntroAll=1` is set.
  * **No world beyond R1M2B → R1M2C → R1M3A has been played in co-op yet.**

This is a read-only analysis of stock `PSYCHO.REZ` and `KPCPatch.rez` (1.13), `object.lto` (base 0x10000000) and `cshell.dll` (base 0x20000000).
The machine-readable companion `inventory.json` (one record per world, RVA evidence in `_meta.globals`) and the helper data (`objects.json`, `exits.json`, `exit_brushes.json`, `wav_durations.json`, `cmds.txt`, `cmds2.txt`) were produced locally in `C:\KPCTest\scratch-campaign\`. They are derived from the user's installed game data and are deliberately **not** in the repository (no game assets or extracts are redistributed).

Tags:
- `[confirmed: …]` means the evidence was seen directly in data or code.
- `[inferred]` means a reasoned conclusion that was not observed at runtime.

Path conventions:
- World files are `WORLDS\REALMn\<NAME>.DAT`.
- Scripts are under `MAPDATA\…`.
- A KPCPatch.rez file overrides the main REZ only when it is at exactly the same path.

---

## (a) Canonical order (WORLDS\WORLDORDER.TXT, main REZ only, not patched)

The order and flags are [confirmed: WORLDORDER.TXT]. QuestNames are looked up as `realm*4+stage` in QUESTNAMES.TXT [inferred].

The realm start indices are [confirmed: `MAPDATA\EXTERNAL_DATA.TXT [WorldOrder]`]:
- WaterStart = 0
- EarthStart = 12
- AirStart = 25
- FireStart = 35
- NightmareStart = 46

They are consumed by cshell `0x9bd0` (GetRealmStartWorld → WorldOrder[index]).

| idx | world | flag | realm# | stage# | first | quest | realm start key |
|---|---|---|---|---|---|---|---|
| 0 | worlds/realm1/r1m0 | INTRO | 0 | 0 | 1 | (Bad Streets) | WaterStart |
| 1 | worlds/realm1/r1m1a | 0 | 0 | 0 | 0 | Bad Streets | |
| 2 | worlds/realm1/r1m1b | WORLDMAP | 0 | 0 | 0 | Bad Streets | |
| 3 | worlds/realm1/r1m2a | 0 | 0 | 1 | 1 | Pain Plaza | |
| 4 | worlds/realm1/r1m2b | 0 | 0 | 1 | 0 | Pain Plaza | |
| 5 | worlds/realm1/r1m2c | WORLDMAP | 0 | 1 | 0 | Pain Plaza | |
| 6 | worlds/realm1/r1m3a | 0 | 0 | 2 | 1 | Cathedral | |
| 7 | worlds/realm1/r1m3b | 0 | 0 | 2 | 0 | Cathedral | |
| 8 | worlds/realm1/r1m3c | WORLDMAP | 0 | 2 | 0 | Cathedral | |
| 9 | worlds/realm1/r1m4a | 0 | 0 | 3 | 1 | Freak Show | |
| 10 | worlds/realm1/r1m4b | 0 | 0 | 3 | 0 | Freak Show | |
| 11 | worlds/realm1/r1m4c | WORLDMAP | 0 | 3 | 0 | Freak Show | |
| 12 | worlds/realm2/r2m0 | INTRO | 1 | 0 | 1 | (Earth Rift) | EarthStart |
| 13 | worlds/realm2/r2m1a | 0 | 1 | 0 | 0 | Earth Rift | |
| 14 | worlds/realm2/r2m1b | 0 | 1 | 0 | 0 | Earth Rift | |
| 15 | worlds/realm2/r2m1c | WORLDMAP | 1 | 0 | 0 | Earth Rift | |
| 16 | worlds/realm2/r2m2a | 0 | 1 | 1 | 1 | Morbid Manor | |
| 17 | worlds/realm2/r2m2b | 0 | 1 | 1 | 0 | Morbid Manor | |
| 18 | worlds/realm2/r2m2c | WORLDMAP | 1 | 1 | 0 | Morbid Manor | |
| 19 | worlds/realm2/r2m3a | 0 | 1 | 2 | 1 | Unholy Ground | |
| 20 | worlds/realm2/r2m3b | 0 | 1 | 2 | 0 | Unholy Ground | |
| 21 | worlds/realm2/r2m3c | WORLDMAP | 1 | 2 | 0 | Unholy Ground | |
| 22 | worlds/realm2/r2m4a | 0 | 1 | 3 | 1 | Beast Pens | |
| 23 | worlds/realm2/r2m4b | 0 | 1 | 3 | 0 | Beast Pens | |
| 24 | worlds/realm2/r2m4c | WORLDMAP | 1 | 3 | 0 | Beast Pens | |
| 25 | worlds/realm3/r3m0 | INTRO | 2 | 0 | 1 | (Waste Wharf) | AirStart |
| 26 | worlds/realm3/r3m1a | 0 | 2 | 0 | 0 | Waste Wharf | |
| 27 | worlds/realm3/r3m1b | WORLDMAP | 2 | 0 | 0 | Waste Wharf | |
| 28 | worlds/realm3/r3m2a | 0 | 2 | 1 | 1 | Poison Plant | |
| 29 | worlds/realm3/r3m2b | WORLDMAP | 2 | 1 | 0 | Poison Plant | |
| 30 | worlds/realm3/r3m3a | 0 | 2 | 2 | 1 | Acropolis | |
| 31 | worlds/realm3/r3m3b | 0 | 2 | 2 | 0 | Acropolis | |
| 32 | worlds/realm3/r3m3c | WORLDMAP | 2 | 2 | 0 | Acropolis | |
| 33 | worlds/realm3/r3m4a | 0 | 2 | 3 | 1 | Midway | |
| 34 | worlds/realm3/r3m4b | WORLDMAP | 2 | 3 | 0 | Midway | |
| 35 | worlds/realm4/r4m0 | INTRO | 3 | 0 | 1 | (Doom Forge) | FireStart |
| 36 | worlds/realm4/r4m1a | 0 | 3 | 0 | 0 | Doom Forge | |
| 37 | worlds/realm4/r4m1b | 0 | 3 | 0 | 0 | Doom Forge | |
| 38 | worlds/realm4/r4m1c | WORLDMAP | 3 | 0 | 0 | Doom Forge | |
| 39 | worlds/realm4/r4m2a | 0 | 3 | 1 | 1 | Molten Vein | |
| 40 | worlds/realm4/r4m2b | 0 | 3 | 1 | 0 | Molten Vein | |
| 41 | worlds/realm4/r4m2c | WORLDMAP | 3 | 1 | 0 | Molten Vein | |
| 42 | worlds/realm4/r4m3a | 0 | 3 | 2 | 1 | Netherstation | |
| 43 | worlds/realm4/r4m3b | WORLDMAP | 3 | 2 | 0 | Netherstation | |
| 44 | worlds/realm4/r4m4a | 0 | 3 | 3 | 1 | Gloomvale | |
| 45 | worlds/realm4/r4m4b | WORLDMAP | 3 | 3 | 0 | Gloomvale | |
| 46 | worlds/realm5/r5m1a | 0 | 4 | 0 | 1 | Nucleus | NightmareStart |
| 47 | worlds/realm5/r5m1b | 0 | 4 | 1 | 0 | (none: index 17 missing) | |

**Worlds that are not in WORLDORDER** [confirmed: REZ listing]:
- `worlds/realm0/tutorial` (`WORLDS\REALM0\TUTORIAL.DAT`). cshell references `worlds\realm0\tutorial` at 0x129a4.
- The deathmatch maps PM01–PM10 in `WORLDS\DEATHMATCH`. The patch adds PM04, PM05 and PM07–PM10 and replaces PM01–PM03 and PM06. These are out of scope for the campaign.

No other single-player `.DAT` files exist [confirmed]. Every WORLDORDER entry has a `.DAT` file [confirmed].

**Sequence used by the exits** [confirmed: every CExitTrigger.NextWorld, by case-insensitive compare]: the exits form exactly the WORLDORDER chain. The chain is TUTORIAL → `menu`, and R1M0 → R1M1A → … → R4M4B → R5M1A → R5M1B → `Endgame`. No branches or optional paths exist, and each world has exactly one CExitTrigger.

---

## How transitions work (engine evidence)

1. **CExitTrigger is a brush trigger volume.** [confirmed: object.lto classdef 0x12d240, parent `CTriggerBrush` (0x12d470); every exit has a world-model brush record in its .DAT, see `exit_brushes.json`]
   - Its activate method is vtable slot +0x70 = **0x2d450**. Message 0x27 (sent by script `trigger_generic`) is routed to the same slot via 0x2d400.
   - 0x2d450 returns without doing anything when `[game+0x1d0] != 0`, i.e. when the game type is not single-player [confirmed].
   - 0x2d530 compares NextWorld with `_stricmp` (0x105190):
     - `menu` → 0xf9150(0)
     - `endgame` → 0xf9150(1), which sends MID_ENDWORLD 0x72
     - anything else → MID_PLAYER_EXITLEVEL (0x19) carrying the current world, NextWorld and StartPointName [confirmed]
2. **Ways an exit fires.** Each world uses one of these [confirmed: object properties and script lines listed per world]:
   - (i) walk-in: the player touches the exit brush. This is [inferred] from the shared +0x70 touch slot of sibling `CTriggerBrushCinematic` (0x2e4a0, IsPlayer filter 0x2e200) and from the user-confirmed R1M3A→R1M3B transition.
   - (ii) "use" on a mirror `CModelDeco` (exit_a/exit_b model, `skins\decos\exit.dtx`) whose `use_command1` is `0 trigger_generic <exit>`.
   - (iii) a `CSwitchSlide` command, or a door's `open_command`.
   - (iv) a script `trigger_generic <exit>`, usually at the end of a boss or victory cinematic. Each such script has an `endingscript` companion that fires the exit immediately (`trigger_generic … at 0–1 s`). That companion is presumably the skip/abort path [inferred].
3. **Script commands.** The complete script command set is in `cmds.txt` and `cmds2.txt` [confirmed]:
   - 84 commands are registered via 0xc43a0, plus `camera_pos`, `camera_path` and `endingscript` via 0xc4440.
   - **No level-change script command exists.** The only other level change is the cheat/console "ExitLevel" handler 0xfaf60 (table entry at 0xfbd80). It finds the WorldOrder successor of the current world and then a CExitTrigger whose NextWorld matches; failing that, it uses the first exit [confirmed].
4. **World map.** The client handler for MID_PLAYER_EXITLEVEL is cshell 0xcb79d [confirmed]. It reads a byte; the server writes `[game+0x1cc]` at 0x2d6fc.
   - If that byte != 1 and the current world's WORLDORDER flag is `WORLDMAP`, the client shows the world map first (state 2).
   - Otherwise it loads the next world directly.
   - What "continue" does on the world-map screen (it presumably loads the server's NextWorld) was not traced [inferred].
5. **INTRO and OPT flags.** These are used only by the client world-map tally builder, cshell 0x38f50 [confirmed]. It compares against "OPT" (0x10cdc4) and "INTRO" (0x10cdbc); INTRO and OPT entries are excluded from stage tallies. No stock entry uses OPT. The server does not read the INTRO flag; object.lto has no "INTRO" string [confirmed].
6. **Saving.** Quicksave and save are refused on two lists [confirmed: cshell tables 0x11a934 and 0x11a908, used at 0xca0a0 and 0xca1d4]:
   - intro worlds `r1m0 r2m0 r3m0 r4m0 r5m0`
   - boss worlds `r1m4c r2m4c r3m4b r4m4b r5m1b` ("You cannot save in a boss encounter.")
7. **Player starts** [confirmed: object.lto 0xf08a0 and 0xf12f1].
   - A `PlayerStart` is a candidate only when its `GameType` equals `[game+0x1d0]`. Single player is 0; types 1–4 pick a random candidate.
   - Single player uses the start named in the exit's `StartPointName`, otherwise the first candidate.
   - In multiplayer, if no MP start exists, the game retries with game type 0, i.e. single-player starts. If none exists at all, it spawns at <0 0 0>.
   - Only R1M1A (9) and R1M1B (16) contain GameType-1 starts. Every other campaign world has only GameType-0 starts.
8. **Starting cinematic.** `WorldProperties.starting_cinematic` is played at 0x104830 only when `[game+0x1d0]==0` [confirmed]. On a dedicated multiplayer server, both the intro scripts and every CExitTrigger are therefore inert unless the co-op layer bypasses the `+0x1d0` gate.
9. **Loading background** [confirmed: cshell 0xad7b0 / 0xad710 / 0xad750].
   - The rule is `_splitpath(world)`. If the file name matches `[Rr]<digit>[Mm]<digit>`, the client uses `screens/LoadingBackgrounds/` + `R<d>M<digits>` + `.pcx`. Otherwise, or if the load fails, it uses `menu/titlescreen.pcx`.
   - So R1M1A/B use R1M1.pcx, …, R5M1A/B use R5M1.pcx, and TUTORIAL uses titlescreen.pcx.
   - `R5M2.PCX` is never selected.

---

## (b) Per-world details

Notes on the columns:
- **Succ** is the exit's NextWorld; all of them match the WORLDORDER successor.
- **Music** is `WorldProperties.MusicDirectory`.
- **LB** is the loading background.
- **Spawns** is CMonsterSpawner / CSpawner / placed monsters.
- **Starts**: unless shown otherwise, each world has exactly one GameType-0 PlayerStart.

### Water realm (Starbearer)

| World | Pred → Succ | Exit (class / name / brush dims*) and how it fires | Intro script (starting_cinematic) | Music | LB | Spawns | Boss |
|---|---|---|---|---|---|---|---|
| R1M0 | — (WaterStart, new game) → R1M1A, StartPointName `PlayerStart0` | CExitTrigger `CExitTrigger0` (~80×24×8, hidden). Fired by `R1BLACKROOM.SCR` line 82 `79 trigger_generic CExitTrigger0`; the ending script `R1BLACKROOMEND.SCR` fires it at 0 s [confirmed] | `Mapdata\Realm1\R1M0\R1blackroom.scr` (79 s) | ravensong | R1M0 | 0/0/0 | – |
| R1M1A | R1M0 → R1M1B, `PlayerStart0` | `CExitTrigger0` (16×16×8). The touch brush `CTriggerCommand1` (`command2 = 0 scriptplay …r1m1a_victory.scr`) leads to `8 trigger_generic CExitTrigger0` [confirmed] | r1m1a_intro.scr (53 s) | song1 | R1M1 | 35/3/37; starts: GT0×2, GT1×9 | – |
| R1M1B | R1M1A → R1M2A | `Gubba` (8×64×112). The mirror `CModelDeco1.use_command1 = 0 trigger_generic Gubba` [confirmed] | r1m1b_intro.scr | song1 | R1M1 | 42/5/66; GT0×1, GT1×16 | – |
| R1M2A | R1M1B → R1M2B | `CExitTrigger0` (16×48×64), walk-in [inferred] | citysquare1start.scr (45.5 s) | song5 | R1M2 | 28/7/6 | – |
| R1M2B | R1M2A → R1M2C | `CExitTrigger0` (96×40×88), walk-in [inferred] | cs2start.scr | song5 | R1M2 | 31/20/11 | – |
| R1M2C | R1M2B → R1M3A | `exit`. The mirror `exitmiddle.use_command1` [confirmed] | citysquare3start.scr | song5 | R1M2 | 38/10/9 | – |
| R1M3A | R1M2C → R1M3B | `CExitTrigger0` (24×80×80), walk-in [inferred; transition user-confirmed in co-op] | cathedral1start.scr (53 s) | song1 | R1M3 | 53/4/1 | CBossControllerCathFatlady (mid-boss) |
| R1M3B | R1M3A → R1M3C | `CExitTrigger0` (16×168×176), walk-in [inferred] | ca2intro.scr | song6 | R1M3 | 40/4/9 | – |
| R1M3C | R1M3B → R1M4A | `exit`. Mirror `CModelDeco2.use_command1` and `CSwitchSlide0.command1` [confirmed] | cath3intro.scr | song1 | R1M3 | 29/7/4 | – |
| R1M4A | R1M3C → R1M4B | `exit`. `CDoorSliding0.open_command1 = 0 trigger_generic exit` [confirmed] | fshowintro.scr (patched `INTROSCRIPT.SC` in closure) | song2 | R1M4 | 50/5/1 | – |
| R1M4B | R1M4A → R1M4C | `exit`. Mirror `CModelDeco0.use_command1` [confirmed] | freakshow2intro.scr | song3 | R1M4 | 21/12/1 | – |
| R1M4C | R1M4B → **R2M0** | `exit` (tiny, hidden). Chain: Fortunado controller `after_dead_script_obj=sc3` → CScriptObject `sc3` (`2 scriptplay …R1M4end.scr`) → `R1M4END.SCR` line 65 `64 trigger_generic exit`. The ending script `R1M4END1.SCR` fires it at 1 s [confirmed] | intro.scr → fortunado.scr (patched: adds endingscript Fortunado1.scr) | boss1 | R1M4 | 0/0/0 | CBossControllerFortunado |

### Earth realm (Beast King)

| World | Pred → Succ | Exit / trigger | Intro | Music | LB | Spawns | Boss |
|---|---|---|---|---|---|---|---|
| R2M0 | R1M4C → R2M1A, `PlayerStart0` | `CExitTrigger0` (~80×24×8). `R2BLACKROOM.SCR` line 95 `93 trigger_generic CExitTrigger0`; END script at 0 s [confirmed] | R2blackroom.scr (93 s) | ravensong | R2M0 | 0/0/0 | – |
| R2M1A | R2M0 → R2M1B | `CExitTrigger0` (176×112×32), walk-in [inferred] | r2m1a_intro.scr (36.5 s) | song6 | R2M1 | 82/11/44 | – |
| R2M1B | R2M1A → R2M1C | `CExitTrigger0` (256×32×256), walk-in [inferred] | chasm2start.scr | song6 | R2M1 | 21/12/21 | – |
| R2M1C | R2M1B → R2M2A | `exit`. Mirror `CModelDeco1` [confirmed] | chasm3start.scr | song6 | R2M1 | 29/15/7 | – |
| R2M2A | R2M1C → R2M2B | `CExitTrigger0` (72×74×8), walk-in [inferred] | man01intro.scr (40 s) | song6 … transition beat | R2M2 | 48/20/34 | – |
| R2M2B | R2M2A → R2M2C | `CExitTrigger0` (16×112×128), walk-in [inferred] | man02intro.scr | song5 | R2M2 | 86/12/38; also 1 `StartPoint` | – |
| R2M2C | R2M2B → R2M3A | `levexit01`. Mirror `CModelDeco5` (exit_b) [confirmed] | man03intro.scr | song2 | R2M2 | 32/27/33 | – |
| R2M3A | R2M2C → R2M3B | `CExitTrigger0` (224×224×256), walk-in [inferred] | cemetary1start.scr | song6 | R2M3 | 25/7/6; also 1 `GameStartPoint` (GameType 0) | – |
| R2M3B | R2M3A → R2M3C | `exit` (192×136×32), walk-in [inferred; no use reference] | cm2intro.scr | song1 | R2M3 | 37/13/2 | – |
| R2M3C | R2M3B → R2M4A | `exit`. Mirror `CModelDeco0` [confirmed] | cm3intro.scr | song4 | R2M3 | 38/6/15 | – |
| R2M4A | R2M3C → R2M4B | `CExitTrigger0` (108×50×36), walk-in [inferred] | beast01intro.scr | song3 | R2M4 | 36/16/25 | – |
| R2M4B | R2M4A → R2M4C | `CExitTrigger0` (224×130×20), walk-in [inferred] | beast02intro.scr | song3 i2 | R2M4 | 37/33/31 | – |
| R2M4C | R2M4B → **R3M0** | `exit`. `CBossControllerTiberius.ending_cinematic = …tiberiusend.scr` → line 38 `35 trigger_generic exit`; END1 at 1 s [confirmed] | tiberiusintro.scr (**unpatched**, see anomalies) | **empty**; the script sets boss4 | R2M4 | 6/0/3 | CBossControllerTiberius |

### Air realm (Celestial)

| World | Pred → Succ | Exit / trigger | Intro | Music | LB | Spawns | Boss |
|---|---|---|---|---|---|---|---|
| R3M0 | R2M4C → R3M1A, `PlayerStart0` | `CExitTrigger0` (16×16×8). `R3BLACKROOM.SCR` line 74 `56.5 trigger_generic CExitTrigger0`; END at 0 s [confirmed] | R3blackroom.scr (56.5 s) | ravensong | R3M0 | 0/0/0 | – |
| R3M1A | R3M0 → R3M1B, StartPointName **`booboo01`** | `sloopy`. The locked door `CDoorz02` (lock `Cockatoo`) `open_command1 = scriptplay …r3m1a_victory.scr` → `8 trigger_generic sloopy` [confirmed] | r3m1a_intro.scr (55 s) | song6 | R3M1 | 65/6/60 | – |
| R3M1B | R3M1A → R3M2A | `kooklook`. Mirror `CModelDeco1` [confirmed]. The only start is named `booboo01` | **none** (starting_cinematic "") | song2 | R3M1 | 17/13/14 | – |
| R3M2A | R3M1B → R3M2B | `CExitTrigger0` (64×96×32), walk-in [inferred] | nuke1start.scr (40 s) | song2 | R3M2 | 29/11/6 | – |
| R3M2B | R3M2A → R3M3A | `exittrigger`. Mirror `exitmiddle` [confirmed] | nuke2start.scr | song2 | R3M2 | 31/26/12 | – |
| R3M3A | R3M2B → R3M3B | `CExitTrigger0` (156×88×16), walk-in [inferred] | urbanchasm01intro.scr | song1 | R3M3 | 46/9/26 | – |
| R3M3B | R3M3A → R3M3C | `CExitTrigger0` (128×116×24), walk-in [inferred] | uchasm02intro.scr | song1 i2 | R3M3 | 24/24/16 | – |
| R3M3C | R3M3B → R3M4A | `exit`. `CSwitchSlide0.command1`; the mirror only has `cprint "trigger an exit brush here"` [confirmed] | uchasm03intro.scr | song1 | R3M3 | 21/16/10; GT0×2 (both named PlayerStart0) | – |
| R3M4A | R3M3C → R3M4B | `CExitTrigger0` (64×112×64), walk-in [inferred] | midway1start.scr (38 s) | song3 | R3M4 | 6/20/20 | – |
| R3M4B | R3M4A → **R4M0** | `exit`. `CStargrave.DeathScriptObject=sc3` → `1 scriptplay …exit.scr` → `EXIT.SCR` line 32 `26 trigger_generic exit`; `EXITA.SCR` at 1 s [confirmed] | stargrave.scr (patched: adds endingscript stargrave1.scr) | boss2 | R3M4 | 0/0/0 | CStargrave |

### Fire realm (Demon)

| World | Pred → Succ | Exit / trigger | Intro | Music | LB | Spawns | Boss |
|---|---|---|---|---|---|---|---|
| R4M0 | R3M4B → R4M1A, `PlayerStart0` | `CExitTrigger0` (8×24×8). `R4BLACKROOM.SCR` line 175 `72.5 trigger_generic CExitTrigger0`; END at 0.01 s [confirmed] | R4blackroom.scr (72.5 s) | ravensong | R4M0 | 0/0/0 | – |
| R4M1A | R4M0 → R4M1B | `CExitTrigger0` (64×80×24), walk-in [inferred] | steelworks1start.scr (38 s) | song5 | R4M1 | 12/8/5 | – |
| R4M1B | R4M1A → R4M1C | `CExitTrigger0` (60×128×64), walk-in [inferred] | steelworks2start.scr | song5 | R4M1 | 16/24/16 | – |
| R4M1C | R4M1B → R4M2A | `exit`. Mirror `exitmiddle`. The door `door_exit` is opened by the Unipsycho controller's `ending_cinematic` map24uni_end.scr [confirmed] | unibossentry.scr | song4 | R4M1 | 0/0/4 | CBossControllerMap24Unipsycho |
| R4M2A | R4M1C → R4M2B | `exitout01`. Switch `CSwitchRotating5` → `scriptplay sewer01exit.scr` → `6.5 trigger_generic exitout01` [confirmed] | sewer01intro.scr | song3 | R4M2 | 21/33/23 | – |
| R4M2B | R4M2A → R4M2C | `CExitTrigger0` (12×80×124), walk-in [inferred] | sewer02intro.scr | song2 | R4M2 | 21/23/16 | – |
| R4M2C | R4M2B → R4M3A | `exit`. `CSwitchSlide0`; the mirror is a placeholder cprint [confirmed] | sewer3intro.scr | song3 i2 | R4M2 | 12/33/27 | – |
| R4M3A | R4M2C → R4M3B | `CExitTrigger0` (96×96×16), walk-in [inferred] | subway1start.scr | song3 | R4M3 | 34/15/12 | – |
| R4M3B | R4M3A → R4M4A | `exit`. Mirror `CModelDeco0` [confirmed] | sub2intro.scr | song6 i3 | R4M3 | 59/6/29 | – |
| R4M4A | R4M3B → R4M4B | `finalfire`. `CSwitchSlide0.command1` and `CDoorSliding3.open_command1` [confirmed] | r4m4a_intro.scr (45 s) | song5 | R4M4 | 129/20/30 | – |
| R4M4B | R4M4A → **R5M1A** | `exit`. `CBossControllerBlackwell.ending_cin_obj=bossend` → `1 scriptplay Blackwellend.scr` → line 41 `32 trigger_generic exit`; END1 at 0.1 s [confirmed] | blackwellintro.scr | boss3 | R4M4 | 4/0/0 | CBossControllerBlackwell |

### Nightmare realm

| World | Pred → Succ | Exit / trigger | Intro | Music | LB | Spawns | Boss |
|---|---|---|---|---|---|---|---|
| R5M1A | R4M4B → R5M1B | `CExitTrigger0` (112×112×96), walk-in [inferred] | nightmare1intro.scr (25 s) | nightmare1 | R5M1 | 55/22/6 | CBossCharacterEyeTurret |
| R5M1B | R5M1A → **`Endgame`** | `exit`. `CNightmareSpider.DeathScriptObject=final` → `0 scriptplay nightmareboss-end.scr` → line 34 `87 trigger_generic exit`; ENDA at 0.5 s [confirmed] | nightmarebossintro.scr (32 s) | nightmare1 | R5M1 | 3/0/22 | CNightmareFetus (stages → `wake`/`change` scripts), CNightmareSpider |
| TUTORIAL | menu → **`menu`** | `exit` (32×8×16). `CSwitchSlide0.command1 = 0 trigger_generic exit` [confirmed] | `mapdata\realm0\r0m1\r0m1a\tutintro.scr` | tutorial | titlescreen.pcx | 5 CMonsterSpawner | – |

\*Brush dimensions come from an empirical decode of the world-model record. The rule is (A+B)/2 = 2·C, and the table shows (B−A)/2. The true size may be twice that, so the decode is ambiguous [inferred]. All dimensions are only useful to tell big doorway volumes from tiny hidden script-only brushes.

---

## (c) Intro cinematic worlds R1M0, R2M0, R3M0, R4M0 ("black room", Madam Raven)

**Common facts** [confirmed: objects.json, scripts]:
- Each world has these objects and no others:
  - one `WorldProperties` (`MusicDirectory = "ravensong intensity 1"`, `starting_cinematic = Mapdata\RealmN\RNM0\RNblackroom.scr`)
  - one CExitTrigger `CExitTrigger0` (NextWorld `worlds\realmN\RNM1a`, StartPointName `PlayerStart0`)
  - **one** `PlayerStart0` (GameType 0)
  - CCinematicPath/Point camera rigs
  - raven/table/crystal-ball `CModelDeco`
  - 18 `CModel`
- R2M0 also has `CTrapSwinging`. R3M0 also has `CTrapSwinging`. R4M0 also has `CTrapSwinging`, `CLava`, `CLeak flame1` (start_on false), `CTorch`×3 and 2 `CSoundAmbient`.
- **They contain no monsters, CMonsterSpawner, CSpawner or item pickups.**
- Each has an exit-looking mirror `CModelDeco1` (exit_a.abc). Its `use_command1 = 0 trigger_generic Gubba` is a copy-paste leftover from R1M1B: no object named Gubba exists in R1M0, R2M0 or R4M0. R3M0 has no such mirror.
- The exit brush is tiny and does not overlap the player start. For example, R1M0's exit centre is at (−336,216,−328) and its start at (−456,340,−224); R4M0's exit centre is (−120,200,−384) and its start (−168,52,−328). So the exit is script-only in practice [inferred].
- Every script begins with `endingscript …END.scr`, `user_input off` and `letterbox on`. None of them restores input or letterbox (R3M0 only does `camera_reset`), because the world changes immediately. Each END script stops all narration streams and fires `trigger_generic CExitTrigger0` at about 0 s; it is presumably the skip path [inferred].

Narration: each stream is `streamsound player <wav> local` with a matching `subtitle`. Durations are measured from the WAV headers (all 22.05 kHz 16-bit mono) [confirmed].

| World | Script (size) | Narration streams: start time and length in s, end in brackets | Last audio ends | Fade-out | **Exit fires** |
|---|---|---|---|---|---|
| R1M0 | R1BLACKROOM.SCR (2966 B) | 3.1 + 3.23; 8 + 8.13; 16 + 10.14; 26.4 + 4.21; 31 + 19.54; 50 + 17.57; 67.5 + 9.16 (→76.66) | 76.66 | fog to black 70–76.4 | **79.0** |
| R2M0 | R2BLACKROOM.SCR (3605 B) | 3.5 + 4.07; 9 + 5.14; 15.1 + 9.92; 25 + 10.84; 37.2 + 24.71; 65 + 11.28; 76.5 + 14.74 (→91.24). `r2-ravenintro-4.wav` (13.21 s) is never streamed; it is only `stopstream`ed | 91.24 | 90–92.5 | **93.0** |
| R3M0 | R3BLACKROOM.SCR (2719 B) | 3 + 4.59; 8 + 10.81; 19 + 6.63; 26 + 8.62; 35 + 14.26; 49 + 7.57 (→56.57) | 56.57 | fog to white 51–54 | **56.5** (with `camera_reset`) |
| R4M0 | R4BLACKROOM.SCR (5491 B) | 3 + 8.04; 12 + 7.08; 20 + 13.53; 34 + 5.27; 40 + 11.08; 54 + 12.30 (→66.3) | 66.3 | 68–72.4 | **72.5** (`sound_ambient_off squeak` at 72) |

Other script content [confirmed]:
- `camera_path` (MR05/MR00/MR09/MR08 in R1/R2, path0–2 in R3, campathmirror/fly1/fly2 in R4)
- `camera_fov` zooms
- `model_anim`/`model_anim_loop` on Madam1/2 or Raven1/2
- `model_appear`/`model_disappear` of the crystal ball
- `fog_fade`
- R4M0 only: `leak_on Flame1` and `sound flamesoundspot`

Nothing in these scripts changes the world except the final `trigger_generic CExitTrigger0`.

**Gates on a dedicated multiplayer server (game type != 0)** [confirmed]:
1. The starting cinematic is not played (0x104830).
2. CExitTrigger activation is ignored (0x2d450), even if a script or a player fires it.
3. `cheatscript` is ignored (0xb8ad0); this is irrelevant for R?M0.
4. In multiplayer, the world-map screen is skipped only when the server byte `[game+0x1cc]` == 1 (cshell 0xcb7c7).
5. No GameType-1 starts exist, so spawning falls back to the single `PlayerStart0` (0xf12f1), and all players share one spawn point.

On a stock MP server, an R?M0 map would load and then sit forever. Advancement needs a server-side override, for example:
- play the script and allow the exit;
- force a change after the measured duration (79 / 93 / 56.5 / 72.5 s);
- or call the ExitLevel path 0xfaf60 [inferred].

**Lobby-world candidate** [inferred]: **R1M0** is the best harmless pre-campaign lobby.
- It has no monsters, spawners, items, lava, swinging trap or damage objects.
- Its single SP start is used as the MP fallback.
- Its exit is inert in MP without the script, and the mirror's use command targets a non-existent object.
- Music is ravensong, and the loading background R1M0.pcx exists.
- It is listed as no-save by the client.

Caveats:
- (1) The geometry is a small cinematic set. It is unverified whether it is fully enclosed and walkable for several players stacked on one start.
- (2) If the co-op layer plays starting cinematics in MP (as it does for R1M3A), R1M0 will run the 79 s narration with `user_input off`/`letterbox on` and then try to exit. As a lobby it should be loaded with the cinematic suppressed, or used deliberately as the "campaign intro" with exit at about 79 s or on skip.

R2M0, R3M0 and R4M0 are less clean: they have `CTrapSwinging`, and R4M0 also has `CLava` and a flame leak.

---

## (d) Nightmare and the end-game flow

- **Reached from R4M4B** [confirmed]:
  - Blackwell dies → `bossend` → `Blackwellend.scr` (narration `r4m4-end.wav`) → `32 trigger_generic exit` → NextWorld `worlds\realm5\r5m1a`.
  - R4M4B is flagged WORLDMAP, so a single-player client shows the world map first.
  - No unlock check exists in the server exit path; it is a plain NextWorld [confirmed: 0x2d530 has no realm logic].
  - A new game can also start directly at Nightmare via `NightmareStart = 46` [confirmed config].
  - The world-map UI has replay text "You must complete the Nightmare Realm before you can replay a Nightmare stage" and slots r5m1–r5m4 (cshell 0x10c388–0x10c3f4) [confirmed]. How the realm-select UI gates NightmareStart was not traced.
- **R5M1A distinct opening** [confirmed]: `nightmare1intro.scr`, 25 s.
  - It has letterbox, fog, camera paths path1–4, `sound_ambient` laugh/circus and `trigger_door startdoor open`, then `user_input on` at 25 s. It has no narration.
  - Its "special setup" is `scriptload` + `cheatscript mapdata\dev\rescoe\cheat5.scr`. CHEAT5 would force `player class Celestial`, set health to 250, and give all armour pieces and all weapons and ammo.
  - `cheatscript` only runs when the console variable `CheatScripts` is non-zero and the game is single player (0xb8ad0). **In a stock game the loadout setup is skipped** [confirmed gate; stock CheatScripts value not checked → inferred off].
  - Exit: walk-in `CExitTrigger0` to R5M1B.
- **R5M1B finale** [confirmed]:
  - Intro `nightmarebossintro.scr` (R5-Intro.wav, 31.51 s; text = `R5-INTROTEXT.TXT`; the same cheat5 cheatscript; `player teleport bloodstart` at 31).
  - Boss `CNightmareFetus`: EndStage1 → `wake` → nightmareboss-2.scr (R5-2.wav, 32.1 s); EndStage2 → `change` → nightmareboss-3.scr (R5-3.wav, 17.09 s, 44.1 kHz).
  - Then `CNightmareSpider` death → `final` → `nightmareboss-end.scr` (R5-END.wav, 83.99 s) → `87 trigger_generic exit` → NextWorld **`Endgame`**.
  - Server 0xf9150(1) → MID_ENDWORLD(1) → client 0xc88b0 plays `movies\EndCine.smk`. The `movies` folder is absent from C:\Games\Psycho, presumably on CD [confirmed absent]. What follows (credits or menu) was not traced [inferred: back to menu].
- **Realm finales** [confirmed, all scripted exits, all WORLDMAP-flagged]:
  - R1M4C Fortunado → `R1M4END.SCR` 64 s → R2M0
  - R2M4C Tiberius → `tiberiusend.scr` 35 s → R3M0
  - R3M4B Stargrave → `exit.scr` 26 s → R4M0
  - R4M4B Blackwell → 32 s → R5M1A
  - None of them goes to credits or the menu; only R5M1B does (`Endgame`), and TUTORIAL (`menu`).

## (e) Realm ↔ KISS avatar mapping [confirmed]

| Realm (WORLDORDER #) | Avatar | Evidence |
|---|---|---|
| Water (0) | **Starbearer** | Pamphlet `STARBEARER.TXT` "His is the element of water"; R1BLACKROOM "the Starbearer's champion"; water-realm dev CHEAT.SCR `player class starbearer` |
| Earth (1) | **Beast King** | `BEASTKING.TXT` "His is the realm of earth"; TIBERIUSEND "The Beast King joins the Starbearer"; R2 cheat scripts `player class BeastKing`; Tiberius pamphlet "avatar of the Beast King" |
| Air (2) | **Celestial** | `CELESTIAL.TXT` "His is the element of air"; R3BLACKROOM "The Celestial is the arbiter of balance … Lay claim to the air realm"; R3 cheat scripts `player class Celestial` |
| Fire (3) | **Demon** | `DEMON.TXT` "His is the element of fire"; R4BLACKROOM "The source of the Demon's power … the Fire Realm"; R4 cheat scripts `player class Demon` |
| Nightmare (4) | none; the finale features all four | R4M4B "The Four Who Are One stand united"; cheat5 forces Celestial (dev only) |

The fallen avatars (bosses) are Fortunado (water), Tiberius (Beast King/earth), Stargrave (Celestial/air) and Blackwell (fire) [confirmed: pamphlets].

## (f) Anomalies and things to watch

1. **No NextWorld ≠ successor mismatches.** All 47 non-final campaign exits point to the WORLDORDER successor. Case and slash differences (`Worlds\Realm1\R1M3b`, `worlds\realm4\R4M2b`, …) are harmless: comparisons use `_stricmp` (0x105190), and the client normalises `\` to `/` [confirmed]. Special targets are `menu` (TUTORIAL) and `Endgame` (R5M1B).
2. **No branching or optional maps.** No `OPT` flag is used, although the client supports it (cshell 0x10cdc4) [confirmed]. Each world has exactly one exit.
3. **Nightmare bookkeeping.**
   - R5M1B is `realm 4, stage 1, first 0`, so stage 1 has no "first map", and QUESTNAMES has no 18th entry.
   - Only "Nucleus" exists; the world-map UI has slots r5m1–r5m4.
   - `R5M2.PCX` exists but is never selected, because R5M1B maps to R5M1.pcx [confirmed rule].
   - The cshell no-save table lists a nonexistent `worlds/realm5/r5m0` [confirmed].
4. **Dead or leftover references.**
   - cshell 0x128a0 loads `worlds\realm1\r1m1a_roadhouse`, which does not exist [confirmed string; caller reachability unknown].
   - The R1M0, R2M0 and R4M0 mirrors target `Gubba` (missing).
   - The R3M3C and R4M2C mirrors only `cprint "trigger an exit brush here"`.
   - R2M2C `SC01` plays `cinematics\man03a.scr` (missing; the real file is mapdata\…\MAN03A.SCR).
   - R4M4B `spawnhelper3`/`spawnhelper4` play `SpawnC.scr`/`SpawnD.scr` (missing).
   - `r2-ravenintro-4.wav` is unused.
5. **Patch overrides (KPCPatch.rez)** [confirmed by diff]:
   - `R1M4END.SCR` is byte-identical to the main file, so it has no effect.
   - `FORTUNADO.SCR` adds `endingscript Fortunado1.scr`, a new file.
   - `R3M4B STARGRAVE.SCR` adds `endingscript stargrave1.scr`, a new file.
   - `R1M4A INTROSCRIPT.SC` changes a `Wait` from 2.8 to 2.3.
   - `EXTERNAL_DATA.TXT` has weapon/deathmatch tweaks; the [WorldOrder] section is unchanged.
   - **The `TIBERIUSINTRO.SCR`/`TIBERIUSINTRO1.SCR` patch sits at `MAPDATA\REALM2\R2M4C\` (missing the `R2M4\` level), but R2M4C references `mapdata\realm2\r2m4\r2m4c\tiberiusintro.scr`.** So the patched skip handler is likely never used [inferred: lookup is by exact path].
   - The patch also adds or replaces deathmatch maps only (PM01–PM10).
6. **Odd properties.**
   - R3M1B has no starting_cinematic.
   - R2M4C's MusicDirectory is empty (music comes from its script).
   - R3M3C has two GameType-0 starts with the same name; SP uses the first.
   - R1M1A has two SP starts (PlayerStart0 and PlayerStart1) plus nine MP starts, all named PlayerStart2.
   - R3M1A→R3M1B uses StartPointName `booboo01`; every other campaign exit leaves it empty, except R?M0 → `PlayerStart0`.
7. **Single-player-only gates relevant to co-op** [confirmed]:
   - `[game+0x1d0]` (game type) blocks starting cinematics, CExitTrigger activation and cheatscript.
   - In the client, `[game+0x1cc]` via MID_PLAYER_EXITLEVEL controls whether the world map is shown.

## Open questions

- Exact touch dispatch into slot +0x70 for CExitTrigger. It is inferred from the sibling class plus observed play; a runtime check of one walk-in exit outside R1M3A, e.g. R2M3B `exit`, would close it.
- The true brush extents (decode factor of 2), and whether R?M0 geometry is a safe enclosed space for several players.
- When exactly `endingscript` runs (skip or abort?). The world-map "continue" path, and whether the realm-select UI can start at NightmareStart before Nightmare is completed.
- The stock value of the `CheatScripts` console variable.
- What follows EndCine.smk.
