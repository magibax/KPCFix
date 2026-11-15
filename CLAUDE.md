# CLAUDE.md

KPCFix: unofficial fixes for *KISS Psycho Circus: The Nightmare Child* **1.13** (LithTech 1.x, 2000). It is two products, each a 32-bit proxy DLL that patches the original game **in memory only**:

| Product | Ships as | Source | Patches |
|---|---|---|---|
| Client | `dinput.dll` (repo root) | `source/src/` | `client.exe`, `cshell.dll` |
| Server Tools | `server-tools/KPCServerTools.dll`, installed as `winmm.dll` | `server-tools/server-tools-source/` | `object.lto`, `server.dll` |

**This is a co-op branch.** It adds the experimental co-op campaign to Server Tools (`src/coop_*.h`, most of `proxy.c`) and co-op-era features to the client.

Start here:
- **[coop/STATUS.md](coop/STATUS.md)**: what is installed where, the next playtest, and open work.
- [coop/README.md](coop/README.md): how co-op works for players.
- [coop/CONFIGURATION.md](coop/CONFIGURATION.md): every setting.
- [coop/ENGINE-NOTES.md](coop/ENGINE-NOTES.md): addresses and ABIs.
- The general docs in `docs/` (architecture, development, releasing) also apply here.

## Branches

- **`master`**: the stable release. No co-op code. CI rejects co-op markers in its Server Tools.
- **`coop/mvp-lobby-untested`**: new co-op work goes here first.
- **`coop/mvp-lobby`**: co-op work the user has confirmed in a two-PC playtest. Merge into it only after that confirmation.

When merging `master` into a co-op branch:
- keep the co-op version of code, binaries and manifests;
- take documentation from `master`;
- check that the merge deletes no co-op file.

Commit on the working branch. **Never push unless asked.**

## Rules

- **Never ship, edit or commit original game files**: `object.lto`, `server.dll`, `dedicated.exe`, `client.exe`, `cshell.dll`, `.rez`, worlds, sounds, images.
  - Game install: `C:\Games\Psycho`.
  - Copies for reverse engineering and tests: `C:\KPCTest\fixtures`, `C:\KPCTest\clientfix`, `C:\KPCTest\re`, `C:\KPCTest\KPCFix-region-research`.
- **Every patch is applied in memory, after checking the original bytes.** A mismatch refuses the install (fail closed). Never write game files on disk.
- **Tracked DLLs must rebuild byte for byte from the committed source.** Commit source, DLL and build report together, and refresh the manifests (`python tools/refresh_manifests.py`).
- **Never put branch build hashes into `previous_proxy_hashes`.** That list is for reviewed releases only.
- **Every change gets a test.** Report results faithfully, including what was not run. Record DLL hashes in `coop/STATUS.md`.
- **Test installs are real game folders.** Back up before replacing anything (`tools/install_game.py` does). Don't edit the user's `KPCServerTools.ini` or `KPCFix.ini` unless asked, and then back up first.
- **Files are LF** (see `.gitattributes`). Python's `Path.write_text` writes CRLF on Windows, so write bytes instead.
- **Shared code:** `sha256.c`, `sha256.h` and `winmini.h` must stay byte-identical in `source/src/` and `server-tools/server-tools-source/src/`.

## Build and test (Windows host, Docker Desktop)

If Docker is stopped, start `C:\Program Files\Docker\Docker\Docker Desktop.exe` and wait until `docker info` works.

