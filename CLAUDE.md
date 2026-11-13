# CLAUDE.md

KPCFix: unofficial fixes for **KISS Psycho Circus: The Nightmare Child 1.13** (LithTech
1.x, 2000). Two independent products, each a proxy DLL that patches the original game
**in memory only**:

| Product | DLL | Patches | Source |
| --- | --- | --- | --- |
| Client fix | `dinput.dll` (repo root) | `client.exe`, `cshell.dll` (`703813a4`) | `source/` (`src/*.c`, `*.S`, `profiles.h` per edition) |
| Server Tools | `server-tools/KPCServerTools.dll`, installed as `server.dll` | `object.lto` (`3a03f879`), `server.dll` (original kept as `server.original.dll`) | `server-tools/server-tools-source/` (`src/proxy.c`, `logging.S`, `src/coop_*.h`) |

Read [ARCHITECTURE.md](ARCHITECTURE.md) for the product design and
[coop/COOP-MVP.md](coop/COOP-MVP.md) for the co-op campaign.

## Current work

* Branches: **`coop/mvp-lobby-untested`** takes new co-op fixes that have not been
  playtested yet; **`coop/mvp-lobby`** holds co-op work confirmed in play (merge untested
  work into it only after the user confirms it works in game); **`main`** holds the stable
  release only (do not merge co-op work into it unless asked). The experimental co-op
  campaign covers the chat lobby, WORLDORDER campaign, shared progress, per-player cheats
  and persistence.
* **Merging `main` into a co-op branch:** `main` has its own stable copies of shared files
  (client and Server Tools source, DLLs, manifests, INIs). Keep the co-op version of code,
  binaries and manifests, take documentation from `main`, and check the merge deletes no
  co-op files before committing.
* **Start every session with [coop/NEXT-SESSION-HANDOFF.md](coop/NEXT-SESSION-HANDOFF.md)**:
  what is installed, the two-PC test plan and its results, the triage table of
  playtest reports, open items. Session prompts live in `coop/NEXT-PROMPTS-*.md`.
* Settings: [coop/CONFIGURATION.md](coop/CONFIGURATION.md) (every `[Coop]` key, with the
  evidence behind it) and `server-tools/KPCServerTools-default.ini`.

## Hard rules

* Never ship, edit or commit original game files (`object.lto`, `server.dll`,
  `client.exe`, `cshell.dll`, `.rez`, worlds, sounds, images). Original binaries for RE
  stay in `C:\KPCTest\re` and `C:\KPCTest\fixtures`.
* Every patch is in memory and checked against the expected original bytes before it
  is written; a mismatch refuses the install (fail closed). Never write game files on disk.
* Every change gets tests (see below). Report results faithfully; record DLL hashes.
* Commit on the working branch; **do not push** unless asked. Don't add branch build
  hashes to `previous_proxy_hashes` (reviewed releases only).
* Test installs are real game folders: back up before replacing anything
  ([coop/UPGRADE-TEST-INSTALL.md](coop/UPGRADE-TEST-INSTALL.md)), never touch the user's
  `KPCServerTools.ini` or `server.original.dll`.

## Build and test (Windows host, Docker Desktop running)

Scripts live **outside the repo** and copy from `C:\Projects\KPCFix-coop` (the main
checkout). When working in another worktree, use a copy pointed at that worktree.

