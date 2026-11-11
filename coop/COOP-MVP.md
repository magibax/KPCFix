# Co-op MVP: chat lobby and the R1M2B → R1M2C route (kpc-coop/1)

**Status (2026-11-11): IMPLEMENTED and CI/emulator-tested. Server side WINDOWS-TESTED without clients (DLL `262b6ead…`). NOT YET WINDOWS-TESTED with real clients.**
Experimental: this code is only on `feature/coop-campaign` and its child branches. It is not
part of the stable Server Tools R3 on `main`. Every switch defaults to off.

| Feature | State | Evidence |
| --- | --- | --- |
| Lobby/session state machine (ready, host, capacity, presence, epochs, debounce, route, timeouts) | IMPLEMENTED, CI-TESTED | `server-tools-source/tests/test_coop_session.c` (64- and 32-bit, `-Werror`) |
| Proxy hooks installed only on the hash-verified original object image, original bytes checked at all 8 sites | IMPLEMENTED, EMULATOR-TESTED (private) | `check_proxy.py <EU fixtures> --coop-session` runs the shipped DLL against the original `object.lto` |
| Chat-command lobby: names, ready/unready, away/join, host start, status | IMPLEMENTED, EMULATOR-TESTED | as above (synthetic players, native broadcast stubbed) |
| No telefrag between session players (both spawn paths) | IMPLEMENTED, EMULATOR-TESTED | native `0xf10c0` is never entered in a session |
| R1M2B separated placements (3 fixed positions) with slot reclamation | IMPLEMENTED; placements WINDOWS-TESTED earlier (trial build) | `SPAWN-ENGINE-TRACE.md`; reclamation is new and only emulator-tested |
| Server-coordinated start (fresh R1M2B) and exit to R1M2C through the native multiplayer level change | IMPLEMENTED, EMULATOR-TESTED | native end-of-level `0x100760` and NextLevel `0x1005c0` were stubbed in emulation |
| Exit trigger reached by players in multiplayer | **UNVERIFIED** | the hook exists; whether original multiplayer code calls the exit handler at all must be confirmed on Windows. The host `!next` fallback works independently |
| Intro cinematics | **NOT SUPPORTED** (skipped) | native multiplayer did not start the authored intro in earlier trials; it is not forced |
| Golden-mirror exits | **BLOCKED** | exit objects with flag `+0x152` take an unverified single-player branch; the session logs and ignores them |
| Generic safe spawning on other maps (allocator) | **DISABLED** | full-body clearance is not proven; see `GENERAL-ALLOCATOR.md` |
| Install / idempotent re-run / in-place upgrade from the live-tested v7 proxy (`7112b5ed`) / uninstall | WINDOWS-TESTED (scripts only, disposable copy, 2026-11-11) | original `server.dll` `83b7bce8…` restored; `object.lto` `3a03f879…` unchanged throughout |
| Launcher worker co-op validation (wrong start map, maplist without R1M2C) | WINDOWS-TESTED (scripts only) | rejected after the real version checks, before any server start; INI untouched |
| Native empty server: DLL load, hash gates, hooks on stock `object.lto`, session armed, build line names the loaded DLL's SHA-256 | WINDOWS-TESTED (no clients, 2026-11-11) | [server smoke evidence](#native-windows-server-smoke-evidence-no-clients) |
| Hooked native NextLevel: epoch per level change, world name matches the engine's `Loaded world`, phase stays LOBBY before Start, object module loaded once per server run | WINDOWS-TESTED (console `nextlevel`, no clients) | as above |
| Regressions: PM01 normal start writes `CoopSession=0`; `[ServerTools] Enabled=0` is pure pass-through; altered `object.lto` is rejected by the launcher checks and, if started directly, by the DLL (exit 1) | WINDOWS-TESTED (no clients) | as above |
| Windows gameplay (2 clients: lobby chat, Start, spawns, combat, exit, transition, rejoin) | **NOT TESTED** | follow the test protocol below and return the logs |

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
  * the phase is GAMEPLAY;
  * this epoch has not already changed world;
  * the destination is the next route world.

  Repeated touches from any number of players are counted as duplicates. Native code still
  takes its multiplayer branch. Game type is never falsified.
* **World change** sets the native maplist cursor so that the next entry is the route world.
  It fails closed if the maplist lacks that world. It then calls the native multiplayer
  end-of-level routine (3 s intermission, scoreboard message), the same path as frag and time
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
* maplist `R1M2B, R1M2C`;
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
Events are `spawn` (path, slot, client, name, telefrag=skipped), `say`, `change`, `end-level`, `exit-seen` (first exit-gate arrival per epoch, whatever the lobby decides),
`world-loading`, `world-loaded`, `presence`, `exit-unsupported-flag` and `change-failed`.
At most 3000 lines are written per server process.

