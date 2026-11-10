# Configuration

Restart the client after editing KPCFix.ini. Use 0 or 1 for switches. The shipped and missing-key values agree except where shown.

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
| KPCFix | ForceDeveloper | 1 | Setup-KPCFix.ps1 applies Developer=1 to default.cfg, autoexec.cfg and existing profiles. Direct game launches skip this step. Independent of StockMouse and Enabled. |
| HUD | ScalePercent | 150 | Missing key or 0 selects 100. Explicit scale accepts 50–400. |
| Cursor, Tooltip | ScalePercent | 0 | Each section independently inherits the effective HUD scale at 0. Override with 50–400. |
| Multiplayer | EmptyBindIPFix | 1 | Handles an empty local address during direct-IP discovery. |
| Multiplayer | NetTrace | 0 | Requires an open log for socket diagnostics. |
| AspectRatio | Cinematics4x3 | 1 | Centres fullscreen movies at 4:3. |
| AspectRatio | CinematicFilter | 1 | Enables movie smoothing when the aspect fix is active. |

Out-of-range active sensitivity or scale values cause client patch setup to report an error. Disabling the runtime lock or setup options leaves previously saved game configuration values in place.

Launch-KPCFix.cmd runs setup with -OnLaunch. A marker limits the display preset to the initial setup on this path. The game and launcher subsequently use the saved resolution. DDrawCompat-CLIENT.ini supplies the presentation defaults, including borderless, VSync wait(1), and flipend(60). DDrawCompatOverlay-CLIENT.ini can override matching renderer settings. The distributed renderer INI explains the other wrapper options and their interaction with game settings.

Server setup seeds KPCServerTools.ini from KPCServerTools-default.ini only when the active file is missing. Under [ServerTools], Enabled=1 enables dedicated hooks and AllowCheats=0 starts with permission disabled. Use 0 or 1. The host can change permission live with kpc_cheats. Before starting a server from the GUI, its worker writes the checkbox value to AllowCheats. The checkbox starts unchecked. A direct dedicated-server start reads the saved INI. The launcher writes map and rule choices to kpc-gui-server.cfg.
