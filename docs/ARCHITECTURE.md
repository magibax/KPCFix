# Architecture

KPCFix is two independent products. Each is a 32-bit proxy DLL that Windows loads from the game folder. Each checks the original game files and patches them **in memory only**. No game file on disk is ever changed.

| Product | Installed as | Loaded by | Patches |
|---|---|---|---|
| Client | `dinput.dll` | `client.exe` | `client.exe`, `cshell.dll` |
| Server Tools | `winmm.dll` (from `KPCServerTools.dll`) | `dedicated.exe` | `object.lto`, `server.dll` |

Both follow the same rules:
- Identify the exact 1.13 files by SHA-256.
- Check the original bytes at every patch site before writing.
- Refuse to install (fail closed) on any mismatch.

## Client (`dinput.dll`)

### Startup

`dinput.dll` exports the DirectInput factory functions and forwards them to the Windows `dinput.dll`. `DllMain` only records the module. The game's first DirectInput call loads the real backend and starts game setup, which:

1. hashes `client.exe` and the on-disk `cshell.dll`, and checks their file and PE layout;
2. picks the shell layout for the edition (see below);
3. stages each group of edits in a transaction: the original bytes are checked, memory protection is restored afterwards, the instruction cache is flushed, and the group is rolled back on failure.

The client and shell edits are separate stages. A failure in the shell stage is logged separately from the client edits. The shell may be loaded before or after the proxy, and both cases must keep working: if it is loaded later, a `LoadLibraryA` hook in the client applies the shell edits when it arrives. With `Enabled=0`, or in a process other than the game, the DLL only forwards DirectInput. `DllCanUnloadNow` returns `S_FALSE`, because the installed hooks call into the DLL.

### Editions

All four 1.13 editions share one `client.exe`. Their `cshell.dll` comes in three layouts:
- Europe and USA: one shared layout;
- Germany;
- Italy.

`source/src/profiles.h` holds, for each layout, the fingerprints, the hook sites, the original bytes and the stack layouts. The Italian shell, for example, uses a different stack frame in its settings code. A field set to 0 means the layout is unverified. That feature then stays stock and logs "not verified". [COMPATIBILITY.md](COMPATIBILITY.md) has the details.

### Modules

| Area | Files |
|---|---|
| Setup, file checks, transactions, settings | `kpcfix.c` |
| Frame timing | `timing.S`: the clock guard plus eight separate fractional carries for animation sites. A shared carry would couple unrelated animations. |
| Movement on the ground (shell) | `ground.S` |
| Local server minimum step | `minstep_hook.S`: takes over the clock guard's slot at `client.exe` `0x664b2` when `LocalServerMinStepMs` is set. The clock guard removes the engine's 10 ms floor, and some AI stalls on shorter steps. |
| Mouse | `input_interval.S` (input clock); the sensitivity constants and Input Rate lock are in `kpcfix.c` |
| Menus, Input Rate slider, cursor, tooltips | `ui.c`, `ui_hooks.S` |
| Saved Input Rate edits, with backups | `configs.c` |
| HUD scaling, enemy health bars and labels | `hud.c`, `hud_hooks.S`, `shell_layout.h` |
| 4:3 movies and the movie filter | `cinematic.c`, `cinematic_hook.S` |
| Console key | `console.c`: drops the character the console key types (`client.exe` `0x6dc9` key-down, `0x700a` `WM_CHAR`) |
| Multiplayer bind fix, optional network trace | `kpcfix.c` |

Rendering, VSync and frame pacing come from DDrawCompat (`ddraw.dll`, configured by `DDrawCompat-CLIENT.ini`). DDrawCompat is a separate third-party DLL, independent of the patch code.

## Server Tools (`winmm.dll`)

### Loading

