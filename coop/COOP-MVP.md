# KPCFix co-op campaign: how it works

Status: **experimental**, on the `coop/mvp-lobby` branch only. It runs the original
*KISS Psycho Circus* 1.13 single-player campaign on the dedicated server for 1–3
players. Every feature below is built and tested with unit tests and emulation against
the original binaries. A feature that also has a **live** marker has been seen working
in a real two-PC game. The detailed reverse-engineering evidence of earlier sessions is
kept in [archive/COOP-MVP-EVIDENCE-2026-11-12.md](archive/COOP-MVP-EVIDENCE-2026-11-12.md).

* All settings: [CONFIGURATION.md](CONFIGURATION.md).
* Campaign map data: [CAMPAIGN-MAP-INVENTORY.md](CAMPAIGN-MAP-INVENTORY.md).
* Design of the unfinished parts: [CAMPAIGN-ARCHITECTURE.md](CAMPAIGN-ARCHITECTURE.md).
* Current test status and next steps: [NEXT-SESSION-HANDOFF.md](NEXT-SESSION-HANDOFF.md).

Addresses below are RVAs in the original files:
* `object.lto` `3a03f879…` (base `0x10000000`);
* Europe-family `server.dll` `83b7bce8…`;
* `client.exe` `16131ccf…` (fixed base `0x1ca0000`);
* Europe/USA `cshell.dll` `703813a4…` (base `0x20000000`).

Nothing on disk is changed; all patches are applied in memory after checking the
original bytes, and fail closed when the bytes differ.

## Playing

1. **Server:** start the server tools launcher with the co-op checkbox, or run
   `Start-KPC-Selected-Server.ps1 -Map random -Cheats Enabled -Coop`.
   `-Coop` executes `kpc-coop-server.cfg` (your GUI config plus the co-op map list
   `kpc-coop-maps.txt` and no start armour) and sets `CoopSession=1`. Set
   `CoopCampaign=1` and `CoopIntro=1` in `KPCServerTools.ini [Coop]` for the
   campaign.
2. **Clients:** install the KPCFix client (`dinput.dll`, `KPCFix.ini`) on every PC and
   join the server from the multiplayer browser. Everyone arrives in the lobby, a
   deathmatch map (PM01–PM10). One player can start alone (`CoopMinPlayers=1`).
3. **Host** (the earliest player still present, shown with `*` in `!coop`):
   `!difficulty …` if wanted, then `!campaign full` (or a realm). Everyone loads R1M0
   at once.

### Chat commands

| Command | Who | Effect |
| --- | --- | --- |
| `!campaign full` | host, lobby | Starts the whole game from R1M0 now (no `!ready` needed; `CoopMinPlayers` applies) |
| `!campaign water\|earth\|air\|fire\|nightmare` (or `1`–`5`) | host, lobby | Starts at that realm's first world now and carries on to the end of the game. `from <realm>` is an alias |
| `!campaign` | anyone | Shows the selection and help. Any other input gets the help reply |
| `!difficulty lullaby\|daydream\|sleepless\|coldsweat\|nightmare` (or `0`–`4`, `easy`/`normal`/`hard`) | host, lobby | Difficulty from the next world loaded. `!difficulty` alone shows it. Alias `!skill` |
| `!abort` | host, any state | Resets everything back to the lobby: everyone goes back to a lobby deathmatch map (PM01–PM10, never the one loaded now; a campaign world is never the lobby), the campaign flags are reset (no scripted sequences in the lobby), readiness is cleared, the selection returns to full. During a world change it waits for that world to load. In the lobby it resets in place |
| `!ready` / `!unready`, `!start` | members / host | The older start: everyone ready, host starts the current selection |
| `!next` | host, in play | Forces the next world (debugging; exits are host-only anyway) |
| `!changelevel <world>` | host, lobby or play (campaign mode) | Everyone goes to that WORLDORDER world (`R1M2A` or `worlds\realm1\R1M2A`, any case), and the campaign carries on from there (natural exits lead to its successor). Unknown names get the format; others get "Only the host" |
| `!goto <player>` / `!back` | anyone, lobby or play | Moves you next to that player (whole name or a unique prefix); `!back` returns to where you were. 3 s cooldown (`CoopTeleport`) |
| `!kill` | anyone | Kills the player who typed it (normal death and respawn) |
| `!class` / `!class starbearer\|beastking\|celestial\|demon` | anyone | Next or named player class; weapons, armour and ammo are kept, and the new class's melee / class weapon is added where the player had one |
| `!all` | anyone, `kpc_cheats 1` | Every armour piece, every weapon, full ammo, for the typer only |
| `!gimmie`, `!nextarmor` | anyone, `kpc_cheats 1` | 50 of each ammo + the next missing weapon / the next missing armour piece, for the typer only |
| `!coop` / `!status`, `!help`, `!leave` / `!join` | anyone / member | Status line, command list, sit out / rejoin |

