# Co-op next-session handoff (2026-11-12, whole-campaign pass)

How everything works: [COOP-MVP.md](COOP-MVP.md). Settings: [CONFIGURATION.md](CONFIGURATION.md).
Older evidence: [archive/COOP-MVP-EVIDENCE-2026-11-12.md](archive/COOP-MVP-EVIDENCE-2026-11-12.md).

## Where things stand

* **Branch:** `coop/mvp-lobby`. `main` is untouched. The whole-campaign pass is commit
  `6d59bfe`; the playtest fixes below are in the working tree on top of it.

## Playtest 2026-11-12 (two PCs, whole campaign): triage

Evidence: `C:\Games\Psycho\KPCServerTools.log` (session from R1M0 to R1M2A), object.lto
static analysis. Status: **fixed** = in the working tree, unit and emulator tested, not yet
two-PC tested.

| # | Report | Cause | Status |
| --- | --- | --- | --- |
| 1 | Player 2 teleported in front of the camera when the host took an armour piece; again with the demon gauntlets | `ee900` (IsPlayer) takes a *pointer* to the HOBJECT; the activator hook stored the pointer, so it never matched and every personal `player` command went to the first player (log: `-> first player` everywhere). The second report was still on the old DLL `9ff661bf` | **fixed** in `9e7103e` (cutscene, camera and invulnerability for everyone; teleport/animate/lookat only for the picker) |
| 2 | Unsheathe sound plays at the touch, not near the end of the cutscene | armour touch `c8340`: multiplayer sets the give flag `0x1013e1b8` to 1 (`c8348`), single player uses ReallyGiveArmor (false) and leaves it to the script's `giveitem` | **fixed** (`c8348` gate reads 0 with CoopScriptEvents; armour shared only when actually given at the touch) |
| 3 | Non-hosts start level-ending sequences (R1M1A victory) | walk-in `CTriggerCommand1` → `r1m1a_victory.scr` → `trigger_generic CExitTrigger0`; only the exit itself was host-gated, so the cutscene played and then the exit was refused | **fixed** for `CTriggerCommand` (`CoopHostSequences`): the trigger stays armed for the host. Switches/levers (`CSwitch` `44dd6`) and other command-list owners are not gated yet |
| 4 | After a refused non-host mirror click the host's click did nothing | mirror = `CModelDeco` use command `trigger_generic Gubba`; the deco marks itself used (`+0x1a1`) before the exit refusal | **fixed** (deco re-armed when its exit is refused) |
| 5 | Key pickup chat message | campaign keys are `inventory add key` (CPickupTrigger), not items; the inventory event `b6730` gives to the **first player only** (so keys went to the wrong player) | **fixed**: every player gets it, chat `<name> found the key (everyone has it)`; tickets too. `inventory remove` still only the first player |
| 6 | Only the host may skip cutscenes (space / mouse1) | skip runs the running script's `endingscript` (list `c50c0`, registry `0x1015a0b8`); the client message → skip path is not found yet | open |
| 7 | R1M2A → R1M2B did not change after the belt lift | log ends after `cs1belt.scr`; there is no exit in the lift: the exit volume `CExitTrigger0` is at (1504,−208,−2944), the lift drops at x≈−280. Needs a retest: does the lift go down (`trigger_door liftpiece1/2 open` at 8.8 s) and is the exit volume reached? | needs retest |
| 8 | Items/ammo/armour lost on world change | multiplayer respawns with a fresh inventory; needs a per-member snapshot at world-loading and re-give on spawn (player inventory layout not mapped yet) | open |
| 9 | World active behind the loading picture (pickup sounds, enemies) | the server simulates as soon as the world is loaded; needs the world held (e.g. `coop_server_frame` skipping simulation, but not networking) until every member has spawned | open, design needed |
| 10 | Secrets/collectables HUD shows the deathmatch player list | cshell stats panel (vtable `0xff3c0`, draw `0xa4150`): `0xa41c8` calls the multiplayer test `0xcab80`; multiplayer jumps to the player list `0xa42cd`, otherwise it draws seven counters (`0xa06e0`) from globals `0x201c4cf0`–`0x201c4d08`. Their writer is not located (no direct stores; filled through a pointer or copy) | open. Patch plan: replace the `0xa41c8` call with "multiplayer and not a campaign world" (`kpc_campaign_world`, loading.c) once the counters are known to be filled in co-op (likely needs the server to send the secret/collectable message to every member) |
| 11 | Golden mirror invisible in the R1M1B lever cutscene | `camera_path` moves only the first player's server view position (`c2c9c` → `ef920`), so other clients are not sent what the camera sees | **fixed (probable cause)**: view position applied to every player |
| 12 | Breakables missing on one client until minimise/restore (doorway in R1M1A) | unknown, client side (object creation during the spawn hold?) | open |
| 13 | Chat typing triggers bound keys (`I` invulnerability) | the `OnCommandOn` filter (6d59bfe) was active in the playtest, but Invuln never goes through it: stock cshell Invuln (`0xc3a00`) is `ret`; the **server** polls `IsCommandOn(client, 93)` (object.lto `0xef1b4`, NoTarget 91 at `0xef204`) from the command bits client.exe sends every update | **fixed** in the client: client.exe's command update reads input-enabled (`0x23c84`, `[cm+0x1014]`) as 0 while the message editor types, so the frame's command buffer is cleared (no movement, fire or cheat reaches shell or server). `test_console.py` |
| 14 | Ammo: pickups for everyone, consumption personal | — | **fixed**: received amount shared per pickup |
| 15 | Keep audio when minimised, always; option in the Audio menu (spare space) | sound pause `0x6d5c`/`0x6cdb`; music pause/resume in OnEvent 4/3 (cshell `0xc80e2` → `0xd9ad0`, `0xc80ba` → `0xd9b00`); local server paused through the active-window test `0x157c5` (stock `alwaysfocused` overrides only that). Pausing only the audio in single player would let narration run ahead of the paused script, so nothing pauses | **fixed**: with `KeepAudioOnFocusLoss` on (console variable, INI default 1) nothing pauses in any mode; Setup > Audio > "Keep Running When Minimised" checkbox below Sound Quality. The Audio screen's 8-widget array is full (`+0x64`..`+0x80`, selection `+0x84`, fixed index tables `0x109d18`), so the box is a KPCFix-owned instance of the game's checkbox class (`0xd060`, images `0xd1b0`) driven from the screen's vtable slots (`0xfd264` +8..+0x18); mouse only, no keyboard selection. `test_focus.py`. **Needs an in-game look** (position, label font, click) |
| 16 | Lobby: random deathmatch map with its own spawn points | — | **fixed**: launcher `-Map random` → PM01–PM10, `CoopLobbyWorld`; stock spawns in deathmatch worlds |
| 17 | Spawns trapped in the ground (R1M3A stone staircase near the fat lady) | not in the logs yet (that run did not reach R1M3A). R1M3A uses the single-player start plus spawn spread for later players, so the spread's checked spot is the first suspect | needs the log of a run with the trap (`spawn-spread` lines) |
| 18 | Server prints `Missing sound file "monsters/aclown/aclowntaunt.wav"` | stock data bug: PSYCHO.REZ references the file but does not ship it | no action (harmless) |
| 19 | Breakables (windows, boxes) throw no shards in multiplayer | No game-type gate on either path. **Windows**: object.lto `0x483cc` clears the window's `FLAG_VISIBLE` (`0x483fd`–`0x48413`), then sends message `0x39` (object, effect `0x4b`, vectors, `shard_size`) to all clients; cshell OnMessage `0xcb194` → registry `0x80020` → shard manager `0x2019e6c0` handler `0x5bd30`, which builds shards from the window's world-model polygons (client API `+0x2c0`/`+0x2c4`/`+0x2c8`) and exits only if the object handle is null ("not in the PVS", `0x5bdaf`). Suspect: on a remote client the now-invisible window is already gone when the message is read (a local game shares the object). **Boxes**: server debris (`0x754d1`) runs only when the server's `GibDetail` (`0x1015bb94`, default 0.0 if the console variable is missing) is non-zero; `dedicated.exe` seeds `gibdetail` "2" and the server cfgs set 2 | diagnostic added: `[Multiplayer] LogWindowBreaks=1` logs each break message and whether the window object was present (`KPCFix.log`, `test_console.py`). Next playtest: break a window on a non-host and send `KPCFix.log`. If "MISSING", fix on the server (Server Tools: hide the window one frame after the message). Boxes: check the server log/console `GibDetail` |
| 20 | Per-player cheats "gimmie gimmie gimmie" and "next armor" | — | **fixed**: chat `!gimmie` / `!nextarmor`, need `kpc_cheats 1`. **User wants keybinds instead of chat**: the stock actions already send it: `GimmieGimmieGimmie` (98, cshell `0xc4c90`) and `NextArmor` (100, `0xc4120`) send message `0x3b` with cheat ids `0x2f` / `0x39` (`0xc3680`). Server follow-up: Server Tools answers those per player (same permission as Invuln) and the chat commands can go |
| 21 | Disconnect button in the in-game menu | the main menu (init `0x21320`) is five **image** buttons, not text: Game, Setup, View, Continue (shown when connected or a game is loaded, x 8, y `0x158`), Quit; each loads `menu/main_<g,s,v,c,q>_{n,mo,s,glow}.pcx` (`0x10b160`..) through vtable `+0x40`, at fixed offsets `+0x64` stride `0x12c` with fixed 5-item loops (`0x21790`). No Disconnect image exists and assets are not shipped | blocked (images). Proposal: draw a text item with the menu's bitmap font in the 48-px gap between View (ends y 296) and Continue (y 344), driven like the Audio checkbox (wrap the menu's draw / mouse / click slots), and on click run the shell's own disconnect path (OnEvent 1 handling `0xc7f6d` / `SetMenuMode 0xc6f30`); needs the disconnect call located first |
* **Builds in the working tree:** server `server-tools/KPCServerTools.dll` (build with
  `C:\KPCTest\pass2\build-coop.sh`, byte-identical across rebuilds) and client
  `dinput.dll` (`C:\KPCTest\clientbuild\build-coop.sh`). Hashes: `SHA256SUMS`,
  `server-tools/SHA256SUMS-Server-Tools`.
