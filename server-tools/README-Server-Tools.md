# KPCFix Server Tools

Windows dedicated-server tools for official game version **1.13**. Supports the supplied Europe/UK, USA, German and Italian 1.13 builds.

## Update an existing DLL installation

Close the game and server, copy these files into the game directory, then run **Setup-KPCServerTools.cmd** again to deploy the new DLL. Keep your existing `server.original.dll` and `KPCServerTools.ini`. The setup recognizes the preceding proxy build.

If setup reports an unrecognised active `server.dll`, leave the official `server.original.dll` in place. Once you have confirmed it is the matching original, move the active `server.dll` to a separate backup folder and rerun setup. Setup verifies the original against the dedicated executable before installing the proxy. Keep the moved DLL if you need to restore your previous installation.

## Install or migrate

1. Close the game and dedicated server for this one-time setup. Back up your game folder.
2. Extract this package into the game directory, normally `C:\Games\Psycho\`, beside `Psycho.exe`, `dedicated.exe`, `server.dll` and `object.lto`. Keep the package's supporting files together.
3. Run **Setup-KPCServerTools.cmd**. It preserves the official engine as `server.original.dll`, installs our proxy as `server.dll`, and restores recognized earlier Server Tools patches to the exact original `object.lto`. Unsupported files are rejected.
4. Run **KPC-Server-Launcher.cmd** to choose from the single all-maps list, adjust server rules and initial cheat permission, then start the server. The compact GUI keeps the campaign disclaimer at the top. Its launcher terminal remains visible; VBScript is not required. Clients should discover it in their game browser.

Do not manually rename or overwrite the original `server.dll`: setup handles the backup. `KPCServerTools.dll` is the supplied proxy payload, which setup copies to the engine's expected filename. Keep `server.original.dll` beside it.

The launcher uses its own generated `kpc-gui-server.cfg` and map list; it does not rewrite the shipped `server.cfg`. All 59 maps are listed, including campaign maps and scripted M0 introductions. Campaign maps were not designed for multiplayer and may be buggy.

After setup, ordinary server starts no longer modify game binaries. The game can remain open while you start the dedicated server. The existing client `dinput.dll` is independent and remains installed as before.

## Features

- Graphical map selection and server-rule controls.
- Optional, per-player noclip and invulnerability using the client's existing developer key bindings.
- Live host-console permission: `kpc_cheats 1` enables cheats; `kpc_cheats 0` disables them and revokes active cheats. `kpc_cheats` shows status.
- Everyone sees **Cheats enabled** or **Cheats disabled** when permission changes. Player toggles announce **name toggled cheat ON/OFF** in chat only, without a centre-screen copy.
- Player chat is mirrored to the server console. Patch messages have one `[KPCFix Server Tools]` prefix and preserve the command prompt and pending input.
- Disabled cheat requests are rejected silently. A player's cheat state applies only to that player.

## Settings and removal

Setup creates `KPCServerTools.ini` if absent:

```ini
[ServerTools]
Enabled=1
AllowCheats=0
```

**Important:** `Start-KPC-Selected-Server.ps1` explicitly requires the `AllowCheats` key in `KPCServerTools.ini`; if it is absent, the graphical server launcher stops with `AllowCheats is missing from KPCServerTools.ini.` Existing INI files are preserved by setup, so re-running setup does not fill a missing key. Retain `AllowCheats=0` (or `AllowCheats=1` when explicitly desired) under `[ServerTools]` whenever adding co-op settings. The key must exist even if cheats are disabled.

`Enabled=0` makes the proxy pass calls through to the official engine without installing Server Tools hooks; restart the server after changing it. `AllowCheats` sets initial permission, which the graphical launcher updates from its checkbox. Use the console command for live changes. Chat logging and announcements are active whenever the tools are enabled. Startup/errors are also appended to `KPCServerTools.log`.

To remove the proxy, close the game/server and run **Restore-Original-Server.cmd**. It restores the preserved official engine to `server.dll`; `object.lto` remains stock. Old on-disk patch installers are moved to `KPCServerTools-Legacy` during migration; do not run them with this DLL installation.

## How it works and validation

The small 32-bit proxy forwards the engine's two exports and intercepts its lookup of `ObjectDLLSetup`. It validates the official binaries, then installs the established Server Tools hooks in the loaded object module. It does not change the object file, PE headers or relocation table. Original game binaries are not bundled.

Compiled-proxy emulation passed for Europe and US engine profiles at relocated DLL/object addresses, with cheats enabled, disabled, pass-through and altered-object rejection. Hook tests cover separate player state, live revocation, client OFF acknowledgements, chat announcements and prompt restoration. These checks supplement Windows testing; they do not replace it. The stable R3 warning-spacing fix was confirmed in a Windows playtest. Record the DLL hash and repeat the applicable live checks when testing a different build.

Source and build/test instructions are in `server-tools-source/README.md`. The stable download is [Server Tools R3](https://github.com/magibax/KPCFix/releases/tag/server-tools-v1.0.0-r3).

## Installing alongside KPCFix

Server Tools uses its own README, license, checksums, compatibility checker, build list and source directory. Extracting either release over the other does not overwrite those files. Run `Check-KPCVersion.cmd` for the client; the server launcher automatically runs its separate `Check-KPCServerVersion.ps1` checks.

## Experimental co-op branch: spawn-point instrumentation

This section applies **only** to the `feature/coop-campaign` branch, not to the stable Server Tools release. Source now includes a conditional spawn-position trace plus an **opt-in, R1M2B-only three-client spawn trial**. Both hooks are off by default. The trial is not collision-validated and must not be used on public servers.

The branch includes a rebuilt experimental DLL and matching build metadata. After deploying it with setup, edit the existing `KPCServerTools.ini` and restart the server:

```ini
[ServerTools]
Enabled=1
AllowCheats=0

[Coop]
TrialSeparateSpawns=1
TraceSpawns=1
TraceCollisionABI=1
```

`AllowCheats` is required for the graphical server launcher to proceed. `TraceCollisionABI=1` enables read-only diagnostic segment rays and does **not** enable automatic collision-aware spawning.

Run R1M2B with at most three clients in a fresh server session, then inspect `KPCServerTools.log`. The first 64 candidate position requests are written only to the log; no user-facing chat or screen text is generated. To end the trial, set all three `[Coop]` keys to 0 and restart the server. `Enabled=0` continues to pass through to the original engine. Do not ship a new release without rebuilding the tracked binary and build manifest, checking hashes, and performing Windows runtime verification.

See [engine trace findings](https://github.com/magibax/KPCFix/blob/feature/coop-campaign/coop/SPAWN-ENGINE-TRACE.md), [site verifier](https://github.com/magibax/KPCFix/blob/feature/coop-campaign/coop/tools/check_spawn_site.py) and [log decoder](https://github.com/magibax/KPCFix/blob/feature/coop-campaign/coop/tools/decode_spawn_log.py).
