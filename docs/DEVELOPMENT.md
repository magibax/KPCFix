# Development

How to build, test and install KPCFix from source. For how the code works, see [ARCHITECTURE.md](ARCHITECTURE.md). For releases, see [RELEASING.md](RELEASING.md).

## Repository layout

| Path | What |
|---|---|
| repository root | The client's shipped files: `dinput.dll`, `KPCFix.ini`, `DDrawCompat-CLIENT.ini`, setup and check scripts, `SUPPORTED_BUILDS.json`, `SHA256SUMS`, `README.md` |
| `source/` | Client source (`src/`), builder (`build.py`), emulator tests (`test_*.py`, `verify.py`), and the tracked build outputs `SYMBOLS.json` and `BUILD_RESULT.json` |
| `server-tools/` | Server Tools' shipped files (`KPCServerTools.dll`, scripts, `SERVER_TOOLS_BUILD.json`, `SHA256SUMS-Server-Tools`, `README-Server-Tools.md`) |
| `server-tools/server-tools-source/` | Server Tools source (`src/proxy.c`, `logging.S`), builder (`build_dll.py`), tests (`tests/`) |
| `tools/` | Packaging, manifest refresh, test-install updater, build wrapper, PowerShell tests, native server smoke |
| `docs/` | Developer documentation |

`ddraw.dll` (DDrawCompat) is not in git. Releases take it from the newest published client release, pinned by SHA-256 (see [RELEASING.md](RELEASING.md)).

## Branches

| Branch | Holds |
|---|---|
| `master` | The stable release. No co-op code. |
| `coop/mvp-lobby` | Co-op campaign work that has been confirmed in a two-PC playtest |
| `coop/mvp-lobby-untested` | New co-op work, before it is playtested |

The co-op branches are built on top of `master`. A stable fix goes to `master` first, then gets merged into the co-op branches. Co-op work only reaches `master` when it is ready to ship.

## Toolchains

Both DLLs build byte-for-byte reproducibly, but only with the pinned toolchains:

| Product | Toolchain | Local Docker image | CI |
|---|---|---|---|
| Client | GCC 13.3.0, binutils 2.42 | `kpc-client-gcc13:local` | Ubuntu 24.04 runner |
| Server Tools | GCC 14.2.0, binutils 2.44 | `kpc-gcc14:local` | `debian:trixie-slim` container |

