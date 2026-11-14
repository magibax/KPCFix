# CLAUDE.md

Guidance for Claude Code (claude.ai/code) in this repository.

KPCFix: unofficial fixes for **KISS Psycho Circus: The Nightmare Child 1.13** (LithTech
1.x, 2000). Two independent products, each a proxy DLL that patches the original game
**in memory only**:

| Product | DLL | Patches | Source |
| --- | --- | --- | --- |
| Client fix | `dinput.dll` (repo root) | `client.exe`, `cshell.dll` (`703813a4`) | `source/` (`src/*.c`, `*.S`, `profiles.h` per edition) |
| Server Tools | `server-tools/KPCServerTools.dll`, installed as `winmm.dll` (earlier installs: as `server.dll` with the engine kept as `server.original.dll`) | `object.lto` (`3a03f879`); `server.dll` stays the original and is loaded by the DLL | `server-tools/server-tools-source/` (`src/proxy.c`, `logging.S`, `src/coop_*.h`) |

Design: [ARCHITECTURE.md](ARCHITECTURE.md). Co-op campaign: [coop/COOP-MVP.md](coop/COOP-MVP.md).
Every `[Coop]` setting with its evidence: [coop/CONFIGURATION.md](coop/CONFIGURATION.md).
Map of the co-op documents: [coop/README.md](coop/README.md).

## Start here

* **Read [coop/NEXT-SESSION-HANDOFF.md](coop/NEXT-SESSION-HANDOFF.md) first**: what is
  installed where, the next test plan, open work. Finished test rounds are in `coop/archive/`.
* Branches: **`coop/mvp-lobby-untested`** takes new co-op work before it is playtested;
  **`coop/mvp-lobby`** holds work the user confirmed in game (merge into it only after
  that confirmation); **`main`** is the stable release (no co-op work unless asked).
* Merging `main` into a co-op branch: keep the co-op version of code, binaries and
  manifests, take documentation from `main`, and check that the merge deletes no co-op file.

## Hard rules

* Never ship, edit or commit original game files (`object.lto`, `server.dll`,
  `client.exe`, `cshell.dll`, `.rez`, worlds, sounds, images). RE copies live in
  `C:\KPCTest\re` and `C:\KPCTest\fixtures`.
* Every patch is in memory and checked against the expected original bytes first; a
  mismatch refuses the install (fail closed). Never write game files on disk.
* Every change gets tests. Report results faithfully, including what was not run, and
  record DLL hashes.
* Commit on the working branch; **do not push** unless asked. Branch build hashes never go
  into `previous_proxy_hashes` (reviewed releases only).
* Test installs are real game folders: back up before replacing anything
  ([coop/UPGRADE-TEST-INSTALL.md](coop/UPGRADE-TEST-INSTALL.md)). Do not edit the user's
  `KPCServerTools.ini` or `server.original.dll` unless the user asks; then back up first.

## Build and test (Windows host, Docker Desktop running)

Builds are byte-identical across rebuilds: a hash change means a source change. Docker
Desktop may be stopped; start `C:\Program Files\Docker\Docker\Docker Desktop.exe` and wait
for `docker info`.

**`tools/build-changed.sh`** (or `tools\build-changed.cmd`) is the normal entry point. It
hashes the inputs (`source/` + `KPCFix.ini`; `server-tools/server-tools-source/` +
`legacy-object-patches.json` + `SERVER_TOOLS_SUPPORTED_BUILDS.json`; state in
`.git/kpc-build-state/`), rebuilds only what changed (client and server at once, each
compiling its sources in parallel) in disposable copies under `build/`, copies the outputs
back (`dinput.dll`; `source/SYMBOLS.json`, `source/BUILD_RESULT.json`;
`server-tools/KPCServerTools.dll`, `SERVER_TOOLS_BUILD.json`), then:
1. packages: `tools/refresh_manifests.py` rewrites both manifests, `tools/package_release.py`
   writes the two release ZIPs to `dist/` (only `CLIENT_FILES` / `SERVER_FILES`; a new
   shipped file must be tracked, listed there and given a manifest line, or packaging stops
   with "Manifest/tree mismatch"; the client ZIP needs the pinned `ddraw.dll`), and
   `dist/client/` gets the three laptop files;