The stock cheat **keys** GimmieGimmieGimmie and NextArmor (bind them in Setup >
Controls) also work per player with `kpc_cheats 1`, CyclePlayerClass without it
(`CoopCheatKeys`). GimmieGimmieGimmie gives 200 of each ammo up to the stock cap,
100 + 100 per armour piece (`f3440`), as in single player.

The dedicated server's own console also takes `changelevel <world>` (campaign mode;
no host check and no player minimum).

Commands are read from the game's own chat relay (object `0xfeec4`) and answered with
its broadcast; no new network messages. Replies are at most 111 bytes.

## Session and campaign

* **Phases:** `LOBBY` → `TRANSITION` → `LOADING` → `INTRO_PENDING` → `INTRO` /
  `PLAYING` → `TRANSITION` … The map **epoch** increases on every native NextLevel.
* **Campaign graph** (`CoopCampaign=1`): the 48 worlds of the original
  `WORLDORDER.TXT` (`coop_campaign.h`, checked against the user's REZ). A selection is a
  slice from a realm's first world to the end. Worlds missing from the native
  50-entry maplist are written into its next slot in memory.
* **World changes** use the native pending-NextLevel flag (`+0x1ae9`, consumed by the
  server's own Update) without the intermission scoreboard.
* **Exits** (`0x2d455`): a natural exit (mirror, exit volume) must lead to the next
  world of the route and, during play, must be taken by **the host**; any other player
  gets "Only the host (*) can take the exit". Exits fired by a script or object (the
  R?M0 intros end with `trigger_generic CExitTrigger0`) pass. Leaving the last world of
  the selection completes it and returns to the lobby. **Live:** natural exits,
  scripted intro exits.
* **Lobby world:** a random deathmatch map (PM01–PM10) picked by the launcher on every co-op start and written to `CoopLobbyWorld`; players spawn at its own multiplayer starts. The DLL default without the key is R1M2B.
* **Membership:** keyed by the engine's client handle; disconnects are detected from
  the engine's player list; an empty session returns to a new lobby.

## Arrival in a world

* **Single-player start** (`CoopSinglePlayerStart=1`): the multiplayer start
  strategy (`0xf12be` → `f1070`) returns none, so the engine's own fallback picks the
  single-player start; the second spawn path runs its lookup (`0xf15f2` → `f08a0`)
  with game type 0. **Live:** R1M1A starts at `PlayerStart0`.
* **No telefrag** between players (`0xf13a4`, `0xf167a` skip `f10c0`).
* **Spawn spread** (`CoopSpawnSpread=1`): the first player keeps the start; a player
  whose start is taken this world is moved to the first candidate of the allocator
  grid (`coop_allocator.h`, 96-unit steps) with a clear line from the start, a floor
  at the start's height (±16) and a player box (±20, ±46, ±20) in interior BSP leaves
  only (`IntersectSegment` `g_pServerDE+0x90`, `TraceBspVolume` classifier;
  `server.dll 83b7bce8` only). Unknown fails closed: the start is shared. Moving
  world models and live actors are not checked. R1M2B keeps its three tested
  positions.
* **Waiting player placed:** the client-enter handler only attaches the object in
  multiplayer (`0xfdfdb`); it is now also moved to the single-player start at once,
  so a player whose spawn is held behind the loading picture stands on the floor
  instead of falling (the old death sound). The real spawn still happens on the
  client's `0x17` reply.
* **Loading picture** (client `CampaignLoadingArt`): the original per-stage picture
  through the shell's `PreLoadWorld` (`0xcd360`, vtable slot `0xff73c`) for at least
  `CampaignLoadingMinimumMs` (5 s). **Live:** shown on the desktop.
* **Spawn hold** (client `CampaignLoadingHoldSpawn=1`): the client holds its reply to
  the server's `0x17` (cshell `0xbeadc` → `0xbf300`, `0xbeae3` → `0xbf400`) while the
  picture is under its minimum. The server creates the player only on that reply
  (`0xfec56` → `f12a0`), so nobody stands in the world behind the picture. Released
  every frame from the client update (`client.exe 0x13876`) and the screen draw; only
  after the connection has entered a world (the first load from the browser is never
  held; new connection = the "Got ID packet" print `0x6986e`). **Live:** players enter
  alive after the picture.
* **Intros** (`CoopIntro=1`, `CoopIntroAll` = campaign): each world's original
  `starting_cinematic` (lookup at `0x1045b1`) starts once every present member has
  spawned (`CoopIntroSettleMs`, campaign default 0) and plays on the server for
  everyone. The intro scripts also preload the level's other scripts (`scriptload`;
  R1M1A's intro loads the lever's `r1m1a_bigdoor.scr` and the gauntlets'
  `r1m1a_gauntlets.scr`). **Live:** R1M0, R1M1A, R1M3A.
* **Music** (`CoopMusic=1`): each player gets the world's music start once after it
  spawns (the stock one-time broadcast happens before anyone is in). **Live.**

## Running the single-player campaign logic

* **Script events** (`CoopScriptEvents=1`): five cinematic events return at a
  multiplayer gate (the load `mov reg,[shell+0x1d0]`): `player …` (`0xc1a89`; 1040
  lines in the campaign scripts), `disable_ai` (`0xc28a5`; 208), `wake_monster_group`
  (`0xc269b`; 17), disable targetting (`0xc2a75`) and `cheatscript` (`0xb8ad8`; still
  needs the `CheatScripts` cvar). In a session they run their single-player code. The
  `player` command (`0xc1abe` → `f4da0`) goes:
  * to **every player**: `giveitem`, `invulnerable`, `sethealth`, `class`;
  * to **the activator** only: `teleport`, `lookat`, `lookatangle(s)`, `powerup`,
    `animate`, `alignbody`. The activator is the player who last set off a trigger
    (recorded at the IsPlayer tests `0x2e205`, `0x2ffea`, `0x3005a`), else the first
    player.

  **Live:** the R1M1A gauntlets sequence and the lever (door, camera, Headless); before
  the activator rule the other player was teleported too.
* **Cutscene cameras** (`camera_path`, letterbox, input lock) are sent by the server to
  every client (`c58c0`, message `0x3d`), so every player sees every scripted camera.
  With `invulnerable true` going to everyone, all players are protected while one plays.
* **Monster spawners** (`CoopSpawners=1`): stock multiplayer forces them off
  (`0x93c5d`). **Live:** R1M3A `spawn6`. A spawner only spawns while its reference
  object is within `ActiveRadius`; activation makes that the first player in the list
  (`d54c8`), so a room set off by another player stayed empty (R4M1A). With
  `CoopSpawnerNearest=1` the per-frame position read (`0xd51fb`) uses the nearest live
  player instead.
* **Enemy health bars** (`CoopHealthBars=1`): the single-player bar manager
  (gate `0xf8fe0`), broadcast at most 10 Hz. **Live.**
* **Difficulty** (`!difficulty`, or `CoopSkill`): written to `0x15b678` before each
  world load.

## Shared progress

* **Items** (`CoopSharedItems=1`): every pickup goes through `cc120` → `cc180(item,
  player)` (`0xcc145`). After the toucher's own pickup, a progression item (item id
  from `item+0x54`, table `0x141e40`) is given to every other player with the engine's
  own give call `f3440(player, id, 1, 1)`: weapons (ids 3–14, UltimateWeapon `0x23`),
  armour (`0x27`–`0x2c`), Pamphlet `0x19`, Key `0x24`, Ticket `0x25`. Health, ammo,
  charms and other power-ups stay with whoever takes them. Once per item object per
  world.
