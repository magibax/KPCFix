# KPCFix Server Tools

Tools for hosting *KISS: Psycho Circus* dedicated servers, for game version **1.13**. They work with the Europe/UK, US, German and Italian 1.13 server files.

Server Tools has its own version number, separate from the KPCFix client. "1.13" always means the game patch.

> **Co-op branch build.** This build adds the experimental co-op campaign. See [coop/README.md](https://github.com/magibax/KPCFix/blob/coop/mvp-lobby/coop/README.md) for hosting and commands, and [coop/STATUS.md](https://github.com/magibax/KPCFix/blob/coop/mvp-lobby/coop/STATUS.md) for what has been tested. It is not a release.

## Features

- **Launcher.** Pick any of the 59 maps, set the server rules and start the server from one window.
- **Cheats, per player.** Players can use noclip and invulnerability with the game's existing developer keys, but only while the host allows cheats.
- **Live cheat switch.** In the server console, `kpc_cheats 1` allows cheats and `kpc_cheats 0` turns them off, including any cheats players have on. `kpc_cheats` shows the current setting. Everyone sees **Cheats enabled** or **Cheats disabled**, and a player's toggle is announced in chat.
- **Chat in the console.** Player chat is shown in the server console without breaking what you are typing.
- **Co-op campaign** (experimental): up to three players through all 48 campaign worlds, with a lobby, shared progress and synchronised loading. Choose **Co-op campaign** in the launcher.
- **Real-time server timing in co-op.** The original server runs game time about 25% too fast, in steps too short for some enemies (the R2M2A Ballbuster gets stuck in the garage door). In a co-op session the server runs in real time (`[Coop] CoopServerFrameMs`). On this branch, deathmatch keeps the original timing.

**US server note:** the co-op timing fixes need the Europe/UK, German or Italian `server.dll`. Everything else works on the US build.

## Install

1. Close the game and any running server. Back up your game folder.
2. Extract this ZIP into the game folder, next to `dedicated.exe`, `server.dll` and `object.lto` (usually `C:\Games\Psycho`).
3. Run **Setup-KPCServerTools.cmd** once.
4. Start servers with **KPC-Server-Launcher.cmd**. Players find the server in the game's server browser.

Setup copies `KPCServerTools.dll` into the game folder as `winmm.dll`. `dedicated.exe` loads `winmm.dll` from its own folder. Server Tools passes every `winmm` call on to the Windows copy, then loads the original `server.dll` and patches the game in memory. **No game file is changed:** `server.dll` and `object.lto` stay the original files.

The game itself (`Psycho.exe`, `client.exe`) also loads `winmm.dll` from the game folder. For the game, Server Tools only passes the calls on and changes nothing. That is why setup asks you to close the game too.

Setup refuses to continue if any of these is true:
- the game files are not a supported 1.13 set;
- a `winmm.dll` from another mod is in the folder;
- `server.dll` is not the original and setup cannot restore it.

It never replaces a file it doesn't recognise.

This build still runs if it is installed the old way, as `server.dll` with the engine renamed `server.original.dll` (for example under Wine without the override below). Setup always installs it as `winmm.dll`.

**Wine:** Wine uses its own built-in `winmm.dll` unless told otherwise. Run the server with `WINEDLLOVERRIDES="winmm=n,b"`.

## Upgrading

Close the game and server, extract the new ZIP over the old files and run **Setup-KPCServerTools.cmd** again. Your `KPCServerTools.ini` is kept.

Releases before v1.1.0 replaced `server.dll` and kept the original as `server.original.dll`. Setup moves such an install over: it puts the original engine back as `server.dll` and installs `winmm.dll`. Setup also restores an `object.lto` that a very early release patched on disk, and moves the old on-disk patch installers into a `KPCServerTools-Legacy` folder.

## Using the launcher

The launcher writes its own `kpc-gui-server.cfg` and map list, and leaves the game's `server.cfg` alone. All 59 maps are listed. Campaign maps were not designed for multiplayer and may be buggy.

The **Allow cheats on startup** box sets the cheat permission the server starts with. You can still change it live with `kpc_cheats`. The game can stay open while you start a server.

## Settings

Setup creates `KPCServerTools.ini` the first time:

```ini
[ServerTools]
Enabled=1
AllowCheats=0

[Coop]
; ...
```

| Setting | Meaning |
|---|---|
| `Enabled` | `0` runs the original server with no Server Tools features. Restart the server after changing it. |
| `AllowCheats` | Cheat permission at server start. The launcher sets it from its checkbox. |

The `[Coop]` settings are described in the file and in [coop/CONFIGURATION.md](https://github.com/magibax/KPCFix/blob/coop/mvp-lobby/coop/CONFIGURATION.md). The launcher writes the ones a co-op start needs.

Startup messages, errors and the co-op session log are written to `KPCServerTools.log`. Include it with every co-op bug report.

## Uninstall

Close the game and server, then run **Restore-Original-Server.cmd**. It removes `winmm.dll`. If the folder still has an old `server.original.dll`, it puts that back as `server.dll`. `object.lto` is not touched.

You can then delete the other Server Tools files:
- `KPCServerTools.dll`, `KPCServerTools.ini`, `KPCServerTools-default.ini`, `KPCServerTools.log`;
- the `*.ps1` and `*.cmd` files from this ZIP;
- `SERVER_TOOLS_*.json`, `SHA256SUMS-Server-Tools`, `legacy-object-patches.json`, `maps.json`, `server-template.cfg`;
- `kpc-gui-*`, `kpc-coop-*`, `README-Server-Tools.md` and `LICENSE-Server-Tools.txt`.

## Alongside the KPCFix client

Server Tools and the client use different file names, so you can extract both into one game folder in either order. Check the client with `Check-KPCVersion.cmd`. The server launcher runs its own check (`Check-KPCServerVersion.ps1`) each time.

Source code and developer docs: [github.com/magibax/KPCFix](https://github.com/magibax/KPCFix).