| Task | Command |
| --- | --- |
| Server build (disposable copy, pinned GCC 14.2 / binutils 2.44 image `kpc-gcc14:local`) | `C:\KPCTest\pass2\build-coop.sh <dir>` → `C:\KPCTest\pass2\<dir>\server-tools\` |
| Server unit tests (all `tests/test_coop_*.c`, 64 and 32 bit, `-Werror`) | `C:\KPCTest\pass2\unit-coop.sh` |
| Server emulator matrix (`tests/check_proxy.py` on the real `object.lto` fixture, 48 option sets) | `C:\KPCTest\pass2\matrix-coop.sh <dir>` (takes ~10 min; run in the background) |
| Server live checks | `check_live.py <object.lto> <dedicated.exe> [--disabled]` with `C:\KPCTest\re-venv\Scripts\python.exe -I` |
| Native server smoke (real `dedicated.exe`, no clients) | `coop/tools/Invoke-CoopServerSmoke.ps1 -GameDir C:\KPCTest\smoke1\Psycho -Mode Coop\|Normal\|PassThrough -Report <json>` in its own console (`Start-Process -Wait`) |
| Client build | `C:\KPCTest\clientbuild\build-coop.sh <dir>`; copy `out\dinput.dll` to the root, `SYMBOLS.json` and `BUILD_RESULT.json` to `source\` |
| Client tests | `C:\KPCTest\clientbuild\suite-coop.sh`; `source/audit_sites.py` inside the `kpc-client-gcc13:local` container (needs `objdump`) |

Builds are byte-identical across rebuilds: a hash change means a source change.

After a server build: copy `KPCServerTools.dll` and `SERVER_TOOLS_BUILD.json` into
`server-tools/`, stage, then recompute `server-tools/SHA256SUMS-Server-Tools` from the
**git index** (`git show :path`, LF content; `core.autocrlf=true` makes working copies
CRLF). Add new source/test files to that list. The root `SHA256SUMS` covers the client
package only.

## Server Tools conventions (`proxy.c`)

* **Hook sites**: entries of the `OptionalSite` table in `optional_sites()` (RVA, size,
  original bytes; `make_call` writes `call rel32` + NOPs). Bump `OPTIONAL_SITES`, add
  `make_call(&s[n], …)` and an `enabled` rule. Prefer replacing an existing
  `call rel32` or 6-byte `call [reg+disp]`/`mov reg,[shell+0x1d0]`; never sites with
  relocated absolute addresses. Naked/asm entries call C by its plain name (ELF objects,
  no leading underscore).
* **Settings**: `[Coop]` keys read in `initialize()` with `GetPrivateProfileIntW`; each
  gets an `effective <Key>=…` log line, a default-INI entry and a `CONFIGURATION.md` row.
  Session features are only active with `CoopSession=1`.
* **Policy vs engine**: pure, freestanding logic goes in `src/coop_*.h` (no engine
  pointers) with a `tests/test_coop_*.c` unit test; `proxy.c` reads engine memory with
  guarded reads (`coop_rd`, `bsp_mem_readable`, `coop_copy_str`) and calls original
  routines by RVA from `coop_image`.
* **Logging**: `coop_log(event, detail)` → `KPCServerTools.log`, bounded per world epoch;
  chat replies via `CoopResult` + `coop_execute` (broadcast; there is no unicast chat).
* **Emulator test** (`tests/check_proxy.py`): add new INI keys to `profile_int`, new
  sites to `sites` and `hooked_optional` in `coop_scenario`, and a behaviour check with
  recording stubs (`stubs[obj+RVA]`). Fake players have no vtable and read as dead. A
  default `ee7d0` stub exists for per-frame code. Keep fake memory regions from
  overlapping. Add new option flags as runs in `matrix-coop.sh`.

## Reverse engineering

* Tools: `C:\KPCTest\re-venv\Scripts\python.exe -I` (capstone, pefile). Keep helper
  scripts in the session scratchpad, **not** in `C:\KPCTest\tools` (a `dis.py` there
  shadows the stdlib `dis` module); always run Python with `-I`.
* Useful helpers to recreate: linear disassembly at an RVA with string annotations,
  regex grep over the disassembly in an RVA range, xref (call/jmp/imm32 to a target),
  string → code refs. Images load at base `0x10000000` (object.lto), `0x20000000` (cshell).
* World data: `C:\KPCTest\scratch-campaign\objects.json` (every world's objects and
  properties, e.g. spawners, triggers, items); scripts and notes in `C:\KPCTest\scratch-*`.
* Evidence first: before writing a fix, confirm the cause in the log
  (`C:\Games\Psycho\KPCServerTools.log`) and the disassembly; a prompt's suggested cause
  can be wrong (the R4M1A spawner report was not the host gate).

### object.lto facts used often (base `0x10000000`)

| What | Where |
| --- | --- |
| Server shell / game type | `[0x1016de60]`, `+0x1d0` game type (0 = single player), maplist `+0x210` (50 × `0x7f`), count `+0x1ae0` |
| `g_pServerDE` | `[0x1016de80]`: C function pointers at offsets (`+0x1d0` GetObjectPos, `+0x1e0` SetObjectPos, `+0x20` RemoveObject) and a vtable at `[de]` for messages (`+0xac` Start, `+0x8` byte, `+0x14` string, `+0xb4` End) |
| Player list | `[0x10157724]+4` → nodes `{HOBJECT, next}`; `ee7d0(&hobj)` → `CPlayer*` (0 if not a player); `f4370(client)` → player |
| CPlayer | `+8` HOBJECT, `+0x54` inventory, `+0x120` client, `+0x1e8` name, `+0x260` armour bits, `+0x264` class (0 Starbearer, 1 Beast King, 2 Celestial, 3 Demon), vtable `+0x50` damage, `+0x60` is dead |
| Items | ids table `0x141e40` (stride `0x20`): weapons 2–14, ammo `0x11`–`0x14`, shard `0x1e`, pamphlet `0x19`, key `0x24`, ticket `0x25`, armour `0x27`–`0x2c`; give `f3440(player,id,n,notify)`, has `f25f0`, count `dd190(inv,id)` |
| Scripts | play `c5340(name)`, playing `c55b0`, registry `[0x1015a0b8]`/`[0x1015a0b4]`; `player` command dispatcher `f6890` |
| Cheats | client message `0x3b` → `fb910`; flags `0x1016d8a0`, handlers `0x1016d98c` |

## Docs to keep in step with a change

`coop/CONFIGURATION.md` (settings), `server-tools/KPCServerTools-default.ini`,
`coop/COOP-MVP.md` (how it works), the handoff's triage table, test plan and
"Where things stand" (installed hashes, backups, laptop package). Write plain, short
sentences; state evidence (static analysis / emulator / native smoke / two-PC) for
every claim. When editing docs with Python, use raw strings for Windows paths (`\r`,
`\b` in `C:\KPCTest\backup` otherwise become control characters).

## Test machines

* Desktop `C:\Games\Psycho`: game client, host and dedicated server (`server.dll` = Server Tools).
* `C:\KPCTest\smoke1\Psycho`: disposable copy for the native smoke.
* Laptop: second player, client only; gets packages `C:\KPCTest\pkg-client-<dinput hash>\`.
* Setup refuses branch proxies as "Unrecognized active server.dll": after the backup,
  move `server.dll` into the backup folder and rerun `Setup-KPCServerTools.ps1`.