* **Start armour:** off in co-op (`kpc-coop-server.cfg`), because an armour item hides
  itself in multiplayer when the server hands that piece out at spawn (`0xc8520`).
  **Live:** the R1M1A gauntlets are in the bar.
* **Secrets:** the found counter is one world counter (`0x1015b690`), already shared;
  the "secret found" message (`0x30c9e` → `f8d90`) now goes to every player.
* **Friendly fire** (`CoopFriendlyFire=0`): the player damage handler `f3bc0` skips
  damage from another player (hook on its teamplay test `0xf3c08`, jump to `f3f1e`).
  Self-damage and monster damage are unchanged. A co-op death would otherwise cost the
  victim every weapon and armour piece.

## Personal inventory, respawning pickups, cheats

Built 2026-11-12 (`7dbba5eb`), emulator and native-smoke tested, not yet two-PC tested.

* **Persistence** (`CoopKeepInventory=1`, `CoopKeepOnDeath=1`): each member's weapons
  (2–14), armour pieces (`player+0x260` bits), BlackDiamonds (`0x1e`), ammo (`0x11`–`0x14`)
  and class (`player+0x264`) are saved every second while alive and right before every
  world change, keyed by the member (`coop_inventory.h`). 400 ms after a spawn the
  class is applied and whatever is missing is given with `f3440` (armour first: it
  raises the ammo cap; weapons need the gauntlets and the player's class, so a weapon
  from another class is given with the class set to it for the call). A start from
  the lobby forgets every snapshot (fresh run); `!abort` and changes along the
  campaign keep them. Ammo is kept exactly: the stock ammo a restored weapon brings
  with it is removed again (two-PC RT3–RT5).
