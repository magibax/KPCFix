# Configuration

Restart the client after editing KPCFix.ini. Use 0 or 1 for switches. The shipped and missing-key values agree except where shown.

## Common tweaks

| To… | Set in KPCFix.ini |
|---|---|
| Size the HUD for 2560×1440 | `ScalePercent=200` under `[HUD]` |
| Use the original HUD size | `ScalePercent=100` under `[HUD]` |
| Restore the original mouse and Input Rate slider | `StockMouse=1` under `[KPCFix]` |
| Make movies sharper (no smoothing) | `CinematicFilter=0` under `[AspectRatio]` |
| Play movies stretched, as originally | `Cinematics4x3=0` under `[AspectRatio]` |
| Turn off all game fixes | `Enabled=0` under `[KPCFix]` |

Frame-rate settings are in `DDrawCompat-CLIENT.ini`; see the README's Frame rate section.

## All settings

| Section | Setting | Shipped | Effect |
|---|---|---:|---|
| KPCFix | Enabled | 1 | Set to 0 for system DirectInput forwarding without game patches. |
| KPCFix | Log | 1 | Enables KPCFix.log. |
| KPCFix | StockMouse | 0 | 1 restores the stock sensitivity range and overrides InputClock, LockInputRate and their dependent settings. |
| KPCFix | InputClock | 1 | Uses the guarded input interval in KPCFix mouse mode. |
| KPCFix | SensitivityRangePercent | 10 | Accepts 1–100. The slider endpoints equal the percentage divided by 1000 and by 5 respectively. |
| KPCFix | LockInputRate | 1 | Locks the runtime value to zero in KPCFix mouse mode. |
| KPCFix | HideInputRate | 1 | Hides the slider when the lock is active. |
| KPCFix | NormalizeInputRateConfigs | 1 | With the lock active, updates autoexec.cfg and existing profile configurations. First edits create backups. |
| KPCFix | LocalServerMinStepMs | 16 | 0 or 1–50. The single-player (local) server never simulates a frame shorter than this; shorter frames are carried into the next one, so game time stays real time at any frame rate. KPCFix's timing fix removes the engine's 10 ms floor, and without this minimum some AI stalls above 60 FPS, such as the R2M2A Ballbuster in the garage door. 0 keeps Timing V3 alone. |
| KPCFix | ConsoleKeyFix | 1 | The console toggle key no longer types its own character into the console (the apostrophe on UK keyboards). 0 = original behaviour. |
| KPCFix | ForceDeveloper | 1 | Setup-KPCFix.ps1 applies Developer=1 to default.cfg, autoexec.cfg and existing profiles. Direct game launches skip this step. Independent of StockMouse and Enabled. |
| HUD | ScalePercent | 150 | Missing key or 0 selects 100. Explicit scale accepts 50–400. |
| Cursor, Tooltip | ScalePercent | 0 | Each section independently inherits the effective HUD scale at 0. Override with 50–400. |
| Multiplayer | EmptyBindIPFix | 1 | Handles an empty local address during direct-IP discovery. |
| Multiplayer | NetTrace | 0 | Requires an open log for socket diagnostics. |
| AspectRatio | Cinematics4x3 | 1 | Centres fullscreen movies at 4:3. |
| AspectRatio | CinematicFilter | 1 | Enables movie smoothing when the aspect fix is active. |

Out-of-range active sensitivity or scale values cause client patch setup to report an error. Disabling the runtime lock or setup options leaves previously saved game configuration values in place.

Launch-KPCFix.cmd runs setup with -OnLaunch. A marker limits the display preset to the initial setup on this path. The game and launcher subsequently use the saved resolution. DDrawCompat-CLIENT.ini supplies the presentation defaults, including borderless and VSync wait(1) at the desktop refresh rate, with no separate FPS cap (`FpsLimiter = off`; earlier releases added flipend(60)). DDrawCompatOverlay-CLIENT.ini can override matching renderer settings. The distributed renderer INI explains the other wrapper options and their interaction with game settings.

Server setup seeds KPCServerTools.ini from KPCServerTools-default.ini only when the active file is missing. Under [ServerTools], Enabled=1 enables dedicated hooks and AllowCheats=0 starts with permission disabled. Use 0 or 1. The host can change permission live with kpc_cheats. ServerFrameMs (default 16, 0–50, values above 50 use 50) is the minimum server simulation step: the original dedicated server simulates 10 ms about every 8 ms, so game time runs about 1.25× real time and some AI stalls (the R2M2A Ballbuster in the garage door). 0 keeps the original timing. It applies to the Europe, German and Italian `server.dll` (`83b7bce8…`) only. The USA `server.dll` has different code addresses that have not been mapped yet, so it keeps the original timing and the log says so. Before starting a server from the GUI, its worker writes the checkbox value to AllowCheats. The checkbox starts unchecked. A direct dedicated-server start reads the saved INI. The launcher writes map and rule choices to kpc-gui-server.cfg.

The graphical launcher automatically adds `AllowCheats` under `[ServerTools]` if an older active `KPCServerTools.ini` omits it, then updates its value to the selected checkbox state. The launcher preserves the existing text and line-ending style. It saves UTF-8 without a byte-order mark, because the DLL's INI reader cannot see a first-line `[ServerTools]` section behind one. Setup preserves existing configurations. Experimental co-op settings are intentionally absent from the stable build and documented only on the [experimental branch](https://github.com/magibax/KPCFix/blob/coop/mvp-lobby/server-tools/KPCServerTools-default.ini).
