# Co-op settings

## Server Tools (`KPCServerTools.ini`)

Server Tools reads `KPCServerTools.ini` when the server starts. It reads the `[Coop]` keys again on every live switch (`!coop on|off`, `!reset`). At startup each setting's value is logged as `[KPCFix Co-op] effective <Key>=<value>`. The **Default** column is the value used when a key is missing. Setup creates the INI from `KPCServerTools-default.ini` only if none exists, so an existing INI keeps its values when you upgrade.

On this branch, `[ServerTools]` has only `Enabled` and `AllowCheats`. The stable build's `ServerFrameMs` is not read here; the co-op equivalent is `[Coop] CoopServerFrameMs` (session only).

Most co-op features apply only in a session (`CoopSession=1`). The launcher's **Co-op campaign** start writes:
- `CoopSession=1`;
- `CoopCampaign=1`;
- `CoopIntro=1`;
- `CoopLobbyWorld=<random PM map>`.

A deathmatch start writes `CoopSession=0`.

### Session and campaign

| Key | Default | Effect |
|---|---:|---|
| `CoopSession` | 0 | Co-op session on (lobby, campaign, all session rules). |
| `CoopCampaign` | 1 | `1` = the 48-world campaign, started by the host with `!campaign`. `0` = the fixed test route R1M2B → R1M2C → R1M3A, started with `!ready` / `!start`. |
| `CoopLobbyWorld` | empty | Lobby world for start, completion and `!reset`. Empty means R1M2B. The launcher always writes a random PM01–PM10. |
| `CoopMinPlayers` | 1 | Players needed before the host can start (1–3). Applies only in the lobby. |
| `CoopMaxPlayers` | 3 | Session capacity (1–3). Keep the server's `maxclients` at or below it. |
| `CoopChatControl` | 1 | Outside a session, any player may type `!coop on` or `!reset`. |
| `CoopIntermission` | 0 | `1` = stock scoreboard intermission between worlds (waits for a click). `0` = change world directly. |
| `CoopSkill` | absent | Difficulty written before each world loads: 0 Lullaby … 4 Nightmare. Absent = unchanged. `!difficulty` sets it live. |
| `CoopTestStartWorld` | empty | Test only: route world that `!start` loads. |

### Intros, timing and loading

| Key | Default | Effect |
|---|---:|---|
| `CoopIntro` | = `CoopCampaign` | Plays each world's `starting_cinematic` once for the session. The campaign needs it: intro scripts also load each level's other scripts (the R1M1A lever). |
| `CoopIntroAll` | = `CoopCampaign` | Plays the intro in every world. `0` limits it to R?M0 and R1M3A. |
| `CoopIntroSettleMs` | 0 (campaign), 3000 (route) | Wait after the last spawn before the intro starts (max 30000). |
| `CoopSyncLoad` | 1 | At a change to a campaign world, the server sends `0x70` (hold) to every member. KPCFix clients then wait behind the loading picture until every member has loaded, and the server sends `0x71` (go), or 60 s pass. `0` = each client enters on its own. |
| `CoopScriptClock` | = `CoopIntro` | Script timers never run ahead of real time. The dedicated server credits each 8 ms frame as 10 ms, which made subtitles outrun narration. |
| `CoopSoundClock` | = `CoopScriptClock` | Sound timers run on real time, so long narration isn't cut short. Needs the Europe-family `server.dll`. |
| `CoopServerFrameMs` | 16 | 0–50. Shortest server step in ms, so game time is real time (the R2M2A Ballbuster). Needs the Europe-family `server.dll`. |
| `CoopStartScoreboard` | 0 (campaign), 1 (route) | Shows the stock scoreboard from the start until the intro. Needs `CoopIntermission=0`. |
| `CoopHostSkip` | 1 | Only the host can skip or continue a cutscene. |

### Players and spawning

