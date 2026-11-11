# Co-op MVP: chat lobby and the R1M2B → R1M2C → R1M3A route (kpc-coop/1)

**Status (2026-11-11):**
* **Live Windows client tested (DLL `262b6ead…`, one client):** join, lobby, `!ready`, `!start`, and a host `!next` change R1M2B → R1M2C.
* **Natural exits WINDOWS-TESTED (build `coop-session-v2`, `0018a3f8…`, one client):** R1M2B → R1M2C through the hallway exit, and R1M2C → R1M3A through the golden mirror, both **without `!next`**. The R1M3A → R1M3B exit was detected and refused as off-route.
* **Later on 2026-11-11, desktop + laptop (user-tested):**
  * scoreboard-free transitions (`CoopIntermission=0`);
  * original per-client music (`CoopMusic=1`), including a second laptop joining a running
    game; not beat-synchronised;
  * the original R1M3A opening cinematic (`CoopIntro=1`, DLL `13ca1d0a…`), with the two
    machines matching each other.
* **Narration cadence: RESOLVED, user-confirmed** (DLL `e86bb352…`, desktop + laptop):
  subtitles, camera and the window smash no longer run ahead of the narration
  (`CoopScriptClock`, [section D](#d-script-clock-cadence-narration-versus-subtitles)).
* **Open after that test, fixes built but NOT yet tested with clients** (DLL `1b8816cb…`, commit
  `aa9949c`; emulator- and native-server-tested only):
  * narrator cut off after "…will be a useful ally for you": a server sound timer on engine
    time ([section E](#e-narrator-cut-off-after-will-be-a-useful-ally-for-you-sound-timer));
  * no enemy health bars: the server never sends them in multiplayer
    ([section F](#f-enemy-health-bars-in-co-op));
  * no enemies after the smash (`spawn6`): monster spawners are never created in multiplayer
    ([section G](#g-enemies-missing-after-the-r1m3a-window-smash-spawn6));
  * new request, the stock scoreboard from `!start` until the cinematic
    ([section H](#h-stock-scoreboard-from-start-until-the-cinematic-coopstartscoreboard)).
  * Resume from [NEXT-SESSION-HANDOFF.md](NEXT-SESSION-HANDOFF.md).
* **Campaign (2026-11-11, later):** the opt-in `CoopCampaign=1` (commit `1524fb6`, DLL
  `009eacd6…`) replaces the fixed route with the 48-world WORLDORDER graph and host realm
  selection. It is emulator- and native-server-tested only. See
  [CAMPAIGN-ARCHITECTURE.md](CAMPAIGN-ARCHITECTURE.md) and
  [CAMPAIGN-MAP-INVENTORY.md](CAMPAIGN-MAP-INVENTORY.md).
* **Not yet tested:** two-player combat, death, respawn and reconnect regression (deferred).

Experimental: this code is only on `feature/coop-campaign` and its child branches. It is not
part of the stable Server Tools R3 on `main`. Every switch defaults to off.

| Feature | State | Evidence |
| --- | --- | --- |
| Lobby/session state machine (ready, host, capacity, presence, epochs, debounce, route, timeouts) | IMPLEMENTED, CI-TESTED | `server-tools-source/tests/test_coop_session.c` (64- and 32-bit, `-Werror`) |
| Proxy hooks installed only on the hash-verified original object image, original bytes checked at every optional site (11 in `e86bb352`) | IMPLEMENTED, EMULATOR-TESTED (private), WINDOWS-TESTED (server) | `check_proxy.py <EU fixtures> --coop-session`; [server smoke evidence](#native-windows-server-smoke-evidence-no-clients) |
| Chat lobby: join announce, `!coop`, `!ready`, host `!start` | **WINDOWS-TESTED, 1 client** (`262b6ead`) | [live client evidence](#live-windows-client-evidence) |
| No telefrag between session players (both spawn paths) | IMPLEMENTED, EMULATOR-TESTED; live: `telefrag=skipped` logged for 1 client | a second player is needed to observe the protection itself |
| R1M2B separated placements (3 fixed positions) with slot reclamation | IMPLEMENTED; placements WINDOWS-TESTED earlier (trial build); slot 0 reused after a live rejoin | `SPAWN-ENGINE-TRACE.md` |
| Server-coordinated Start (fresh R1M2B, epoch 1) | **WINDOWS-TESTED, 1 client** | native intermission, world reload and respawn observed |
| Host override `!next` R1M2B → R1M2C | **WINDOWS-TESTED, 1 client** | a **command-initiated** change, not evidence of a natural exit |
| Exit handler reached in multiplayer | **WINDOWS-OBSERVED** with the activator recorded (`activator=member:johnny`) | [natural transition evidence](#natural-transition-evidence-coop-session-v2) |
| Natural exits → next route world: R1M2B hallway → R1M2C; R1M2C golden mirror → R1M3A | **WINDOWS-TESTED, 1 client** (`0018a3f8`), no `!next` | one change per epoch; the off-route R1M3B exit was refused with a single chat reply |
| Intermission scoreboard and next-map music during transitions | **UNDER INVESTIGATION** | [transition intermission and music](#transition-intermission-and-music-investigation) |
| Map changes at the end of a scripted sequence | **UNVERIFIED** | no direct level-change call from script code in `object.lto`; see [transition mechanisms](#three-transition-mechanisms) |
| Intro cinematics | **EXPERIMENTAL, R1M3A only** (`CoopIntro=1`; default: skipped) | stock multiplayer never plays the authored intro. With `CoopIntro=1` the R1M3A intro was user-tested on two clients ([section C](#c-synchronised-original-intro-r1m3a-proof-of-concept)); the narration cadence fix awaits client validation ([section D](#d-script-clock-cadence-narration-versus-subtitles)) |
| Generic safe spawning on other maps (allocator) | **DISABLED** | full-body clearance is not proven; see `GENERAL-ALLOCATOR.md` |
| Install / idempotent re-run / in-place upgrade (`7112b5ed` → `262b6ead` → `0018a3f8`) / uninstall | WINDOWS-TESTED (scripts, disposable copy) | original `server.dll` `83b7bce8…` preserved; `object.lto` `3a03f879…` unchanged throughout |
| Launcher worker co-op validation (wrong start map, maplist without R1M2C/R1M3A) | WINDOWS-TESTED (scripts only; R1M3A check added in v2) | rejected after the real version checks, before any server start |
| Native empty server for `0018a3f8`: load, hooks, session armed, NextLevel epochs through R1M2B, R1M2C, R1M3A; normal and pass-through regressions | WINDOWS-TESTED (no clients) | R1M3A loads under the dedicated server |
| Two clients: separation, combat, natural exit, rejoin | **NOT TESTED** | follow the test protocol below and return the logs |
| Script cadence (`CoopScriptClock`) | **USER-CONFIRMED** (desktop + laptop, `e86bb352`) | [section D](#d-script-clock-cadence-narration-versus-subtitles) |
| Full narration (`CoopSoundClock`) | IMPLEMENTED, EMULATOR- and SERVER-TESTED; clients NOT TESTED | [section E](#e-narrator-cut-off-after-will-be-a-useful-ally-for-you-sound-timer) |
| Enemy health bars (`CoopHealthBars`) | IMPLEMENTED, EMULATOR- and SERVER-TESTED; clients NOT TESTED | [section F](#f-enemy-health-bars-in-co-op) |
| Monster spawners / R1M3A `spawn6` (`CoopSpawners`, `CoopSkill`) | IMPLEMENTED, EMULATOR- and SERVER-TESTED (spawner creation logged natively); spawning with players NOT TESTED | [section G](#g-enemies-missing-after-the-r1m3a-window-smash-spawn6) |
| Stock scoreboard from `!start` to the cinematic (`CoopStartScoreboard`) | IMPLEMENTED, UNIT- and EMULATOR-TESTED; clients NOT TESTED; not visible during the world load itself (client limitation) | [section H](#h-stock-scoreboard-from-start-until-the-cinematic-coopstartscoreboard) |

## How it works

### Lobby transport and protocol
Players type chat commands. The game's own reliable chat relay carries them. The server
reads each message where it already relays chat (object `0xfeec4`, ESI = player, EAX = text) and
answers with the same broadcast routine that announces cheat permission changes, which is
Windows-tested. No new network messages, client changes or open ports are introduced.
Only the dedicated server makes decisions.

| Command | Who | Effect |
| --- | --- | --- |
| `!coop` / `!status` | anyone | Prints `[Co-op] LOBBY R1M2B e0: *Alice(R) Bob(-) 1/2 ready, need 2`. `*` is the host, `(R)` ready, `(-)` not ready, `(away)` sitting out |
| `!ready` / `!unready` | member, lobby only | Toggle readiness |
| `!leave` / `!join` | member | Sit out (does not count for Start) / rejoin if capacity allows |
| `!start` | host | Allowed when present ≥ `CoopMinPlayers` and every present member is ready |
| `!next` | host, during play | Host override: advance to the next route world when the exit cannot be reached (for example when the exit trigger is not delivered in multiplayer) |
| `!help` | anyone | Command list and protocol version |

Input is untrusted. A message is a command only if it starts with `!`, has a single
alphanumeric word and no arguments, and is at most 64 bytes. Names are reduced to
printable ASCII, 31 characters. Replies are at most 111 bytes.

**Host** is the earliest-joined member who is present and not away. Host status passes
automatically on disconnect. The server operator keeps the native console (`kick`,
`nextlevel`, `stop`).

### Session phases
`LOBBY` (WAITING_FOR_READY) → `TRANSITION` → `WORLD_LOADING` → `INTRO_PENDING` → `GAMEPLAY` → `TRANSITION` → …

* **Start** reloads R1M2B through the native multiplayer level-change path. Everyone enters a
  fresh world together. The map **epoch** increments on every native NextLevel, including a
  console `nextlevel`.
* **INTRO_PENDING** waits until the first participant spawns, so a world with no players does
  not "start" anything. Intros are not synchronised and are skipped. This is announced to players.
* **Exit**: an exit object in multiplayer reaches its game-type gate at `0x2d455`. The session
  reads its `NextWorld` (`+0x54`). It requests a change only if all of these hold:
  * `+0x152` equals 1, the value the original code sets for every exit in a multiplayer game
    ([evidence](#exit-objects-and-the-meaning-of-0x152)); any other value fails closed;
  * the phase is GAMEPLAY;
  * this epoch has not already changed world;
  * the exit's own `NextWorld` normalises to exactly the next route world. An empty, unknown
    or off-route destination is refused, and the refusal is announced at most once per epoch.

  Destinations come only from the map's exit object, never from player input.

  Repeated touches from any number of players are counted as duplicates. Native code still
  takes its multiplayer branch. Game type is never falsified.
* **World change** sets the native maplist cursor so that the next entry is the route world.
  It fails closed if the maplist lacks that world. It then calls the native multiplayer
  end-of-level routine (scoreboard message; continues after ≥ 3 s when a player presses fire/use), the same path as frag and time
  limits. If no NextLevel follows within 30 s, the next event reports a timeout and allows
  a retry.
* **Late join / slow load:** a joiner during play is admitted (`joined in progress`) and spawns
  natively. After a world change, members have 90 s to respawn before being dropped.
  They are re-admitted on arrival.
* **Disconnect:** the session compares members with the engine's own player list (the same
  list the telefrag and revocation code use). A missing object means the member left, and
  their slot is reclaimed. If everybody leaves during play, the session returns to a new
  lobby generation.
* **Off-route level change** (an operator `nextlevel` to an unrelated map) ends the session.
  The lobby is restored.

### Spawning
* In a session, the native telefrag routine (`0xf10c0`) is skipped on both spawn paths
  (`0xf13a4`, `0xf167a`). Static analysis shows its only effect is to iterate the player list
  and send 1000 damage to overlapping players. Teammates therefore cannot kill each other on
  spawn.
* On **R1M2B** the first spawn path moves members 0, 1 and 2 to the three Windows-tested
  positions. Slots belong to members and are reclaimed on disconnect.
* On **R1M2C** no separated placement is validated. Players may spawn **overlapping** at the
  authored start. Nobody dies, but players can be stuck until one moves. This is an explicit
  MVP restriction.

### Configuration (`KPCServerTools.ini`, `[Coop]`)
| Key | Default | Meaning |
| --- | ---: | --- |
| `CoopSession` | 0 | Enables everything above. The launcher's co-op checkbox writes 1 for that start and 0 for normal starts |
| `CoopMinPlayers` | 2 | 1–3; players needed before `!start` |
| `CoopMaxPlayers` | 3 | 1–3; session capacity |

`[ServerTools] Enabled=1` is required. The launcher's co-op mode also writes the following server settings:
* maplist `R1M2B, R1M2C, R1M3A`;
* `maxclients` ≤ 3;
* `fraglimit 0`, `timelimit 0`;
* `keepitems 1`.

Deathmatch and normal starts are unchanged.

### Observability
At startup `KPCServerTools.log` records:
* `[KPCFix Co-op] build=coop-session-v1 proto=kpc-coop/1 sha256=<SHA-256 of the loaded DLL file>`;
* `effective CoopSession=…`;
* `session armed …`.

Each session event is one bounded line:
`[KPCFix Co-op] g=<generation> e=<epoch> seq=<n> <PHASE> <event> <detail>`.
Events are:
* `spawn` (path, slot, client, name, `telefrag=skipped`);
* `say`, `change`, `end-level`;
* `exit-seen`: the first 8 exit-handler arrivals per epoch, whatever the session decides,
  with `next=`, `flag152=`, `exit=<object address>`, `activator=member:<name>|player|object|none`
  and `start=<StartPoint>`;
* `exit` (an accepted natural exit) and `exit-refused` (with the reason);
* `world-loading`, `world-loaded`, `presence` and `change-failed`.
At most 3000 lines are written per server process.

## Verified engine evidence (original 1.13 object.lto, SHA-256 3a03f879…)
Static disassembly is recorded here. These are runtime targets, not redistributed code.

| RVA | Finding |
| --- | --- |
| `0x2d400` | Exit object message handler: message `0x27` reads the activator HOBJECT and calls `vtbl+0x70` (`0x2d450`). Other messages go to base `0xd3b50` |
| `0xc2785` | Generic target-message loop: sends `0x27` (sender NULL, activator written into the message) to listed targets. Whether trigger volumes run this loop in multiplayer is the open runtime question. The session logs one `exit-seen` line per epoch when the gate is reached |
| `0x2d450` | Exit object handler (vtable `0x118aa4`). `[0x1016de60]+0x1d0` (game type) ≠ 0 returns; otherwise `0x2d510` → `0x2d530` sends message `0x19` to clients. Single-player level changes are client-driven, so they are not used |
| `0x2d18f` | Exit constructor clears `+0x152` |
| `0x2d250`, `0x2d266`, `0x2d2e4` | Exit property reader sets `+0x152 = 1` if `NextWorld` is missing or longer than 127 characters, or `StartPoint` is too long |
| `0x2d33f`–`0x2d352` | The same reader sets `+0x152 = 1` whenever the shell's game type `+0x1d0` is non-zero, i.e. **for every exit in a multiplayer game** |
| `0x2d380`, `0x2d3e0` | Exit initial update and update: when `+0x152 == 1`, schedule an update after 0.1 s, then call server interface `vtbl+0x20` with the exit's own HOBJECT (the call the single-player handler also makes). It is not object removal, since the R1M2C exit still received a trigger in multiplayer long after spawn. Its exact meaning is unidentified |
| `0x2d490`, `0x2d4d0` | Save/load `+0x152` as a byte |
| `0x2d530` callers | Only the exit handler (via `0x2d510`) and the developer `ExitLevel` command (`0xfb0e9`, `0xfb17e`, `0xfb5bd`). No script or cinematic code calls the single-player level change directly |
| `0xfb0d7` | Developer `ExitLevel` command finds `CExitTrigger` and prints `trigger+0x54` as "Exiting to level %s". This confirms `+0x54` is `NextWorld` |
| `0xfde00` | Dedicated server app message handler: `GAMEINIT`, `NEXTLEVEL` → `0x1005c0`, `SERVHOST`, `SAY:` |
| `0x1005c0` | NextLevel: maplist `0xfc640` (cursor `[0x16da78+0x10]`, names `shell+0x210+i*0x7f`, count `shell+0x1ae0`), message `0x46`, LoadWorld |
| `0x100760` | Multiplayer end-of-level: message `0x48`, flag `+0x1ae8`. Once ≥ 3.0 s (`0x117370`) have passed **and a player presses fire/use**, `0x1007c0` (called only from the player method `0xef960` when input flags `+0x134 & 0x2800` are set) sets `+0x1ae9`, and the server update calls `0x100720` → NextLevel. The time-limit check calls it at `0xfc626` |
| `0xf10c0` | Telefrag: iterates player list (`[0x157724]+4`) and sends 1000 damage to overlapping boxes. No other effect |
| `0xfee9a` | Chat relay in the server's message handler: player = client user data, text = message string, name = `player+0x1e8` |
| `0xf8fe0` | "Is single player" (`[shell+0x1d0] == 0`); only callers: the enemy health-bar manager (`0xea6d2`, `0xea7c3`, `0xeaa10`, `0xeab11`), [section F](#f-enemy-health-bars-in-co-op) |
| `0x93c5d` | `CMonsterSpawner::ReadProp` forces MiscFlags `\|= 0x10` before `0xf9000(skill, flags)`: no monster spawner in multiplayer, [section G](#g-enemies-missing-after-the-r1m3a-window-smash-spawn6) |
| `0xc5430` / `0xc569b` | Script abort (plays the `endingscript`) / normal completion (does not) |

## Windows test protocol (needed: two client PCs or one PC plus one other machine)

Use a **disposable copy** of the game folder for the server if you can. The setup
moves the official `server.dll` to `server.original.dll`. `Restore-Original-Server.cmd`
reverses this. Original game files are never rewritten.

1. **Install and verify.**
   * Back up `server.dll`, `server.original.dll`, `KPCServerTools.dll`, `SERVER_TOOLS_BUILD.json`
     and `KPCServerTools.ini`.
   * Extract the CI artifact ZIP into the game folder and run `Setup-KPCServerTools.cmd`.
   * Check the hashes:
     ```powershell
     (Get-FileHash .\KPCServerTools.dll -Algorithm SHA256).Hash
     (Get-Content .\SERVER_TOOLS_BUILD.json -Raw | ConvertFrom-Json).proxy_sha256
     (Get-FileHash .\server.dll -Algorithm SHA256).Hash   # equals the two above
     (Get-FileHash .\object.lto -Algorithm SHA256).Hash   # 3A03F879... (unchanged)
     ```
2. **Empty server.** Run `KPC-Server-Launcher.cmd`, tick **EXPERIMENTAL co-op session**
   and start. The log must show `build=coop-session-v2 … sha256=<same hash>`,
   `effective CoopSession=1`, `session armed`, and `Runtime object hooks installed`.
   No player-facing event should occur while nobody is connected.
   * Optional: run `Invoke-CoopServerSmoke.ps1` from the test bundle first (see
     [server smoke evidence](#native-windows-server-smoke-evidence-no-clients)).
     Its JSON report records hashes and the log.
   * The console prints `Server started on <address>, port 42972`. On the developer
     host it showed a Hyper-V virtual adapter (`172.28.144.1`), but the socket listens
     on `0.0.0.0:42972` (all interfaces; checked with `Get-NetUDPEndpoint`). Clients join
     `127.0.0.1` on the same PC, or the server PC's LAN address.
3. **Lobby.**
   * Client A joins and spawns on R1M2B. Chat shows `A joined the lobby`.
   * Client B joins. B spawns +96 Z from A, and nobody dies.
   * Both type `!coop`; status lists both names and the host `*`.
   * B types `!start`: refused (not host). A types `!start`: refused (B not ready).
   * B types `!ready` then `!unready` then `!ready`. A types `!ready` then `!start`.
4. **Start.** Expect the scoreboard/intermission for about 3 s, then R1M2B reloads for both.
   The chat should say `R1M2B started (epoch 1)`. Check that both spawn apart, nobody
   dies, and both can move, shoot and take damage from enemies. Record whether enemies and
   doors behave as in single player.
5. **Objective and exit (do NOT type `!next`).** Play to the R1M2B hallway exit together.
   * Expected: `Exit reached -> R1M2C`, intermission, and both load R1M2C (epoch 2). The log
     shows `exit-seen … activator=member:<name>` followed by `exit next=…`.
   * On R1M2C, right-click the golden mirror. Expected: `Exit reached -> R1M3A` (epoch 3).
   * If nothing happens, check the log. `exit-seen` followed by `exit-refused` gives the
     reason. No `exit-seen` line means the exit handler was not called. Only then may the host
     type `!next`, and the report must say that the change was command-initiated.
   * Walk back into the exit volume repeatedly. There must be exactly one change (duplicate case).
6. **Slow join / disconnect / rejoin.** On R1M2C, B disconnects. Within a minute A types
   `!coop` and sees `B left`. B rejoins and should see `joined in progress`. Kill B
   (enemy or `!next` from an unrelated scenario) and confirm the respawn has no telefrag.
7. **Restart.** Close the dedicated server and start it again in co-op mode. Expect a fresh
   lobby (generation 1, epoch 0).
8. **Regression.**
   * Start PM01 without the co-op box. `KPCServerTools.ini` must show `CoopSession=0`, and
     deathmatch spawning and telefrag must be normal.
   * Repeat with `[ServerTools] Enabled=0`: the log must show pass-through.
   * Uninstall with `Restore-Original-Server.cmd` and check the original `server.dll` hash.
9. **Send back:** the full `KPCServerTools.log`, the effective `KPCServerTools.ini`, the
   three hashes from step 1, the client region and edition, and notes or screenshots for
   steps 3–8.

## Native Windows server smoke evidence (no clients)

Run on 2026-11-11 on Windows 11 Pro (10.0.26300) in a **disposable copy** of an
installed Europe/USA 1.13 game. The copy was upgraded from the live-tested v7 proxy
with the packaged `Setup-KPCServerTools.cmd`. No client connected. Hashes:
`KPCServerTools.dll` = `server.dll` = `262b6ead146e2ca33639d5ecf6435e67b9e42033de7fa1aae57bc06891bc0f5d`,
`server.original.dll` `83b7bce8…`, `object.lto` `3a03f879…` (stock), `dedicated.exe` `b940fd6d…`.

[`tools/Invoke-CoopServerSmoke.ps1`](tools/Invoke-CoopServerSmoke.ps1) drives the real
launcher worker and types console commands into the server's own console:

| Mode | Result | What it showed |
| --- | --- | --- |
| `Coop` | PASS (15 checks) | build line with the loaded DLL's SHA-256, `effective CoopSession=1`, hooks on stock `object.lto`, `session armed … min=2 max=3`. Three console `nextlevel` commands gave `e=1 R1M2B`, `e=2 R1M2C`, `e=3 R1M2B` (`seq` 1-3, phase LOBBY); world names equal the engine's own `Loaded world:` lines; no player events; clean `quit` (exit 0) |
| `Normal` (PM01) | PASS | `effective CoopSession=0`, normal hooks, PM01 loaded |
| `PassThrough` | PASS | `Original engine pass-through.` only; INI restored |
| Coop hold (`-CommandFile`, `-MinPlayers 1`) | PASS | server held until `quit` from the command file; INI restored byte-for-byte |
| Tampered `object.lto` (one byte flipped, separate copy) | Rejected | launcher worker refuses before starting the server; `dedicated.exe` started directly logs `ERROR: unsupported or altered object.lto` and exits with code 1 |

Findings from these runs:
* **`start <map>` prepends the map to the maplist** (documented in the game's
  `dedicated.txt`), so a co-op start uses the list `R1M2B, R1M2B, R1M2C`. The
  session is unaffected: it selects route worlds by name, and the first matching
  entry wins.
* **The object module stays loaded across native level changes.** Hooks were
  installed once per server run, so session state (generation, epoch, members)
  survives the R1M2B → R1M2C change. A server restart creates a fresh session.
* The server version check named the wrong file when `object.lto` was altered
  ("active server.dll is not the selected Server Tools build"). It now reports the
  `object.lto` mismatch when the active proxy is recognised. The same check exists
  on `main`.

**Client launch: use `Psycho.exe` → Play.** Starting `client.exe` directly (or `Psycho.exe`
with arguments) from a copied folder exits with code 0 after about 1.5 s. That is not a valid
baseline. The working path, captured by a process monitor when the user clicked Play
(2026-11-11 04:36:02), was:
```text
C:\Games\Psycho\CLIENT.EXE -windowtitle "The Nightmare Child" -exedir "C:\Games\Psycho" -rez "E:\Psycho" -rez "C:\Games\Psycho\psycho.rez" -rez "C:\Games\Psycho" -rez "C:\Games\Psycho\KPCPatch.rez"
```
Its parent was the elevated `Psycho.exe`. This install uses `RUNASADMIN` with silent admin
elevation, so a non-elevated automation tool cannot click Play (`BM_CLICK` was refused with
access denied). A person clicks Play.

Reproduce (in a disposable copy with the CI package installed):
```powershell
Start-Process powershell.exe -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File .\Invoke-CoopServerSmoke.ps1 -GameDir C:\KPCTest\Psycho -Mode Coop -Report C:\KPCTest\coop.json'
```
Repeat with `-Mode Normal` and `-Mode PassThrough`. For a client session, add
`-CommandFile C:\KPCTest\cmd.txt`, then append console commands (for example
`say hello`, `list_ids`, `quit`) to that file. Send back the JSON reports.

## Live Windows client evidence

**DLL `262b6ead…`, 2026-11-11, one client** (`johnny`; original Europe/USA 1.13 client started
with `Psycho.exe` → Play; server in the disposable copy `C:\KPCTest\smoke1`, `CoopMinPlayers=1`
for this test only). Excerpt from `KPCServerTools.log`:
```text
g=1 e=0 seq=0 LOBBY say [Co-op] johnny joined the lobby. Type !ready (help: !coop)
g=1 e=0 seq=0 LOBBY spawn path=f13a4 slot=0 client=67778240 name=johnny zbits=3308781568 telefrag=skipped
g=1 e=0 seq=1 LOBBY say [Co-op] LOBBY R1M2B e0: *johnny(R) 1/1 ready, need 1
g=1 e=0 seq=2 TRANSITION change [Co-op] All ready. Starting -> R1M2B
g=1 e=1 seq=3 INTRO_PENDING world-loaded worlds\realm1\R1M2B
g=1 e=1 seq=4 PLAYING say [Co-op] R1M2B started (epoch 1). Intro skipped. Reach the exit together.
g=1 e=1 seq=5 TRANSITION change [Co-op] Host override exit -> R1M2C
g=1 e=2 seq=6 INTRO_PENDING world-loaded worlds\realm1\R1M2C
g=1 e=2 seq=7 PLAYING exit-seen next=worlds/realm1/r1m3a flag152=1
g=1 e=2 seq=7 PLAYING exit-unsupported-flag next=worlds/realm1/r1m3a flag152=1
g=1 e=2 seq=7 PLAYING say [Co-op] End of the MVP route
```
* **Observed:**
  * the lobby, `!ready` and a host `!start`, with native intermission, world reload and
    respawn (epoch 1);
  * the client handle stayed the same across both level changes;
  * a rejoin reclaimed slot 0.
* **Command-initiated:** the R1M2B → R1M2C change came from the host `!next`. It is not
  evidence of a natural exit.
* **Exit handler:** on R1M2C the handler was reached with `NextWorld` `worlds/realm1/r1m3a`
  while the player used a golden mirror. That build refused it. Without activator logging,
  the line alone does not prove that the right-click caused it. The final line is the reply
  to a separate `!next` at the end of the old two-world route.

## Exit objects and the meaning of +0x152

Static analysis of the original `object.lto` (see the evidence table) shows that `+0x152` is
**not** a mirror marker. The property reader sets it for every exit when the game type is
multiplayer, and also for a missing or overlong `NextWorld`/`StartPoint`. Build `262b6ead`
therefore refused every natural exit in co-op, including the R1M2B hallway exit that had never
been tested live. `coop-session-v2` accepts the verified native value 1 and validates the
destination itself. In multiplayer, the flag cannot tell a valid exit from a broken one.

## Three transition mechanisms

| Mechanism | Path to a world change | Status |
| --- | --- | --- |
| Right-click golden mirror | client Use → mirror/trigger object → message `0x27` to the exit object → handler `0x2d450` (hooked at `0x2d455`) | Handler arrival observed on Windows. Acceptance implemented in v2, not yet observed live |
| Walk-through exit volume | trigger volume → generic target loop `0xc2785` → message `0x27` → the same handler | Inferred from static analysis; v2 logs the activator for each arrival |
| End of a scripted sequence | No script or cinematic code calls the single-player level change `0x2d530` directly, so such a change must trigger an exit object (logged by v2, typically with `activator=none` or `object`) or use a path outside `object.lto` | **UNVERIFIED**. Instrumented only; script playback stays out of scope |

All three paths end at the same server decision: one change per epoch, to the next route world,
through the native multiplayer end-of-level and NextLevel. The developer `ExitLevel` command
calls the single-player level change directly and **bypasses the session**. Do not use it in
tests.

## Natural transition evidence (coop-session-v2)

DLL `0018a3f8…`, one client (`johnny`), cheats allowed on the disposable server only.
Excerpt from `KPCServerTools.log`. No `!next` was typed:
```text
g=1 e=1 seq=4 PLAYING exit-seen next=worlds/realm1/r1m2c flag152=1 exit=0x03280ef4 activator=member:johnny start=-
g=1 e=1 seq=5 TRANSITION exit next=worlds/realm1/r1m2c flag152=1 exit=0x03280ef4 activator=member:johnny start=-
g=1 e=1 seq=5 TRANSITION change [Co-op] Exit reached -> R1M2C
g=1 e=2 seq=6 INTRO_PENDING world-loaded worlds\realm1\R1M2C
g=1 e=2 seq=7 PLAYING exit-seen next=worlds/realm1/r1m3a flag152=1 exit=0x03280ef4 activator=member:johnny start=-
g=1 e=2 seq=8 TRANSITION change [Co-op] Exit reached -> R1M3A
g=1 e=3 seq=9 INTRO_PENDING world-loaded worlds\realm1\R1M3A
g=1 e=3 seq=10 PLAYING exit-seen next=Worlds/Realm1/R1M3b flag152=1 exit=0x03280ef4 activator=member:johnny start=-
g=1 e=3 seq=10 PLAYING exit-refused [Co-op] Exit destination is outside the MVP route; not changing world
```
The user reported the R1M2C change was made by right-clicking the golden mirror. The
`exit=` address repeats across worlds because the engine reuses that object slot; it is not a
stable exit identity.

## Transition intermission and music (investigation)

These are two **independent** problems.

**User observations** (build `0018a3f8`, one client, natural transitions):
1. **Scoreboard:** at each transition the multiplayer scoreboard stays up until a click. Enemy
   sounds continue behind it. The click reveals the next map, and the scoreboard sometimes
   reappears.
2. **Music:** no music plays while the scoreboard shows. After the click, about one bar plays
   and then stops. **Control:** ordinary stock multiplayer never plays background music either.

### A. Scoreboard: CONFIRMED stock "click to continue" intermission

Confirmed by static analysis of the stock 1.13 `object.lto` (`3a03f879…`) and `cshell.dll`
(`703813a4…`). Message names come from cshell's `MID_*` table at `0x2011ca6c`.

| Step | Code | Finding |
| --- | --- | --- |
| Session change (current build) | `0x100760` | Stores the server time at `+0x1ae4`, broadcasts **`0x48` MID_MULTI_INTERMISSION** (`0x47` in single player) and sets `+0x1ae8` |
| Continue condition | `0x1007c0` | Its **only** caller is the player method `0xef960`, and only while the player's input flags `+0x134 & 0x2800` are set (fire/use). It needs ≥ 3.0 s (`0x10117370`; 0.5 s in single player), then sets `+0x1ae9`. **The world does not change until a player clicks.** The old world keeps running in the meantime |
| Pending change | `0xffb70` (server shell Update) → `0x100720` → `0x1005c0` | NextLevel sends `0x46` MID_CHANGING_LEVELS and loads the world |
| Client `0x48` | `0xcb4f7` → `0xcdd50` | Game state `[shell+0x1398]=7`; screen manager shows screen 2 (scoreboard) |
| Client `0x46` | `0xcb47a` → `0xcdc80(0)` | State 5. With argument 0 the screen is **not** closed |
| Client OnEnterWorld | vtable slot 8, `0xc7d20` | Closes the screen only if state ≠ 5, so not after a level change |
| Client `0x1a` MID_PLAYER_ADDED | `0xcb26f` | In multiplayer: state 5 → 1. Then, if the screen is none or 2 **and** client interface `vtbl+0x230` returns non-NULL (inferred: `GetClientObject`), it closes the screen (`0xad3c0`) and calls `0xc6f30(0,0,0)`. The server sends it from its client-enter code (`0xfbe20`, callers `0xfeb86`/`0xfebf0`/`0xfec35`). This likely arrives before the client object exists, which would explain the second appearance (inference) |

**Fix (build `565dec76…`, commit `c5ce65a`; emulator-tested and native server smoke-tested; NOT yet tested with a client):** with the new default
`[Coop] CoopIntermission=0`, the session no longer calls `0x100760`. It sets the shell's native
pending-NextLevel flag `+0x1ae9`, which the server update consumes on the next frame through
the hooked NextLevel. This is the native post-intermission path without the scoreboard
broadcast or the click wait. The client sees the same message flow as after a server-console
`nextlevel`, which never shows the scoreboard. The change is refused (`change-failed`) if a
native intermission is already active. `CoopIntermission=1` restores the stock behaviour.

### B. Multiplayer music: one-time broadcast at world initialisation

**Confirmed by static analysis** (`object.lto` `3a03f879…`):
* `WorldProperties` passes the world's music property to the parser `0xbad50` (call site
  `0x10494b`).
* The parser builds a music event (vtable `0x1011d240`, 0x190 bytes) and broadcasts
  `MID_MUSIC_LOAD` `0x4d` (song index) or `MID_MUSIC_LOAD_STRING` `0x4a`.
* `WorldProperties` immediately runs the event's `vtbl+8` = `0xbaa90`, which broadcasts:
  * `[GROOVE 0x52]`, then `[TRANSITION 0x51]`;
  * the song (when the name at `+0x108` is set):
    * with the `play` flag `+0x188`: PLAY `0x4e`/READY `0x4f` by intensity
      (`0x4e + (intensity set)`), string forms `0x4b`/`0x4c`;
    * otherwise always READY `0x4f`/`0x4c`;
  * then `[INTENSITY 0x50]` and `[FADEOUT 0x53]`.
* Every one of these uses `hClient = NULL` and is sent **once, at world initialisation**.
* An earlier byte scan found no PLAY sender because its ID is computed. No other code
  creates these events.

**Live evidence it explains:**
* A player who joins an already-running server receives none of it: no music.
* After `!start`, the reloading client receives it while still loading and hears about one
  bar. That the client's own world switch then stops it is still an inference.

**Fix: `[Coop] CoopMusic=1` (default in sessions; commits `db7649c`, `872de1e`, DLL
`e17c150f…`):**
* When `WorldProperties` creates the event, the session snapshots its fields: `+0x108` song,
  `+0x188` play, `+0x189` intensity, `+0x18a` groove, `+0x18b` transition, `+0x18c` fadeout.
* Each member is scheduled once per epoch when it spawns.
* 1.5 s later, a per-frame hook (server shell Update → `0x100720`, call site `0xffbbf`,
  original `e8 5c 0b 00 00`) sends **that client only** the original sequence in string
  form: LOAD_STRING, [GROOVE], [TRANSITION], READY/PLAY_STRING, [INTENSITY], [FADEOUT].
  The engine calls are the same `g_pServerDE` vtable slots the original code uses.
* What this gives:
  * a first join into a preloaded world gets music;
  * a late joiner gets it without restarting anyone else;
  * respawns don't resend;
  * a world without a music event sends nothing, so a previous map's track is never resent;
  * a reconnect is delivered again.
* `CoopMusic=0` restores the stock behaviour. Nothing changes outside a session.

| Evidence | Status |
| --- | --- |
| Snapshot on the real engine (`TraceMusic=1`) | **Windows, no client:** `song=song5 play=0 intensity=1 transition=2` for R1M2B/R1M2C, `song1` for R1M3A |
| Exact per-client message sequences | **Emulated** (compiled DLL, stock `object.lto`, recorded `StartMessage`/`Write*`/`EndMessage`): first join, second player, map change, world without music, R1M3A, no resend on respawn |
| Audible, sustained playback | **WINDOWS-TESTED by the user (2026-11-11, DLL `e17c150f…`, then `13ca1d0a…`):** song5/song1 heard; a second laptop joining a running game also got music. Not beat-synchronised across clients. (Before that test this row read "NOT TESTED"; the emulator does not run `cshell.dll` or DirectMusic) |

**Limits and open points:**
* **Same track for every player, not the same beat.** All players get the same song,
  transition mode and intensity, but each starts playback when its own delivery arrives
  (1.5 s after its spawn). Players who change map together start within network delay;
  a late joiner starts from the beginning of the song.
* **No adaptive changes in multiplayer.** In single player, level scripts may issue further
  music events (for example intensity changes). Those scripts do not run in multiplayer,
  and only the world's default music is delivered.
* **Possible extra bar.** The stock broadcast still reaches clients that are reloading a
  world, so the short phrase after `!start` may still be heard before the per-player start.

### C. Synchronised original intro (R1M3A proof of concept)

**Confirmed by static analysis** (`object.lto` `3a03f879…`, `cshell.dll` `703813a4…`):

| Step | Code | Finding |
| --- | --- | --- |
| Script lookup | `WorldProperties` → `0x1045b1` → `0xc50c0(path)` | Runs in every mode. Native R1M3A trace on the multiplayer server: `mapdata\realm1\r1m3\r1m3a\cathedral1start.scr`, `script object found` |
| Play gate | `WorldProperties` `0x104830` | Calls `0xc5340(name)` **only when the game type is 0**. This is why multiplayer never shows the intro |
| Script player | `0xc5340` / `0xc55b0` | `0xc5340` resets a loaded script and appends it to the playing list (`Playing Script: %s`). `0xc55b0(name)` = still playing. Both cdecl, one argument |
| Script clock | server Update `0xffb70` → `0xffba5` → `0xc5630` | Advances playing scripts every frame, with no game-type gate |
| Presentation | `0xc5770`…`0xc5a10` | All eight `MID_CINEMATIC` (`0x3d`) senders (camera, letterbox, user input) broadcast to **all clients** (`hClient 0`, guaranteed) |
| Client | cshell `0xcb203` → `0xa7a60` | `MID_CINEMATIC` dispatcher with 8 subtypes; no game-type gate in the dispatcher or its handlers |
| Input/letterbox values | parse `0xbfb90`/`0xb7490` | `on` = 1, `off` = 0: `user_input on` = `0xc5a10(1)`, `letterbox off` = `0xc59a0(0)` |
| `player invulnerable` | `Player` event `0xc1a80` | Gated off in multiplayer (and in single player it affects only the first player). Not reproduced |

The R1M3A script (`CATHEDRAL1START.SCR`, 51.7 s) sets letterbox on and user input off at 0 s,
loads further level scripts, plays four camera paths with subtitles and fog fades, sets `music song1
intensity 3` at 47.7 s, triggers `spawn6`, briefly disables AI, and releases at 51.7 s
(letterbox off, user input on, camera reset). Its ending script is `cathedral1astart.scr`.

**Implementation (commit `23c68f4`, DLL `13ca1d0a…`):**
* `[Coop] CoopIntro=1` (default 0) arms the intro when the R1M3A route world loads in a started
  session. The proof-of-concept allowlist is R1M3A only.
* The session stays in `INTRO_PENDING` until every present member has spawned in the new epoch,
  plus a 3 s settle. Readiness means the server has created that player in the new world after
  the client entered it.
* After a 30 s barrier timeout it starts with whoever is in. It **never** starts with zero
  players.
* It then calls the original `0xc5340` **once**. The script runs server-side once, so its
  gameplay side effects (script loads, `spawn6`, AI) happen once. Its presentation events go
  to every client through the original broadcasts.
* During `INTRO_RUNNING`, `!start`, `!next` and exits are refused. When `0xc55b0` reports the
  script finished, the session returns to `GAMEPLAY`.
* After 120 s the session sends `user_input on` and `letterbox off` with the original helpers and
  returns to `GAMEPLAY` (best effort; no camera reset is sent).
* `[Coop] CoopTestStartWorld=R1M3A` (test only) makes `!start` load R1M3A. Only the three route
  names are accepted; anything else is ignored and logged.

**Policies and limits:**
* **Timing:** all clients receive the same broadcast stream, so they start the pan within network
  latency of each other. There is no frame-level synchronisation.
* **Late joiners:** a player who joins during the intro sees its remaining broadcasts from that
  point. It is never restarted.
* **Disconnects:** a disconnect while waiting is removed from the barrier (detected at the next
  event). A disconnect during playback has no effect on the others.
* **Invulnerability:** players are input-locked but not invulnerable. The original
  `player invulnerable` line is gated off in multiplayer.
* **Voice-over:** `streamsound player … local` targets "player". In the two-client test both
  machines heard the narration. With `13ca1d0a` it **trailed the subtitles** (fixed by
  `CoopScriptClock`, [section D](#d-script-clock-cadence-narration-versus-subtitles), user-
  confirmed). It is still **cut off** after "…useful ally for you", by the server's sound timer
  ([section E](#e-narrator-cut-off-after-will-be-a-useful-ally-for-you-sound-timer)).

| Evidence | Status |
| --- | --- |
| Session policy (barrier, settle, timeouts, start once, refusals, completion) | Unit tests (64/32-bit) |
| Compiled DLL against stock `object.lto`: lookup snapshot inside NextLevel, arm, barrier, single `c5340` call, `!next` refused, completion via `c55b0`, overrun release `c5a10(1)`/`c59a0(0)`, invalid test start | **Emulated** (script player and helpers are recording stubs) |
| Native server: DLL loads, intro and test-start settings effective, lookup hook active, R1M2B/R1M3A load, server keeps running | **Windows, no client** (3 runs in a disposable copy, then the standard Coop/Normal/PassThrough smoke in `smoke1`). One earlier run in the second copy exited silently during a world start. That copy's port had been reset to 42972 while another test server already used it, so a port collision is the probable cause: no crash record, and it did not recur on a free port |
| Camera pan, letterbox, input lock and subtitles on clients; two-client presentation | **WINDOWS-TESTED by the user (2026-11-11, desktop + laptop, DLL `13ca1d0a…`):** camera pans, letterbox, scripted events and coordination worked, and the two machines matched each other (by eye, not measured). Server log: `intro-start` t=356469 ms, `intro-finished` t=399016 ms. (This row read "NOT TESTED" before that run) |
| Narration versus subtitles and script events (DLL `13ca1d0a…`) | **BUG, user-observed:** the voice-over trailed the subtitles on both clients, and the window smash at 47.8 s cut off narration. The server ran the ≥ 53 s script in 42.547 s of wall clock (start to "finished"). Root cause in [section D](#d-script-clock-cadence-narration-versus-subtitles) |

### D. Script clock cadence (narration versus subtitles)

**Symptom (user-observed, DLL `13ca1d0a…`, desktop + laptop):** the narration trailed the
subtitles on both machines by about the same amount, and the scripted window smash cut it off.

**Authored data** (`PSYCHO.REZ`, read-only): `r1m3-intro.wav` is PCM mono 22 050 Hz, **44.19 s**.
The script starts it at 1.0 s (`streamsound player … local`). At real-time rate the voice ends
at script time 45.19 s, which is 2.6 s *before* `47.8 window_break`. In the authored timing the
smash therefore never interrupts the narration.

**Root cause (confirmed by static analysis; stock `dedicated.exe` `b940fd6d…`, `server.dll` `83b7bce8…`, `object.lto` `3a03f879…`):**

| Step | Code | Finding |
| --- | --- | --- |
| Host loop | `dedicated.exe` `0x401c7b`–`0x401c94` | Busy loop; runs a server frame (`0x403ad0` → `IServerMgr` vtable `+0x50` = `server.dll 0x2358c`) only after `timeGetTime` advanced **≥ 8 ms** |
| Server time | `server.dll 0x2ed76` | `time = (timeGetTime() − base) × 0.001` |
| Frame time | `server.dll 0x2a05f`, `0x2a19c`–`0x2a1d0` | `elapsed = time − last`, then **clamped to [0.01, 0.2] s**. That dt drives one game frame and the game time `+0x20c` |
| Game update | `object.lto 0xffb70` | Caps dt at 0.2 s; passes it to the script updater `0xc5630` (call at `0xffba5`) and to `0xea6d0` |
| Script clock | `0xc6260` | `script_time += dt`; dispatches every event whose authored time ≤ script time |
| Completion | `0xc6380` | Finished only when every event has been dispatched **and** no running event is left. `intro-finished` cannot happen before script time 53.0 |
| Running events | `camera_path` update `0xb3230`, fog fade `0xc2b60` | Advanced by the same dt, so camera paths, fades and event cues all share the script clock |

Every frame is credited at least 10 ms. When the host's timer resolution allows frames faster
than 100 Hz (8 ms frames: up to 10/8 = **1.25×**), game and script time outrun the wall clock.
Narration is streamed by each client in real time, so subtitles, camera cuts and the smash run
ahead of it. At 1.25× the smash comes 37.4 s into the 44.2 s clip, cutting about 6.8 s. The
subtitle lead grows by 0.2 s per script second (about 1.5 s at the 8.5 s subtitle and 7.6 s at
the 38.9 s subtitle). That growth is a **prediction**: the user reported "about 2 s", and no
per-subtitle client timing has been recorded.

**Native measurements** (empty server, `smoke1`, DLL `e86bb352…`, read-only observer
`TraceTiming=1`, `CoopScriptClock=0`; `clock-calibration` = first 10 s of frames):

| Host timer state (`NtQueryTimerResolution`) | Frames / 10 s | Engine ms per 10 000 ms wall |
| --- | --- | --- |
| 15.625 ms (nothing raising it) | 703 | 10 215 (1.02×) |
| 1 ms held by a helper process (as a game client on the same PC would) | 1052 | 11 531 (1.15×) |
| User's live intro run, client on the server PC (`13ca1d0a`): ≥ 53 s of script in 42.547 s | — | ≥ 1.246× |

The inflation therefore depends on the host's timer state. A repeat under the 1 ms request ran
698 frames per 10 s (1.02×), so the same request does not always give the same cadence. It is a
property of the stock dedicated server, not of the co-op code. **Not measured:** stock single
player (the client hosts its own server; its frame pacing differs), and 60 versus 165 FPS clients.

**Fix (`e86bb352…`): `[Coop] CoopScriptClock`** (default = `CoopIntro`). It adds one guarded
call site, `0xffba5` (`e8 86 5a fc ff`, original bytes verified). For every frame the scripts
receive `min(engine dt, wall time not yet given)` (`coop_script_clock.h`, `GetTickCount`):
* script time never runs ahead of wall time, and never moves in a larger step than the
  original engine dt;
* after a hitch, scripts catch up at no more than the original rate (unpaid time is capped at
  250 ms);
* only the `0xc5630` argument changes. Engine game time, `0xea6d0`, physics, AI, the intro
  barrier, run-once execution and all broadcasts are unchanged.

`CoopScriptClock=0` restores the engine dt (for A/B tests). With `TraceTiming=1` the site
also observes, forwarding dt unchanged when the governor is off. It logs:
* `clock-calibration` once;
* during an intro, one `intro-event i=<n> at_ms=<authored> script_ms=<clock> wall_ms=<since start>`
  line per dispatched script event (at most 128 per intro);
* `intro-clock frames=… engine_ms=… script_ms=… wall_ms=…` at the end.

| Evidence | Status |
| --- | --- |
| Governor arithmetic: 1.25× host gives 8 s per 8 s; slow engine not accelerated; bounded catch-up; coarse ticks; 32-bit wrap | Unit test `test_coop_script_clock.c` (64/32-bit, `-Werror`) |
| Compiled DLL against stock `object.lto`, with a stand-in for `c5630` that steps the authored R1M3A times exactly as `c6260` does, under 40 ms frames credited 50 ms | **Emulated:** 53 s script = 53 040 ms wall governed, 42 440 ms observed (dt forwarded bit-identically). Narration → smash 46.8 s governed versus 37.4 s observed. Site left original when neither option is set |
| Native server: DLL loads, `effective CoopScriptClock=1`, hooks installed; governed calibration `engine_ms=10213 script_ms=9993 wall_ms=10000`; Normal and PassThrough smoke PASS | **Windows, no client** |
| Native Coop smoke (three console `nextlevel`s in quick succession on an empty server) | **Intermittent crash, PRE-EXISTING.** Access violation in `object.lto+0x87c41`, inside the AI "Building path table." routine (`0x87be0`, called from a path-network object's staged update `0x95259`). Crashed 3 of 4 runs with `e86bb352` and 1 of 3 with the previous user-tested `13ca1d0a`, same offset and same failing step (`nextlevel 2`). Not caused by the script clock: there is no code path from `0xc5630` into it. Not yet checked under pass-through |
| Narration, subtitles and smash on real clients | **USER-CONFIRMED (2026-11-11, desktop + laptop, `e86bb352`):** subtitles and events no longer run ahead of the narration. Server log of that run: `intro-clock frames=6598 engine_ms=66015 script_ms=53005 wall_ms=53000`, and every `intro-event` within 7 ms of its authored time. The narration still stops early: a different cause, [section E](#e-narrator-cut-off-after-will-be-a-useful-ally-for-you-sound-timer) |

**Hypotheses still open:**
* client-side stream start latency (the clip may start a little after its broadcast);
* whether stock single player shows the same inflation at high client frame rates.

Neither is needed to explain the observed cut-off.

### E. Narrator cut-off after "…will be a useful ally for you" (sound timer)

**Live report (DLL `e86bb352…`, desktop + laptop, 2026-11-11):**
* The subtitle/script cadence fix from section D works in real gameplay.
* The narration still stops abruptly right after "…will be a useful ally for you". This is a
  separate problem: the lifetime of the voice playback, not the script clock.

**Not an authored end** (`PSYCHO.REZ`, read-only, `R1M3-INTRO.WAV` 44.19 s, envelope measured
in 50 ms windows):

| WAV time | Speech |
| --- | --- |
| 30.0–36.45 s | "By way of the Scourge, the earth element will be a useful ally for you" ("…will be a useful ally for you" ≈ 34.95–36.45 s) |
| 36.55–37.2 s | "here," |
| 37.55–43.7 s | "as the way in is not as straight as it first seems." (the 38.9 s subtitle) |
| 43.7–44.19 s | silence |

The clip itself contains the whole last sentence, so the voice is cut, not finished.

**Root cause (static analysis; stock `server.dll` `83b7bce8…`, `object.lto` `3a03f879…`):**

| Step | Code | Finding |
| --- | --- | --- |
| Script event | `streamsound` parser `0xbd010`, `local` → mode 2, execute `0xbcf2d` | Calls `0x52c80`, then `g_pServerDE->PlaySound` (vtbl `+0x174`) with flags `0x430`, and keeps the handle in a 4-slot stream table (`0x10159c74`). Nothing in `object.lto` kills it: `0xbcd30` only polls `IsSoundDone` (vtbl `+0x17c`) and clears the slot |
| Track lifetime | `server.dll 0x2c6e6`–`0x2c70a` | time left `+0x20` = duration + min(0.5 s, duration/2) = **44.69 s** |
| Countdown | updater `0x29f0b` (only caller `0x2a1fc`: ECX = server manager, one float = the clamped frame dt `[esi+0x1f0]`, `ret 4`) → `0x2c805` | time left −= frame dt |
| Delivery | per-client set builder `0x1f920` (`0x1f94f`–`0x1f961`) | A track whose time left ≤ 0 is no longer put into the client's sound set, so the client stops it. `IsSoundDone` (`0x277e2`) reports it done at the same moment |

The frame dt is the inflated engine dt from section D. The user's own run (`smoke1` log,
`intro-clock frames=6598 engine_ms=66015 script_ms=53005 wall_ms=53000`) ran the engine at
**1.2456×** wall time. 44.69 s / 1.2456 = **35.9 s** of real time after the stream started, which
is "…ally for you" in the WAV, before any client start latency. The earlier report "the window
smash cut it off" (DLL `13ca1d0a…`) is consistent with the same timer: at 1.25× it expired
about 1.5 s before the smash.

* **Same on both clients (prediction):** the track is removed from every client's set on the
  same server frame, so the cut-off should be identical on both machines (each heard from its
  own stream start). The `smoke1` server log of that run lists one member (`johnny`), so the
  two-machine comparison is the user's observation, not a logged fact.
* **Single player:** the client-hosted server runs one frame per rendered frame, so the 10 ms
  minimum only inflates time above 100 FPS. Not measured.
* **Ruled out:** the ending script `CATHEDRAL1ASTART.SCR` (`0 stopstream …r1m3-intro.wav`) runs
  only when the intro is aborted (`0xc5499` → `0xc5340`), not on normal completion
  (`0xc569b` → `0xc56a6`). The live log shows the normal timeline to 53 s and "finished".

**Fix `[Coop] CoopSoundClock`** (default = `CoopScriptClock`, commit `aa9949c`):
* the call `server.dll 0x2a1fc` (`e8 0a fd ff ff`) goes to the proxy;
* the proxy forwards `min(engine dt, unpaid wall time)` to `0x29f0b`, with its own instance of
  the script-clock governor (unit-tested arithmetic);
* only server-side sound timers change. Scripts keep their own governor, and engine game time,
  physics and AI are untouched.

| Guard | Behaviour |
| --- | --- |
| `server.dll` hash `83b7bce8…` (Europe/Germany/Italy profile) and the original 5 bytes | Required. Otherwise the site stays original and the log says why (never fatal) |
| USA `server.dll` (`93178237…`) | Not supported: no fixture to verify the site |
| `CoopSoundClock=0`, no session, `Enabled=0` | Site left original |

**Evidence:**

| Evidence | Status |
| --- | --- |
| Emulator, compiled DLL on stock `server.dll`: 44.69 s timer, 40 ms frames credited 50 ms | Expires after **44 760 ms** of wall time governed (stock 35 750). Site original in every disabled mode |
| Native server, `smoke3` (port 42990), DLL `1b8816cb…` | `sound clock hook installed (server.dll 2a1fc -> 29f0b)`; Coop, Hold, Normal and PassThrough smoke PASS. In Normal and PassThrough the hook is not installed |
| Full narration on the desktop and laptop | **NOT TESTED.** Two-PC test in [NEXT-SESSION-HANDOFF.md](NEXT-SESSION-HANDOFF.md) |

With `TraceTiming=1` the intro also logs
`sound-clock frames=… engine_ms=… sound_ms=… wall_ms=… governor=on`.

### F. Enemy health bars in co-op

**Live report:** enemy health bars never appear when enemies take damage in co-op.

**Root cause: a missing show-event, not missing health data or a HUD problem** (confirmed by
disassembly):

| Step | Code | Finding |
| --- | --- | --- |
| Gate | `object.lto 0xf8fe0` | `return [shell+0x1d0] == 0` (single player). Its only four callers are the enemy bar manager |
| Update (every frame from `0xffbab`) | `0xea6d0`, gate call `0xea6d2` | Ages and expires entries, then calls Send. In multiplayer it returns at the gate |
| OnDamage (69 AI/monster call sites) | `0xea7c0`, gate `0xea7c3` | Records the monster key, style, name and health/max from the server's own values in the global 10-entry table (`0x101642d8`) |
| Remove on death (28 call sites) | `0xeaa10`, gate at its first instruction | Clears the entry |
| Send (only from Update `0xea783`) | `0xeab10`, gate `0xeab11` | Writes the table and calls vtbl `+0xc4` with ID `0x56`, client 0 (all clients) and flag `0x80` (guaranteed) |
| Client | cshell `0xcb998` → `0xb8c80` (`MID_MONSTER_HEALTH_BAR_DATA 0x56`), reset `0x57` → `0xb8960` | No multiplayer test in the handler, the draw (`0xb8dc0` via `0xcdb48`) or the per-frame fade. The font is created in OnEnterWorld for both modes |

So in multiplayer the server never fills or sends the table. The stock client already accepts
and draws the message with its original geometry. KPCFix only rescales that same draw.

**Fix `[Coop] CoopHealthBars`** (default 1 in a session):
* the four gate calls go to session predicates that answer "proceed";
* Send is limited to **10 Hz**. In single player it runs every frame; on a dedicated server
  (up to 125 frames/s) that would be a guaranteed broadcast storm;
* no client change. Player health uses other messages and is not touched;
* everyone sees the bars of any monster anyone damages, because the table is global, not per
  player;
* a late joiner gets the full table within 100 ms;
* bars clear through the original Remove on death, or by the original timeout.

**Limits:**
* The `DamageBars` console variable is the dedicated server's (missing, so 0 = all bars). A
  client's own setting has no effect.
* Stock quirk: the Remove loop skips the last of the 10 slots, which then expires by timeout.
* Bar timeouts age with engine dt (≈1.25× fast: a 5 s bar lasts about 4 s).

| Evidence | Status |
| --- | --- |
| Emulator: the four sites, gate results, Send at 0/50/49/1/100/250 ms gives 1,0,0,1,1,1, sites original when disabled or outside a session | PASS |
| Native server: hooks installed with the stock `object.lto`, server runs and quits cleanly | PASS (no clients) |
| Bars visible and consistent on both clients | **NOT TESTED** |

### G. Enemies missing after the R1M3A window smash (`spawn6`)

**Live report:** the window smash (47.8 s) happened, but the enemies from
`47.9 trigger_generic spawn6` never appeared on either client.

**Root cause (confirmed by disassembly and world data):**
* `trigger_generic` (parser `0xbf390`, execute `0xbf520` → `0x300a0`) looks the name up in the
  object group table (`[0x10164264]`). For each member it sends message `0x27` with the head
  of player list 0 as activator. If the group does not exist, it silently does nothing.
* In `R1M3A.DAT`, group `spawn6` is three `CMonsterSpawner` objects, all with
  `AttackActivator=1` and `Retrigger=0`:

  | Spawner | Spawns | MiscFlags |
  | --- | --- | --- |
  | `CMonsterSpawner7` | 10 × CHeadless | 6 (Normal + Hard) |
  | `CMonsterSpawner41` | 5 × CHeadless | 1 (Easy) |
  | `CMonsterSpawner35` | 3 × CGasbag | 6 (`SpawnOutOfView=1`) |

* `CMonsterSpawner::ReadProp` (`0x93a80`) does `or al,0x10` (**forced "not in multiplayer"**,
  `0x93c5d`) before the shared filter `0xf9000(skill, flags)` (call `0x93c65`).
  * `0xf9000` returns 0 in multiplayer whenever bit `0x10` is set.
  * The object is then marked not-to-create (`0x93c71`–`0x93c7b`).
  * So **no `CMonsterSpawner` exists in multiplayer**, and group `spawn6` is empty.
* Placed monsters and `CSpawner` use the authored flags (`0xd4b44`), which is why R1M2B still
  has enemies. The trigger, spawn and AI code after creation has no game-type gate:
  * `0x27` → `0xd4fe0` → Activate `0xd5490`;
  * Update `0x93cb0` → `0xd4ce0` CreateObject;
  * `AttackActivator` → `0x93e00`.

**Fix `[Coop] CoopSpawners`** (default 1 in a session):
* the two bytes `0c 10` at `0x93c5d` are NOPed, so the authored MiscFlags pass to `0xf9000`;
* difficulty and a designer's own `0x10` are still honoured;
* creation happens once, on the server, at world load. The monsters replicate like any placed
  monster, and nothing runs per client;
* this restores the single-player monster spawners on **every** route world (R1M2B/R1M2C get
  them too), including `bosspawn`/`fatladyboss` groups. `0` restores stock multiplayer.

**Difficulty:**
* The native server reports `skill=2` (Sleepless, the Normal bit).
* At that skill, spawn6 creates `CMonsterSpawner7` (10 CHeadless) and `CMonsterSpawner35`
  (3 CGasbag); the Easy-only `CMonsterSpawner41` does not exist.
* Optional `[Coop] CoopSkill=0…4` writes the original variable `0x1015b678` before every world
  load. It uses the stock skill command's names: 0 Lullaby, 1 Daydream, 2 Sleepless,
  3 Cold Sweat, 4 Nightmare.
* Absent = unchanged.

**Risks to observe:**
* `disable_ai` (`0xc28a0`) and `player invulnerable` return early in multiplayer. Monsters
  spawned at 47.9 s may therefore act in the last ~3.8 s of the letterbox while players have
  no input and are not invulnerable.
* `AttackActivator` targets the most recently joined player (the head of player list 0).
* Reconnect does not re-trigger anything: the script runs once per epoch. `Retrigger=0` means
  a second trigger would *deactivate* an active spawner, which the abort-only ending script
  could do.

| Evidence | Status |
| --- | --- |
| Emulator: NOP present only in sessions with `CoopSpawners=1`; `TraceSpawners` hook runs the real `0xf9000`: (skill 0, flags 1) → 1, (0,6) → 0, (2,6) → 1, (0,0x11) → 0, (4,4) → 1 | PASS |
| Native server, `TraceSpawners=1`, skill 2 | filter calls per world: R1M2B 39 accepted / 12 rejected, R1M2C 41/7, R1M3A 42/15. In the 64 logged decisions every rejection is Easy-only (flags 1) or Hard-only (flags 4). The server keeps running and quits cleanly (exit 0) |
| spawn6 enemies appear once, visible and fighting on both clients | **NOT TESTED** (needs players: the trigger needs an activator) |

### H. Stock scoreboard from `!start` until the cinematic (`CoopStartScoreboard`)

**Request:** show the original multiplayer scoreboard to every player from the host's
`!start`, keep it up while the map loads and during the intro barrier, and close it
automatically when the R1M3A cinematic begins. There must be no click gate, and no scoreboard
on natural exits.

**Client evidence** (cshell `703813a4…`, confirmed by disassembly):

| Message / code | Behaviour |
| --- | --- |
| `0x48` MID_MULTI_INTERMISSION → `0xcdd50` | No payload. In multiplayer: state 7, screen 2 (scoreboard). No client timer and no click handling (key handler `0xabd00` returns 1; fire/use `0xcd3b0` is an empty `ret`). The 3 s + fire/use gate is purely the server's native end-level (`0x100760`/`0x1007c0`), which is **not used** |
| `0x46` MID_CHANGING_LEVELS | State 5. The renderer (`0xcdb57`–`0xcdb6f`) does not draw screen 2 in state 5, so **the scoreboard cannot be seen while the client changes world** |
| Re-entry: `0x1a` and `0x69` MID_CAN_RENDER (sent by the server's client-enter code) | Either closes screen 2 (`0xcb2d8`, `0xcbba6`) |
| `0x69` → `0xcbb86` | No payload. Closes screen 2 and returns to state 1 (`0xc6f30`) |
| Showing `0x48` twice without a close in between | Loses the saved HUD flag. The implementation pairs every show with one close |

**Sequence** (pure policy `coop_scoreboard.h`, unit-tested):
1. At `!start`, send `0x48` to each present member. Hold the world change for 1 s, then use the
   usual pending-NextLevel flag (`+0x1ae9`), still without `0x100760`.
2. NextLevel: each client's stock re-entry closes it (state 5 shows a blank/loading screen
   meanwhile; client limitation).
3. In a world whose intro is armed, send `0x48` again to each member 250 ms after it spawns,
   including late joiners during the barrier. Members spawn in sequence, so each client may
   show the new world for a moment between its re-entry and this show.
4. When the barrier releases, send `0x69` to every shown member **before** the script player
   (`0xc5340`) is called. The camera/letterbox sequence therefore starts with the screen
   closed and the original input policy.
5. Any other outcome closes it:
   * a world without an intro (stock re-entry);
   * change failed or timed out (`0x69`);
   * barrier timeout (the intro still starts);
   * intro failure;
   * a 45 s failsafe.

   A member that left is forgotten without sending to a stale handle. Natural exits never show
   it.

**Not solved server-side:** keeping it **visible during** the blocking world load. A client
patch of cshell `0xcdb6f` (`32 c0` → `b0 01`, draw screen 2 also in state 5) is the
identified candidate. It is not implemented: it is untested and edition-specific (three shell
profiles).

| Evidence | Status |
| --- | --- |
| `test_coop_scoreboard.c` (64/32-bit): intro flow, no-intro world, change failed/timeout, barrier timeout, failsafe, disconnect + late join into the same slot, no use on natural exits | PASS |
| Emulator, compiled DLL: `!start` sends `0x48` to both clients, change held, shown again after each spawn, `0x69` to both before `c5340`; nothing on natural exits | PASS |
| What the players see | **NOT TESTED** |

### Client-side evidence (no code change)
* In the client console, `c_DebugMessages 1` makes the stock client print
  `Client OnMessage: <id> <MID name>` for every message from the server.
* A server-console `nextlevel` changes world without `0x48`.

## Natural transition test (build `coop-session-v2`, cheats on the disposable server only)

1. Check that the server log starts with `build=coop-session-v2 … sha256=0018a3f8…` and `route=R1M2B>R1M2C>R1M3A`.
2. Launch the client with `Psycho.exe` → Play and join. In chat (**T**), type `!ready`, then `!start`.
3. Noclip is allowed only because this test server starts with `AllowCheats=1`. Use the client
   bindings `V` (NoClip) and `I` (Invuln).
4. **Without typing `!next`**, walk into the R1M2B hallway exit. Expected log lines:
   `exit-seen … activator=member:<name>`, then `exit next=worlds/realm1/r1m2c`, then
   intermission and epoch 2. Note the time of entering the volume.
5. On R1M2C, right-click (Use) the golden mirror and note the time. Expected:
   `exit-seen next=worlds/realm1/r1m3a … activator=member:<name>`, then `exit …`, then
   `world-loaded worlds\realm1\R1M3A` (epoch 3).
6. Use the mirror or exit again during the intermission: there must be no second change.
7. On R1M3A, report what any exit logs (off-route: `exit-refused`, one chat reply).

## Known limitations and risks
* A natural exit has not been observed changing the world on Windows yet. `!next` remains the
  host fallback and must be reported as command-initiated.
* Single-player level scripts (`CS2START.SCR`), cinematic events and AI scripting may not run
  in multiplayer. Enemies, doors and objectives may differ from single player. This is to be
  observed in step 4.
* With `CoopSpawners=1` (default in a session) every route world also creates its authored
  monster spawners. That is single-player content, so there are more enemies than in the
  earlier co-op tests. `disable_ai` and `player invulnerable` remain gated off in multiplayer.
* The respawn location after death is the native one, the level start, and is not separated.
* Overlapping spawns can occur on R1M2C (no telefrag, possible temporary stuck).
* Only one server profile was emulated (Europe-family `server.dll`). The object hooks depend
  only on `object.lto`, which is identical in all supported regions.
* Joining clients receive replies through broadcast chat, so all players see them.
