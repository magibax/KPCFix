# KPCFix for KISS Psycho Circus 1.13

A Windows drop-in for the official Europe, US, German and Italian Covermount 1.13 builds of *The Nightmare Child*. One x86 DirectInput proxy handles all four. It selects a shell by its file contents, so the Windows display language and the game folder name can be anything. DDrawCompat 0.7.2 supplies the display wrapper.

## Install

Keep the runtime files in the same folder as Psycho.exe, normally C:\Games\Psycho. Do not put them in a nested KPCFix subfolder.

### Files to put beside Psycho.exe

The Windows runtime needs these files in the game folder:

- dinput.dll and KPCFix.ini
- ddraw.dll and DDrawCompat-CLIENT.ini
- Launch-KPCFix.cmd and Setup-KPCFix.ps1
- Check-KPCVersion.ps1, Check-KPCVersion.cmd, and SUPPORTED_BUILDS.json

These do not replace the game's Psycho.exe, client.exe, cshell.dll, d3d.ren, resources, or other regional files. Keep the game's own version-matched files. The source, verification, licenses, and documentation in the full project archive are for review and are not required in the game folder. To use the bundled server launcher, also place Start-KPC-Server-Windows.cmd, Start-KPC-Random-Server.ps1, kpc-lan.cfg, and kpc-lan-maps.txt beside the game files.

### Checking the installed game

From PowerShell, change to the game folder and run:

```powershell
Set-Location 'C:\Games\Psycho'
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\Check-KPCVersion.ps1
```

The policy change lasts only for that PowerShell window. The checker now prints a pass/fail result and the recognized edition when run directly; previously it only defined helper functions, so it appeared to do nothing. Check-KPCVersion.cmd is also included and invokes the check with a one-command execution-policy bypass.

1. Apply the official update for your edition through version **1.13**. Europe, Germany and Italy normally update through 1.12. The US release updates through 1.10. Keep the client, shell, resources and server files from your own installation.
2. Close the game. Back up any existing local `dinput.dll`, `ddraw.dll` and INI files you are replacing. Extract the contents of this ZIP into the folder containing `Psycho.exe`.
3. Run **Launch-KPCFix.cmd** for each game session. It checks the client and shell, applies the 1080p preset on first use, sets Developer=1, then starts the game. On a fresh game profile, use **Display** in the game launcher to enumerate devices. Select **1920×1080** if the launcher or a new profile has chosen a lower mode. Set keyboard bindings yourself in the game controls menu or the game config files.
4. In `KPCFix.log`, look for your shell profile and the line reporting 13 installed HUD sites. Shift+F12 toggles the on-screen flip and present rates. The default is **1920×1080, 60 FPS, VSync on**, with a 150% HUD.

The DLL checks both executable hashes before patching the client. It also checks the shell it actually loads and the instructions at each patch site. An unsupported or modified game pair gets no KPCFix game patches. The DirectInput proxy can still forward Windows input calls.

## Fixes and settings

| Setting | Behaviour |
|---|---|
| Timing and ground contact | Apply the animation fractional carries, timing guard and ground-contact fix when using higher render rates. The default cap is 60. |
| Mouse | The input clock and interval guards improve mouse look and cursor response. InputRate defaults to zero and the obsolete slider is hidden. |
| Stock mouse | Set `StockMouse=1` in `KPCFix.ini` and restart. This restores the original clock, editable Input Rate slider and sensitivity range. The selection in the slider reflects the currently saved value. |
| Sensitivity | In the default mouse mode, `SensitivityRangePercent=10` gives the slider a range of 0.01–2.0. |
| HUD, cursor and tooltips | `[HUD] ScalePercent=150` at 1080p, or 200 at 1440p, keeps approximately the proportions of the 720p HUD. Cursor and tooltip scales follow the HUD by default. The item carousel scales counts around the projected item positions. |
| Direct IP multiplayer | The empty local bind address uses the wildcard address when querying a server from a fresh menu. Explicit addresses keep their original behaviour. `NetTrace=1` enables optional socket diagnostics. |
| VSync and frame cap | In `DDrawCompat-CLIENT.ini`, `VSync = wait(1)` requests VSync. Use `VSync = off` to disable that wait. `FpsLimiter = flipend(60)` caps game flips at 60 independently of desktop refresh. See the INI for exclusive mode and other caps. |

The shipped KPCFix.ini sets ForceDeveloper=1. Before the game starts, the launcher sets Developer=1 in default.cfg, autoexec.cfg and current Profiles/*.cfg files. Set ForceDeveloper=0 to preserve those stored values. KPCFix does not alter keyboard bindings; choose and save them yourself in the game. New profiles keep the game's own keyboard defaults until you change them. Developer and mouse settings are independent.

For another resolution, choose it in the game and set the desired HUD size. To apply a resolution to the existing profiles again, close the game and run the following in PowerShell from the game folder. Supply your own width and height.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Setup-KPCFix.ps1 -Width 2560 -Height 1440
```

## Dedicated server

Run **Start-KPC-Server-Windows.cmd** from a full, patched game installation. It checks the local server files, starts regular deathmatch on UDP **42972** and queues a random first map from **PM01–PM10**. Subsequent maps follow `kpc-lan-maps.txt`. The default is four players, 20 frags, 15 minutes. Edit `kpc-lan.cfg` to change those rules. Use the host's LAN IP to join. If the console remains at `Ready.`, type `start worlds\deathmatch\PM01`.

The US release ships a different dedicated executable and server library from the other editions. Keep them together. The regional comparison and archived game documentation findings are in **REGIONAL-NOTES.md**.

## Rollback

Close the game. To restore the configuration captured by the first KPCFix setup, run the following from the game folder. This restores complete backed-up config files, including their then-current settings. Profiles created later are kept.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Setup-KPCFix.ps1 -Restore
```

The input normalizer also saves the first changed version of each eligible file as `.kpc-inputrate.bak`. You can recover an individual original InputRate from that copy. Remove the local KPCFix and DDrawCompat DLLs and restore any DLLs or INIs you backed up at installation.

## Verification and credits

The release includes the patch source, build instructions, exact supported SHA-256 values and CPU regression results. Native Windows rendering and live server joins with this rebuilt DLL still need a playtest. KPCFix is licensed under MIT. DDrawCompat is narzoul's separate 0BSD project. Its unmodified 0.7.2 DLL and license are supplied here. [DDrawCompat upstream](https://github.com/narzoul/DDrawCompat)