* **Window shards** (`CoopWindowShards=1`): the window's hide is held 200 ms after its
  break message, so remote clients still have the window when they build the shards.
  KPCFix clients hide it as soon as the shards are built (`HideBrokenWindow`).
* **Respawning pickups** (`CoopItemRespawnSec=15`): campaign worlds author
  `Respawn=false`. At the pickup's hide call (`0xcc29e`) a weapon, ammo, BlackDiamond,
  armour piece, health or power-up gets the stock deathmatch respawn (`+0xb4`, `+0xb8`);
  the respawn (`0xcbfff` → `cc080`) waits while a player stands on the spot. A returned
  armour piece ("armor shard") is picked up quietly, with no cutscene, and only by a
  player who lacks it (`0xc837b`). Pamphlets, keys, tickets and the ultimate weapon
  never respawn.
* **Cheat keys** (`CoopCheatKeys=1`): the client sends cheat message `0x3b`; the server
  handler `fb910` refused GimmieGimmieGimmie `0x2f`, CyclePlayerClass `0x31` and
  NextArmor `0x39` in multiplayer (flag 2 of table `0x1016d8a0`). Around that call the
  three entries are allowed and routed to Server Tools handlers that check
  `kpc_cheats`, act on the sender only and announce it.
* **Powerup effect** (`CoopPowerupResend=1`): `player powerup` broadcasts message
  `0x39` (activator, effect `0x35`) once; it is sent again to the other members 400 ms
  later, in case their client did not yet have the just-teleported activator.

## Timing

* **Server frame step** (`CoopServerFrameMs=16`): the dedicated server called its frame
  (`server.dll 0x235b0` → `0x2a05f`) about every 8 ms and simulated each as 10 ms
  (clamp `0x2a19c`): game time ran ~1.25× real time and AI stepped like ~100 FPS. Calls
  sooner than 16 ms after the last simulated frame are skipped; the frame function
  measures its own elapsed time, so the next frame simulates all of it. Fixes the R2M2A
  Ballbuster garage scene (monster MoveTo stops while the ground test fails,
  `0x97fac`). Consequence: the server sends updates, including scripted camera
  positions, 62.5 times a second; the KPCFix client interpolates the camera between
  them (`SmoothCutsceneCamera`).
