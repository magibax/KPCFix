# KPCFix

KPCFix brings smooth mouse look, direct IP multiplayer fixes and optional high frame rates to the Windows version of *KISS: Psycho Circus — The Nightmare Child*. It supports the official **1.13** Europe, US, German and Italian Covermount builds.

- **Smooth mouse input.** Turning the camera and moving the menu cursor feel smoother, especially on a high-refresh display. The sensitivity slider gives finer control at low settings. To restore the original mouse behaviour and Input Rate slider, set `StockMouse=1` in `KPCFix.ini` and restart.
- **Multiplayer from the main menu.** Direct IP now lets you query and join a server immediately after launching the game. This fixes the “No Response” that previously meant loading a local map before you could connect.
- **Play above 60 FPS.** Timing and movement fixes address slow-motion animations and excess sliding after releasing a movement key. In playtesting, the game has generally been stable with FPS unlocked, and narration, camera pans and animations have typically kept their expected pace. Some scripted sequences may still be unstable and need more playtesting.
- **Readable HUD at native resolution.** Enemy health bars, their centred name labels, the menu cursor and tooltips follow the HUD scale. The default HUD scale is 150% at 1080p. For 1440p, select 2560×1440 in game and set `ScalePercent=200` under `[HUD]` in `KPCFix.ini`.
- **4:3 cinematics.** Fullscreen movies are centred on widescreen displays. Movie smoothing is enabled by default. The HUD and cursor use point filtering to keep their pixel edges sharp. In the `[AspectRatio]` section of `KPCFix.ini`, set `CinematicFilter=0` for sharper movies or `Cinematics4x3=0` for the original stretched playback.

**TO DO:** 
- Engine performance optimisation: Inspect the engine for opportunities to improve FPS, reduce rendering overhead and optimise performance without introducing instability.
- Anisotropic filtering & antialiasing: Investigate enabling or improving anisotropic filtering and antialiasing, including within the game's menus.
- Graphics configuration: Investigate c_GlobalDetail, GibDetail, LodOffset and other graphics settings to understand their behaviour, performance impact and sensible defaults for modern hardware.
- Graphics fidelity improvements: Investigate engine-level methods to improve rendering quality, texture filtering, model detail, draw distances and other visual effects.

## Install

1. Patch a clean installation of your game to **1.13**, then close the game and launcher. Back up any existing local DLLs or INIs you are replacing.
2. [Download the Windows release ZIP](https://github.com/magibax/KPCFix/releases/download/kpcfix-1.13-r3/KPCFix-Windows-1.13.zip). Extract its contents beside `Psycho.exe`, usually in `C:\Games\Psycho\`.
3. Before launching the game, run `Check-KPCVersion.cmd` from that folder. Continue only if it reports **PASS** and identifies your region as a supported official 1.13 build.
4. **Recommended for the first launch after patching:** run `Launch-KPCFix.cmd`. It applies the initial KPCFix setup and opens the shipped `Psycho.exe` launcher. In the launcher, open **Display** and choose your desired resolution. Start the game and save/select a profile with that same resolution. The initial preset is **1920×1080, 60 FPS, VSync on**.
5. After that first setup, once the desired resolution is saved in both the launcher and your game profile, you can start the game directly with the shipped `Psycho.exe`. Run `Check-KPCVersion.cmd` again after replacing or repatching game files. Keyboard bindings are yours to configure in the game.

## Unlock FPS

Edit these three lines in `DDrawCompat-CLIENT.ini` and restart the game. **Shift+F12** toggles the FPS display.

```ini
FullscreenMode = exclusive(0)
VSync = off
FpsLimiter = off
```

For a 165 FPS cap, use `FpsLimiter = flipend(165)`. If a scripted sequence misbehaves, restore the shipped values (`borderless`, `wait(1)`, and `flipend(60)`) and replay it at 60 FPS.