**`tools\build-changed.cmd`** is the normal entry point. It:
1. rebuilds whatever changed;
2. refreshes the manifests;
3. writes the ZIPs and the `dist/client/` laptop bundle to `dist/`;
4. installs into `C:\Games\Psycho` with backups to `C:\KPCTest\backup\`.

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md). Record the new hashes and the backup folder in `coop/STATUS.md` afterwards.

| Task | Command |
|---|---|
| Server build in a disposable copy | `sh C:/KPCTest/pass2/build-coop.sh <name>` → `C:\KPCTest\pass2\<name>\server-tools\` |
| Server unit tests (every `tests/test_coop_*.c`, 32 and 64 bit, `-Werror`) | `sh C:/KPCTest/pass2/unit-coop.sh` |
| Server emulator matrix | `sh C:/KPCTest/pass2/matrix-coop.sh <name>` (about 20 s) |
| One emulator run | `C:\KPCTest\re-venv\Scripts\python.exe -I <copy>\server-tools\server-tools-source\tests\check_proxy.py C:\KPCTest\fixtures\Europe --winmm --coop-session --campaign` (it checks the DLL next to it) |
| Native server smoke | `coop/tools/Invoke-CoopServerSmoke.ps1 -GameDir C:\KPCTest\smoke1\Psycho -Mode Coop\|Normal\|PassThrough -Report <json>`, in its own console (`Start-Process -Wait`). A garbled `start` line ("No game running") is a console-input glitch: rerun. |
| Client build | `sh C:/KPCTest/clientbuild/build-coop.sh <name>`, then copy `out\dinput.dll` to the root, and `SYMBOLS.json` and `BUILD_RESULT.json` to `source\` |
| Client suite (Europe) | `sh C:/KPCTest/clientbuild/suite-coop.sh` (tests the root `dinput.dll`) |
| Client suite, four editions | `sh C:/KPCTest/clientbuild/suite-stable.sh C:/KPCTest/scratch-coop-nov14/research` |
| Client site audit | In `kpc-client-gcc13:local`: `python3 source/audit_sites.py --fixtures /f/Europe --output /tmp/a.json`, with `C:\KPCTest\clientfix\Europe` mounted at `/f/Europe` |
| Asset-free co-op tool tests | `python -m unittest discover -s coop/tools -p "test_*.py"` |
| Packaging and PowerShell | `python -m unittest discover -s tools -p "test_*.py"`; `powershell -File tools/Test-PowerShellSyntax.ps1`; `powershell -File tools/Test-KPCServerIni.ps1` |

Notes:
- The wrappers live outside the repo and hardcode this checkout, `C:\Users\magi\Documents\GitHub\KPCFix`.
- All runners are parallel (`JOBS=<n>` limits them). The `*.serial.sh` copies run one case at a time.
- Ignore `build.sh`, `unit.sh`, `suite.sh` and `matrix.sh`: they belong to a deleted checkout.
- `build_dll.py` writes into the folder it runs in, so always build in a disposable copy.

**CI** (`.github/workflows/coop-build.yml`, on every push to `coop/**`):
- The DLL and `SERVER_TOOLS_BUILD.json` must rebuild byte for byte in `debian:trixie-slim`.
- `git add --renormalize .` must change nothing, and the shared sources must be identical.
- Each `tests/test_coop_*.c` has its own step, and `coop/tools` tests run.
- It greps the DLL for literal strings: `build=coop-session-v2 proto=kpc-coop/1`, `effective <Key>=…` lines and trace markers. **Renaming a log line or setting means updating the workflow in the same commit.**

## Server Tools conventions (`proxy.c`)

- **Hook sites** are `OptionalSite` entries in `optional_sites()` (RVA, size, original bytes; `make_call` writes `call rel32` plus NOPs).
  - To add one: bump `OPTIONAL_SITES`, add `make_call(&s[n], …)` and an `enabled` rule.
  - Prefer an existing `call rel32`, or a 6-byte `call [reg+disp]` / `mov reg,[shell+0x1d0]`. Never use a site with relocated absolute addresses.
  - Every session-only change must go through this table: `coop_switch_session` (`!coop on|off`, `!reset`) installs and restores exactly these sites at run time.
- **Settings:** `[Coop]` keys are read in `coop_read_config()`, at startup and on every live switch. Each key needs all of these:
  - an `effective <Key>=…` log line;
  - an entry in `KPCServerTools-default.ini`;
  - a row in `coop/CONFIGURATION.md`;
  - a case in `check_proxy.py`'s `profile_int` (unknown keys there return the `--cheats` flag).

  Session features also need `CoopSession=1`.
- **Policy vs engine:**
  - Freestanding logic goes in `src/coop_*.h`, with a `tests/test_coop_*.c` unit test.
  - `proxy.c` reads engine memory with guarded reads (`coop_rd`, `bsp_mem_readable`, `coop_copy_str`).
  - It calls original routines by RVA from `coop_image` (`campaign_image` is the object image, also outside a session).
- **Chat:** replies go through `CoopResult` + `coop_execute`. They are broadcast only; there is no unicast. Messages are capped at `COOP_MSG_MAX` (112) characters.
- **Logging:** use `coop_log(event, detail)`. The budget is 1000 lines per map epoch, so cap repeating lines.
- **Emulator (`tests/check_proxy.py`):**
  - New sites go in `sites` / `hooked_optional` in `coop_scenario`.
  - Behaviour checks use recording stubs (`stubs[obj+RVA]`).
  - Fake players have no vtable: `coop_player_dead` reads them as dead, `coop_player_dead_known` as alive.
  - New option flags become lines in `matrix-coop.sh`.
- **Loading:** Server Tools loads as `winmm.dll` and redirects `dedicated.exe`'s `LoadLibraryA("server.dll")`. Loaded as `server.dll` (an old install) it uses `server.original.dll`. Keep both paths working on this branch.

## Client conventions (`source/`)

- `src/profiles.h` holds the 1.13 fingerprints and three `cshell.dll` layouts: Europe+USA, Germany, Italy.
- A shell hook needs its site, original bytes and stack layout for each layout. An unverified layout leaves the field 0; the feature then logs "not verified" and stays stock. Most co-op-era client hooks are verified for Europe/USA only.
- A feature is:
  - a `.c` file;
  - a `*_hook.S` bridge, when the calling convention needs one;
  - a `source/test_<feature>.py` emulator test, listed as `<suite>|<arguments>` in `suite-coop.sh`.
- `SYMBOLS.json` and `BUILD_RESULT.json` are tracked build outputs.

## Reverse engineering

- Python: `C:\KPCTest\re-venv\Scripts\python.exe -I` (capstone, pefile, unicorn).
  - Keep helper scripts in a scratch folder (`C:\KPCTest\scratch-*`), never in `C:\KPCTest\tools`: a `dis.py` there shadows the standard library.
  - World data: `C:\KPCTest\scratch-campaign\objects.json`.
- Addresses and ABIs are in [coop/ENGINE-NOTES.md](coop/ENGINE-NOTES.md). Add new findings there.
- Evidence first: confirm a cause in `KPCServerTools.log` or `KPCFix.log` and in the disassembly before fixing. When evidence is missing, add logging that will show it in the next playtest.
- Unicorn test rigs:
  - Rewriting a call stub at the same address reruns the old translated code, so give each new call its own stub address.
  - Two code hooks on one stub address corrupt the stack.
- Bash heredocs and inline Python lose a level of backslashes. Use the Edit or Write tool for text with backslashes.

## Docs

Keep [coop/STATUS.md](coop/STATUS.md) current: builds, hashes, test results, next test plan and open work. It replaces dated handoff and results files; don't add new ones. Feature behaviour goes in `coop/README.md` and settings in `coop/CONFIGURATION.md`, each with its evidence level (static, emu, smoke, live). Write plain, short sentences.

## Test machines

- Desktop, `C:\Games\Psycho`: client, host and dedicated server (Server Tools as `winmm.dll`).
- `C:\KPCTest\smoke1\Psycho`: a disposable copy for the native smoke test.
- Laptop: second player, client only. It gets `dist\client\` or `C:\KPCTest\pkg-client-<dinput hash>\`.
