# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

KPCFix: unofficial fixes for **KISS Psycho Circus: The Nightmare Child 1.13** (LithTech
1.x, 2000). Two independent products, each a proxy DLL that patches the original game
**in memory only**:

| Product | DLL | Patches | Source |
| --- | --- | --- | --- |
| Client fix | `dinput.dll` (repo root) | `client.exe`, `cshell.dll` (`703813a4`) | `source/` (`src/*.c`, `*.S`, `profiles.h` per edition) |
| Server Tools | `server-tools/KPCServerTools.dll`, installed as `winmm.dll` (earlier installs: as `server.dll`, engine kept as `server.original.dll`) | `object.lto` (`3a03f879`), `server.dll` (unchanged; loaded by the DLL) | `server-tools/server-tools-source/` (`src/proxy.c`, `logging.S`, `src/coop_*.h`) |

Read [ARCHITECTURE.md](ARCHITECTURE.md) for the product design and
[coop/COOP-MVP.md](coop/COOP-MVP.md) for the co-op campaign.

## Current work

* Branch **`coop/mvp-lobby`**: experimental co-op campaign (chat lobby, WORLDORDER
  campaign, shared progress, per-player cheats, persistence). `main` holds the stable
  release only; do not merge co-op work into it unless asked.
* **Start every session with [coop/NEXT-SESSION-HANDOFF.md](coop/NEXT-SESSION-HANDOFF.md)**:
  what is installed, the two-PC test plan and its results, the triage table of
  playtest reports, open items. Finished session prompts are in `coop/archive/`; [coop/README.md](coop/README.md) maps every co-op document.
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

The wrapper scripts live **outside the repo** and have the source checkout hardcoded
as `C:\Users\magi\Documents\GitHub\KPCFix`. Before the first run, check that path
against the checkout you are in (`git rev-parse --show-toplevel`); if it differs (a
worktree, a second clone), use a copy of the script pointed at your checkout, or the
script builds and tests the wrong tree. The scripts without `-coop` in the name
(`build.sh`, `unit.sh`, `suite.sh`, `sums.py`) belong to an older `main` checkout,
`C:\Projects\KPCFix-audit`, which is gone; do not use them as they are.

