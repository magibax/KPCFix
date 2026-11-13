# Co-op handoff: next sweep = two-PC test results (2026-11-12)

How everything works: [COOP-MVP.md](COOP-MVP.md). Settings: [CONFIGURATION.md](CONFIGURATION.md).
Project rules and tooling: [CLAUDE.md](../CLAUDE.md). Older evidence:
[archive/COOP-MVP-EVIDENCE-2026-11-12.md](archive/COOP-MVP-EVIDENCE-2026-11-12.md).

**The next session starts from the user's two-PC test report.** Fill in the "Result"
column of [Test plan and results](#test-plan-and-results) (or paste the report in chat),
read the logs listed under [Logs to collect](#logs-to-collect), then for each failure
follow [If a test fails](#if-a-test-fails) and update the triage table (move confirmed
items to "Live-confirmed").

## Where things stand

* **Branch:** `coop/mvp-lobby` (local commits since `417876e` are not pushed; `main`
  untouched).
  * `5c8363f` Server Tools: spawner radius from the nearest player, cheat keybinds,
    inventory persistence, pickup respawn, powerup re-send, `!kill` `!all` `!class`
    `!changelevel` (prompt B of [archive/NEXT-PROMPTS-2026-11-12.md](archive/NEXT-PROMPTS-2026-11-12.md)).
  * `2539a18` and the following docs commit: findings, install, this handoff, `CLAUDE.md`.
  * Prompt A (client: Audio checkbox state and position) runs in its own session on
    worktree branch `claude/zealous-cori-401c64` (`.claude/worktrees/zealous-cori-401c64`),
    started at `417876e`. Not merged here. Its build needs a new laptop package. Both
    branches edit this file: expect a small merge conflict.
* **Installed** (desktop `C:\Games\Psycho` = game client + host + dedicated server;
  `C:\KPCTest\smoke1\Psycho` = disposable smoke server):

  | Part | Hash | Commit | Backup of what it replaced |
  | --- | --- | --- | --- |
  | Server Tools `server.dll` | `7dbba5eb` | `5c8363f` | `C:\KPCTest\backup\games-psycho-20261112-before-7dbba5eb` (previous `035f23a2` files; the moved proxy as `server.dll.active-moved`); smoke1: `C:\KPCTest\backup\smoke1-20261112-before-7dbba5eb` |
  | Client `dinput.dll` | `2ab53bd7` | `ea4d168` | `C:\KPCTest\backup\games-psycho-20261112-before-2ab53bd7` |

  **Update, 2026-11-12 23:14 (desktop only; uncommitted working tree):** `tools/install_game.py`
  installed Server Tools `7d01059a` as `server.dll` and client `cf896d63` as `dinput.dll`, and added
  `FpsLimiter = off` to the desktop's `DDrawCompat-CLIENT.ini`. Backup of the replaced files
  (the `7dbba5eb` proxy, the `2ab53bd7` client, the old INI and docs): `C:\KPCTest\backup\psycho-20261112-231420-before-7d01059a`.
  `smoke1` is unchanged. New laptop package: `C:\KPCTest\pkg-client-cf896d63` (not yet on the laptop).
  The rows above describe the previous install.

  **Update, 2026-11-13 14:17 (desktop; uncommitted working tree on `coop/mvp-lobby-untested`):**
  Server Tools `dcc66b4c` as `server.dll` and client `1e97026a` as `dinput.dll`
  (`tools/install_game.py --client --server`; the script does nothing without those flags).
  Backup: `C:\KPCTest\backup\psycho-20261113-141719-before-dcc66b4c`. Laptop package:
  `C:\KPCTest\pkg-client-1e97026a` (not yet on the laptop). These builds carry the fixes in
  "Fixes for the T results" below.

  The laptop needs only the client: package `C:\KPCTest\pkg-client-2ab53bd7` (unchanged by
  `7dbba5eb`; Server Tools runs only on the hosting PC).
* **Tests passed** for `7dbba5eb`: server unit tests 26/26 (64/32-bit), emulator matrix
  48 modes, `check_live.py` (both modes), native smoke Coop/Normal/PassThrough on `smoke1`
  (`C:\KPCTest\smoke-7dbba5eb-*.json`). Client (unchanged): suite 20/20, `audit_sites.py`.
* **Decisions (answered 2026-11-13):** see "Two-PC results 2026-11-13" below.

## Test plan and results

Server: `Start-KPC-Selected-Server.ps1 -Map "worlds\realm1\R1M2B" -Cheats Enabled -Coop`
(`[Coop] CoopCampaign=1`, `CoopIntro=1`, `TraceScripts=1`). Host = the first player in
(`*` in `!coop`). Server console `kpc_cheats 1` unless a step says otherwise. Result:
PASS / FAIL / not run, plus what was seen.

| # | Row | Do | Expected | Log marker (`KPCServerTools.log`) | Result |
| --- | --- | --- | --- | --- | --- |
| T1 | — | Connect from the server browser | no water sound behind "Connecting" | — || PASS |
| T2 | — | Host `!difficulty coldsweat`, then `!campaign water` | R1M0 intro starts without a pause | `intro-start` || PASS |
| T3 | 1, 3 | R1M1A: non-host walks into the exit and the victory trigger; non-host takes the gauntlets | exit and victory refused ("Only the host"), host can start both; gauntlets sequence moves only the picker; both get the gauntlets | `exit-refused`, `sequence-refused`, `-> activator` || PASS: exit and victory refused for the non-host, host starts both; nobody is teleported by an armour pickup any more. Gauntlets sharing **not testable**: both players spawned with gauntlets + melee weapon although nothing was selected in the launcher (see N3) |
| T4 | — | Shoot each other | no damage | — || PASS |
| T5 | 14 | One player picks up a weapon, a pamphlet, ammo; find a secret | the other gets weapon/pamphlet and the ammo amount; both see the secret message | `shared item=`, `shared ammo=`, `secret found` || PASS. Wanted: a switch for ammo sharing only (N6) |
| T6 | 22 | Host `!changelevel R4M1A`; the non-host walks alone into a spawner room while the host stays back | monsters spawn | `spawner reference moved to the nearest player` || PASS |
| T7 | 20, 23 | Each player presses GimmieGimmieGimmie, NextArmor, CyclePlayerClass (bind them in Setup > Controls) | only the presser gets items / changes class; chat `<name> used ... (key)` | `cheat [Co-op] ... (key)` || PARTIAL: only the presser gets items, chat line shown. Gimmie via the key does not add 200 per press: ammo stops at 200 (stock adds 200 each time up to the max). `!all` covers "max everything" (N8) |
| T8 | 20 | `kpc_cheats 0`, press the keys and type `!all` | "Cheats are disabled", nothing changes | — || PASS, but the refusal messages differ between cheat types and not every refused type says anything (N9) |
| T9 | 23 | With weapons and ammo: `!class demon`, then the cycle key 4 times | same weapons, armour, ammo afterwards; model changes | `is now Demon` || FAIL: after `!class` / the cycle key the slot 1 melee and slot 6 ultimate (class-specific) are lost; armour kept (N10) |
| T10 | 8 | Collect weapons, armour pieces, shards, ammo; host takes the exit | all back about 0.5 s after spawning in the next world (both players) | `inventory-saved`, `inventory-restored ... gives=` || FAIL: armour kept across the exit, weapons not (N11) |
| T11 | 8 | Host `!abort`, then `!campaign water` | items kept in the lobby and in R1M0 | `inventory-restored` || PASS as built (items kept on `!abort` and into R1M0), but **not wanted**: a fresh `!campaign` must clear every player's items and ammo; only R4M4B -> R5M1A keeps them (N12) |
| T12 | 26, 8 | `!kill` | normal death and respawn; items back | `used !kill`, `inventory-restored` || PASS |
| T13 | 27 | `!all` | all armour, weapons, full ammo, only for the typer | `used !all` || PASS |
| T14 | 24 | Take a weapon, ammo and an armour shard; then stand on a taken spot | each returns after 15 s; waits while someone stands on it; armour pieces, pamphlets, keys never return | `item-respawn scheduled/waiting/respawned` || FAIL: an armour shard did not respawn with nobody on the spot; weapons respawn as expected on a deathmatch map (N13) |
| T15 | 25 | One player takes an armour piece, the other watches the cutscene | watcher sees the powerup glow/animation | `powerup effect 0x35 re-sent to 1 observer(s)` || PASS |
| T16 | 28 | Non-host `!changelevel R1M2A`; host `!changelevel worlds\realm1\R1M2A` from the lobby and again during play; `!changelevel R9M9Z` | refused / loading picture, everyone arrives / "Unknown world"; the next natural exit goes to R1M2B | `change [Co-op] Host changed level` || PASS |
| T17 | 7 | R1M2A belt lift | lift goes down, the exit at (1504,−208,−2944) is reachable and changes to R1M2B | `exit-seen` || not run |
| T18 | 19 | Break a window as the non-host | shards visible on both PCs | client `KPCFix.log` window-break line (`LogWindowBreaks=1`) || FAIL: the window goes from intact to gone with no shards (screenshots); wooden boxes throw no debris either (row 19) |
| T19 | 15 | Setup > Audio checkbox (needs the prompt A client) | lit = on, placed in the gap above Enable Sound FX | — || FAIL: the Prompt A client was never built (see below); the box is still below Sound Quality, shown unlit while on. Wanted: lit = on, and a better position (N14) |
| T20 | — | R2M2A garage; any cutscene camera | Ballbuster clears the door; camera not choppy | — || PASS |

Anything else seen (crashes, wrong player affected, sounds behind the loading picture,
players stuck in the floor (row 17)): add a line under the table with the world and time.

## Fixes for the T results (2026-11-13, Server Tools `dcc66b4c`, client `1e97026a`)

The T results were taken on the build of `coop/mvp-lobby-untested` (Server Tools
`7d01059a`, client `cf896d63`). Tests passed for the new builds: server unit tests 26/26
(64/32-bit), emulator matrix 50/50 (two new `--no-window-shards` runs), `check_live.py` both
modes, client suite 20/20, packaging of both ZIPs. **Not run:** native server smoke,
`audit_sites.py`, two-PC test.

| T | Cause (evidence) | Fix | Retest |
| --- | --- | --- | --- |
| T10 | The snapshot asked `f25f0` about weapons; `f25f0` answers only for armour ids `0x27`–`0x2c` (disassembly), so every `inventory-saved` line in the desktop log has `weapons=0` | Weapon ownership read from the inventory list, `dd170(inv, id)` (the test `f3440` itself uses). The same bug made chat `!gimmie` always pick BeastClaws and `!all` re-give every weapon; both fixed | T10, T12 |
| T9 | `f2090` (class apply) does not touch the inventory; the slots show the **current class's** weapon, which the player did not own | Class change adds the new class's weapon in each group of four (ids 3–6 melee, 7–10, 11–14) where the player owns a class weapon (`def+8` class, `def+0xc` any-class read at run time); old weapons kept. `class-change` log line | T9 |
| T11, T3 | The lobby (PM05) gives gauntlets (its pickups; log `respawned item=Gauntlets` in LOBBY); the snapshot taken at `!campaign` (`armour=1`, log line 3015) was restored in R1M0/R1M1A | A start from the lobby (`!campaign`, `!start`, lobby `!changelevel`) clears every snapshot (`inventory-cleared`). `!abort` and changes along the campaign (R4M4B → R5M1A too) keep them | T11, then T3 (gauntlets sharing) |
| T8 | Same text everywhere, but one 3 s throttle shared by every key and player, so a second refused key said nothing (and an unsigned compare let a repeat through 1 ms in 2) | One message for every refused cheat: `... (host console: kpc_cheats 1): <cheat> refused for <name>`, throttled per player and cheat (signed compare) | T8 |
| — | Decision: `!class` and the class-cycle key need no `kpc_cheats` | Done (chat and key) | T9 with `kpc_cheats 0` |
| T7 | Not a bug: the key runs the stock `fa8f0`, which gives 200 of each ammo through `f3440`; `f3440` caps ammo at 100 + 100 per armour piece. With only the gauntlets the cap is 200 (log: `armour=1 ... ammo=200/200/200/200`), with all six 700. Single player behaves the same | No change; documented in COOP-MVP.md | Optional: press Gimmie with several armour pieces |
| T18 | Desktop `KPCFix.log` (`LogWindowBreaks=1`): every break logged "window object MISSING" on that client. Server `483cc` hides the window (`SetObjectFlags` at `0x48413`) before sending message `0x39` | New `CoopWindowShards=1`: the new flags are applied with the visible bit kept; the visible bit is cleared 200 ms later (frame hook). Wooden boxes: not changed (no evidence yet; the server cfgs set `gibdetail 2`) | T18: windows on both PCs; boxes: note whether they now throw debris |
| T14 | Not settled. Static analysis says a shard pickup (`cd2a0` → `cc120` → `cc180`, non-instant branch) reaches the hide at `cc29e`, and shards are authored like other items. The respawn log was capped at 24 lines per world, and the busy R4M1A world filled it with health/ammo lines, so the log cannot show what happened to the shard | Diagnostics only: the first `scheduled` / `respawned` / `skipped` / `touched` line per item id per world is always logged; a respawnable item that is not scheduled logs why (`force`, `Respawn`, `expiry`); a shard pickup logs `touched` | T14: take a shard in a campaign world, wait 20 s, send `KPCServerTools.log` |
| T19 | Draw `0xd31c` shows `checkbox_checked.pcx` (lit) when `+0x14c` is 1; the box stored "not on" there, as Enable Music does | Client: `+0x14c` = on; box moved to the gap above Enable Sound FX (y 216; Enable Sound FX is at 256, the music slider at 156 + font offset). `test_focus.py` updated | T19: look, position, click |
| T17 | — | Not run | T17 |

**Other releases (user, 2026-11-13: the patch should work on every release at some point).**
The Audio checkbox exists only for the Europe/USA `cshell.dll` layout (`audio_vtable` and
the checkbox entries are set only there in `source/src/profiles.h`); Germany and Italy fall
back to the console variable. Supporting them needs the Audio screen vtable, the checkbox
class, its image names and the widget positions found and verified in those two
`cshell.dll` builds, plus their `test_focus.py` runs. Check the other shell-UI hooks in
`profiles.h` the same way when that work starts. Server Tools has one `object.lto`
(`3a03f879`) for every release, so the server fixes apply everywhere.

## Two-PC results 2026-11-13: decisions and work list

Desktop on Server Tools `7d01059a` + client `cf896d63`.

**Decisions (user):** keep chat `!gimmie` / `!nextarmor`; keep `CoopKeepOnDeath=1`;
`!class` and the class-cycle key must **not** need `kpc_cheats 1`.

**Prompt A (Audio checkbox) was never built.** Its worktree survives at
`C:\Projects\kpc old\postclean\KPCFix\.claude\worktrees\zealous-cori-401c64`, has no
commits past `6966363`, and its `source/src/ui.c` is identical to this branch. Its session
ended about 6 minutes after the worktree was made. Nothing was lost; the work is still to do (N14).

| # | Request / failure | Side | Notes |
| --- | --- | --- | --- |
| N1 | `!changelevel <map>` accepts **any** world, including `PMxx` and `TUTORIAL` | server | today `coop_campaign_find` accepts only campaign worlds |
| N2 | No 2-player minimum for `!campaign` or `!changelevel` | server | "Need 2 players" in `coop_session.h` |
| N3 | Players spawn with gauntlets + melee weapon on a fresh R1M1A although the launcher selected nothing | server | blocks the T3 gauntlets test; find who gives them (default inventory, persistence restore, or a launcher/INI flag) |
| N4 | First armor shard pickup per world: **everyone** watches the cutscene (all invulnerable during it, each player's own invulnerable state restored after), and **everyone** gets weapon slot 1 (own class's melee) and armour slot 1. Later powerup cutscenes play for the picker only. "First" resets when the world is reloaded | server | extends rows 1, 25. Players dead at that moment get the slot-1 items at respawn |
| N5 | Inventory rule: every player's weapons and armour stay equal when they came from campaign pickups; cheat-given items are personal. After a death, restore exactly what the player had, plus anything shared while dead | server | refines `CoopKeepOnDeath` / row 8 |
| N6 | Ammo sharing switch on its own: INI setting **and** host chat `!ammoshare on\|off` | server | row 14 |
| N7 | "Opping": host promotes a player (`!op <name>` / `!deop`) who may then run every host-only command and skip cutscenes | server | |
| N8 | Gimmie key adds ammo only up to 200, stock adds 200 per use up to the max | server | T7; `!all` stays the max-everything command |
| N9 | Same refusal message for every refused cheat, and one for each refused type | server | T8 |
| N10 | Class change keeps every weapon: slot 1 and slot 6 become the new class's melee/ultimate when the player had them | server | T9; `f3440` refuses another class's weapon |
| N11 | Weapons lost across a natural exit (armour kept) | server | T10; check `inventory-restored ... gives=` in the log |
| N12 | A fresh `!campaign` clears everyone's items and ammo; worlds keep them along the campaign; R4M4B -> R5M1A keeps them; `!abort` keeps them (lobby only) | server | T11 |
| N13 | Armour shards do not respawn (weapons do) | server | T14; shard item `0x1e` may not take the `0xcc29e` path |
| N14 | Audio checkbox: lit = on, and move it to a better spot | client | T19, row 15 |
| N15 | Window shards and wooden-box debris still missing | client + server | T18, row 19; read the `LogWindowBreaks` lines in the laptop's `KPCFix.log` and the server's `GibDetail` |
| N16 | Any player can skip a cutscene / scripted sequence (including the RxM0 intros) for everyone with any key; only host or op may | server | row 6, needs N7 |
| N17 | `!reload`: `!changelevel <current world>` | server | needs N1 for non-campaign worlds |
| N18 | Server console world change: `start worlds\deathmatch\PM05` drops every client, and the dropped client stayed "connected" with no menu (killed client.exe). Need a console command that changes world without dropping clients (e.g. `kpc_changelevel <world>`, same path as `!changelevel`) | server (+ client hang) | in that log, chat `!changelevel PM02` on a deathmatch world ran the stock `changelevel` (next map, PM06) and also dropped the client |

## Logs to collect

* Desktop: `C:\Games\Psycho\KPCServerTools.log` (server) and `C:\Games\Psycho\KPCFix.log` (client).
* Laptop: `KPCFix.log` in its game folder.
* Note the time of each failure; with `[Coop] TraceTiming=1` co-op lines carry `t=<ms>`.

## If a test fails

* **T6 spawner:** no `spawner reference moved` line → the reference was not a player
  (sender of message `0x27`, `d501a`) or only one player was in the list. Check the
  `TraceScripts` lines for the `CTriggerCommand` play, then the spawner's `+0x170`.
* **T7/T8 keys:** nothing at all → the client never sent `0x3b` (keybind missing in the
  client config) or the hook `0xfead7` is off (`effective CoopCheatKeys=1` line).
* **T10–T12 persistence:** no `inventory-saved` → the snapshot saw the player as dead
  (`vtable+0x60`) or not in the live list; `inventory-restored ... gives=0` with items
  missing → `f3440` refused them (class or gauntlets rule, `f3467`–`f349b`).
* **T14 respawn:** `scheduled` but never `respawned` → the item update stops for hidden
  items or `+0x81` was cleared; never `scheduled` → the item already had `Respawn` set
  or has an expiry (`+0xe4`).
* **T15 powerup:** re-sent but still invisible → the client effect `0x35` handler
  (cshell SFX table `0x201bf94c`) probably plays only for the local player; then the fix
  belongs in the client.
* **T16 changelevel:** a "maplist" error → `coop_prepare_maplist` could not write the slot.

## Code review 2026-11-12: found in the code and in the desktop log

Source: review of `proxy.c` / `coop_session.h` / client `loading.c`, and the last run in
`C:\Games\Psycho\KPCServerTools.log` (build `7dbba5eb`, ended 19:14, two players, worlds
R1M0, R1M1A, R4M1A, R4M1B, R4M1C, R4M2A, R3M4B). R1, R2, R3 and R5 are fixed in Server Tools
build `7d01059a` and R4 in client build `cf896d63` (both in the repo and installed on the desktop at 23:14; the laptop still has client
`2ab53bd7` and needs package `pkg-client-cf896d63`). Tests passed for these builds: server unit tests 26/26, emulator matrix
48/48, `check_live.py` both modes, client suite 20/20, packaging of both ZIPs
(`tools/build-changed.cmd`). Not run: `audit_sites.py`, native server smoke, two-PC test.
R6 is open: see the pickup notes.

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

Log markers seen in that run (evidence for the test plan above; the user's own report still decides PASS/FAIL):
`inventory-saved`/`inventory-restored` 14 each with `gives=11..12` (T10), `spawner reference moved` 23 (T6),
`(key)` cheat lines 152 (T7), `used !kill` 4 (T12), `used !all` 8 (T13), `item-respawn` scheduled 16 /
respawned 16 / waiting 2 (T14), `effect 0x35 re-sent` 3 (T15), `Host changed level` 2 (T16), `shared ammo` 5,
`secret` 1. Not seen: `sequence-refused`, `is now <class>` (T9), `Cheats are disabled` (T8), `change-failed`, `intro-abort`.

## Playtest 2026-11-12 (two PCs, whole campaign): triage

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

## Live-confirmed (two PCs)

R1M2B lobby, `!campaign` start, loading picture and spawn hold (players enter alive),
R1M0 and R1M1A intros, single-player start in R1M1A, gauntlets pickup present,
gauntlets equip sequence, the R1M1A lever (door, camera, Headless), natural exits,
music, health bars, monster spawners (host-triggered). Single player:
`LocalServerMinStepMs=16` fixes the R2M2A Ballbuster above 60 FPS.

## Built earlier, still waiting for a two-PC test

| Item | Setting / command |
| --- | --- |
| Intro starts with the last spawn (no interval) | `CoopIntroSettleMs` campaign default 0 |
| Natural exits during play: host only | built in |
| Difficulty in the lobby | `!difficulty lullaby…nightmare` (host) |
| `!abort` resets everything from any state | built in |
| `player teleport/animate/lookat/powerup` only for the activator; `giveitem/invulnerable` for everyone | `CoopScriptEvents=1` |
| Weapons, armour, pamphlets, keys, tickets shared; secret message to everyone | `CoopSharedItems=1` |
| No friendly fire (self and monster damage unchanged) | `CoopFriendlyFire=0` |
| Server frames ≥ 16 ms (co-op Ballbuster, real game time) | `CoopServerFrameMs=16` |
| Client: no spawn hold on the first connect (no water ambience in the menu) | built in |
| Client: chat messages 8 s | `ChatMessageSeconds=8` |
| Client: `LocalServerMinStepMs` default 16; VSync at refresh rate, no 60 cap (`DDrawCompat-CLIENT.ini`) | defaults |

## Open items (in order)

1. Act on the two-PC results above.
2. **World held behind the loading picture** (row 9): no simulation until every member
   has spawned.
3. **Only the host skips cutscenes** (row 6): find the client message → `endingscript` path.
4. **Collectables HUD in co-op** (row 10) and the **Disconnect menu item** (row 21).
5. (done: `SmoothCutsceneCamera`, client `camera.c`; needs the in-game look of T20.)
6. **Switches/levers host gate** (row 3: `CSwitch` `44dd6` and other command-list owners).
7. **Collision-checked spawn allocator** (`coop/GENERAL-ALLOCATOR.md`) and a
   **protected waiting state** between client enter (`0xfdfb0`) and spawn (`0xfec56`).

## Tooling

* Installing a new build on the test PCs: [UPGRADE-TEST-INSTALL.md](UPGRADE-TEST-INSTALL.md).
* `C:\KPCTest\pass2\`: `build-coop.sh <dir>`, `unit-coop.sh`, `matrix-coop.sh <dir>`. They read the checkout
  `C:\Users\magi\Documents\GitHub\KPCFix` (see `CLAUDE.md`).
* `C:\KPCTest\clientbuild\`: `build-coop.sh <dir>`, `suite-coop.sh`.
* `coop/tools/Invoke-CoopServerSmoke.ps1` (native server smoke; never on a folder with
  a running server).
* RE: `C:\KPCTest\re-venv` (capstone, pefile), `C:\KPCTest\re` (original binaries),
  `C:\KPCTest\scratch-*` notes, `C:\KPCTest\scratch-campaign\objects.json` (every
  world's objects and properties). Docker Desktop must be running for builds.
