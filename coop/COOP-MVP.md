# Co-op MVP: chat lobby and the R1M2B → R1M2C route (kpc-coop/1)

**Status (2026-11-11): IMPLEMENTED and CI/emulator-tested. NOT YET WINDOWS-TESTED with real clients.**
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
| Windows gameplay (2 clients, combat, transition) | **NOT TESTED** | follow the test protocol below and return the logs |

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
Events are `spawn` (path, slot, client, name, telefrag=skipped), `say`, `change`, `end-level`,
`world-loading`, `world-loaded`, `presence`, `exit-unsupported-flag` and `change-failed`.
At most 3000 lines are written per server process.

## Verified engine evidence (original 1.13 object.lto, SHA-256 3a03f879…)
Static disassembly is recorded here. These are runtime targets, not redistributed code.

| RVA | Finding |
| --- | --- |
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
   * If nothing happens at the exit, note it. The log will lack an `exit` line, which means
     multiplayer does not deliver the trigger. The host then types `!next` to confirm that the
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
