# KPCFix

KPCFix brings smooth mouse look, direct IP multiplayer fixes and optional high frame rates to the Windows version of *KISS: Psycho Circus — The Nightmare Child*. It supports the official **1.13** Europe, US, German and Italian Covermount builds.

- **Smooth mouse input.** Turning the camera and moving the menu cursor feel smoother, especially on a high-refresh display. The sensitivity slider gives finer control at low settings. To restore the original mouse behaviour and Input Rate slider, set `StockMouse=1` in `KPCFix.ini` and restart.
- **Multiplayer from the main menu.** Direct IP now lets you query and join a server immediately after launching the game. This fixes the “No Response” that previously meant loading a local map before you could connect.
- **Play above 60 FPS.** Timing and movement fixes address slow-motion animations and excess sliding after releasing a movement key. In playtesting, the game has generally been stable with FPS unlocked, and narration, camera pans and animations have typically kept their expected pace. Some scripted sequences may still be unstable and need more playtesting.
- **Readable HUD at native resolution.** The HUD, menu cursor and tooltips scale together. The default HUD scale is 150% at 1080p. For 1440p, select 2560×1440 in game and set `ScalePercent=200` under `[HUD]` in `KPCFix.ini`.

**TO DO:** 
- Graphics configuration: Investigate c_GlobalDetail, GibDetail, LodOffset and other graphics settings to understand their behaviour, performance impact and sensible defaults for modern hardware.
- Anisotropic filtering & antialiasing: Investigate enabling or improving anisotropic filtering and antialiasing, including within the game's menus.
- Enemy health bar scaling: Fix enemy health bars not respecting the HUD ScalePercent setting.
- Cinematic aspect ratio: Restore the intended 4:3 aspect ratio of cinematics when displayed on modern widescreen monitors.
- Engine performance optimisation: Inspect the engine for opportunities to improve FPS, reduce rendering overhead and optimise performance without introducing instability.
- Graphics fidelity improvements: Investigate engine-level methods to improve rendering quality, texture filtering, model detail, draw distances and other visual effects.
- Modern hardware defaults: Establish recommended graphics settings for modern PCs, prioritising maximum visual fidelity while maintaining stability and compatibility.

## Install

1. Update your edition of the game to **1.13** and close it. Back up any existing local DLLs or INIs you are replacing.
2. [Download the Windows release ZIP](https://github.com/magibax/KPCFix/releases/download/regional-1.13-r1/KPCFix-1.13-Regional-Windows.zip). Extract its contents beside `Psycho.exe`, usually in `C:\Games\Psycho\`.
3. Run `Launch-KPCFix.cmd`. On a fresh install, click **Display** in the game launcher to enumerate devices. The default is **1920×1080, 60 FPS, VSync on**. Keyboard bindings are yours to configure in the game.

## Unlock FPS

Edit these three lines in `DDrawCompat-CLIENT.ini` and restart the game. **Shift+F12** toggles the FPS display.

```ini
FullscreenMode = exclusive(0)
VSync = off
FpsLimiter = off
```

For a 165 FPS cap, use `FpsLimiter = flipend(165)`. If a scripted sequence misbehaves, restore the shipped values (`borderless`, `wait(1)`, and `flipend(60)`) and replay it at 60 FPS.

## Host a LAN game

Run `Start-KPC-Server-Windows.cmd` from your patched game folder. It starts a four-player deathmatch on a random map from PM01–PM10. Players join using the host’s LAN IP and UDP port **42972**. Edit `kpc-lan.cfg` to change the rules.

See the [regional compatibility notes](REGIONAL-NOTES.md) for build details. KPCFix uses the [MIT license](LICENSE). The Windows release includes [DDrawCompat 0.7.2](https://github.com/narzoul/DDrawCompat) under its [0BSD license](LICENSES/DDrawCompat-0BSD.txt).