2. installs into `C:\Games\Psycho` with `tools/install_game.py` (`--game-dir`,
   `--no-install`, `--dry-run-install`): copies only changed files, backs up to
   `C:\KPCTest\backup\`, adds missing keys to `KPCFix.ini` / `DDrawCompat-CLIENT.ini`
   without changing existing lines, never touches `KPCServerTools.ini` or original files,
   refuses while `client.exe` / `dedicated.exe` run, and does not move an old `server.dll`
   install to `winmm.dll` (run `Setup-KPCServerTools.ps1` in the game folder for that).
   Record the hashes and the backup folder in the handoff afterwards.

The wrapper scripts live **outside the repo** with the checkout hardcoded as
`C:\Users\magi\Documents\GitHub\KPCFix`; from a worktree or second clone use a copy that
points at it. Scripts without `-coop`/`-stable` in the name (`build.sh`, `unit.sh`,
`suite.sh`, `matrix.sh`) belong to a deleted checkout: do not use them. All runners are
parallel (one job per CPU; `JOBS=<n>` limits them, `verify.py --jobs`); the `*.serial.sh`
copies run one case at a time.

| Task | Command |
| --- | --- |
| Server build (disposable copy, image `kpc-gcc14:local`, GCC 14.2 / binutils 2.44) | `C:\KPCTest\pass2\build-coop.sh <dir>` → `C:\KPCTest\pass2\<dir>\server-tools\` |
| Server unit tests (every `tests/test_coop_*.c`, 64 and 32 bit, `-Werror`) | `C:\KPCTest\pass2\unit-coop.sh` |
| Server emulator matrix (`check_proxy.py` on the real `object.lto` fixture) | `C:\KPCTest\pass2\matrix-coop.sh <dir>` (about 20 s); stable: `matrix-stable.sh <dir> <fixture root>` |
| Native server smoke (real `dedicated.exe`, no clients) | `coop/tools/Invoke-CoopServerSmoke.ps1 -GameDir C:\KPCTest\smoke1\Psycho -Mode Coop\|Normal\|PassThrough -Report <json>`, in its own console (`Start-Process -Wait`). Coop uses the fixed test route (`-CoopMode Route`). A garbled `start` line in the report's console ("No game running") is a console-input glitch: rerun |
| Client build (image `kpc-client-gcc13:local`, GCC 13.3 / binutils 2.42) | `C:\KPCTest\clientbuild\build-coop.sh <dir>`; copy `out\dinput.dll` to the root and `SYMBOLS.json`, `BUILD_RESULT.json` to `source\` |
| Client suite (Europe) | `C:\KPCTest\clientbuild\suite-coop.sh` (tests the root `dinput.dll`) |
| Client suite, four editions | `C:\KPCTest\clientbuild\suite-stable.sh <root>`; `<root>` needs `patched\<Region>` and `unpatched\<Region>`: `C:\KPCTest\scratch-coop-nov14\research` has junctions to `C:\KPCTest\KPCFix-region-research` (whose folder is named `patched to 1.13`) |
| Client site audit | in `kpc-client-gcc13:local`: `python3 source/audit_sites.py --fixtures /f/Europe --output /tmp/a.json` with `C:\KPCTest\clientfix\Europe` mounted at `/f/Europe` (the folder name must be the region) |
| Packaging tests | `python -m unittest discover -s tools -p "test_*.py"`; `python tools/package_release.py --component server --output-dir <dir>` |
| Launcher INI test | `powershell -File tools/Test-KPCServerIni.ps1` |
| Asset-free co-op tool tests | `python -m unittest discover -s coop/tools -p "test_*.py"` |

Single steps without the wrappers: one unit test is
`gcc -std=c11 -Wall -Wextra -Werror -I server-tools/server-tools-source/src server-tools/server-tools-source/tests/test_coop_session.c -o /tmp/t && /tmp/t`
(again with `-m32`); one emulator run is `python.exe -I <built copy>\server-tools\server-tools-source\tests\check_proxy.py C:\KPCTest\fixtures\Europe --coop-session --campaign`
(it checks the DLL next to it); one client test is `python.exe source/test_focus.py --fixtures C:\KPCTest\clientfix\Europe --symbols source/SYMBOLS.json --dependency-dir C:/KPCTest/re-venv/Lib/site-packages --output <json>`
(extra flags per test: see `suite-coop.sh`). `server-tools-source/tests/run_checks.py`
(assembly harness for `logging.S`) needs Unicorn and binutils in one Python; the Windows
venv has no binutils and the Linux image no Unicorn, so it runs only where both exist.
`build_dll.py` writes the DLL and manifest in place: always build in a disposable copy.

### CI (a push to `coop/**`)

`coop-spawn-trace-build.yml` runs on co-op branches; `release-integrity.yml` on `main`
and release tags.
* The tracked `KPCServerTools.dll` and `SERVER_TOOLS_BUILD.json` must rebuild byte for
  byte from the committed source (`debian:trixie-slim`). Never commit source without the
  matching DLL and manifest.
* `git add --renormalize .` must change nothing (LF everywhere; a few files `-text`).
* `sha256.c`, `sha256.h`, `winmini.h` must be identical in `source/src/` and
  `server-tools/server-tools-source/src/`.
* The workflow greps literal strings in the DLL (`build=coop-session-v2
  proto=kpc-coop/1`, `effective <Key>=…` lines, trace markers) and six fixed
  `previous_proxy_hashes`; renaming a log line or setting means updating it in the same
  commit. Each `tests/test_coop_*.c` has its own step. Every `.ps1` must parse in
  Windows PowerShell 5.1.
* On `main`, `release-integrity.yml` fails if `[KPCFix Co-op]` or `TraceCampaign` appears
  in the server source or DLL: co-op stays off `main`.

## Server Tools conventions (`proxy.c`)

* **Hook sites** are `OptionalSite` entries in `optional_sites()` (RVA, size, original
  bytes; `make_call` writes `call rel32` + NOPs): bump `OPTIONAL_SITES`, add
  `make_call(&s[n], …)` and an `enabled` rule. Prefer an existing `call rel32` or a
  6-byte `call [reg+disp]` / `mov reg,[shell+0x1d0]`; never sites with relocated
  absolute addresses. Naked entries call C by its plain name. Every session-only change
  must go through this table: `coop_switch_session` (chat `!coop on|off`, `!reset`)
  installs and restores exactly these sites at run time.
* **Settings**: `[Coop]` keys are read in `coop_read_config()` (startup and every live
  switch); each needs an `effective <Key>=…` log line, a default-INI entry, a
  `coop/CONFIGURATION.md` row and an entry in `check_proxy.py`'s `profile_int` (unknown
  keys there return the `--cheats` flag). Session features need `CoopSession=1`.
* **Policy vs engine**: freestanding logic in `src/coop_*.h` with a `tests/test_coop_*.c`
  unit test; `proxy.c` reads engine memory with guarded reads (`coop_rd`,
  `bsp_mem_readable`, `coop_copy_str`) and calls original routines by RVA from
  `coop_image` (`campaign_image` is the object image also outside a session).
* **Chat**: replies go through `CoopResult` + `coop_execute` (broadcast only; there is no
  unicast). Messages are capped at `COOP_MSG_MAX` (112) characters.
* **Logging**: `coop_log(event, detail)`; 1000 lines per map epoch, so cap repeating lines.
* **Emulator** (`tests/check_proxy.py`): new sites go in `sites` / `hooked_optional` in
  `coop_scenario`, behaviour checks use recording stubs (`stubs[obj+RVA]`); fake players
  have no vtable (`coop_player_dead` reads them as dead, `coop_player_dead_known` as
  alive). New option flags become lines in `matrix-coop.sh`.

## Client conventions (`source/`)

* `src/profiles.h` holds the 1.13 fingerprints and three `cshell.dll` layouts
  (Europe+USA, Germany, Italy). A shell hook needs its site, original bytes and stack
  layout per layout (Italy allocates its stack differently); unverified layouts leave the
  field 0 and the feature logs "not verified" and stays stock.
* `src/kpcfix.c` validates files, picks the layout and stages edits in transactions; the
  shell may load before or after the proxy (the `LoadLibraryA` hook covers the latter).
* A feature is a `.c` file (plus a `*_hook.S` bridge when the calling convention needs
  it) and a `source/test_<feature>.py` emulator test, listed as `<suite>|<arguments>` in
  `suite-coop.sh`. `SYMBOLS.json` and `BUILD_RESULT.json` are tracked build outputs.

## Reverse engineering

* `C:\KPCTest\re-venv\Scripts\python.exe -I` (capstone, pefile). Keep helper scripts in a
  scratch folder (e.g. `C:\KPCTest\scratch-coop-nov14\dis.py <file> <base> <rva:count>…`),
  never in `C:\KPCTest\tools` (a `dis.py` there shadows the stdlib). Bases: object.lto
  `0x10000000`, cshell `0x20000000`, client.exe `0x1ca0000`.
* World data: `C:\KPCTest\scratch-campaign\objects.json`. Research agents may write
  listings to `C:\KPCTest\scratch-*`.
* Evidence first: confirm a cause in `KPCServerTools.log` / `KPCFix.log` and the
  disassembly before fixing; a suggested cause can be wrong. When evidence is missing,
  add logging that will show it in the next playtest.

### object.lto (base `0x10000000`)

| What | Where |
| --- | --- |
| Server shell | `[0x1016de60]`: `+0x1d0` game type (0 = single player), maplist `+0x210` (50 × `0x7f`), count `+0x1ae0`, pending NextLevel `+0x1ae9`, intermission `+0x1ae8`; maplist cursor `0x1016da88` |
| `g_pServerDE` | `[0x1016de80]`: function pointers (`+0x1d0` GetObjectPos, `+0x1e0` SetObjectPos, `+0x20` RemoveObject, `+0xf4` GetClientUserData) and a message vtable at `[de]` (`+0xac` Start, `+0x8` byte, `+0x14` string, `+0xb4` End) |
| Players | list `[0x10157724]+4` → `{HOBJECT, next}`; `ee7d0(&hobj)` → `CPlayer*`; CPlayer `+8` HOBJECT, `+0x54` inventory, `+0x120` client, `+0x198` state (1 alive, 3 dying, 2 dead; SetState `f0670`), `+0x1e8` name, `+0x260` armour bits, `+0x264` class; vtable `0x1011efb0` (`+0x50` damage `f3bc0`, `+0x60` is dead) |
| Death and respawn | damage handler drops the weapon at `f3f16` → DropWeapon `f3660` (ids 7–14); multiplayer respawn on fire/jump in `ef960` → `f15c0` (`ef9dc`) |
| Items | ids table `0x141e40` (stride `0x20`); give `f3440(player,id,n,notify)`, has `f25f0`, count `dd190`; item update `cbf10`, hide/remove `cc570(item, force)`, respawn `cc080`, lifetime timer `+0x9c` (`cc780`, death drops only), monster-drop expiry `+0xe4` |
| Scripts | play `c5340(name)`, playing `c55b0`, loaded registry `[0x1015a0b8]`/`[0x1015a0b4]`, running list `[0x1015a0a8]`; MID_CINEMATIC `0x3d` → `c5830` at `feb3e` (`0x80` continue, `0x81` skip → `c54f0`) |
| Messages | `CServerShell::OnMessage` `fe3a0` (sender at `[esp+0x1d4]` inside); cheats `0x3b` → `fb910` |

### cshell (base `0x20000000`, Europe/USA)

| What | Where |
| --- | --- |
| Client shell | `[0x201c8de8]`; state `+0x1398` (1 in world, 5 changing level, 7 intermission); screen manager `+0x138c` (`+4` type: 0 loading art, 2 server scoreboard, 4 key scoreboard; show `ad1b0(type)`, close `ad3c0`) |
| Commands | vtable `0xff720`: OnCommandOn `+4` (`0xff724` → `c9d80`), OnCommandOff `+8` (`0xff728` → `cab60`, empty); Scoreboard action 41 (`ca64c`) |

## Docs to keep in step

`coop/CONFIGURATION.md`, `server-tools/KPCServerTools-default.ini`, `coop/COOP-MVP.md`,
`server-tools/README-Server-Tools.md`, `KPCFix.ini` comments, and the handoff (test plan,
open work, "Where things stand"). Plain, short sentences; name the evidence (static
analysis, emulator, native smoke, two-PC playtest) for every claim.

Editing gotcha: shell commands lose one level of backslashes in heredocs and inline
Python, so `\\` becomes `\` and `"\r\n"` breaks. Use the Edit tool for text with
backslashes, or build them with `chr(92)` in a script.

## Test machines

* Desktop `C:\Games\Psycho`: client, host and dedicated server (Server Tools as `winmm.dll`).
* `C:\KPCTest\smoke1\Psycho`: disposable copy for the native smoke.
* Laptop: second player, client only; gets `C:\KPCTest\pkg-client-<dinput hash>\` or `dist\client\`.
