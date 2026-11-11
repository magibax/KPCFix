# Co-op MVP: chat lobby and the R1M2B → R1M2C → R1M3A route (kpc-coop/1)

**Status (2026-11-11):**
* **Live Windows client tested (DLL `262b6ead…`, one client):** join, lobby, `!ready`, `!start`, and a host `!next` change R1M2B → R1M2C.
* **Natural exits WINDOWS-TESTED (build `coop-session-v2`, `0018a3f8…`, one client):** R1M2B → R1M2C through the hallway exit, and R1M2C → R1M3A through the golden mirror, both **without `!next`**. The R1M3A → R1M3B exit was detected and refused as off-route.
* **Not yet tested:** a second client.

Experimental: this code is only on `feature/coop-campaign` and its child branches. It is not
part of the stable Server Tools R3 on `main`. Every switch defaults to off.

| Feature | State | Evidence |
| --- | --- | --- |
| Lobby/session state machine (ready, host, capacity, presence, epochs, debounce, route, timeouts) | IMPLEMENTED, CI-TESTED | `server-tools-source/tests/test_coop_session.c` (64- and 32-bit, `-Werror`) |
| Proxy hooks installed only on the hash-verified original object image, original bytes checked at all 8 sites | IMPLEMENTED, EMULATOR-TESTED (private), WINDOWS-TESTED (server) | `check_proxy.py <EU fixtures> --coop-session`; [server smoke evidence](#native-windows-server-smoke-evidence-no-clients) |
| Chat lobby: join announce, `!coop`, `!ready`, host `!start` | **WINDOWS-TESTED, 1 client** (`262b6ead`) | [live client evidence](#live-windows-client-evidence) |
| No telefrag between session players (both spawn paths) | IMPLEMENTED, EMULATOR-TESTED; live: `telefrag=skipped` logged for 1 client | a second player is needed to observe the protection itself |
| R1M2B separated placements (3 fixed positions) with slot reclamation | IMPLEMENTED; placements WINDOWS-TESTED earlier (trial build); slot 0 reused after a live rejoin | `SPAWN-ENGINE-TRACE.md` |
| Server-coordinated Start (fresh R1M2B, epoch 1) | **WINDOWS-TESTED, 1 client** | native intermission, world reload and respawn observed |
| Host override `!next` R1M2B → R1M2C | **WINDOWS-TESTED, 1 client** | a **command-initiated** change, not evidence of a natural exit |
| Exit handler reached in multiplayer | **WINDOWS-OBSERVED** with the activator recorded (`activator=member:johnny`) | [natural transition evidence](#natural-transition-evidence-coop-session-v2) |
| Natural exits → next route world: R1M2B hallway → R1M2C; R1M2C golden mirror → R1M3A | **WINDOWS-TESTED, 1 client** (`0018a3f8`), no `!next` | one change per epoch; the off-route R1M3B exit was refused with a single chat reply |
| Intermission scoreboard and next-map music during transitions | **UNDER INVESTIGATION** | [transition intermission and music](#transition-intermission-and-music-investigation) |
| Map changes at the end of a scripted sequence | **UNVERIFIED** | no direct level-change call from script code in `object.lto`; see [transition mechanisms](#three-transition-mechanisms) |
| Intro cinematics | **NOT SUPPORTED** (skipped) | native multiplayer did not start the authored intro; it is not forced |
| Generic safe spawning on other maps (allocator) | **DISABLED** | full-body clearance is not proven; see `GENERAL-ALLOCATOR.md` |
| Install / idempotent re-run / in-place upgrade (`7112b5ed` → `262b6ead` → `0018a3f8`) / uninstall | WINDOWS-TESTED (scripts, disposable copy) | original `server.dll` `83b7bce8…` preserved; `object.lto` `3a03f879…` unchanged throughout |
| Launcher worker co-op validation (wrong start map, maplist without R1M2C/R1M3A) | WINDOWS-TESTED (scripts only; R1M3A check added in v2) | rejected after the real version checks, before any server start |
| Native empty server for `0018a3f8`: load, hooks, session armed, NextLevel epochs through R1M2B, R1M2C, R1M3A; normal and pass-through regressions | WINDOWS-TESTED (no clients) | R1M3A loads under the dedicated server |
| Two clients: separation, combat, natural exit, rejoin | **NOT TESTED** | follow the test protocol below and return the logs |

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
| Audible, sustained playback | **NOT TESTED.** The emulator does not run `cshell.dll` or DirectMusic |

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
* The respawn location after death is the native one, the level start, and is not separated.
* Overlapping spawns can occur on R1M2C (no telefrag, possible temporary stuck).
* Only one server profile was emulated (Europe-family `server.dll`). The object hooks depend
  only on `object.lto`, which is identical in all supported regions.
* Joining clients receive replies through broadcast chat, so all players see them.
