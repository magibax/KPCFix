# CLAUDE.md

KPCFix: unofficial fixes for *KISS Psycho Circus: The Nightmare Child* **1.13** (LithTech 1.x, 2000). It is two products, each a 32-bit proxy DLL that patches the original game **in memory only**:

| Product | Ships as | Source | Patches |
|---|---|---|---|
| Client | `dinput.dll` (repo root) | `source/src/` | `client.exe`, `cshell.dll` |
| Server Tools | `server-tools/KPCServerTools.dll`, installed as `winmm.dll` | `server-tools/server-tools-source/` | `object.lto`, `server.dll` |

Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) before changing code. Use [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for building and testing, and [docs/RELEASING.md](docs/RELEASING.md) for releases.

## Branches

- **`master`**: the stable release. No co-op code. CI rejects co-op markers in the Server Tools DLL and source.
- **`coop/mvp-lobby-untested`**: new co-op work goes here first.
- **`coop/mvp-lobby`**: co-op work the user has confirmed in a two-PC playtest. Merge into it only after that confirmation.

Stable fixes land on `master`, then merge into the co-op branches. Commit on the working branch. **Never push unless asked.**

## Rules

- **Never ship, edit or commit original game files**: `object.lto`, `server.dll`, `dedicated.exe`, `client.exe`, `cshell.dll`, `.rez`, worlds, sounds, images.
  - Game install: `C:\Games\Psycho`.
  - Copies for reverse engineering and tests: `C:\KPCTest\fixtures`, `C:\KPCTest\clientfix`, `C:\KPCTest\re`, `C:\KPCTest\KPCFix-region-research`.
- **Every patch is applied in memory, after checking the original bytes.** A mismatch refuses the install (fail closed). Never write game files on disk.
- **Tracked DLLs must rebuild byte for byte from the committed source.** Commit source, DLL and build report together (`dinput.dll` + `source/SYMBOLS.json` + `source/BUILD_RESULT.json`, or `KPCServerTools.dll` + `SERVER_TOOLS_BUILD.json`). Refresh the manifests when a shipped file changes (`python tools/refresh_manifests.py`).
- **Every change gets a test.** Report results faithfully, including what was not run, and record DLL hashes.
- **Test installs are real game folders.** `tools/install_game.py` backs up before changing anything. Don't edit the user's `KPCServerTools.ini` or `KPCFix.ini` unless asked, and then back up first.
  - Native server tests run in a disposable copy (`C:\KPCTest\smoke-master\Psycho`), never in `C:\Games\Psycho`.
  - `tools/build-changed.sh` installs into `C:\Games\Psycho` by default, which may hold a co-op build. On `master`, pass `--no-install` unless the user wants the stable build there.
- **Files are LF** (see `.gitattributes`). Python's `Path.write_text` writes CRLF on Windows, so write bytes instead. `git add --renormalize .` must change nothing.
- **Shared code:** `sha256.c`, `sha256.h` and `winmini.h` must stay byte-identical in `source/src/` and `server-tools/server-tools-source/src/`.

## Build and test (Windows host, Docker Desktop)

If Docker is stopped, start `C:\Program Files\Docker\Docker\Docker Desktop.exe` and wait until `docker info` works.

| Task | Command |
|---|---|
| Build what changed, package, install | `tools\build-changed.cmd [--client\|--server] [--no-install]` |
| Server build in a disposable copy | `sh C:/KPCTest/pass2/build-coop.sh <name>` → `C:\KPCTest\pass2\<name>\server-tools\` |
| Server emulator matrix | `sh C:/KPCTest/pass2/matrix-stable.sh <name> "C:/KPCTest/KPCFix-region-research/patched to 1.13"` (12 cases, Europe + USA) |
| Server live-hook test | `C:\KPCTest\re-venv\Scripts\python.exe -I <copy>\server-tools\server-tools-source\tests\check_live.py <object.lto> <dedicated.exe> [--disabled]` |
| Native server smoke | `Start-Process powershell.exe -Wait -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File tools\Invoke-ServerSmoke.ps1 -GameDir C:\KPCTest\smoke-master\Psycho -Mode Normal\|PassThrough -Report <json>'` |
| Client build | `sh C:/KPCTest/clientbuild/build-coop.sh <name>`, then copy `out\dinput.dll` to the root, and `SYMBOLS.json` and `BUILD_RESULT.json` to `source\` |
| Client suite, four editions | `sh C:/KPCTest/clientbuild/suite-stable.sh C:/KPCTest/scratch-coop-nov14/research` (tests the root `dinput.dll`) |
| Packaging tests | `python -m unittest discover -s tools -p "test_*.py"` |
| PowerShell | `powershell -File tools/Test-PowerShellSyntax.ps1` (tracked files only) and `tools/Test-KPCServerIni.ps1` |

Notes on the wrappers:
- The wrapper scripts in `C:\KPCTest\pass2` and `C:\KPCTest\clientbuild` live outside the repo and hardcode this checkout, `C:\Users\magi\Documents\GitHub\KPCFix`.
- The `*-coop.sh` build scripts work for any branch.
- Use `matrix-stable.sh` and `suite-stable.sh` on `master`; their `*-coop.sh` counterparts test co-op features.
- Ignore `build.sh`, `unit.sh`, `suite.sh` and `matrix.sh`: they belong to a deleted checkout.

## Code conventions

**Client (`source/`)**
- `src/profiles.h` holds the 1.13 fingerprints and three `cshell.dll` layouts: Europe+USA, Germany, Italy.
- A shell hook needs its site, original bytes and stack layout for each layout (Italy's stack frames differ). An unverified layout leaves the field 0; the feature then logs "not verified" and stays stock.
- A feature is a `.c` file, plus a `*_hook.S` bridge when the calling convention needs one, plus a `source/test_<feature>.py` emulator test. Add the test to `verify.py` and `suite-stable.sh`.

**Server Tools (`server-tools-source/`)**
- `src/proxy.c` does:
  - the `winmm` forwarding and the `dedicated.exe` `LoadLibraryA` redirect (`winmm_attach`, `server_load_a`);
  - the backend checks (`initialize`);
  - the object hooks (`apply_object`) and the server frame hook.
- `logging.S` holds the chat, console and cheat hooks, assembled into `object.lto`'s free space.
- `winmm_exports.txt` lists the forwarded `winmm` exports.
- New `[ServerTools]` keys need: a default-INI entry, a row in `docs/CONFIGURATION.md`, and a case in `tests/check_proxy.py`'s `profile_int`.

## Reverse engineering

- Python: `C:\KPCTest\re-venv\Scripts\python.exe -I` (capstone, pefile, unicorn). Keep helper scripts in a scratch folder (`C:\KPCTest\scratch-*`), never in `C:\KPCTest\tools`: a `dis.py` there shadows the standard library.
- Image bases: `object.lto` `0x10000000`, `cshell.dll` `0x20000000`, `client.exe` `0x1ca0000`. The Europe-family `server.dll` is `83b7bce8…`; the USA one is `93178237…`.
- Evidence first: confirm a cause in `KPCServerTools.log` or `KPCFix.log` and in the disassembly before fixing it. When evidence is missing, add logging that will show it in the next playtest.
- Bash heredocs and inline Python lose a level of backslashes. Use the Edit or Write tool for text with backslashes.

## Docs

User docs ship in the ZIPs: `README.md` and `server-tools/README-Server-Tools.md`. Developer docs are in `docs/`. Keep them in step with the code, in plain, short sentences. Don't add status logs, audit reports or session handoffs to `master`.