1. `dedicated.exe` imports `WINMM.dll`, so Windows loads `winmm.dll` from the game folder before the server starts.
2. In `DllMain`, Server Tools loads the system `winmm.dll` from the Windows system folder. It fills a table of jump stubs, one per `winmm` export (`winmm_exports.txt`, built by `build_dll.py`), so every `winmm` call reaches Windows unchanged.
3. If the host process is a supported `dedicated.exe` (checked by hash), Server Tools redirects that executable's `LoadLibraryA` import. When the server calls `LoadLibraryA("server.dll")`, it gets the Server Tools module back instead.
4. The server calls `CreateServer` on that module. Server Tools then:
   - checks that `server.dll` is the original engine for this `dedicated.exe`;
   - loads it under its own name;
   - redirects the engine's `GetProcAddress` import;
   - hands the call on to the real `CreateServer`.
5. When the engine looks up `ObjectDLLSetup` in `object.lto`, Server Tools checks the loaded object module and installs its hooks in memory (`apply_object`). It doesn't touch PE headers or relocations.

In any other process (`client.exe`, `Psycho.exe`) Server Tools only forwards `winmm`. Single-player games load the real `server.dll` directly and are not affected.

With `[ServerTools] Enabled=0`, the engine runs unhooked (pass-through). Unsupported or altered files stop the server with a message in the console and in `KPCServerTools.log`.

### What it patches

- **Chat, console and cheats** (`logging.S`, assembled into `object.lto`'s free space at build time):
  - mirrors chat to the server console;
  - adds the `kpc_cheats` console command;
  - keeps per-player cheat state (noclip, invulnerability);
  - announces permission changes and revokes active cheats.
- **Spawn warning.** Trims the extra line break in the stock "Could not find any multiplayer start points" warning.
- **Server frame** (`ServerFrameMs`, Europe-family `server.dll` `83b7bce8…` only):
  - The engine's Update (`0x2358c`) calls the frame function `0x2a05f(flags, time)` at `0x235b0`. Server Tools redirects that call.
  - A frame less than `ServerFrameMs` after the last simulated frame is skipped, so the next frame simulates the whole elapsed time.
  - The USA `server.dll` keeps its original timing; its matching addresses are not mapped yet.

### Installing

`Setup-KPCServerTools.ps1`:
- checks `dedicated.exe`, `server.dll` and `object.lto` against `SERVER_TOOLS_SUPPORTED_BUILDS.json`;
- restores a known old on-disk `object.lto` patch (`Restore-LegacyObject.ps1`, data in `legacy-object-patches.json`);
- copies `KPCServerTools.dll` to `winmm.dll`.

It moves an install from before v1.1.0 (proxy as `server.dll`, engine as `server.original.dll`) back to the original `server.dll`. It only ever replaces files it recognises: a hash in `SERVER_TOOLS_BUILD.json`, or a DLL carrying the `[KPCFix Server Tools]` marker. It renames files instead of using `File.Replace`, because a stock game file can't be replaced that way without administrator rights. `-Uninstall` removes `winmm.dll`.

The launcher (`KPC-Server-Launcher.ps1`) writes `kpc-gui-server.cfg` and a map list. Its worker (`Start-KPC-Selected-Server.ps1`):
1. runs `Check-KPCServerVersion.ps1`;
2. writes `AllowCheats`;
3. starts `dedicated.exe -nogamespy -exec kpc-gui-server.cfg`;
4. types the first map into the server's console.

## Build

Each product builds with Python, GCC and GNU binutils. The flow is:
1. compile freestanding 32-bit x86 objects;
2. convert them to COFF;
3. link a PE32 DLL against build-only import stubs.

Builds are byte-for-byte reproducible with the pinned toolchains (client: GCC 13.3 / binutils 2.42; server: GCC 14.2 / binutils 2.44). A changed hash always means changed source.

`sha256.c`, `sha256.h` and `winmini.h` exist in both `source/src/` and `server-tools/server-tools-source/src/`, so each product builds from its own folder. CI checks that the two copies stay identical.

Both DLLs are Windows binaries. Under Wine, `winmm.dll` needs a native override (`WINEDLLOVERRIDES="winmm=n,b"`). Rendering, input and audio need their own testing there.
