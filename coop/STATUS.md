# Co-op status

Last updated 2026-11-15. Update this file whenever a build is installed or a playtest result comes in. It is the only status log; don't add dated handoff or results files.

Evidence levels used below:

| Level | Meaning |
|---|---|
| **live** | Confirmed in a two-PC playtest |
| **smoke** | Native `dedicated.exe` run, no players |
| **emu** | Emulator or unit tests only |
| **static** | Disassembly only |

## Current build

| Product | SHA-256 | Installed |
|---|---|---|
| Server Tools | `1b25dabe…` | Desktop `C:\Games\Psycho` as `winmm.dll` (`server.dll` original `83b7bce8…`); `C:\KPCTest\smoke1\Psycho` |
| Client | `b676e832…` | Desktop. The laptop still needs `C:\KPCTest\pkg-client-b676e832\`. |

This is the round 6 build, installed on 2026-11-14. Both co-op branches hold it. At the user's request the round 5 and 6 builds were moved onto `coop/mvp-lobby` before round 6 was playtested, so the two branches currently have the same code.

Tests passed for this build:
- server unit tests: 26/26;
- server emulator matrix: 58/58;
- native smoke: Coop, Normal and PassThrough;
- client suite: 20/20 (Europe), and 65/65 across the four editions;
- `audit_sites.py`.

Not run: `run_checks.py` (it runs in CI) and the two-PC test of round 6.

## Confirmed in play (live)

**Campaign flow**
- Lobby start with `!campaign full` and `!campaign <realm>`.
- `!next`, `!previous` and `!campaign` during play.
- `!changelevel` to any world.
- `!difficulty`, `!reset`.
- Exits: host-only, and only to the next world in campaign order. Script exits in the R?M0 intros work.
- The R1M1A victory sequence is host-only.

**Loading**
- Synchronised loading behind the picture.
- The skip picture.
- The R?M0 picture is held until the intro starts.
- Smooth loading pictures.

**Players and progress**
- Inventory kept across exits, `!kill` and deaths: weapons, armour, class.
- Shared weapons, armour, pamphlets and secrets.
- Armour pieces respawn quietly.
- No friendly fire.
- World restart when everyone is dead.
- `!class`.
- Cheat keys per player.

**World**
- Monster spawners.
- Enemy health bars.
- Window, crate and barrel breaks, without crashes.
- Narration stops on a skip, and voice lines stop at map changes.
- The R2M2A Ballbuster clears the garage door.
- Boss music is kept on death.

**Client**
- Scoreboard while the key is held.
- Every chat feature (clipboard, 200 characters, scrollback, history, hidden while loading, usable in cutscenes).
- Single-player chat and cheat lines.
- Menu disconnect.
- Restoring the window in a world.

## Next playtest (round 6 first)

Use the launcher with **Co-op campaign** and **Test logging**. Afterwards, collect:
- `KPCServerTools.log` from the server;
- `KPCFix.log` from the desktop and from the laptop.

Record PASS, FAIL or not run for each ID.

| ID | Test | Expected |
|---|---|---|
| RG1 | Alt+Tab and minimise in movies, menus, a loading picture and a world | The picture is back at once. If a movie or menu stays black, try `KeepRendererEverywhere=0`. |
| RG2 | R?M0 loading picture | Madam Raven's narration starts as the picture closes. |
| RG3 | `!campaign earth`, then `!campaign fire` during R2M0's intro | R4M0 at normal zoom (cutscene FOV reset). |
| RG4 | `!reset`, and `!changelevel PM03` | The song stops. |
| RG5 | Chat | Arial lowercase text, with the selection in yellow. |
| RF11 | `!goto` next to walls | Never ends up inside a wall. |
| RB5 | Cutscene skip by a non-host | Refused; only the host can skip. |
| RA3 | Fire some ammo, then exit, `!kill` and `!next` | The ammo count is kept exactly. |
| RA11 | R1M2A belt lift | Waits on the RT11 decision below. |

Not yet confirmed:
- `!coop on` and `!coop off` (emu; the smoke test can't type chat);
- key and ticket announcements;
- intros of most worlds after realm 1.

## Decisions waiting for the user

- **RT11: the R1M2A belt lift.** Option 4 is researched and feasible: drive the `liftpiece1` and `liftpiece2` `CDoorSliding` objects from the server (`0x2c420`, state `+0x28c`, open `0x2cce0`, close `0x2ccf0`).
- **Spawn spread hardening.** The checks don't cover doors, world models, volumes or live actors.
- **Monster ammo drops vanish too early.** This is a stock bug: the lifetime is about 2 × server time + 5 s (`object.lto` `70ec9`–`70eea`).
- **Live taskbar preview** (DWM iconic bitmaps).
- **A real "Disconnect" menu label.** It needs a self-drawn menu item.
- **R1M1A from the launcher's Deathmatch mode.** It runs as stock multiplayer, with no intro or lever.

## Open bugs and work

**Bugs**
- Weapons dropped at death sometimes stay even with a lifetime set. Reported by the user; static analysis doesn't explain it.
- Switches and levers are not host-only yet (`CSwitch` `0x44dd6`).
- The collectables HUD shows the deathmatch list in co-op (cshell `0xa41c8`).
- A breakable is sometimes missing on one client until a minimise and restore.
- A spawn trapped in the ground at R1M3A.
- A held spawn reply is still sent after disconnecting to the menu.
- First armour shard: everyone watches its cutscene and gets the slot-1 weapon and armour (N4).
- Wooden-box debris (N15).
- A dropped client still shows as connected (N18).
- Original-game jukebox bug in R1M1A, R2M1A and R4M1A. Fix idea: on `jukeoff`, also `script_object_abort musicfader` and restore the intensity.

**Work**
- Equal campaign pickups, with death restoring them exactly (N5).
- An ammo-share switch, `!ammoshare` (N6).
- `!op` and `!deop` (N7).
- `!reload` (N17).
- Measure the cost of the per-frame player-list walk before caching it. A cache risks stale handles.
- Map the USA `server.dll` for the server-frame and sound clocks.
- The Audio checkbox for the German and Italian shells.

**Port to `master` when ready:** `ChatBlocksActions`, `KeepAudioOnFocusLoss` and `ResyncAudioOnFocus`.

## If a test fails

1. Find the matching `[KPCFix Co-op]` lines in `KPCServerTools.log`: they include the epoch, the phase and the event. Then check the client's `KPCFix.log` around the same time.
2. Confirm the cause in the log and the disassembly before changing code. A plausible explanation can be wrong.
3. If the logs don't show the cause, add logging for the next round instead of guessing.
