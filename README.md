# KPCFix

Fixes for *KISS: Psycho Circus – The Nightmare Child* for modern PCs.

> **Co-op branch.** This build includes the experimental co-op campaign and extra client features that are still being tested. See [coop/README.md](https://github.com/magibax/KPCFix/blob/coop/mvp-lobby/coop/README.md). For the stable release, use the [Releases](https://github.com/magibax/KPCFix/releases) page.

KPCFix works with the Europe/UK, US, German and Italian (Covermount) editions of the game, patched to version **1.13**.

## What it fixes

- **Frame rate.** Running the game above 60fps causes numerous issues with scripted sequences and AI. KPCFix fixes the game's timing, so everything runs at normal speed at any frame rate. By default the game runs at your monitor's refresh rate with VSync.
- **Mouse.** Mouse input is buggy on modern machines (choppy, dropping inputs, limited to a 60hz polling rate). KPCFix resolves all of this, giving buttery smooth mouse input at all framerates, particularly noticable above 60fps. The "Input Rate" slider has been removed (it sets the interval over which an "average" of mouse inputs is taken, so it's rather misleading), and the sensitivity slider range has been tweaked to suit modern mice.
- **HUD size.** The HUD, menu cursor and tooltips scale together. The default is 150%, which suits 1920×1080. For 4K, set to 300%.
- **Multiplayer.** You can join a server straight after starting the game. A bug prevented joining any multiplayer game unless you loaded a local map first.
- **Smaller fixes.** Movies and loading screens are shown in their intended 4:3 aspect ratio, with optional smoothing enabled by default. The console key no longer types a stray character. 

Every option is in `KPCFix.ini` and in the [configuration reference](https://github.com/magibax/KPCFix/blob/master/docs/CONFIGURATION.md).

## Issues

Please report any bugs you find. Increasing the minimum simulation step to 16ms seems to have ironed out most issues, but some may remain.

## Install

1. Patch a clean install of the game to **1.13**. Close the game and its launcher.
2. If your game folder already has a `dinput.dll`, `ddraw.dll` or `DDrawCompat-CLIENT.ini` from another mod, back them up. KPCFix replaces them.
3. Download the latest `KPCFix-Windows` ZIP from [Releases](https://github.com/magibax/KPCFix/releases). Extract it into the game folder, next to `Psycho.exe` (usually `C:\Games\Psycho`).
4. Run `Check-KPCVersion.cmd`. Continue only if it says **PASS** and names your edition.

Run `Check-KPCVersion.cmd` again whenever you reinstall or repatch the game.

## First launch

1. Run `Launch-KPCFix.cmd`. The first time, it sets the game to 1920×1080 and opens the game launcher.
2. In the launcher, open **Display** and pick your resolution.
3. Start the game. If you are using a pre-existing user profile, set your desired resolution in the video settings once you load the profile in-game.

After that you can start the game with `Psycho.exe` as usual.

`Launch-KPCFix.cmd` also turns on the game's `Developer` setting each time it runs. Set `ForceDeveloper=0` in `KPCFix.ini` to stop that. Every game config file it changes is backed up first, with a `.kpcfix-display.bak` extension.

## Frame rate

The game runs borderless with VSync at your monitor's refresh rate, for example 60, 144 or 165 FPS. Press **Shift+F12** in game to show the frame rate.

To change it, edit `DDrawCompat-CLIENT.ini` and restart the game:

| To | Set |
|---|---|
| Cap at 60 FPS | `FpsLimiter = flipend(60)` |
| Cap at another rate, such as 120 FPS | `FpsLimiter = flipend(120)` |
| Turn off VSync and every cap | `FullscreenMode = exclusive(0)`, `VSync = off`, `FpsLimiter = off` |

## Settings

Edit `KPCFix.ini` and restart the game. Each option is explained in the file. The [configuration reference](https://github.com/magibax/KPCFix/blob/master/docs/CONFIGURATION.md) lists them all, with common changes such as the HUD size for 1440p or the original mouse.

## Uninstall

1. Close the game.
2. Optional: put back the game config files exactly as they were before KPCFix. This also undoes any changes you made to them since.
   ```
   powershell -ExecutionPolicy Bypass -File Setup-KPCFix.ps1 -Restore
   ```
3. Delete the files KPCFix added:
   - `dinput.dll`, `ddraw.dll`
   - `KPCFix.ini`, `KPCFix.log`, `DDrawCompat-CLIENT.ini`
   - `Launch-KPCFix.cmd`, `Setup-KPCFix.ps1`, `Check-KPCVersion.cmd`, `Check-KPCVersion.ps1`
   - `SUPPORTED_BUILDS.json`, `SHA256SUMS`, `README.md`, `LICENSE`, the `LICENSES` folder
   - `KPCFix-Display-Preset.applied`
   - If you skipped step 2: `KPCFix-Display-Backups.json` and any `.kpcfix-display.bak` files
4. Put back any DLLs or INIs you backed up when you installed.

## Hosting a server

**KPCFix Server Tools** is a separate download on the [Releases](https://github.com/magibax/KPCFix/releases) page. You don't need it to join servers.

It adds:
- a graphical launcher for all 59 maps;
- server rule settings;
- chat shown in the server console;
- real-time server timing.

See the [Server Tools README](https://github.com/magibax/KPCFix/blob/master/server-tools/README-Server-Tools.md).

## In development

**Co-op campaign.** Co-op through the whole campaign is being built and tested on separate branches. It is not part of this release:
- [`coop/mvp-lobby`](https://github.com/magibax/KPCFix/tree/coop/mvp-lobby) has the work confirmed in play.
- [`coop/mvp-lobby-untested`](https://github.com/magibax/KPCFix/tree/coop/mvp-lobby-untested) has the newest builds, not yet tested.

## For developers

The release ZIP holds only what you need to play. Source code and developer docs are in the [repository](https://github.com/magibax/KPCFix). Start with [docs/DEVELOPMENT.md](https://github.com/magibax/KPCFix/blob/master/docs/DEVELOPMENT.md).

KPCFix and Server Tools have their own version numbers, such as KPCFix v1.1.3 and Server Tools v1.1.0. "1.13" always means the game patch.

## Credits and licence

KPCFix is released under the [MIT licence](LICENSE). The bundled `ddraw.dll` is [DDrawCompat](https://github.com/narzoul/DDrawCompat) by narzoul, licensed under 0BSD (see `LICENSES/`).