| Task | Command |
| --- | --- |
| Build what changed (client and/or server, skips unchanged ones) | `tools\build-changed.cmd` or `sh tools/build-changed.sh` (options: `--client`, `--server`, `--force`, `--no-package`, `--no-install`, `--dry-run-install`; see the script header) |
| Server build (disposable copy, pinned GCC 14.2 / binutils 2.44 image `kpc-gcc14:local`) | `C:\KPCTest\pass2\build-coop.sh <dir>` → `C:\KPCTest\pass2\<dir>\server-tools\` |
| Server unit tests (all `tests/test_coop_*.c`, 64 and 32 bit, `-Werror`) | `C:\KPCTest\pass2\unit-coop.sh` |
| Server emulator matrix (`tests/check_proxy.py` on the real `object.lto` fixture, one `run …` line per option set) | `C:\KPCTest\pass2\matrix-coop.sh <dir>` (takes ~10 min; run in the background) |
| Server live checks | `check_live.py <object.lto> <dedicated.exe> [--disabled]` with `C:\KPCTest\re-venv\Scripts\python.exe -I` |
| Native server smoke (real `dedicated.exe`, no clients) | `coop/tools/Invoke-CoopServerSmoke.ps1 -GameDir C:\KPCTest\smoke1\Psycho -Mode Coop\|Normal\|PassThrough -Report <json>` in its own console (`Start-Process -Wait`) |
| Client build | `C:\KPCTest\clientbuild\build-coop.sh <dir>`; copy `out\dinput.dll` to the root, `SYMBOLS.json` and `BUILD_RESULT.json` to `source\` |
| Client tests | `C:\KPCTest\clientbuild\suite-coop.sh`; `source/audit_sites.py` inside the `kpc-client-gcc13:local` container (needs `objdump`) |

Builds are byte-identical across rebuilds: a hash change means a source change.

`tools/build-changed.sh` is the preferred build entry point. It hashes `source/` plus
`KPCFix.ini` (client) and `server-tools/server-tools-source/` plus `legacy-object-patches.json`
and `SERVER_TOOLS_SUPPORTED_BUILDS.json` (server), rebuilds only the component whose inputs
changed, and copies outputs back to the same places as the manual steps above. Its state
lives in `.git/kpc-build-state/`. It then packages on every run:
`tools/refresh_manifests.py` rewrites the hashes in both manifests from the working tree,
`tools/package_release.py` writes the two barebones release ZIPs to `dist/` (only the files
in its `CLIENT_FILES` / `SERVER_FILES`: DLL, INIs, scripts and their data, README, licences;
no source, tests or developer docs), and `dist/client/` gets
the three files a test PC needs (`dinput.dll`, `KPCFix.ini`, `DDrawCompat-CLIENT.ini`). The
client ZIP needs the pinned `ddraw.dll` (`--ddraw`, `KPC_DDRAW`, or `C:\Games\Psycho`).
A new shipped file must be added to that list, tracked, and given a manifest line, or
packaging stops with "Manifest/tree mismatch". `dist/` is ignored by git.

Last it updates the desktop test install with `tools/install_game.py` (game folder
`C:\Games\Psycho`, or `--game-dir` / `KPC_GAME_DIR`; `--no-install` skips it,
`--dry-run-install` only lists the changes). It copies only files that differ, backs up
what it changes to `C:\KPCTest\backup\`, replaces the active `server.dll` only when that is
already a Server Tools proxy, and adds missing keys to `KPCFix.ini` and
`DDrawCompat-CLIENT.ini` without changing existing lines. It never touches
`KPCServerTools.ini`, `server.original.dll` or original game files, and refuses while
`client.exe` or `dedicated.exe` is running. After an install, record the hashes and the
backup folder in the handoff ("Where things stand").

### What the wrappers run (for a single test, or without the wrappers)

| Task | Command |
| --- | --- |
| One server unit test (no game files, any GCC) | `gcc -std=c11 -Wall -Wextra -Werror -I server-tools/server-tools-source/src server-tools/server-tools-source/tests/test_coop_session.c -o /tmp/t && /tmp/t` (repeat with `-m32`) |
| One emulator option set | `python.exe -I <built copy>\server-tools\server-tools-source\tests\check_proxy.py C:\KPCTest\fixtures\Europe --coop-session --campaign` (flags are plain `sys.argv` tests; it checks the DLL next to it, so run it from the built copy) |
| Server assembly harness (synthetic fixtures; Unicorn 2.1.4 + binutils) | `python server-tools-source/tests/run_checks.py` and again with `--usa`, from `server-tools/` |
| Server build | `python3 server-tools-source/build_dll.py` from `server-tools/`. It **writes the DLL and manifest in place**, hence the disposable copy |
| Client build | `python3 source/build.py --build-dir <new dir> --output <new dir>/dinput.dll` (GCC 13.3.0 / binutils 2.42; the output must be a new file) |
| One client test | `python.exe source/test_focus.py --fixtures C:\KPCTest\clientfix\Europe --symbols source/SYMBOLS.json --dependency-dir C:/KPCTest/re-venv/Lib/site-packages --output <json>`. It tests the root `dinput.dll` with `source/SYMBOLS.json`, so copy a new build in first. Some tests need extra flags; see `suite-coop.sh` |
| Asset-free co-op tool tests | `python -m unittest discover -s coop/tools -p "test_*.py" -v` (one file: `-p test_decode_spawn_log.py`; `coop/tools` is not a package, so dotted module names do not work) |
| Packaging tests / package check | `python -m unittest discover -s tools -p "test_*.py" -v`; `python tools/package_release.py --component server --output-dir <dir>` (fails when a tracked file disagrees with `SHA256SUMS-Server-Tools`) |

### CI (what a push to `coop/**` checks)

`.github/workflows/coop-spawn-trace-build.yml` runs on this branch;
`release-integrity.yml` runs on `main` and release tags only.

* The tracked `KPCServerTools.dll` and `SERVER_TOOLS_BUILD.json` must reproduce
  byte-for-byte from the committed source in `debian:trixie-slim` (GCC 14.2.0,
  binutils 2.44). Never commit source without the matching rebuilt DLL and manifest.
* `git add --renormalize .` must change nothing (`.gitattributes`: LF everywhere, a few
  files `-text`).
* `sha256.c`, `sha256.h` and `winmini.h` must be byte-identical in `source/src/` and
  `server-tools/server-tools-source/src/`. Edit both copies.
* The workflow asserts literal strings in the DLL (build banner
  `build=coop-session-v2 proto=kpc-coop/1`, many `effective <Key>=…` lines, trace
  markers) and six fixed entries in `previous_proxy_hashes`. Renaming a log line or a
  setting means updating the workflow in the same commit.
* Each `tests/test_coop_*.c` is compiled by its own step; a new unit test needs a new step.
* `main` works the other way round: `release-integrity.yml` fails if `[KPCFix Co-op]` or
  `TraceCampaign` appears in the server source or DLL. This is why co-op stays off `main`.
* Every tracked `.ps1` must parse under Windows PowerShell 5.1.

After a server build: copy `KPCServerTools.dll` and `SERVER_TOOLS_BUILD.json` into
`server-tools/`, stage, then recompute `server-tools/SHA256SUMS-Server-Tools` from the
**git index** (`git show :path`, LF content; `core.autocrlf=true` makes working copies
CRLF). The manifests list shipped files only (not source or tests). The root `SHA256SUMS`
covers the client package only.

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
* **Logging**: `coop_log(event, detail)` → `KPCServerTools.log`. The budget is 1000 lines per map epoch (then one `log-budget` line), so still cap any repeating line per epoch yourself;
  chat replies via `CoopResult` + `coop_execute` (broadcast; there is no unicast chat).
* **Emulator test** (`tests/check_proxy.py`): add new INI keys to `profile_int`, new
  sites to `sites` and `hooked_optional` in `coop_scenario`, and a behaviour check with
  recording stubs (`stubs[obj+RVA]`). Fake players have no vtable and read as dead. A
  default `ee7d0` stub exists for per-frame code. Keep fake memory regions from
  overlapping. Add new option flags as runs in `matrix-coop.sh`.

## Client conventions (`source/`)

* `src/profiles.h` holds the exact 1.13 fingerprints and three `cshell.dll` layouts
  (Europe+USA, Germany, Italy). A new shell hook needs its site, original bytes and
  stack layout for **each** layout; the Italian build allocates its stack differently.
* `src/kpcfix.c` validates the files, picks the layout and stages the edits; client and
  shell installs are separate stages, and the shell may load before or after the proxy
  (the `LoadLibraryA` hook covers the second case). Keep both paths working.
* A feature is usually a `.c` file plus a `*_hook.S` bridge that preserves the game's
  calling convention, and a `source/test_<feature>.py` emulator test (Unicorn, real
  fixtures). Add the test as a `run` line in `suite-coop.sh`.
* `source/SYMBOLS.json` and `source/BUILD_RESULT.json` are build outputs that are
  tracked and must match the root `dinput.dll`.

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
