# Configuration

KPCFix reads `KPCFix.ini` when the game starts, so restart the game after you edit it. Switches take `0` (off) or `1` (on). If a key is missing, KPCFix uses the value shown under **Default**. Only the HUD scale differs: the shipped file sets 150, but a missing key means 100.

Frame rate and display settings are in `DDrawCompat-CLIENT.ini` (see [Frame rate](#frame-rate-and-display)). Server settings are in `KPCServerTools.ini` (see [Server Tools](#server-tools)).

## Common changes

| To | Set in `KPCFix.ini` |
|---|---|
| Size the HUD for 2560×1440 | `[HUD]` `ScalePercent=200` |
| Use the original HUD size | `[HUD]` `ScalePercent=100` |
| Use the original mouse and Input Rate slider | `[KPCFix]` `StockMouse=1` |
| Make movies sharper (no smoothing) | `[AspectRatio]` `CinematicFilter=0` |
| Play movies stretched, as the original did | `[AspectRatio]` `Cinematics4x3=0` |
| Turn off every game fix | `[KPCFix]` `Enabled=0` |

## `[KPCFix]`

| Setting | Default | Effect |
|---|---:|---|
| `Enabled` | 1 | `0` turns off every game fix. The DLL then just passes DirectInput through to Windows. |
| `Log` | 1 | Writes `KPCFix.log` in the game folder. |
| `StockMouse` | 0 | `1` restores the original mouse sensitivity range. It also overrides `InputClock`, `LockInputRate` and the settings that depend on them. |
| `InputClock` | 1 | Smooth mouse input: reads the mouse on a steady clock. |
| `SensitivityRangePercent` | 10 | 1–100. Sets the range of the sensitivity slider: the lowest setting is this value ÷ 1000 and the highest is this value ÷ 5. |
| `LockInputRate` | 1 | Keeps the game's Input Rate at 0, which the smooth mouse needs. |
| `HideInputRate` | 1 | Hides the Input Rate slider while it is locked. |
| `NormalizeInputRateConfigs` | 1 | While the Input Rate is locked, also writes 0 into `autoexec.cfg` and existing profiles. Each file is backed up before its first change. |
| `LocalServerMinStepMs` | 16 | `0`, or 1–50. In single player, the local server never runs a step shorter than this; shorter frames carry over into the next step. Without it some enemies stall above 60 FPS, such as the R2M2A Ballbuster in the garage door. `0` turns it off. |
| `ConsoleKeyFix` | 1 | The console key no longer types its own character into the console (the apostrophe on UK keyboards). |
| `ForceDeveloper` | 1 | `Launch-KPCFix.cmd` sets `Developer=1` in `default.cfg`, `autoexec.cfg` and existing profiles. Starting `Psycho.exe` directly skips this. Works independently of `Enabled` and `StockMouse`. |

## `[HUD]`, `[Cursor]`, `[Tooltip]`

| Section | Setting | Default | Effect |
|---|---|---:|---|
| `HUD` | `ScalePercent` | 150 | HUD size, 50–400. A missing key or `0` means 100. |
| `Cursor` | `ScalePercent` | 0 | Menu cursor size, 50–400. `0` follows the HUD size. |
| `Tooltip` | `ScalePercent` | 0 | Tooltip size, 50–400. `0` follows the HUD size. |

## `[Multiplayer]`

| Setting | Default | Effect |
|---|---:|---|
| `EmptyBindIPFix` | 1 | Lets you join a server straight after starting the game, without loading a local map first. |
| `NetTrace` | 0 | Logs network calls to `KPCFix.log`, for diagnosing connection problems. Needs `Log=1`. |

## `[AspectRatio]`

| Setting | Default | Effect |
|---|---:|---|
| `Cinematics4x3` | 1 | Shows full-screen movies centred at 4:3 instead of stretched. |
| `CinematicFilter` | 1 | Smooths movies while `Cinematics4x3` is on. |

If a sensitivity or scale value is out of range, KPCFix logs an error and doesn't apply its fixes. Turning a setting off later doesn't undo values already saved in the game's config files.

## Frame rate and display

`DDrawCompat-CLIENT.ini` configures DDrawCompat (the bundled `ddraw.dll`). The shipped file runs the game borderless at the desktop resolution and refresh rate, with VSync (`VSync = wait(1)`) and no extra cap (`FpsLimiter = off`). The README's [Frame rate](../README.md#frame-rate) section shows how to cap the rate or turn VSync off. The file's comments explain the other options.

## First-launch setup

`Launch-KPCFix.cmd` runs `Setup-KPCFix.ps1 -OnLaunch`. On the first run it sets the game's display config to 1920×1080 and writes `KPCFix-Display-Preset.applied`, so later launches leave your resolution alone. Every config file it changes is first backed up as `<file>.kpcfix-display.bak` and listed in `KPCFix-Display-Backups.json`. `Setup-KPCFix.ps1 -Restore` puts those backups back.

## Server Tools

`KPCServerTools.ini` sits in the game folder. Setup creates it from `KPCServerTools-default.ini` only if it doesn't exist, so your settings survive upgrades.

| Setting (`[ServerTools]`) | Default | Effect |
|---|---:|---|
| `Enabled` | 1 | `0` runs the original server: no hooks, no chat logging, no cheat control. Restart the server after changing it. |
| `AllowCheats` | 0 | Cheat permission when the server starts. `kpc_cheats 0/1` in the server console changes it live. |
| `ServerFrameMs` | 16 | `0`, or 1–50 (higher values count as 50). Shortest server step in milliseconds. The original server runs a 10 ms step about every 8 ms, so game time runs about 1.25× too fast, and some enemies stall (the R2M2A Ballbuster). `0` keeps the original timing. Works with the Europe, German and Italian `server.dll` only. With the US `server.dll` the original timing stays, and the log says so. |

The launcher writes the **Allow cheats on startup** checkbox to `AllowCheats` before each start. If an older INI lacks `AllowCheats`, the launcher adds it under `[ServerTools]`. It keeps the rest of the file and its line endings as they were, and saves without a UTF-8 byte-order mark: the DLL's INI reader can't see a `[ServerTools]` header on the first line behind a BOM. The shipped default starts with a comment line for the same reason, so keep a line above `[ServerTools]`. Starting `dedicated.exe` directly uses whatever the INI says.
