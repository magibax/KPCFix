# KPCFix

Fixes and improvements for *KISS: Psycho Circus — The Nightmare Child* on modern Windows PCs.

Works with the Europe, US, German and Italian Covermount editions, patched to **1.13**.

## Features

- **Smooth mouse input.** Camera turning and the menu cursor are smooth, especially on high-refresh displays, and the sensitivity slider gives finer control at low settings.
- **Readable HUD.** The HUD, menu cursor and tooltips scale together. The default is 150%, sized for 1920×1080.
- **Unlocked frame rate.** In the original game, running above 60 FPS broke a lot of things, including narration, scripted sequences, camera pans, animations and enemy movement. KPCFix fixes the game's timing, movement and AI, so everything keeps its normal pace at any frame rate. By default the game runs at your monitor's refresh rate with VSync, and you can turn VSync off for an unlimited frame rate (see [Frame rate](#frame-rate)).
- **Join multiplayer games straight away.** You no longer need to load a local map first to join a multiplayer game.

KPCFix also includes smaller fixes, such as 4:3 cinematics on widescreen displays. Every fix and option is listed in the [configuration reference](https://github.com/magibax/KPCFix/blob/main/CONFIGURATION.md) and described in `KPCFix.ini`.

## Install

1. Patch a clean installation to **1.13**, then close the game and launcher.
2. If your game folder already has a `dinput.dll`, `ddraw.dll` or `DDrawCompat-CLIENT.ini` from another mod, back them up. KPCFix replaces them.
3. [Download the latest Windows client ZIP](https://github.com/magibax/KPCFix/releases) and extract it beside `Psycho.exe` (usually `C:\Games\Psycho\`).
4. Run `Check-KPCVersion.cmd`. Continue only if it reports **PASS** and names your edition.

Run `Check-KPCVersion.cmd` again whenever you replace or repatch game files.

## First launch

1. Run `Launch-KPCFix.cmd`. On its first run it sets your game configuration to **1920×1080** and opens the game launcher.
2. In the launcher, open **Display** and choose your resolution.
3. Start the game and create or select a profile with that **same resolution**.

From then on you can start the game with `Psycho.exe` as usual. The game and launcher keep the resolution you chose.

`Launch-KPCFix.cmd` also sets `Developer` to `1` in the game configuration each time it runs. Set `ForceDeveloper=0` in `KPCFix.ini` to stop this. Any configuration file it changes is backed up first with a `.kpcfix-display.bak` extension.

## Frame rate

By default the game runs in borderless mode with VSync, at your monitor's refresh rate (for example 60, 144 or 165 FPS). Press **Shift+F12** in game to show the frame rate.

To change this, edit `DDrawCompat-CLIENT.ini` and restart the game:

| To… | Set |
|---|---|
| Cap at 60 FPS (the default in earlier releases) | `FpsLimiter = flipend(60)` |
| Use a different cap, such as 120 FPS | `FpsLimiter = flipend(120)` |
| Remove VSync and every limit | `FullscreenMode = exclusive(0)`, `VSync = off`, `FpsLimiter = off` |

Exclusive fullscreen without VSync may tear. If a scripted sequence misbehaves at a high frame rate, cap at 60 FPS and replay it.

## Settings

Edit `KPCFix.ini` and restart the game. Each option is described in the file, and the [configuration reference](https://github.com/magibax/KPCFix/blob/main/CONFIGURATION.md) lists them all with common tweaks, such as HUD size for 1440p or the original mouse.

## Uninstall

Uninstall instructions are included with each [release](https://github.com/magibax/KPCFix/releases).

## Hosting a server

Server hosting tools are a separate download: [KPCFix Server Tools](https://github.com/magibax/KPCFix/releases). The client ZIP already includes the multiplayer fix for joining servers.

Server Tools adds a graphical launcher covering all 59 maps, server-rule settings, a live `kpc_cheats 0/1` switch and console chat logging. Campaign maps can be hosted but are experimental in multiplayer.

The next Server Tools release (v1.0.2) adds real-time server timing, which also stops the R2M2A Ballbuster getting stuck in the garage door. It will work only with the Europe/UK, German and Italian server files at first; a server using the USA `server.dll` will keep the original timing.

Extract the Server Tools ZIP into the game folder, close the game and any server, run `Setup-KPCServerTools.cmd` once, then start servers with `KPC-Server-Launcher.cmd`. The original server engine is kept and can be restored at any time. See the [Server Tools README](https://github.com/magibax/KPCFix/blob/main/server-tools/README-Server-Tools.md) for details, upgrades and removal.

## Roadmap

**Co-op campaign.** Experimental multiplayer co-op through the whole campaign is in development. It is not part of the stable release. Work tested in play is on the [`coop/mvp-lobby`](https://github.com/magibax/KPCFix/tree/coop/mvp-lobby) branch, and the newest, not yet tested, fixes are on [`coop/mvp-lobby-untested`](https://github.com/magibax/KPCFix/tree/coop/mvp-lobby-untested).

**Graphics.** Ongoing research covers engine performance, anisotropic filtering and antialiasing (including in menus), sensible modern defaults for graphics settings such as `c_GlobalDetail`, `GibDetail` and `LodOffset`, and engine-level improvements to texture filtering, model detail and draw distance.

## For developers

The release ZIP contains only what you need to play. Source code and full documentation are in the [GitHub repository](https://github.com/magibax/KPCFix):

- [Building the client](https://github.com/magibax/KPCFix/blob/main/source/BUILD.md)
- [Architecture](https://github.com/magibax/KPCFix/blob/main/ARCHITECTURE.md)
- [Configuration reference](https://github.com/magibax/KPCFix/blob/main/CONFIGURATION.md)
- [Testing and verification](https://github.com/magibax/KPCFix/blob/main/TESTING.md)
- [Edition fingerprints and compatibility notes](https://github.com/magibax/KPCFix/blob/main/COMPATIBILITY-NOTES.md)
- [Release procedure](https://github.com/magibax/KPCFix/blob/main/RELEASING.md)

KPCFix and Server Tools have their own version numbers (for example, client v1.1.3 and Server Tools v1.0.1). “1.13” always means the supported game patch.

## Credits and licence

KPCFix is released under the [MIT licence](LICENSE). The bundled `ddraw.dll` is [DDrawCompat](https://github.com/narzoul/DDrawCompat) by narzoul, licensed under 0BSD (see `LICENSES/`).
