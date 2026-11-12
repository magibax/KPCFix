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
| KPCFix | LocalServerMinStepMs | 16 | 0 (Timing V3 clock_v2 only) or 1–50: the in-process (single-player/local) server does not simulate a frame shorter than this; its time is carried into the next frame (no time inflation, rendering unaffected). KPCFix's clock_v2 removed the stock 10 ms floor, so above 60 FPS the AI stepped in tiny frames: monster MoveTo stops each think tick while the ground test fails (object.lto `0x97fac`), and the R2M2A Ballbuster stalled in the garage door. 16 fixes it (user-confirmed 2026-11-12). |
| KPCFix | ForceDeveloper | 1 | Setup-KPCFix.ps1 applies Developer=1 to default.cfg, autoexec.cfg and existing profiles. Direct game launches skip this step. Independent of StockMouse and Enabled. |
| HUD | ScalePercent | 150 | Missing key or 0 selects 100. Explicit scale accepts 50–400. |
| Cursor, Tooltip | ScalePercent | 0 | Each section independently inherits the effective HUD scale at 0. Override with 50–400. |
| Multiplayer | EmptyBindIPFix | 1 | Handles an empty local address during direct-IP discovery. |
| Multiplayer | NetTrace | 0 | Requires an open log for socket diagnostics. |
| Multiplayer | CampaignLoadingArt | 1 | Multiplayer only, campaign worlds only (`R<d>M<d>…`): shows the original per-stage loading picture through the shell's own loader (`PreLoadWorld` hook) instead of a black screen. Deathmatch, menus and single player keep stock behaviour. Europe/USA shell only; other editions log and keep stock. See coop/CAMPAIGN-ARCHITECTURE.md §4. |
| Multiplayer | CampaignLoadingMinimumMs | 5000 | 0–30000. The picture stays at least this long (monotonic `GetTickCount`); a slower load adds nothing. |
| Multiplayer | CampaignLoadingHoldSpawn | 1 | EXPERIMENTAL, with CampaignLoadingArt. While the picture is up and its minimum has not passed, the client holds its reply to the server's `0x17` (cshell `0xbeadc` → `0xbf300`, `0xbeae3` → `0xbf400`). The server creates the player only on that reply, so the player enters the world when the picture ends instead of standing in it, attackable, behind the picture. Released on the minimum, when the picture is closed or replaced, or after 30 s. Europe/USA shell only. 0 = stock timing. Each load also writes one `Loading world: … (picture shown / stock: reason)` line to KPCFix.log. |
| Multiplayer | ServerTimeoutSeconds | 15 | 0 = stock 120 s; 5–120. Applies only while the client is in a multiplayer world (shell state 1) on its remote connection; then the native disconnect path runs (message 503 and the menu). Loading, scoreboard and connecting keep 120 s. |
| Multiplayer | KeepAudioOnFocusLoss | 1 | Remote multiplayer only: alt-tab/minimise no longer pauses and later resumes server-owned sounds, which made narration resume late and overlap. Single player, a hosting client and menus keep the stock pause. |
| Multiplayer | ChatMessageSeconds | 8 | 1–60 (stock 3). How long chat and server messages stay on screen. Replaces the 3.0 s default the shell's message handler gives every message without its own time (cshell `0xcb38d`, `MOV [ESP+0x10], 3.0f`). Europe/USA shell only; other editions log and keep 3 s. |
| Multiplayer | ChatBlocksActions | 1 | While you type a chat message, keys bound to actions (noclip, invuln, movement, fire) do nothing. cshell `OnCommandOn` (vtable `0xff724`) skips actions while the message editor types (`shell+0x8c`, byte `+0x12ed`). Europe/USA shell only. |
| Multiplayer | SmoothCutsceneCamera | 1 | Multiplayer scripted cameras: the camera samples the server sends (about 60 per second) are interpolated every frame; cuts and pauses snap. cshell `0xab104`. Europe/USA shell only. |
| KPCFix | ConsoleKeyFix | 1 | The console toggle key (virtual key `0xC0`: backtick on US, apostrophe on UK keyboards) no longer types its character into the console. `client.exe` `0x6dc9`, `0x700a`. |
| Multiplayer | ResyncAudioOnFocus | 1 | EXPERIMENTAL, with KeepAudioOnFocusLoss. 300 ms after the window returns in remote multiplayer, the per-frame sound update (`0x13bcd`) runs the engine's own PauseAll/ResumeAll once. Every sound restarts at its real-time position (duration − remaining), so narration that fell behind while the renderer restarted is back in step with the server. 0 = off. |
| AspectRatio | Cinematics4x3 | 1 | Centres fullscreen movies at 4:3. |
| AspectRatio | CinematicFilter | 1 | Enables movie smoothing when the aspect fix is active. |

Out-of-range active sensitivity or scale values cause client patch setup to report an error. Disabling the runtime lock or setup options leaves previously saved game configuration values in place.

Launch-KPCFix.cmd runs setup with -OnLaunch. A marker limits the display preset to the initial setup on this path. The game and launcher subsequently use the saved resolution. DDrawCompat-CLIENT.ini supplies the presentation defaults, including borderless and VSync wait(1) at the desktop refresh rate (no separate FPS cap; the earlier default added flipend(60)). DDrawCompatOverlay-CLIENT.ini can override matching renderer settings. The distributed renderer INI explains the other wrapper options and their interaction with game settings.

Server setup seeds KPCServerTools.ini from KPCServerTools-default.ini only when the active file is missing. Under [ServerTools], Enabled=1 enables dedicated hooks and AllowCheats=0 starts with permission disabled. Use 0 or 1. The host can change permission live with kpc_cheats. Before starting a server from the GUI, its worker writes the checkbox value to AllowCheats. The checkbox starts unchecked. A direct dedicated-server start reads the saved INI. The launcher writes map and rule choices to kpc-gui-server.cfg.

The graphical launcher requires an `AllowCheats` entry under `[ServerTools]`. If it reports a missing key after an upgrade, add `AllowCheats=0` to that section of the active `KPCServerTools.ini`. Setup preserves existing configurations.

Experimental co-op options are not included in stable Server Tools. On
`feature/coop-campaign`, see the canonical
[experimental co-op configuration reference](coop/CONFIGURATION.md)
for the eight `[Coop]` switches and their fail-closed constraints.
