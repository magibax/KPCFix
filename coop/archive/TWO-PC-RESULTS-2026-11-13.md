# Two-PC test 2026-11-13: results and the fixes they led to

Finished record. The current plan is in [../NEXT-SESSION-HANDOFF.md](../NEXT-SESSION-HANDOFF.md).

The test ran on the build of `coop/mvp-lobby-untested` (Server Tools `7d01059a`, client
`cf896d63` on the desktop; the laptop still had client `2ab53bd7`). The fixes are commit
`7623b69` (Server Tools `dcc66b4c`, client `1e97026a`).

## Test plan and results (run on Server Tools `7d01059a`, client `cf896d63`)

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