* **Installed** in `C:\Games\Psycho` (desktop, host and server) and
  `C:\KPCTest\smoke1\Psycho` (disposable test server): see the latest backup folder
  in `C:\KPCTest\backup\` for what each install replaced. The laptop installs from
  `C:\KPCTest\pkg-client-*` or GitHub. How to upgrade:
  [UPGRADE-TEST-INSTALL.md](UPGRADE-TEST-INSTALL.md).
* **Current install (2026-11-12, commit `2b50501`):** `C:\Games\Psycho` has client
  `dinput.dll` `2ab53bd7` and Server Tools `035f23a2`; backup
  `C:\KPCTest\backup\games-psycho-20261112-before-2ab53bd7` (it also holds the moved
  `9ff661bf` `server.dll`). `smoke1` already had `035f23a2`. Laptop package:
  `C:\KPCTest\pkg-client-2ab53bd7`.

## Live-confirmed (two PCs)

R1M2B lobby, `!campaign` start, loading picture and spawn hold (players enter alive),
R1M0 and R1M1A intros, single-player start in R1M1A, gauntlets pickup present,
gauntlets equip sequence, the R1M1A lever (door, camera, Headless), natural exits,
music, health bars, monster spawners. Single player: `LocalServerMinStepMs=16` fixes
the R2M2A Ballbuster above 60 FPS.

## Built and tested, waiting for a two-PC test

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

Tests: server unit tests 24/24 (64- and 32-bit), emulator matrix 43 modes, native server
smoke (Coop/Normal/PassThrough) on `smoke1`, client suite 20/20, `audit_sites.py` (client cshell session: chat command bits, keep running on focus loss + Audio checkbox, window-break log).

## Two-PC test (next)

Both PCs on the current client; server started with
`Start-KPC-Selected-Server.ps1 -Map "worlds\realm1\R1M2B" -Cheats Enabled -Coop`
(`[Coop] CoopCampaign=1`, `CoopIntro=1`, `TraceScripts=1`).

1. Connect from the browser: no water sound behind "Connecting".
2. Host `!difficulty coldsweat`, then `!campaign water`: R1M0 intro starts with no pause.
3. R1M1A: the non-host walks into the exit (refused, "Only the host"); the gauntlets
   sequence moves only the player who triggered it; both get the gauntlets; shoot each
   other (no damage).
4. Pick up a weapon or pamphlet with one player: the other gets it (`item shared` in
   the log); find a secret (both see the message).
5. R2M2A garage: the Ballbuster clears the door and shoots the car.
6. Watch a cutscene camera: report if it looks choppy (62.5 Hz server updates).

Send `KPCServerTools.log` and both `KPCFix.log`.

## Open items (in order)

1. ~~**Chat key capture**~~: fixed (row 13).
2. **Smooth cutscene cameras** (client): interpolate the server's camera samples
   (`0x3d` sub 5 from `c58c0`) between frames.
3. ~~Audio menu option for `KeepAudioOnFocusLoss`~~: done (row 15).
4. **Collision-checked spawn allocator** (`coop/GENERAL-ALLOCATOR.md`).
5. **Protected waiting state** for a player between client enter (`0xfdfb0`) and spawn
   (`0xfec56`): removes the death sound behind the loading picture.

## Tooling

* Installing a new build on the test PCs: [UPGRADE-TEST-INSTALL.md](UPGRADE-TEST-INSTALL.md).
* `C:\KPCTest\pass2\`: `build-coop.sh <dir>`, `unit-coop.sh`, `matrix-coop.sh <dir>`.
* `C:\KPCTest\clientbuild\`: `build-coop.sh <dir>`, `suite-coop.sh`.
* `coop/tools/Invoke-CoopServerSmoke.ps1` (native server smoke; never on a folder with
  a running server).
* RE: `C:\KPCTest\re-venv` (capstone, pefile), `C:\KPCTest\re` (original binaries),
  `C:\KPCTest\scratch-*` notes. Docker Desktop must be running for builds.
