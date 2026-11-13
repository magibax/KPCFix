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
| KPCFix | LocalServerMinStepMs | 16 | 0 or 1–50. The single-player (local) server never simulates a frame shorter than this; shorter frames are carried into the next one, so game time stays real time at any frame rate. KPCFix's timing fix removes the engine's 10 ms floor, and without this minimum some AI stalls above 60 FPS, such as the R2M2A Ballbuster in the garage door. |
| KPCFix | ForceDeveloper | 1 | Setup-KPCFix.ps1 applies Developer=1 to default.cfg, autoexec.cfg and existing profiles. Direct game launches skip this step. Independent of StockMouse and Enabled. |
| HUD | ScalePercent | 150 | Missing key or 0 selects 100. Explicit scale accepts 50–400. |
| Cursor, Tooltip | ScalePercent | 0 | Each section independently inherits the effective HUD scale at 0. Override with 50–400. |
| Multiplayer | EmptyBindIPFix | 1 | Handles an empty local address during direct-IP discovery. |
| Multiplayer | NetTrace | 0 | Requires an open log for socket diagnostics. |
| Multiplayer | CampaignLoadingArt | 1 | Multiplayer only, campaign worlds only (`R<d>M<d>…`): shows the original per-stage loading picture through the shell's own loader (`PreLoadWorld` hook) instead of a black screen. Deathmatch, menus and single player keep stock behaviour. Europe/USA shell only; other editions log and keep stock. See coop/CAMPAIGN-ARCHITECTURE.md §4. |
| Multiplayer | CampaignLoadingMinimumMs | 5000 | 0–30000. The picture stays at least this long (monotonic `GetTickCount`); a slower load adds nothing. |
| Multiplayer | CampaignLoadingHoldSpawn | 1 | EXPERIMENTAL, with CampaignLoadingArt. While the picture is up and its minimum has not passed, the client holds its reply to the server's `0x17` (cshell `0xbeadc` → `0xbf300`, `0xbeae3` → `0xbf400`). The server creates the player only on that reply, so the player enters the world when the picture ends instead of standing in it, attackable, behind the picture. Released on the minimum, when the picture is closed or replaced, or after 30 s. Europe/USA shell only. 0 = stock timing. Each load also writes one `Loading world: … (picture shown / stock: reason)` line to KPCFix.log. |
| Multiplayer | ServerTimeoutSeconds | 15 | 0 = stock 120 s; 5–120. Applies only while the client is in a multiplayer world (shell state 1) on its remote connection; then the native disconnect path runs (message 503 and the menu). Loading, scoreboard and connecting keep 120 s. |
| Multiplayer | KeepAudioOnFocusLoss | 1 | Default of the console variable `KeepAudioOnFocusLoss`, also switched by Setup > Audio > "Keep Running When Minimised". When on, alt-tab/minimise pauses nothing in any mode: the sound pause/resume (`0x6d5c`/`0x6cdb`), the music pause/resume of the render shutdown (cshell `0xc80e2`/`0xc80ba`) and the local server's pause (active-window test `0x157c5`) are skipped, so narration, scripts and music stay in step. With `KeepRendererOnFocusLoss=1` the renderer and client world keep running too. 0 = stock pause (the checkbox can still turn it on). The checkbox is Europe/USA shell only. |
| Multiplayer | KeepRendererOnFocusLoss | 1 | With KeepAudioOnFocusLoss on: alt-tab/minimise no longer shuts the renderer down, so the game draws and animates exactly as when focused. The focus-loss `r_TermRender` (`0x6d1d`) and its restart `r_InitRender` (`0x6c97`) are skipped as a pair, and the client world's window tests (object animation `0x158e3`, object update `0x15922`) report active while the renderer is kept. Keyboard and mouse are still released: the input poll (`0x1374c`, `0x137fc`), mouse centring (`0x696c`) and the input reset on deactivate (`0x13a20`) are stock. A fullscreen surface lost anyway is restored by `d3d.ren` on its next flip (`0x1d542`). 0 = stock shutdown (try it if the picture is broken after returning). |
| Multiplayer | ChatMessageSeconds | 8 | 1–60 (stock 3). How long chat and server messages stay on screen. Replaces the 3.0 s default the shell's message handler gives every message without its own time (cshell `0xcb38d`, `MOV [ESP+0x10], 3.0f`). Europe/USA shell only; other editions log and keep 3 s. |
| Multiplayer | ChatBlocksActions | 1 | While you type a chat message, keys bound to actions (noclip, invuln, movement, fire) do nothing. cshell `OnCommandOn` (vtable `0xff724`) skips actions while the message editor types (`shell+0x8c`, byte `+0x12ed`), and client.exe's command update reads input as disabled (`0x23c84`, `[cm+0x1014]`), so no command bits reach the server either (Server Tools reads Invuln through `IsCommandOn`). Europe/USA shell only. |
| Multiplayer | LogWindowBreaks | 1 | Logs the first 32 window breaks of a session and whether the window object reached this client (the shard manager's handler, cshell `0xfe184` → `0x5bd30`). Diagnosis for missing shards in multiplayer. 0 = off. |
| Multiplayer | HideBrokenWindow | 1 | Right after the client builds a broken window's shards (shard handler `0x5bd30`), the window object is hidden on this client (client API `GetObjectFlags`/`SetObjectFlags` `[cshell+0x1c8dfc]` `+0x290`/`+0x294`, flags `& ~1`), so no window stays standing among its own shards while the server's hide arrives. Europe/USA shell only. 0 = stock. |
| Multiplayer | SmoothCutsceneCamera | 1 | Multiplayer scripted cameras: the camera samples the server sends (about 60 per second) are interpolated every frame; cuts and pauses snap. cshell `0xab104`. Europe/USA shell only. |
| KPCFix | ConsoleKeyFix | 1 | The console toggle key (virtual key `0xC0`: backtick on US, apostrophe on UK keyboards) no longer types its character into the console. `client.exe` `0x6dc9`, `0x700a`. |
| Multiplayer | ResyncAudioOnFocus | 1 | EXPERIMENTAL, with KeepAudioOnFocusLoss. 300 ms after the window returns (after a skipped pause), the per-frame sound update (`0x13bcd`) runs the engine's own PauseAll/ResumeAll once. Every sound restarts at its real-time position (duration − remaining), so narration that fell behind while the renderer restarted is back in step with the server. 0 = off. |
| AspectRatio | Cinematics4x3 | 1 | Centres fullscreen movies at 4:3. |
| AspectRatio | CinematicFilter | 1 | Enables movie smoothing when the aspect fix is active. |

Out-of-range active sensitivity or scale values cause client patch setup to report an error. Disabling the runtime lock or setup options leaves previously saved game configuration values in place.

Launch-KPCFix.cmd runs setup with -OnLaunch. A marker limits the display preset to the initial setup on this path. The game and launcher subsequently use the saved resolution. DDrawCompat-CLIENT.ini supplies the presentation defaults, including borderless and VSync wait(1) at the desktop refresh rate, with no separate FPS cap (`FpsLimiter = off`; earlier releases added flipend(60)). DDrawCompatOverlay-CLIENT.ini can override matching renderer settings. The distributed renderer INI explains the other wrapper options and their interaction with game settings.

Server setup seeds KPCServerTools.ini from KPCServerTools-default.ini only when the active file is missing. Under [ServerTools], Enabled=1 enables dedicated hooks and AllowCheats=0 starts with permission disabled. Use 0 or 1. The host can change permission live with kpc_cheats. ServerFrameMs (default 16, 0–50, values above 50 use 50) is the minimum server simulation step: the original dedicated server simulates 10 ms about every 8 ms, so game time runs about 1.25× real time and some AI stalls (the R2M2A Ballbuster in the garage door). 0 keeps the original timing. It applies to the Europe, German and Italian `server.dll` (`83b7bce8…`) only. The USA `server.dll` has different code addresses that have not been mapped yet, so it keeps the original timing and the log says so. Before starting a server from the GUI, its worker writes the checkbox value to AllowCheats. The checkbox starts unchecked. A direct dedicated-server start reads the saved INI. The launcher writes map and rule choices to kpc-gui-server.cfg.

The graphical launcher automatically adds `AllowCheats` under `[ServerTools]` if an older active `KPCServerTools.ini` omits it, then updates its value to the selected checkbox state. The launcher preserves the existing text and line-ending style. It saves UTF-8 without a byte-order mark, because the DLL's INI reader cannot see a first-line `[ServerTools]` section behind one. Setup preserves existing configurations. Experimental co-op settings are intentionally absent from the stable build and documented only on the [experimental branch](https://github.com/magibax/KPCFix/blob/coop/mvp-lobby/server-tools/KPCServerTools-default.ini).

Experimental co-op options are not included in stable Server Tools. On
`coop/mvp-lobby`, see the canonical
[experimental co-op configuration reference](coop/CONFIGURATION.md)
for the eight `[Coop]` switches and their fail-closed constraints.