Use the Docker images, not WSL. A different GCC (for example WSL's GCC 15) produces different bytes and can break `objcopy`. A changed DLL hash always means changed source. Never commit source without the matching rebuilt DLL and build report: CI rebuilds both products and compares them byte for byte.

The emulator tests need Python with Unicorn 2.1.4 and capstone. On the development PC that is `C:\KPCTest\re-venv`.

## Building

### Quick path: `tools/build-changed.sh`

`tools\build-changed.cmd` (or `tools/build-changed.sh` in Git Bash) needs Docker Desktop running. It:

1. hashes each product's inputs and rebuilds only what changed, in disposable copies under `build/`. Both products build at once.
2. copies the outputs back:
   - client: `dinput.dll`, `source/SYMBOLS.json`, `source/BUILD_RESULT.json`;
   - Server Tools: `server-tools/KPCServerTools.dll`, `server-tools/SERVER_TOOLS_BUILD.json`.
3. refreshes both checksum manifests (`tools/refresh_manifests.py`).
4. writes both release ZIPs to `dist/` (`tools/package_release.py`). The client ZIP needs `ddraw.dll`: pass `--ddraw <path>` or set `KPC_DDRAW`. Otherwise it uses `C:/Games/Psycho/ddraw.dll`.
5. updates the test install at `C:\Games\Psycho` (`tools/install_game.py`; see [below](#updating-a-test-install)).

Options:

| Option | Effect |
|---|---|
| `--client`, `--server` | Build only that product |
| `--force` | Rebuild even if nothing changed |
| `--no-package` | Skip the manifest refresh and the ZIPs |
| `--no-install` | Skip the test install |
| `--dry-run-install` | Print what the install would change, without changing it |
| `--game-dir <path>` | Install into another game folder |

**Note:** the default install target `C:\Games\Psycho` is the main test PC's game. If that folder runs a co-op build, a build from `master` replaces it. Use `--no-install` or `--game-dir` unless you mean to switch it.

### Manual builds

Always build in a fresh directory or a disposable copy: `build_dll.py` writes its DLL and report into the `server-tools/` folder it runs in.

```sh
# Client (from the repository root, inside kpc-client-gcc13:local)
python3 source/build.py --build-dir build/client --output build/client/dinput.dll

# Server Tools (inside kpc-gcc14:local, from a copy of server-tools/)
python3 server-tools-source/build_dll.py
```

On the development PC, `C:\KPCTest\clientbuild\build-coop.sh <name>` and `C:\KPCTest\pass2\build-coop.sh <name>` do the same in Docker. Despite the "coop" in their names, they build whatever is checked out in `C:\Users\magi\Documents\GitHub\KPCFix`.

## Testing

Automated tests run the shipped DLL in an x86 emulator against the real 1.13 game files, with Windows, graphics and network calls mocked. They show that the patch code does what it should with the original game code. They don't replace playing the game.

### Client

| Test | Command |
|---|---|
| All suites, four editions | `C:\KPCTest\clientbuild\suite-stable.sh <research root>`. The root needs `patched/<Region>` and `unpatched/<Region>` folders. |
| Same, from the research archive | `python3 source/verify.py --archive KPCFix-region-research-final.tar.gz --output <dir>` (add `--skip-audit` without `objdump`) |
| Hook-site disassembly audit | `python3 source/audit_sites.py --fixtures <dir named after its region>` (needs binutils; run it in the client image) |
| One suite | `python source/test_<name>.py --fixtures <edition folder> --symbols source/SYMBOLS.json --output <json>` |

The tests check the root `dinput.dll` with `source/SYMBOLS.json`, so copy a new build into place first. Add a `source/test_<feature>.py` for every new client feature.

### Server Tools

| Test | Command |
|---|---|
| Emulator matrix (Europe and USA; normal, cheats, off, altered object, no server frame) | `C:\KPCTest\pass2\matrix-stable.sh <build name> <root with Europe/ and USA/>` |
| One emulator run | `python -I <build copy>/server-tools/server-tools-source/tests/check_proxy.py <edition folder> [--cheats] [--off] [--bad-object] [--no-server-frame]` |
| Live command hooks | `python -I tests/check_live.py <object.lto> <dedicated.exe> [--disabled]` (needs `build/hooks.bin` from a build) |
| Assembly harness | `python tests/run_checks.py [--usa]` (needs Unicorn and binutils in one Python, so it runs in CI) |
| Native smoke (real `dedicated.exe`, no players) | `Start-Process powershell.exe -Wait -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File tools\Invoke-ServerSmoke.ps1 -GameDir <disposable game copy> -Mode Normal\|PassThrough -Report <json>'` |

`check_proxy.py` tests the DLL next to it. It loads it as `winmm.dll` and checks four things:
- `winmm` forwarding;
- the `dedicated.exe` `LoadLibraryA` redirect;
- the hooks in `object.lto`;
- the server-frame hook.

Run the native smoke only in a disposable copy of the game, such as `C:\KPCTest\smoke-master\Psycho`, with Server Tools installed by its own setup.

The co-op branches have their own runners: `unit-coop.sh` and `matrix-coop.sh` for the server, `suite-coop.sh` for the client, and `coop/tools/Invoke-CoopServerSmoke.ps1` for the native smoke (`-Mode Coop|Normal|PassThrough`). Their `CLAUDE.md` lists the commands.

### Scripts and packaging

```sh
python -m unittest discover -s tools -p "test_*.py"
powershell -File tools/Test-PowerShellSyntax.ps1     # every tracked .ps1 must parse in Windows PowerShell 5.1
powershell -File tools/Test-KPCServerIni.ps1         # launcher INI edits
```

### In-game checks before a release

| Area | Check |
|---|---|
| Client, each shell layout | Run `Check-KPCVersion.cmd`, launch twice, check the edition in `KPCFix.log`, then launch `Psycho.exe` directly. Resolution and key bindings are kept. |
| Frame rate | At the default (VSync), at `flipend(60)` and with VSync off: narration, camera pans, animations and enemies keep their pace. In single player the R2M2A Ballbuster clears the garage door. |
| Mouse and HUD | Aiming, Input Rate slider, `StockMouse=1`. HUD scales 100, 150 and 200 at 1080p and 1440p: enemy bars, cursor hotspot, tooltips at screen edges, 4:3 movies. |
| Console key | UK keyboard layout: opening the console types no apostrophe. |
| Multiplayer | Join by IP straight after starting the game. Two players on each server edition: chat, `kpc_cheats` on/off (including while airborne), clean shutdown. |
| Server Tools install | Fresh install; upgrade from a pre-1.1.0 install (`server.original.dll`); repeated setup; uninstall. `server.dll` and `object.lto` hashes stay original throughout, and `KPCServerTools.ini` survives. |
| Server timing | Europe-family server: the log says `Minimum server frame installed, ServerFrameMs=16`, and the R2M2A Ballbuster clears the garage door. USA server: the log says the server frame is not supported. |

Record the DLL hashes, game edition, Windows or Wine version and the result of each check.

## Updating a test install

`tools/install_game.py --game-dir <folder> [--client] [--server] [--dry-run]` updates a real game folder from the checkout. It:
- refuses while `client.exe`, `dedicated.exe` or `Psycho.exe` is running;
- backs up every file it changes to `C:\KPCTest\backup\`;
- copies only files that differ;
- adds new keys to `KPCFix.ini` and `DDrawCompat-CLIENT.ini` without changing existing lines;
- never touches `KPCServerTools.ini`, `server.original.dll` or original game files;
- replaces `winmm.dll` only if it already is a Server Tools build. For a folder without Server Tools, or with an install from before 1.1.0, run `Setup-KPCServerTools.cmd` in that folder instead.

Install only committed builds, and note the DLL hashes.

The second test PC (the laptop) needs only `dinput.dll`, `KPCFix.ini` and `DDrawCompat-CLIENT.ini`. `build-changed` puts them in `dist/client/`.
