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
| 1 | Player 2 teleported in front of the camera when the host took an armour piece | `ee900` (IsPlayer) takes a *pointer* to the HOBJECT; the activator hook stored the pointer, so it never matched and every personal `player` command went to the first player (log: `-> first player` everywhere) | **fixed** (dereference; pickups also set the activator) |
| 2 | Unsheathe sound plays at the touch, not near the end of the cutscene | armour touch `c8340`: multiplayer sets the give flag `0x1013e1b8` to 1 (`c8348`), single player uses ReallyGiveArmor (false) and leaves it to the script's `giveitem` | **fixed** (`c8348` gate reads 0 with CoopScriptEvents; armour shared only when actually given at the touch) |
| 3 | Non-hosts start level-ending sequences (R1M1A victory) | walk-in `CTriggerCommand1` → `r1m1a_victory.scr` → `trigger_generic CExitTrigger0`; only the exit itself was host-gated, so the cutscene played and then the exit was refused | **fixed** for `CTriggerCommand` (`CoopHostSequences`): the trigger stays armed for the host. Switches/levers (`CSwitch` `44dd6`) and other command-list owners are not gated yet |
| 4 | After a refused non-host mirror click the host's click did nothing | mirror = `CModelDeco` use command `trigger_generic Gubba`; the deco marks itself used (`+0x1a1`) before the exit refusal | **fixed** (deco re-armed when its exit is refused) |
| 5 | Key pickup chat message | campaign keys are `inventory add key` (CPickupTrigger), not items; the inventory event `b6730` gives to the **first player only** (so keys went to the wrong player) | **fixed**: every player gets it, chat `<name> found the key (everyone has it)`; tickets too. `inventory remove` still only the first player |
| 6 | Only the host may skip cutscenes (space / mouse1) | skip runs the running script's `endingscript` (list `c50c0`, registry `0x1015a0b8`); the client message → skip path is not found yet | open |
| 7 | R1M2A → R1M2B did not change after the belt lift | log ends after `cs1belt.scr`; there is no exit in the lift: the exit volume `CExitTrigger0` is at (1504,−208,−2944), the lift drops at x≈−280. Needs a retest: does the lift go down (`trigger_door liftpiece1/2 open` at 8.8 s) and is the exit volume reached? | needs retest |
| 8 | Items/ammo/armour lost on world change | multiplayer respawns with a fresh inventory; needs a per-member snapshot at world-loading and re-give on spawn (player inventory layout not mapped yet) | open |
| 9 | World active behind the loading picture (pickup sounds, enemies) | the server simulates as soon as the world is loaded; needs the world held (e.g. `coop_server_frame` skipping simulation, but not networking) until every member has spawned | open, design needed |
| 10 | Secrets/collectables HUD shows the deathmatch player list | client HUD chooses by game type; needs a client patch plus the server's secret/collectable counts | open |
| 11 | Golden mirror invisible in the R1M1B lever cutscene | unknown (camera-view visibility of a WorldModel/deco on clients) | open |
| 12 | Breakables missing on one client until minimise/restore (doorway in R1M1A) | unknown, client side (object creation during the spawn hold?) | open |
| 13 | Chat typing triggers bound keys (`I` invulnerability) | client: open item 1 below | open |
| 14 | Ammo too scarce for several players | proposal: scale ammo pickups (`CoopAmmoScale`) in the pickup hook | open, needs a decision |
| 15 | Keep audio when minimised, always | client `KeepAudioOnFocusLoss=1` already the default, but only in remote multiplayer; single player/hosting keep the stock pause | needs a decision |
* **Builds in the working tree:** server `server-tools/KPCServerTools.dll` (build with
  `C:\KPCTest\pass2\build-coop.sh`, byte-identical across rebuilds) and client
  `dinput.dll` (`C:\KPCTest\clientbuild\build-coop.sh`). Hashes: `SHA256SUMS`,
  `server-tools/SHA256SUMS-Server-Tools`.
* **Installed** in `C:\Games\Psycho` (desktop, host and server) and
  `C:\KPCTest\smoke1\Psycho` (disposable test server): see the latest backup folder
  in `C:\KPCTest\backup\` for what each install replaced. The laptop installs from
  `C:\KPCTest\pkg-client-*` or GitHub.

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
smoke (Coop/Normal/PassThrough) on `smoke1`, client suite 18/18, `audit_sites.py`.

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

1. **Chat key capture** (client): keys typed in the chat box still trigger bound
   actions (Server Tools noclip on a developer key). The key path runs through the
   shell's input object (`shell+0x5c`, `0xbde90`); the chat editor state is not
   located yet. The noclip reply handler is `0xc5640` → `0xc3850`.
2. **Smooth cutscene cameras** (client): interpolate the server's camera samples
   (`0x3d` sub 5 from `c58c0`) between frames.
3. ~~Audio menu option for `KeepAudioOnFocusLoss`~~: dropped (2026-11-12): background
   audio stays on by default, no menu option.
4. **Collision-checked spawn allocator** (`coop/GENERAL-ALLOCATOR.md`).
5. **Protected waiting state** for a player between client enter (`0xfdfb0`) and spawn
   (`0xfec56`): removes the death sound behind the loading picture.

## Tooling

* `C:\KPCTest\pass2\`: `build-coop.sh <dir>`, `unit-coop.sh`, `matrix-coop.sh <dir>`.
* `C:\KPCTest\clientbuild\`: `build-coop.sh <dir>`, `suite-coop.sh`.
* `coop/tools/Invoke-CoopServerSmoke.ps1` (native server smoke; never on a folder with
  a running server).
* RE: `C:\KPCTest\re-venv` (capstone, pefile), `C:\KPCTest\re` (original binaries),
  `C:\KPCTest\scratch-*` notes. Docker Desktop must be running for builds.