| Key | Default | Effect |
|---|---:|---|
| `CoopSinglePlayerStart` | 1 | Players spawn at the world's single-player start, not at its deathmatch starts. |
| `CoopSpawnSpread` | 1 | A player whose start is taken spawns at a checked spot nearby (clear line, floor, body clear of walls). If no spot is found, the start is shared. Needs `server.dll` `83b7bce8…`. |
| `CoopFriendlyFire` | 0 | `0` = players can't hurt each other. |
| `CoopRestartOnWipe` | 0 | When everyone has been dead for 2 s, the world restarts and its intro plays again. Set by the launcher's checkbox. |
| `CoopTeleport` | 1 | Enables `!goto <player>` and `!back`. |
| `CoopCheatKeys` | 1 | With `kpc_cheats 1`, the GimmieGimmieGimmie and NextArmor keys work per player. CyclePlayerClass always works. |

### Items and inventory

| Key | Default | Effect |
|---|---:|---|
| `CoopKeepInventory` | 1 | Each player keeps weapons, armour, shards, ammo and class through world changes. A start from the lobby clears everyone's inventory. |
| `CoopKeepOnDeath` | 1 | Also keep the inventory after dying (needs `CoopKeepInventory`). |
| `CoopSharedItems` | 1 | Weapons, armour, pamphlets, keys and tickets go to everyone. Picked-up ammo amounts are shared. "Secret found" is shown to everyone. |
| `CoopItemRespawnSec` | 15 | 0–600. Taken weapons, ammo, shards, health and power-ups return after this time, once nobody stands on the spot. Armour pieces, pamphlets and keys never respawn. |
| `CoopDropLifetimeSec` | 0 | 0–600. Seconds before a weapon dropped at death disappears. `0` = it stays. Set by the launcher. |
| `CoopPowerupResend` | 1 | Re-sends the armour power-up effect to the players watching. |

### World and script behaviour

| Key | Default | Effect |
|---|---:|---|
| `CoopScriptEvents` | 1 | Runs the single-player script events that stock multiplayer skips (`player …` commands for every player, `disable_ai`, `wake_monster_group` and others). |
| `CoopHostSequences` | 1 | During play, only the host can start a level-ending trigger sequence (the R1M1A victory). Needs `CoopScriptEvents`. |
| `CoopSpawners` | 1 | Creates the map's monster spawners, which stock multiplayer never creates. |
| `CoopSpawnerNearest` | 1 | Spawners measure their activation radius from the nearest player, not the first one who joined. |
| `CoopHealthBars` | 1 | Enemy health bars, sent 10 times a second. |
| `CoopWindowShards` | 1 | A broken window or crate stays 200 ms after its break message, so every client draws its shards. |
| `CoopMusic` | 1 | Each player gets the world's current music state (song, transition, intensity) 400 ms after spawning. |
| `CoopMusicQuietLoad` | 0 | `1` = skip the world-start music broadcast entirely (silent loads). |

### Diagnostics

All default to 0. Turn them on only to collect logs for a bug report. The launcher's **Test logging** box sets `TraceScripts` and `TraceTiming`.

| Key | Logs |
|---|---|
| `TraceScripts` | Script starts (switches, triggers, script objects), refused scripts, switches reaching "on". |
| `TraceTiming` | Adds `t=<ms since the session started>` to co-op log lines. |
| `TraceMusic` | Each world's music event creation. |
| `TraceSpawners` | The monster spawner filter and per-world counts. |
| `TraceCampaign` | Intro lookups and the exit handler. |
| `TraceSpawns`, `TrialSeparateSpawns`, `TraceCollisionABI`, `TracePlayerClearance`, `TraceBspVolume`, `TraceModelRegistry`, `TraceModelState` | Research diagnostics from the spawn and collision work (R1M2B only). Not needed for normal testing. See [ENGINE-NOTES.md](ENGINE-NOTES.md). |

## Client (`KPCFix.ini`)

These keys come on top of the stable client settings in [docs/CONFIGURATION.md](../docs/CONFIGURATION.md). They are all in `[Multiplayer]` except where noted, and switches default to 1. Most of them work only with the Europe/USA shell. With another shell they log "not verified" and stay stock.

**Loading pictures and synchronised loading**