## Verified engine evidence (original 1.13 object.lto, SHA-256 3a03f879…)
Static disassembly is recorded here. These are runtime targets, not redistributed code.

| RVA | Finding |
| --- | --- |
| `0x2d400` | Exit object message handler: message `0x27` reads the activator HOBJECT and calls `vtbl+0x70` (`0x2d450`). Other messages go to base `0xd3b50` |
| `0xc2785` | Generic target-message loop: sends `0x27` (sender NULL, activator written into the message) to listed targets. Whether trigger volumes run this loop in multiplayer is the open runtime question. The session logs one `exit-seen` line per epoch when the gate is reached |
| `0x2d450` | Exit object handler (vtable `0x118aa4`). `[0x1016de60]+0x1d0` (game type) ≠ 0 returns; otherwise `0x2d510` → `0x2d530` sends message `0x19` to clients. Single-player level changes are client-driven, so they are not used |
| `0xfb0d7` | Developer `ExitLevel` command finds `CExitTrigger` and prints `trigger+0x54` as "Exiting to level %s". This confirms `+0x54` is `NextWorld` |
| `0xfde00` | Dedicated server app message handler: `GAMEINIT`, `NEXTLEVEL` → `0x1005c0`, `SERVHOST`, `SAY:` |
| `0x1005c0` | NextLevel: maplist `0xfc640` (cursor `[0x16da78+0x10]`, names `shell+0x210+i*0x7f`, count `shell+0x1ae0`), message `0x46`, LoadWorld |
| `0x100760` | Multiplayer end-of-level: message `0x48`, flag `+0x1ae8`. After 3.0 s (`0x117370`) `0x1007c0` sets `+0x1ae9`, and `0x100720` then calls NextLevel. The time-limit check calls it at `0xfc626` |
| `0xf10c0` | Telefrag: iterates player list (`[0x157724]+4`) and sends 1000 damage to overlapping boxes. No other effect |
| `0xfee9a` | Chat relay in the server's message handler: player = client user data, text = message string, name = `player+0x1e8` |

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
   and start. The log must show `build=coop-session-v1 … sha256=<same hash>`,
   `effective CoopSession=1`, `session armed`, and `Runtime object hooks installed`.
   No player-facing event should occur while nobody is connected.
   * Optional: run `Invoke-CoopServerSmoke.ps1` from the test bundle first (see
     [server smoke evidence](#native-windows-server-smoke-evidence-no-clients)).
     Its JSON report records hashes and the log.
   * The console prints `Server started on <address>, port 42972`. On the developer
     host this address was a Hyper-V virtual adapter (`172.28.144.1`), not the LAN
     interface. Clients should join the server PC's LAN address on port 42972. If
     joining fails, note both addresses in your report.
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
5. **Objective and exit.** Play to the R1M2B hallway exit together.
   * Expected: `Exit reached -> R1M2C`, intermission, and both load R1M2C (epoch 2).
   * If nothing happens at the exit, check the log. An `exit-seen` line without an `exit` line
     means the trigger arrives but was refused (see its `flag152` and phase). No `exit-seen`
     line means multiplayer does not deliver the trigger at all. The host then types `!next` to confirm that the
     server-coordinated change works.
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

**Local client automation was not possible.** In the disposable copy, `client.exe`
and `Psycho.exe` exit with code 0 within about 1.5 s, before `dinput.dll` loads.
This happened with `+connect`/`+name`/windowed arguments and with the
`WINXPSP3 DWM8And16BitMitigation` compatibility layer applied per process. The
original install additionally has `RUNASADMIN`, which needs an interactive UAC
prompt. Client-side tests therefore still need a person at real client PCs.
**Testers:** a copied game folder does not inherit the original folder's
compatibility settings. Start clients from your normal installation.

Reproduce (in a disposable copy with the CI package installed):
```powershell
Start-Process powershell.exe -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File .\Invoke-CoopServerSmoke.ps1 -GameDir C:\KPCTest\Psycho -Mode Coop -Report C:\KPCTest\coop.json'
```
Repeat with `-Mode Normal` and `-Mode PassThrough`. For a client session, add
`-CommandFile C:\KPCTest\cmd.txt`, then append console commands (for example
`say hello`, `list_ids`, `quit`) to that file. Send back the JSON reports.

## Known limitations and risks
* The exit trigger may never fire in multiplayer. This is the largest unknown, and `!next`
  is the fallback.
* Single-player level scripts (`CS2START.SCR`), cinematic events and AI scripting may not run
  in multiplayer. Enemies, doors and objectives may differ from single player. This is to be
  observed in step 4.
* The respawn location after death is the native one, the level start, and is not separated.
* Overlapping spawns can occur on R1M2C (no telefrag, possible temporary stuck).
* Only one server profile was emulated (Europe-family `server.dll`). The object hooks depend
  only on `object.lto`, which is identical in all supported regions.
* Joining clients receive replies through broadcast chat, so all players see them.