* **Single player:** the same stall above 60 FPS is fixed by the client's
  `LocalServerMinStepMs=16` (now the default). **Live:** user-confirmed.
* **Script clock / sound clock** (`CoopScriptClock`, `CoopSoundClock`): limit the
  server's script timer (`0xc5630`) and sound timers (`server.dll 0x2a1fc`) to wall
  time; they fixed the narration cadence and cut-off while frames were inflated, and
  are no-ops with the 16 ms step. **Live.**

## Client side (KPCFix `dinput.dll`)

| Setting | Effect |
| --- | --- |
| `CampaignLoadingArt`, `CampaignLoadingMinimumMs`, `CampaignLoadingHoldSpawn` | Loading picture and spawn hold, above |
| `KeepRendererOnFocusLoss` (1) | With `KeepAudioOnFocusLoss`: minimised, the game keeps rendering and animating as when focused (no `r_TermRender`/`r_InitRender`); keyboard and mouse are released |
| `HideBrokenWindow` (1) | A broken window is hidden on this client as soon as its shards are built |
| `KeepAudioOnFocusLoss`, `ResyncAudioOnFocus` | Remote multiplayer: alt-tab no longer pauses server-owned sounds; 300 ms after the window returns, every sound restarts at its real-time position (PauseAll `0x74c90` + ResumeAll `0x74ce0` from the frame's sound update `0x13bcd`) |
| `ChatMessageSeconds` (8) | Chat and server message lifetime (stock 3 s, cshell `0xcb38d`) |
| `ChatBlocksActions` (1) | While a chat message is typed (editor `shell+0x8c`, byte `+0x12ed`), the shell's `OnCommandOn` (vtable `0xff724` → `0xc9d80`) drops every key action: letters no longer toggle noclip/invuln (actions 90/93) or move |
| `SmoothCutsceneCamera` (1) | Server camera samples (`0x3d`/5 → `0xab104` → `0xa8f30`, camera `[0x1c49dc]` +0x10 position, +0x1c rotation) are interpolated every client frame (position linear, rotation normalised along the short arc; cuts > 256 units and pauses snap) |
| `ConsoleKeyFix` (1) | The console key (VK `0xC0`, `'` on UK keyboards) no longer types its character into the console (`client.exe 0x6dc9`, `0x700a`) |
| `KeepAudioOnFocusLoss` console variable | `KeepAudioOnFocusLoss 0/1` in the console switches it in game (created with the INI value) |
| `ServerTimeoutSeconds` (15) | Lost-server detection in a world |
| `LocalServerMinStepMs` (16) | Single-player AI step, above |

## Logs

`KPCServerTools.log` (server) prints the `effective …` value of every setting at start,
then one line per session event: `[KPCFix Co-op] g=<generation> e=<epoch>
seq=<n> <PHASE> <event> <detail>`. Useful events: `change`, `world-loaded`,
`intro-armed`/`intro-start`/`intro-finished`, `spawn`, `spawn-start`, `exit-seen`,
`exit`/`exit-refused`, `script-event player … -> players=N`, `item shared item=…`,
`secret`, `script …` (with `TraceScripts=1`), `spawner reference moved…`,
`inventory-saved`/`inventory-restored`, `item-respawn …`, `cheat …`, `powerup …`. `KPCFix.log` (client) prints one
`Loading world: …` line per load and the hold/release lines.

## Known limitations

* Spawn spread does not check doors/moving models or live actors.
* Menu items not done yet: a "connected" indicator and a Disconnect button in the
  Escape menu. The Audio-menu checkbox for `KeepAudioOnFocusLoss` works; clicks
  elsewhere on the Audio screen reach the stock widgets again (RT10).
* Stock clients (without KPCFix) see no loading picture and are not held.
* No world maps between stages; no saves. Inventory persists within one server run
  only (lost when a player disconnects or the server restarts).
* Only the host can start level-ending `CTriggerCommand` sequences; switches/levers
  that end a level are not gated yet.
* The `CheatScripts`-gated Nightmare loadout is not forced.
* Worlds after R1M3A have been played only partly in co-op: report anything that
  differs from single player with the server log.
