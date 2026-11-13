# Co-op handoff (updated 2026-11-13, after the RT1–RT12 retest fixes and the winmm.dll loader)

How everything works: [COOP-MVP.md](COOP-MVP.md). Settings: [CONFIGURATION.md](CONFIGURATION.md).
Project rules and tooling: [CLAUDE.md](../CLAUDE.md).
Finished test rounds: [archive/TWO-PC-RESULTS-2026-11-13.md](archive/TWO-PC-RESULTS-2026-11-13.md)
(the T1–T20 results, their causes and fixes, and the user's request list N1–N18),
[archive/COOP-MVP-EVIDENCE-2026-11-12.md](archive/COOP-MVP-EVIDENCE-2026-11-12.md).

## Next steps (in order)

1. **Laptop:** copy `C:\KPCTest\pkg-client-9a7eef01` (`dinput.dll`, `KPCFix.ini`,
   `DDrawCompat-CLIENT.ini`) into the laptop's game folder. The desktop already has both new
   builds (Server Tools now as `winmm.dll`, see below).
2. **Not yet run on these builds:** `source/audit_sites.py` for client `9a7eef01`.
3. **RT11 decision (user):** how to keep the R1M2A belt lift from stranding the other players
   (options in [Open work](#open-work)).
4. **Two-PC retest:** run the [Retest plan](#retest-plan) and fill in its Result column (or
   paste the report in chat). Send both `KPCFix.log` files and the desktop's `KPCServerTools.log`.
5. **After the retest:** for each failure use [If a retest fails](#if-a-retest-fails); move
   what passed to [Live-confirmed](#live-confirmed-two-pcs).
6. **Then the open work** in [Open work](#open-work).

## Where things stand

* **Branch:** `coop/mvp-lobby-untested` (untested fixes; `coop/mvp-lobby` holds confirmed
  work, `main` the stable release). Pushed; co-op CI passes again since `7c50ae8` (its
  `CoopIntro` marker assertion was stale from `6d91b5b` until then).
* **Server Tools now load as `winmm.dll`.** `dedicated.exe` imports `WINMM.dll`, so the DLL
  loads from the game folder like the client's `dinput.dll`, forwards every winmm function to
  the system DLL, and answers the server's `LoadLibraryA("server.dll")` itself; the original
  `server.dll` stays unrenamed. Setup moves an earlier install (proxy as `server.dll`, engine
  as `server.original.dll`) over. See [README-Server-Tools.md](../server-tools/README-Server-Tools.md).
* **Installed:**

  | Machine | Server Tools (`winmm.dll`) | Client `dinput.dll` | Backup of what was replaced |
  | --- | --- | --- | --- |
  | Desktop `C:\Games\Psycho` (client, host, dedicated server) | `6448dadf`; `server.dll` is the original `83b7bce8` again | `9a7eef01` | `C:\KPCTest\backup\psycho-20261113-175417-before-6448dadf` (also holds the old `server.dll` proxy `dcc66b4c` and `server.original.dll`). Reinstalled 2026-11-13 23:20 after the folder had been switched to the stable release (client `288bce4e`, no Server Tools): `C:\KPCTest\backup\psycho-20261113-232042-before-6448dadf` and `C:\KPCTest\backup\psycho-*-ini-before-reset` (the stable `KPCFix.ini`, `ScalePercent=200`). `KPCFix.ini` is now the repo file (`48789c94`); `KPCServerTools.ini` is the new default with `CoopCampaign`, `CoopIntro`, `TraceScripts`, `TraceTiming` and `AllowCheats` on and the start scoreboard left to the DLL default (`d007552a`; previous copy in `C:\KPCTestackup\psycho-20261113-234449-serverini-before-campaign`) |
  | Laptop (second player, client only) | — | `1e97026a` (needs `pkg-client-9a7eef01`) | — |
  | `C:\KPCTest\smoke1\Psycho` (disposable smoke server) | `6448dadf` | — | `C:\KPCTest\backup\psycho-20261113-175538-before-6448dadf` |

  Install with `tools/install_game.py --game-dir <folder> --client --server`. It updates an
  installed `winmm.dll` but never moves an earlier install over: run `Setup-KPCServerTools.cmd`
  in the game folder for that (close the game and `Psycho.exe` too).
* **Tests passed** for `6448dadf` / `9a7eef01`: server unit tests 26/26 (64/32-bit), emulator
  matrix 53/53 (three new `--winmm` cases; the matrix now runs in parallel, about 20 s), client
  suite 20/20, packaging of both ZIPs, native smoke in Coop, Normal and PassThrough mode with
  `winmm.dll` (`C:\KPCTest\smoke1\smoke-winmm-*.json`). **Not run:** `audit_sites.py`, two-PC test.

### Retest results (RT1–RT12, 2026-11-13) and what changed

| RT | Result | Change in this commit |
| --- | --- | --- |
| RT1, RT2, RT6, RT7, RT12 | PASS | — |
| RT3, RT4, RT5 | PASS except ammo: used ammo came back full after a world change (197 → 200) | Restoring a weapon also gave its stock ammo (`f34b7`); the excess over what the player had is now removed again (`coop_inv_ammo_excess`) |
| RT8 | Shards appear, but the window stays visible among them for a moment | Client `HideBrokenWindow=1` hides the window as soon as its shards are built |
| RT9 | The armour shard did not come back | The test targeted BlackDiamond; "armor shard" in the game means an armour piece (`0x27`–`0x2c`), which never respawned. Armour pieces now respawn; a returned one is picked up quietly (no cutscene) by a player who lacks it (user choice 1) |
| RT10 | Every Audio-screen control below the new checkbox was broken | The checkbox's click handler (`0xd590`) says "handled" for every click; clicks now reach it only on its box or label. Tooltip warns about multiplayer audio/narration sync |
| RT11 | PASS, but the belt lift strands the other players (design) | Open: user to choose |
| Extras | — | No 2-player minimum (`CoopMinPlayers=1`); server console `changelevel <world>`; `!goto <player>` / `!back` (`CoopTeleport`); `!abort` and completion go to a random deathmatch lobby (never a campaign world) with the campaign flags reset; client keeps rendering when minimised, input released (`KeepRendererOnFocusLoss`) |

## Retest plan

Server: the launcher with **Co-op campaign** and **Test logging**, or `Start-KPC-Selected-Server.ps1 -Map random -Cheats Enabled -Coop -TestLogging On`
(both write `[Coop] CoopSession=1`, `CoopCampaign=1`, `CoopIntro=1`, `TraceScripts=1`, `TraceTiming=1`). Host = the first player in
(`*` in `!coop`). Server console `kpc_cheats 1` unless a step says otherwise. Use two
different player names. Result: PASS / FAIL / not run, plus what was seen.

| # | Was | Do | Expected | Log marker (`KPCServerTools.log` unless noted) | Result |
| --- | --- | --- | --- | --- | --- |
| RA1 | RT10 | Start the server from the launcher; start the client and `Psycho.exe` | server starts; both programs start with sound (`winmm.dll` is in the folder) | `Loaded as winmm.dll; original server.dll unchanged` | |
| RA2 | N2 | Alone on the server: `!campaign water` | the campaign starts with one player | `change` | |
| RA3 | RT3–RT5 | Fire a few shots (e.g. 197 left), then: host takes an exit; `!kill`; `!abort` | the ammo count stays as it was (197) each time | `inventory-restored` | |
| RA4 | RT9 | Take an armour piece (first time: cutscene), step away, wait 20 s; walk over it again; let the other player (if it lacks the piece) walk over it | it comes back; whoever has it walks over it; one who lacks it gets it without a cutscene, and chat says `... picked up a returned armor shard` | `item-respawn scheduled/respawned`, `item-respawn quiet pickup` | |
| RA5 | RT8 | Break windows as the host and as the non-host | shards on both PCs, no window left standing among them | client `KPCFix.log` window lines | |
| RA6 | RT10 | Setup > Audio: use every control (sliders, Sound Quality, Enable Sound FX...), then the checkbox; read its tooltip | all controls work; the checkbox toggles; the tooltip mentions multiplayer audio/narration going out of sync | — | |
| RA7 | extra | Server console: `changelevel R1M2A`, then `changelevel` alone and `changelevel nowhere` | everyone goes to R1M2A; usage / unknown-world replies | `console` | |
| RA8 | extra | `!goto <other player>` (also a name prefix), then `!back`; try again within 3 s | moved next to the player, then back; the quick retry is refused | `goto` | |
| RA9 | extra | Host `!abort` during play, twice | everyone goes to a deathmatch map (a different one each time); no intro or scripted sequence runs there | `abort`, `change` | |
| RA10 | extra | Minimise the client during play (also mid-narration), wait 10 s, restore | the game keeps running and drawing while minimised (sound, music, animation); keys and mouse are not captured; the picture is normal after restore | client `KPCFix.log`: `renderer kept running` | |
| RA11 | RT11 | After the user's choice: R1M2A belt lift with both players | per the chosen fix | `exit-seen` | |

Anything else seen (crashes, wrong player affected, sounds behind the loading picture,
players stuck in the floor): add a line under the table with the world and time.

## Logs to collect

* Desktop: `C:\Games\Psycho\KPCServerTools.log` (server) and `C:\Games\Psycho\KPCFix.log` (client).
* Laptop: `KPCFix.log` in its game folder.
* Note the time of each failure; with `[Coop] TraceTiming=1` co-op lines carry `t=<ms>`.

## If a retest fails

* **RT1/RT5 (fresh start):** no `inventory-cleared` line → the start did not come through
  `start_pending` (`coop_session.h`), or `CoopKeepInventory=0`.
* **RT3/RT4 (persistence):** `inventory-saved ... weapons=0` while holding weapons → `dd170`
  (inventory list) does not see them; no `inventory-saved` → the snapshot saw the player as
  dead (`vtable+0x60`) or not in the live list; `inventory-restored ... gives=0` with items
  missing → `f3440` refused them (class or gauntlets rule, `f3467`–`f349b`).
* **RT6 (class):** `class-weapons-given=0` while slot 1 stays empty → the weapon definition
  (`[player+0x54+id*4]+8`, class `+8`, any-class byte `+0xc`) was unreadable, or the groups
  of four (3–6, 7–10, 11–14) do not match the slots; see `coop_class_weapons` in `proxy.c`.
* **RA3 (ammo):** the count still rises → `inventory-restored` should list the trim; the
  removal uses the inventory vtable `0x11ef98` `+4` (`coop_inv_trim_ammo` in `proxy.c`).
* **RA4 (armour):** no `scheduled` → the hide call `0xcc29e` was not reached for the piece;
  `respawned` but no quiet pickup → the touch handler's call `0xc837b` is not hooked
  (`CoopItemRespawnSec` must be non-zero) or the player was seen as dead / already owning it.
* **RA5 (windows):** a window still stands among its shards → check `HideBrokenWindow=1` in
  the client's `KPCFix.ini` and the window lines in `KPCFix.log`.
* **RA1 / RA10:** if the client or `Psycho.exe` misbehaves with `winmm.dll` present, run
  `Restore-Original-Server.cmd` to remove it and report; if the picture is broken after
  restoring the window, set `KeepRendererOnFocusLoss=0`.
* **RT11 (lift):** last time the log ended after `cs1belt.scr`; check whether
  `trigger_door liftpiece1/2 open` runs and whether the exit volume is reached.

## Open work

### User requests from the 2026-11-13 test (full text in the archive)

| # | Request | Status |
| --- | --- | --- |
| N1 | `!changelevel` accepts any world (PMxx, TUTORIAL) | open |
| N2 | No 2-player minimum for `!campaign` / `!changelevel` | done (`CoopMinPlayers=1`), RA2 |
| N4 | First armour shard per world: everyone watches the cutscene and gets the slot-1 weapon and armour | open |
| N5 | Campaign pickups keep everyone's weapons and armour equal; cheat items personal; a death restores exactly what the player had plus what was shared while dead | open |
| N6 | Ammo sharing switch: INI setting and host `!ammoshare on\|off` | open |
| N7 | `!op <name>` / `!deop`: an op may run host commands and skip cutscenes | open |
| N16 | Only host or op may skip cutscenes (needs N7; triage row 6) | open |
| N17 | `!reload` = `!changelevel <current world>` (needs N1) | open |
| N18 | Server console world change without dropping clients | console `changelevel <world>` done (RA7); the dropped-client "connected" state is still open |
| N15 | Wooden-box debris | open (windows done in `7623b69`) |
| N13 | Armour shard respawn | done: armour pieces respawn, quiet pickup (RA4) |
| N3, N8–N12, N14 | Start gauntlets, Gimmie ammo, refusal messages, class weapons, weapons across exits, fresh `!campaign`, Audio checkbox | done in `7623b69` (N8: stock behaviour), waiting for the retest |

### RT11: R1M2A belt lift (user to choose)

Picking up the belt (an armour piece) in the lift sends the lift down and strands the others.
Options: (1, recommended) move every player into the lift before the belt cutscene, as
single player has one player; (2) send the lift back up after the cutscene; (3) hold the lift
until every member stands in it; (4) leave it and rely on `!goto`.

### Code and other releases

* **Original game bug: destroying a jukebox leaves the music wrong** (user-confirmed
  2026-11-13 with `[KPCFix] Enabled=0`, so not a KPCFix regression; affects single player and
  any server). Same three scripts in R1M1A, R2M1A and R4M1A (`C:\KPCTest\scratch-campaign\objects.json`):
  * Using the jukebox: `jukeon` plays `jukenoise` and starts `musicfader`.
  * `musicfader`: `2 music intensity 9`, `20 music intensity 0` (silence), `70 music intensity 2`
    in R1M1A (plus `script_object_disable SwitchObj2` at 0 and re-enable at 70), `70 music
    intensity 1` in R2M1A and R4M1A; all `transition measure`. `song1` has no intensity-0 style.
  * Damaging it: `jukeoff` aborts `jukeon` and stops `jukenoise`, but never touches
    `musicfader`. So the music goes silent, plays a bar or two around 35–45 s, fades out, and
    returns only at `musicfader`'s 70 s step. Earlier KPCFix runs where it never returned were
    not matched for timing or trigger zones; recheck that during the fix.
  * **Wanted:** destroying the jukebox stops its music at once and returns to the level's normal
    music (R1M1A intensity 2 and `SwitchObj2` enabled; R2M1A and R4M1A intensity 1).

  Fix ideas (none started):
  1. **Object-module hook (preferred).** When a script named `jukeoff` starts in one of the three
     worlds, also run `script_object_abort musicfader`, the world's normal `music intensity N
     transition measure`, and in R1M1A `script_object_enable SwitchObj2`, through the engine's own
     script command dispatcher. The co-op work already hooks script events and the script clock
     (`TraceScripts`, `CoopScriptClock`), so start from that research. Single player runs
     `object.lto` in the client's in-process server, which KPCFix does not patch yet; the client's
     existing `LoadLibraryA` hook (used for `cshell.dll`) could install it for `object.lto` too.
     Server Tools already patches `object.lto` for dedicated servers, so one shared table of
     world → commands could serve both.
  2. **Property override at world load.** Hook the engine's property reads when the world creates
     `jukeoff` and fill a spare command slot. Not enough on its own: `jukeoff` has one free slot in
     R1M1A and R2M1A (`command8`) and none usable in R4M1A (its `command8` is the commented-out
     `;0 spawn gibs`), but the fix needs two or three commands.
  3. **Ruled out:** editing or shipping changed world files or `.rez` archives (the hard rule
     against game files), or a client-only music workaround that guesses when the jukebox died.

  Test: start each jukebox, damage it before 20 s and again between 20 and 70 s, and confirm normal
  music returns within a bar; also let one play to the end (unchanged), and check `Enabled=0` /
  the hook switched off still shows the original behaviour.
* **R6** (cost of walking the player list): measure first; see the R6 notes below.
* **Other releases** (user, 2026-11-13: the patch should work on every release eventually).
  The Audio checkbox exists only for the Europe/USA `cshell.dll` layout (`audio_vtable` and
  the checkbox entries are set only there in `source/src/profiles.h`); Germany and Italy fall
  back to the console variable. Supporting them needs the Audio screen vtable, the checkbox
  class, its image names and the widget positions found and verified in those two
  `cshell.dll` builds, plus their `test_focus.py` runs. Check the other shell-UI hooks in
  `profiles.h` the same way. Server Tools has one `object.lto` (`3a03f879`) for every
  release, so the server fixes apply everywhere.
* **Ported to `main` and released (2026-11-13):** client in `kpcfix-v1.1.3` (`8f1d29a`);
  Server Tools pinned as v1.0.2 on `main` but not tagged until the user's Windows checks
  (listed in `TESTING.md` on `main`, "Checks for the 2026-11-13 fixes"). Ported:
  `LocalServerMinStepMs=16` (client), `ConsoleKeyFix` (client), the co-op
  `CoopServerFrameMs` hook as stable Server Tools `[ServerTools] ServerFrameMs=16` (always on,
  not session-only), and the DDrawCompat default without the 60 FPS cap (VSync at the monitor
  refresh, `FpsLimiter = off`). Stable runners: `C:\KPCTest\pass2\matrix-stable.sh <dir> <server
  fixture root>` (Europe + USA, 12 cases, ~15 s) and `C:\KPCTest\clientbuild\suite-stable.sh
  <research root>` (four regions, 65 cases, ~2 min). Build with the Docker images, not WSL
  (WSL's GCC 15 / binutils breaks `objcopy`). The research ZIP's German folder holds a
  pre-1.13 build; use `C:\KPCTest\KPCFix-region-research` instead.
* **Open: server frame fix for the USA `server.dll`.** `ServerFrameMs` (stable) and
  `CoopServerFrameMs` (co-op) only hook the Europe-family `server.dll` `83b7bce8…`
  (`0x235b0` → `0x2a05f`, server object `+0x1f8/+0x214/+0x218`). The USA `server.dll`
  `93178237…` keeps original timing, so the R2M2A Ballbuster can still stall on a USA
  server; users are told so in `README.md` and `server-tools/README-Server-Tools.md`. Find
  the USA frame call, frame function and offsets (RE copies in `C:\KPCTest\re`, research
  folder `patched to 1.13/USA/server.dll`), add a second profile to the frame hook in both
  `proxy.c` versions, extend the `check_proxy.py` frame test to the USA fixture, then remove
  the USA notes. Details: `COMPATIBILITY-NOTES.md` on `main`, "Server frame timing".
* **Still to port to `main` later** (not co-op specific; the user is leaving them for now):
  * `ChatBlocksActions` (`console.c`, cshell `OnCommandOn` slot `0xff724` plus the
    `client.exe 0x23c84` input-state read): typing a chat message no longer fires bound
    actions (noclip, invuln, movement).
  * `KeepAudioOnFocusLoss` + `ResyncAudioOnFocus` (`focus.c`): alt-tab no longer pauses
    sound, music or a local game, so narration stays in sync. The Setup > Audio checkbox
    (`ui.c`) is wanted too, but only in its untested-branch form (RT10 click hit-test,
    T19 lit state, position above Enable Sound FX; `checkbox_hit_box`/`_label` in
    `profiles.h`); the `coop/mvp-lobby` checkbox breaks the stock Audio controls.
    `KeepRendererOnFocusLoss` was left out of the port plan (untested).
  Port by hand, not by merging: `main` now has `console.c` with the console key only, so add
  the chat pieces to it, plus the `shell_layout.h`/`profiles.h` fields and `kpcfix.c` hooks,
  and extend `test_console.py`; bring `focus.c` and `test_focus.py` across. Leave out the
  window-shard diagnostic, loading art, chat time, cutscene camera and server timeout.

### Older open items (2026-11-12 triage)

1. **World held behind the loading picture** (row 9): no simulation until every member has spawned.
2. **Only the host skips cutscenes** (row 6, with N7/N16): find the client message → `endingscript` path.
3. **Collectables HUD in co-op** (row 10) and the **Disconnect menu item** (row 21).
4. **Switches/levers host gate** (row 3: `CSwitch` `44dd6` and other command-list owners).
5. **Collision-checked spawn allocator** (`coop/GENERAL-ALLOCATOR.md`) and a protected
   waiting state between client enter (`0xfdfb0`) and spawn (`0xfec56`).
6. Breakables missing on one client until minimise/restore (row 12); spawns trapped in the
   ground in R1M3A (row 17, needs a log with `spawn-spread` lines).

## Live-confirmed (two PCs)

From earlier tests: R1M2B lobby, `!campaign` start, loading picture and spawn hold (players
enter alive), R1M0 and R1M1A intros, single-player start in R1M1A, gauntlets pickup present,
gauntlets equip sequence, the R1M1A lever (door, camera, Headless), natural exits, music,
health bars, monster spawners (host-triggered). Single player: `LocalServerMinStepMs=16`
fixes the R2M2A Ballbuster above 60 FPS.

From the 2026-11-13 test: no water sound behind "Connecting"; `!difficulty` and the R1M0
intro without a pause; the non-host is refused at exits and the R1M1A victory trigger and
the host starts both; nobody is teleported by an armour pickup; no friendly fire; shared
weapons, pamphlets, ammo amounts and the secret message; spawners measured from the nearest
player (R4M1A); per-player cheat keys with the chat line; `!kill`, `!all`; weapons respawn on
a deathmatch map; the powerup effect for the watcher; `!changelevel` (host only, loading
picture, unknown world refused, the next exit continues); R2M2A Ballbuster through the garage
door; smooth cutscene camera.

From the RT1–RT12 retest (2026-11-13): a start from the lobby clears everyone's items (RT1);
the gauntlets sequence moves only the picker and both get them (RT2); weapons, armour and
class are kept across exits, `!kill` and `!abort` (RT3–RT5, ammo fixed since); class changes
add the class weapons, also without cheats (RT6); one refusal message per refused cheat
(RT7); window shards on both PCs (RT8, window hide fixed since); the R1M2A belt lift and
its exit (RT11); Gimmie's ammo cap (RT12).

## Built earlier, not yet confirmed on two PCs

| Item | Setting / command |
| --- | --- |
| Natural exits during play: host only (mirror re-armed after a refusal) | built in |
| `!abort` resets everything from any state | built in |
| Keys and tickets for every player, with the finder's name | `CoopSharedItems=1` |
| Server frames ≥ 16 ms (co-op Ballbuster, real game time) | `CoopServerFrameMs=16` |
| Client: chat messages 8 s | `ChatMessageSeconds=8` |
| Client: a held spawn reply is dropped on a new connection (code review R4) | built in |

## Reference: code review 2026-11-12 (R1–R6)

Source: review of `proxy.c` / `coop_session.h` / client `loading.c`, and the last run in
`C:\Games\Psycho\KPCServerTools.log` (build `7dbba5eb`, ended 19:14, two players, worlds
R1M0, R1M1A, R4M1A, R4M1B, R4M1C, R4M2A, R3M4B). R1, R2, R3 and R5 were fixed in Server
Tools `7d01059a` and R4 in client `cf896d63`; both were in use for the 2026-11-13 two-PC test
(no log-budget or exit-flood problems were reported). R6 is open: see the pickup notes.

| # | Finding | Evidence | Proposed fix |
| --- | --- | --- | --- |
| R1 | The co-op log stops for good after 3000 lines per server process | `coop_log` counts against `COOP_LOG_BUDGET` and nothing resets the counter (`proxy.c`). The 19:14 run wrote 1224 lines in 7 worlds, so a 48-world campaign loses its log around world 17 | **fixed** (`7d01059a`): 1000 lines per map epoch, one `log-budget` line when it runs out, renewed by the next world (`check_proxy.py`) |
| R2 | A non-host using an exit mirror floods the log | 357 `exit-rearmed` lines in that run (from R4M1C on): every refused use re-arms the deco and logs, without the 8-per-world cap that `exit-seen` has. Each line opens and closes the log file on the game thread | **fixed** (`7d01059a`): at most 8 `exit-rearmed` lines per world; the re-arm itself is unchanged. No dedicated test |
| R3 | "Only the host can take the exit" is said once per world | `coop_session_exit_by` announces only on the first refusal of an epoch; later attempts look like a dead mirror | **fixed** (`7d01059a`): said at the first refusal in a world and again at most every 5 s (`test_coop_campaign.c`) |
| R4 | A held spawn reply can be sent after the connection is gone (not observed) | client `loading.c`: `hold_tick` releases the held `0x17` reply when the minimum time passes; a new load clears it, a disconnect does not | **fixed in part** (client `cf896d63`): a new connection drops the held reply instead of sending it (`test_loading_art.py`). Still open: a disconnect to the menu with no new connection sends it when the minimum time passes; the shell state that marks "disconnected" is not identified |
| R5 | Item id is read from the item after the stock pickup ran (not observed) | `coop_item_pickup` reads `item+0x54` again after `cc180`; a non-respawning item may already be removed | **fixed** (`7d01059a`): the id is read once before the stock pickup and the item is not read afterwards. No dedicated test (the existing shared-item and ammo checks pass) |
| R6 | Player list is walked with guarded reads many times per frame | `coop_live_objects` (two `VirtualQuery` calls per node) runs for every spawner update, pickup and script event | open; see "R6 pickup notes" below |

### R6 pickup notes (open; to be done in a separate session)

Goal: stop walking the engine's player list with guarded reads many times per server
frame. This is a performance change only; behaviour must not change.

* **What is slow.** `coop_live_objects` (`proxy.c`) reads the list at
  `[0x10157724]+4` (nodes `{HOBJECT, next}`) through `coop_rd`, and every `coop_rd` is a
  `VirtualQuery` system call (`bsp_mem_readable`): two per node plus two for the head.
* **Callers.** Hot: `coop_spawner_target` (site `0xd51fb`, once per spawner per update,
  with `CoopSpawnerNearest=1`) and `coop_member_player` (from `coop_inv_frame`, per
  member, every frame while a restore is pending and once a second otherwise). Per
  event: `coop_presence`, `coop_exit_trigger`, `coop_player_event_all`,
  `coop_view_set_all`, `coop_view_reset_all`, `coop_share_ammo`, `coop_item_pickup`,
  `coop_inventory_give`, `coop_item_respawn`, `coop_powerup_frame`.
* **Not measured yet.** The cost is an estimate from the code (R4M1A has dozens of
  spawners; the server runs 62.5 frames a second). Measure first: count
  `coop_live_objects` calls per second behind `TraceTiming=1` and log one line every
  10 s. If it is a few hundred a second or less, close R6 as "not worth it".
* **Proposed change.** Keep one cached copy of the list (handles and count) with a
  frame stamp. `coop_frame` (site `0xffbbf`, once per server frame) advances the stamp;
  `coop_live_objects` refills the cache when the stamp differs and otherwise copies it.
  Also invalidate in `coop_spawn_hook`, `coop_client_enter_place` and `coop_next_level`,
  where the list changes inside a frame.
* **Risk to check before coding.** A cached HOBJECT is passed to engine calls
  (GetObjectPos `[de+0x1d0]`, `ee7d0`). If the engine can remove a player object in the
  middle of a frame (disconnect), a cached handle would be stale for the rest of that
  frame. Whether removal is deferred to the end of the frame is **not verified**. If it
  cannot be shown, cache only inside `coop_spawner_target` and keep every other caller
  on the live walk.
* **Tests.** `tests/check_proxy.py` writes the fake player list straight into memory
  and then calls one hook (`roster(...)`, the spawner block near "co-op spawners:
  activation radius"). With a cache those steps need the frame hook called (`frame()`)
  or an explicit invalidation after each `roster`; otherwise they read the old list.
  Add a check that two spawner calls in one frame walk the list once (count the
  `VirtualQuery` stub calls) and that a roster change is seen on the next frame.
* **Related, same session if wanted.** `log_file` opens and closes
  `KPCServerTools.log` for every line (`CreateFileW` / `WriteFile` / `CloseHandle`) on
  the game thread. Lines are now bounded per world (R1, R2), so this is minor; a kept
  handle would need a flush policy so a crash does not lose the last lines.
* **Done means.** Unit tests 26/26, the full `matrix-coop.sh`, `check_live.py` both
  modes, the rebuilt DLL and `SERVER_TOOLS_BUILD.json` copied in,
  `SHA256SUMS-Server-Tools` refreshed from the git index, the R6 row below updated with
  the build hash, and the measured before/after call counts written here.

## Reference: playtest 2026-11-12 triage (rows 1–28)

Evidence: `C:\Games\Psycho\KPCServerTools.log` (R1M0 to R1M2A, R4M1A, R3M0), object.lto
static analysis. **fixed** = committed, unit and emulator tested, not yet two-PC tested.

| # | Report | Cause | Status |
| --- | --- | --- | --- |
| 1 | Player 2 teleported in front of the camera when the host took an armour piece; again with the demon gauntlets | `ee900` (IsPlayer) takes a *pointer* to the HOBJECT; the activator hook stored the pointer, so it never matched and every personal `player` command went to the first player (log: `-> first player` everywhere). The second report was still on the old DLL `9ff661bf` | **fixed** in `71fb1df` (cutscene, camera and invulnerability for everyone; teleport/animate/lookat only for the picker) |
| 2 | Unsheathe sound plays at the touch, not near the end of the cutscene | armour touch `c8340`: multiplayer sets the give flag `0x1013e1b8` to 1 (`c8348`), single player uses ReallyGiveArmor (false) and leaves it to the script's `giveitem` | **fixed** (`c8348` gate reads 0 with CoopScriptEvents; armour shared only when actually given at the touch) |
| 3 | Non-hosts start level-ending sequences (R1M1A victory). Follow-up: in R4M1A the spawner room did nothing for a non-host | walk-in `CTriggerCommand1` → `r1m1a_victory.scr` → `trigger_generic CExitTrigger0`; only the exit itself was host-gated, so the cutscene played and then the exit was refused | **fixed** for `CTriggerCommand` (`CoopHostSequences`): the trigger stays armed for the host. Switches/levers (`CSwitch` `44dd6`) and other command-list owners are not gated yet. The R4M1A report was **not** this gate (the gate already uses `coop_commands_reach_exit`; no `sequence-refused` line in the log): see row 22 |
| 4 | After a refused non-host mirror click the host's click did nothing | mirror = `CModelDeco` use command `trigger_generic Gubba`; the deco marks itself used (`+0x1a1`) before the exit refusal | **fixed** (deco re-armed when its exit is refused) |
| 5 | Key pickup chat message | campaign keys are `inventory add key` (CPickupTrigger), not items; the inventory event `b6730` gives to the **first player only** (so keys went to the wrong player) | **fixed**: every player gets it, chat `<name> found the key (everyone has it)`; tickets too. `inventory remove` still only the first player |
| 6 | Only the host may skip cutscenes (space / mouse1) | skip runs the running script's `endingscript` (list `c50c0`, registry `0x1015a0b8`); the client message → skip path is not found yet | open |
| 7 | R1M2A → R1M2B did not change after the belt lift | log ends after `cs1belt.scr`; there is no exit in the lift: the exit volume `CExitTrigger0` is at (1504,−208,−2944), the lift drops at x≈−280. Needs a retest: does the lift go down (`trigger_door liftpiece1/2 open` at 8.8 s) and is the exit volume reached? | needs retest |
| 8 | Items/ammo/armour lost on world change | multiplayer respawns with a fresh inventory. Inventory mapped: weapons 2–14 and ammo `0x11`–`0x14` are entries of `[player+0x54]` (`dd170`/`dd190`), armour pieces bits of `player+0x260` (`f25f0`), armour shards item `0x1e` (`BlackDiamond`; `powerShardItem_t` definition `0x1421f8`), class `player+0x264` | **fixed** (`CoopKeepInventory`, `CoopKeepOnDeath`): per-member snapshot every second while alive and before every world change (also `!abort`); 400 ms after a spawn the class and whatever is missing are given back (`coop_inventory.h`, `test_coop_inventory.c`) |
| 9 | World active behind the loading picture (pickup sounds, enemies) | the server simulates as soon as the world is loaded; needs the world held (e.g. `coop_server_frame` skipping simulation, but not networking) until every member has spawned | open, design needed |
| 10 | Secrets/collectables HUD shows the deathmatch player list | cshell stats panel (vtable `0xff3c0`, draw `0xa4150`): `0xa41c8` calls the multiplayer test `0xcab80`; multiplayer jumps to the player list `0xa42cd`, otherwise it draws seven counters (`0xa06e0`) from globals `0x201c4cf0`–`0x201c4d08`. Their writer is not located (no direct stores; filled through a pointer or copy) | open. Patch plan: replace the `0xa41c8` call with "multiplayer and not a campaign world" (`kpc_campaign_world`, loading.c) once the counters are known to be filled in co-op (likely needs the server to send the secret/collectable message to every member) |
| 11 | Golden mirror invisible in the R1M1B lever cutscene | `camera_path` moves only the first player's server view position (`c2c9c` → `ef920`), so other clients are not sent what the camera sees | **fixed (probable cause)**: view position applied to every player |
| 12 | Breakables missing on one client until minimise/restore (doorway in R1M1A) | unknown, client side (object creation during the spawn hold?) | open |
| 13 | Chat typing triggers bound keys (`I` invulnerability) | the `OnCommandOn` filter (6d91b5b) was active in the playtest, but Invuln never goes through it: stock cshell Invuln (`0xc3a00`) is `ret`; the **server** polls `IsCommandOn(client, 93)` (object.lto `0xef1b4`, NoTarget 91 at `0xef204`) from the command bits client.exe sends every update | **fixed** in the client: client.exe's command update reads input-enabled (`0x23c84`, `[cm+0x1014]`) as 0 while the message editor types, so the frame's command buffer is cleared (no movement, fire or cheat reaches shell or server). `test_console.py` |
| 14 | Ammo: pickups for everyone, consumption personal | — | **fixed**: received amount shared per pickup |
| 15 | Keep audio when minimised, always; option in the Audio menu (spare space) | sound pause `0x6d5c`/`0x6cdb`; music pause/resume in OnEvent 4/3 (cshell `0xc80e2` → `0xd9ad0`, `0xc80ba` → `0xd9b00`); local server paused through the active-window test `0x157c5` (stock `alwaysfocused` overrides only that). Pausing only the audio in single player would let narration run ahead of the paused script, so nothing pauses | **fixed**: with `KeepAudioOnFocusLoss` on (console variable, INI default 1) nothing pauses in any mode; Setup > Audio > "Keep Running When Minimised" checkbox below Sound Quality. The Audio screen's 8-widget array is full (`+0x64`..`+0x80`, selection `+0x84`, fixed index tables `0x109d18`), so the box is a KPCFix-owned instance of the game's checkbox class (`0xd060`, images `0xd1b0`) driven from the screen's vtable slots (`0xfd264` +8..+0x18); mouse only, no keyboard selection. `test_focus.py`. **Needs an in-game look** (position, label font, click) |
| 16 | Lobby: random deathmatch map with its own spawn points | — | **fixed**: launcher `-Map random` → PM01–PM10, `CoopLobbyWorld`; stock spawns in deathmatch worlds |
| 17 | Spawns trapped in the ground (R1M3A stone staircase near the fat lady) | not in the logs yet (that run did not reach R1M3A). R1M3A uses the single-player start plus spawn spread for later players, so the spread's checked spot is the first suspect | needs the log of a run with the trap (`spawn-spread` lines) |
| 18 | Server prints `Missing sound file "monsters/aclown/aclowntaunt.wav"` | stock data bug: PSYCHO.REZ references the file but does not ship it | no action (harmless) |
| 19 | Breakables (windows, boxes) throw no shards in multiplayer | No game-type gate on either path. **Windows**: object.lto `0x483cc` clears the window's `FLAG_VISIBLE` (`0x483fd`–`0x48413`), then sends message `0x39` (object, effect `0x4b`, vectors, `shard_size`) to all clients; cshell OnMessage `0xcb194` → registry `0x80020` → shard manager `0x2019e6c0` handler `0x5bd30`, which builds shards from the window's world-model polygons (client API `+0x2c0`/`+0x2c4`/`+0x2c8`) and exits only if the object handle is null ("not in the PVS", `0x5bdaf`). Suspect: on a remote client the now-invisible window is already gone when the message is read (a local game shares the object). **Boxes**: server debris (`0x754d1`) runs only when the server's `GibDetail` (`0x1015bb94`, default 0.0 if the console variable is missing) is non-zero; `dedicated.exe` seeds `gibdetail` "2" and the server cfgs set 2 | diagnostic added: `[Multiplayer] LogWindowBreaks=1` logs each break message and whether the window object was present (`KPCFix.log`, `test_console.py`). Next playtest: break a window on a non-host and send `KPCFix.log`. If "MISSING", fix on the server (Server Tools: hide the window one frame after the message). Boxes: check the server log/console `GibDetail` |
| 20 | Per-player cheats "gimmie gimmie gimmie" and "next armor" | message `0x3b` → `fb910` (call `0xfead7`): flags table `0x1016d8a0` (flag 2 = refused in game types 1–4, flag 1 = `[de+0xec](client)`), handlers `0x1016d98c` (`fa8f0`, `fab30`, `fb7b0`) already act on the sender's player | **fixed** (`CoopCheatKeys`): the keybinds work per player under `kpc_cheats 1`; chat `!gimmie` / `!nextarmor` kept. Earlier note: **User wants keybinds instead of chat**: the stock actions already send it: `GimmieGimmieGimmie` (98, cshell `0xc4c90`) and `NextArmor` (100, `0xc4120`) send message `0x3b` with cheat ids `0x2f` / `0x39` (`0xc3680`). Server follow-up: Server Tools answers those per player (same permission as Invuln) and the chat commands can go |
| 21 | Disconnect button in the in-game menu | the main menu (init `0x21320`) is five **image** buttons, not text: Game, Setup, View, Continue (shown when connected or a game is loaded, x 8, y `0x158`), Quit; each loads `menu/main_<g,s,v,c,q>_{n,mo,s,glow}.pcx` (`0x10b160`..) through vtable `+0x40`, at fixed offsets `+0x64` stride `0x12c` with fixed 5-item loops (`0x21790`). No Disconnect image exists and assets are not shipped | blocked (images). Proposal: draw a text item with the menu's bitmap font in the 48-px gap between View (ends y 296) and Continue (y 344), driven like the Audio checkbox (wrap the menu's draw / mouse / click slots), and on click run the shell's own disconnect path (OnEvent 1 handling `0xc7f6d` / `SetMenuMode 0xc6f30`); needs the disconnect call located first |
| 22 | R4M1A: a non-host walking into the spawner room spawned nothing; the host did | spawners (`d5050`, shared by `CMonsterSpawner`/`CSpawner`/`CBaseSpawner`) measure `ActiveRadius` (1024 in R4M1A) from a reference object `+0x170`, which activation sets to the **first** player in the list (`d54c8`) | **fixed** (`CoopSpawnerNearest`): the position read `0xd51fb` uses the nearest live player when the reference is a player |
| 23 | Cycle player class per player, keybind and chat | stock `CyclePlayerClass` (`0x31`, `fab30`: `+0x264 = (+0x264+1)&3`, `f2090(1)`), refused in multiplayer like row 20. Class 0 Starbearer, 1 Beast King, 2 Celestial, 3 Demon; the client's confirmation `0x3e`/`0x31` is a no-op (`cab70`), so the change is server-side only | **fixed**: keybind (row 20) and `!class [starbearer\|beastking\|celestial\|demon]`, `kpc_cheats 1`; inventory kept (weapons of another class are given with the class set to theirs for the call) |
| 24 | Pickups should respawn (15 s, spot clear) | stock deathmatch respawn exists: `cc570` keeps a hidden item and counts `Respawn Time` `+0xb8` down when `Respawn` `+0xb4` is set; the update calls `cc080` (`cbfff`). Campaign worlds author `Respawn=false`, so taken items were removed | **fixed** (`CoopItemRespawnSec=15`): set at the hide call `0xcc29e` for weapons, ammo, shards, health and power-ups; the respawn waits while a player's box overlaps the spot. "After the armour-shard pickup animation": the shard has no separate pickup animation (touch `cc530` gives `0x1e` and the item is hidden at once), so the 15 s count from the pickup. Not two-PC tested |
| 25 | Powerup animation only seen by the picker since row 1 | `player powerup` (`f6ff0`) is not per client: it broadcasts `0x39` (activator object, effect `0x35`) to every client; `player animate` is a server-side animation of the activator's model. Probable cause: an observer's client drops the effect when it does not hold the just-teleported activator object yet | **probable fix** (`CoopPowerupResend`): the effect is re-sent to every other member 400 ms later. Needs the two-PC test; if still invisible, check the client SFX handler for effect `0x35` (cshell table `0x201bf94c`) for a local-player test |
| 26 | `!kill` | — | **added**: damage entry `vtable+0x50` with the damage-brush record (`2f1c0` layout), damager = the player itself (passes `CoopFriendlyFire`), kill-invulnerable set |
| 27 | Chat "everything" cheat | — | **added**: `!all` (`kpc_cheats 1`): every armour piece, every weapon, 1000 of each ammo (capped by `f3440`) |
| 28 | Host `!changelevel <world>` | — | **added** (`coop_session_changelevel`, `coop_campaign_find`): campaign mode, host only, `R1M2A` or `worlds\realm1\R1M2A`; from the lobby or during play; same world change as an exit; the selection becomes "from <world>" so natural exits continue along WORLDORDER (decision: carries on, not a one-off); `!abort` returns to the lobby and the full selection |

## Tooling

* Installing a new build on the test PCs: [UPGRADE-TEST-INSTALL.md](UPGRADE-TEST-INSTALL.md).
* `C:\KPCTest\pass2\`: `build-coop.sh <dir>`, `unit-coop.sh`, `matrix-coop.sh <dir>` (parallel, one job
  per CPU, `JOBS=n` to change; outputs in `<dir>\matrix-out\`; `matrix-coop.serial.sh` is the old
  one-at-a-time runner). They read the checkout
  `C:\Users\magi\Documents\GitHub\KPCFix` (see `CLAUDE.md`).
* `C:\KPCTest\clientbuild\`: `build-coop.sh <dir>`, `suite-coop.sh`.
* `coop/tools/Invoke-CoopServerSmoke.ps1` (native server smoke; never on a folder with
  a running server).
* RE: `C:\KPCTest\re-venv` (capstone, pefile), `C:\KPCTest\re` (original binaries),
  `C:\KPCTest\scratch-*` notes, `C:\KPCTest\scratch-campaign\objects.json` (every
  world's objects and properties). Docker Desktop must be running for builds.
