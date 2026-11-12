# Co-op next-session handoff (2026-11-12, whole-campaign pass)

How everything works: [COOP-MVP.md](COOP-MVP.md). Settings: [CONFIGURATION.md](CONFIGURATION.md).
Older evidence: [archive/COOP-MVP-EVIDENCE-2026-11-12.md](archive/COOP-MVP-EVIDENCE-2026-11-12.md).

## Where things stand

* **Branch:** `coop/mvp-lobby`. `main` is untouched. Commit `7c09079` (client spawn hold,
  audio resync) is pushed; everything after it is **not committed** yet.
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
3. **Audio menu option** for `KeepAudioOnFocusLoss`: reuse the InputRate-menu
   technique (`ui.c`: screen init `0x25610`, key handler `0x25370`, widget Y
   immediates) on the Audio screen.
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