| Key | Default | Effect |
|---|---:|---|
| `CampaignLoadingArt` | 1 | Shows the original loading picture while a campaign world loads. |
| `CampaignLoadingMinimumMs` | 5000 | Minimum time the picture stays up (0–30000). |
| `CampaignLoadingHoldSpawn` | 1 | Keeps the player out of the world until the picture closes. |
| `CampaignLoadingSync` | 1 | Follows the server's `0x70` hold and `0x71` go, so everyone enters together. Sounds are paused while waiting. |
| `LevelChangePicture` | 1 | Shows the next world's picture as soon as a level change starts. |
| `IntroPictureHold` | 1 | Keeps an R?M0 world's picture until the server says the intro started (`0x74`, at most 20 s). |
| `[KPCFix] SinglePlayerLoadingMs` | 5000 | Single player: minimum loading-picture time (0–30000, 0 = stock). |
| `[AspectRatio] LoadingPictureFilter` | 1 | Smooth scaling for loading pictures. They are 4:3 when `Cinematics4x3=1`. |

**Music and sound**

| Key | Default | Effect |
|---|---:|---|
| `CleanMusic` | 1 | Around a load, the old song stops at a bar line and the new world's music starts when the world appears. |
| `KeepMusicOnDeath` | 1 | Dying no longer drops the music to its calm level (the R1M4C boss music). |
| `StopSoundsOnLevelChange` | 1 | Voice lines and sounds stop at a level change. |

**Window focus**

| Key | Default | Effect |
|---|---:|---|
| `KeepAudioOnFocusLoss` | 1 | Alt+Tab and minimise don't pause sound, music or a single-player game. Also switchable in Setup > Audio, or with the console command `KeepAudioOnFocusLoss 0/1`. |
| `ResyncAudioOnFocus` | 1 | On return, kept sounds jump to where they would be in real time. |
| `KeepRendererOnFocusLoss` | 1 | In a world, the renderer keeps running while focus is lost. Input is released. |
| `KeepRendererEverywhere` | 1 | Keeps the renderer in movies, menus and loading screens too. Use `0` if a movie or menu stays black after Alt+Tab. |
| `BackgroundFullSpeed` | 1 | Opts out of Windows background throttling and uses a 1 ms timer. |
| `NoWindowAnimation` | 1 | No minimise or restore animation, so drawing resumes sooner. |

**Chat**

| Key | Default | Effect |
|---|---:|---|
| `ChatMessageSeconds` | 8 | How long messages stay on screen (1–60; stock 3). |
| `ChatBlocksActions` | 1 | While you type, action keys do nothing, so letters can't toggle cheats. |
| `ChatClipboard` | 1 | Ctrl+A, Ctrl+C, Ctrl+X, Ctrl+V. |
| `ChatScrollback` | 1 | Page Up and Page Down show the last 64 messages. |
| `ChatHistory` | 1 | Up and Down recall lines you sent. |
| `ChatLowercaseFont` | 1 | Chat bar in the small Arial font with real lowercase letters. |
| `ChatHideOnLoading` | 1 | Hides chat while a loading picture shows. |
| `ChatInCinematics` | 1 | Chat bar usable in cutscenes. Space typed into it doesn't skip the cutscene. |
| `ChatLongMessages` | 1 | Lines up to 200 characters (needs a Server Tools server to relay them). Incoming lines are capped, and `##` is broken up, because the stock client can crash on it. |
| `SinglePlayerChat` | 1 | Chat bar in single player, with local commands `!gimmie`, `!nextarmor`, `!prevarmor`, `!class`, `!kill`, `!noclip`, `!help`. |

**Multiplayer**

| Key | Default | Effect |
|---|---:|---|
| `MinUpdateRate` | 60 | Object updates per second asked from the server (2–60; stock 10). |
| `ServerTimeoutSeconds` | 15 | Report a lost server after this long without packets (5–120; 0 = stock 120). |
| `MenuDisconnect` | 1 | In the main menu, Quit first disconnects from the server. |
| `ScoreboardHold` | 1 | The scoreboard shows while its key is held. |
| `SmoothCutsceneCamera` | 1 | Smooths the cutscene camera between server updates. |
| `HideBrokenWindow` | 1 | Hides a broken window as soon as its shards appear. |
| `SpawnLightning` | 0 | `1` = the stock lightning bolt at respawns. |
| `LogWindowBreaks` | 1 | Logs the first window breaks of a session to `KPCFix.log` (diagnostic). |

`DDrawCompat-CLIENT.ini` on this branch also sets `AltTabFix = noactivateapp`. It keeps video memory across Alt+Tab and skips DirectDraw's slow (400–700 ms) handling of window activation. The fallback is `AltTabFix = keepvidmem`.
