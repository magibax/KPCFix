# KPCFix Server Tools

Tools for hosting *KISS: Psycho Circus* dedicated servers, for game version **1.13**. They work with the Europe/UK, US, German and Italian 1.13 server files.

Server Tools has its own version number, separate from the KPCFix client. "1.13" always means the game patch.

## Features

- **Launcher.** Pick any of the 59 maps, set the server rules and start the server from one window.
- **Cheats, per player.** Players can use noclip and invulnerability with the game's existing developer keys, but only while the host allows cheats.
- **Live cheat switch.** In the server console, `kpc_cheats 1` allows cheats and `kpc_cheats 0` turns them off, including any cheats players have on. `kpc_cheats` shows the current setting. Everyone sees **Cheats enabled** or **Cheats disabled**, and a player's toggle is announced in chat.
- **Chat in the console.** Player chat is shown in the server console without breaking what you are typing.
- **Real-time server timing.** The original server runs game time about 25% too fast, in steps too short for some enemies. For example, the Ballbuster in R2M2A gets stuck in the garage door. Server Tools runs the server in real time, in steps of at least 16 ms (`ServerFrameMs`).

**US server note:** the timing fix needs the Europe/UK, German or Italian `server.dll`. A server using the US `server.dll` keeps the original timing. Everything else works on the US build.

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
ServerFrameMs=16
```

| Setting | Meaning |
|---|---|
| `Enabled` | `0` runs the original server with no Server Tools features. Restart the server after changing it. |
| `AllowCheats` | Cheat permission at server start. The launcher sets it from its checkbox. |
| `ServerFrameMs` | Shortest server step in milliseconds, 1–50. `0` keeps the original timing. A missing key means 16. |

Startup messages and errors are written to `KPCServerTools.log`.

## Uninstall

Close the game and server, then run **Restore-Original-Server.cmd**. It removes `winmm.dll`. If the folder still has an old `server.original.dll`, it puts that back as `server.dll`. `object.lto` is not touched.

You can then delete the other Server Tools files:
- `KPCServerTools.dll`, `KPCServerTools.ini`, `KPCServerTools-default.ini`, `KPCServerTools.log`;
- the `*.ps1` and `*.cmd` files from this ZIP;
- `SERVER_TOOLS_*.json`, `SHA256SUMS-Server-Tools`, `legacy-object-patches.json`, `maps.json`, `server-template.cfg`;
- `kpc-gui-*`, `README-Server-Tools.md` and `LICENSE-Server-Tools.txt`.

## Alongside the KPCFix client

Server Tools and the client use different file names, so you can extract both into one game folder in either order. Check the client with `Check-KPCVersion.cmd`. The server launcher runs its own check (`Check-KPCServerVersion.ps1`) each time.

Source code and developer docs: [github.com/magibax/KPCFix](https://github.com/magibax/KPCFix).
