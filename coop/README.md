# Co-op campaign (experimental)

This branch adds co-op through the whole *KISS Psycho Circus* campaign for up to three players. It runs on a dedicated server with the co-op build of **KPCFix Server Tools**. Players use the co-op build of the **KPCFix client**.

**This is not a release.** Builds come from this branch, and some features are still untested. For what has and hasn't been confirmed in play, see [STATUS.md](STATUS.md). Settings are in [CONFIGURATION.md](CONFIGURATION.md), and engine details in [ENGINE-NOTES.md](ENGINE-NOTES.md).

## What you get

**The campaign**
- All 48 worlds of the original `WORLDORDER.TXT`, from R1M0 to the end, or from the start of any realm.
- Each world's opening cutscene plays once for everyone.
- The level scripts run as in single player: levers, doors, triggers, monster spawners and boss sequences.
- Each world continues to the next one in campaign order.

**Your progress**
- Each player keeps their weapons, armour pieces, shards, ammo and class through world changes and deaths. It is lost if you disconnect or the server restarts; there are no saves.
- Weapons, armour, pamphlets, keys and tickets that one player picks up are given to everyone. "Secret found" is shown to everyone.
- Taken items come back after 15 s, once nobody is standing on the spot.

**Playing together**
- No friendly fire.
- Players start at the single-player start. Later players are moved to a checked spot nearby.
- Only the host can take a level exit, end a level or skip a cutscene. Everyone else gets a chat reply explaining why.
- If everyone is dead for 2 s, the world can restart (`CoopRestartOnWipe`).

**Loading**
- Everyone loads behind the world's loading picture.
- With KPCFix clients, everyone waits until all players have loaded (60 s at most), then enters together, and the cutscene starts.

**Also**
- Enemy health bars, smooth cutscene cameras, per-player music, and breakables that work for every client.

Players without the KPCFix client can still join. They get no loading picture or synchronised entry, and the other client-side fixes are missing.

## Hosting

1. Build or download this branch's Server Tools. Install it in the game folder with **Setup-KPCServerTools.cmd**. It installs as `winmm.dll`; `server.dll` and `object.lto` stay original.
2. Start **KPC-Server-Launcher.cmd**, tick **Co-op campaign** and start. The launcher then:
   - picks a random deathmatch map (PM01–PM10) as the lobby;
   - turns on the co-op session and the campaign in `KPCServerTools.ini`;
   - removes start armour, so the realm armour pickups stay in the worlds.

   The **Test logging** box adds the detailed logs needed for bug reports.
3. Players join from the server browser and arrive in the lobby. The first player in is the host (marked `*` in `!coop`).
4. The host types `!campaign full`, or `!campaign water` / `earth` / `air` / `fire` / `nightmare` to start at a realm.

From the command line:

```
powershell -File Start-KPC-Selected-Server.ps1 -Map random -Coop [-Cheats Enabled]
```

`-CoopMode Route` runs a short fixed test route (R1M2B → R1M2C → R1M3A) instead of the campaign.

## Chat commands

| Command | Who | What it does |
|---|---|---|
| `!campaign full` / `!campaign <realm>` (`water`, `earth`, `air`, `fire`, `nightmare` or `1`–`5`) | Host | Starts the campaign from R1M0 or from that realm. Works in the lobby and during play. `!campaign` alone shows the help. |
| `!next`, `!previous` | Host | Goes to the next or previous world. |
| `!changelevel <world>` | Host | Goes to any campaign world (`R1M2A`), `TUTORIAL`, or a deathmatch map `PM01`–`PM10` (which becomes the lobby). |
| `!reset` | Host (anyone outside a session) | Back to the state at launch: settings, cheat permission, empty inventories, lobby world. Stops the music. |
| `!difficulty <level>` (`!skill`) | Host | `lullaby`, `daydream`, `sleepless`, `coldsweat`, `nightmare` (or `0`–`4`). Applies from the next world. |
| `!coop on`, `!coop off` | Anyone on a deathmatch server / host | Switches the co-op session on or off live. |
| `!coop` (`!status`), `!help` | Anyone | Session status and the command list. |
| `!goto <player>`, `!back` | Anyone | Moves you next to a player (full name or a unique start of it), or back. Refused if there is no clear spot. |
| `!kill` | Anyone | Kills you; you respawn with your inventory. |
| `!class [starbearer\|beastking\|celestial\|demon]` | Anyone | Next or named class. You keep your inventory, and the class's weapons are added. |
| `!all`, `!gimmie`, `!nextarmor` | Anyone, with cheats allowed | Everything, ammo plus the next weapon, or the next armour piece, for you only. |

The older test-route commands `!ready`, `!unready`, `!start`, `!leave` and `!join` still work. `!abort` was removed; use `!reset`, `!campaign` or `!changelevel`.

**Server console:** `kpc_cheats 0|1` sets the cheat permission. `changelevel <world>` changes the world with no host check.

**Keys:** with cheats allowed, the game's GimmieGimmieGimmie and NextArmor keys work for each player. CyclePlayerClass always works.

## Client features on this branch

The co-op client includes everything in the stable client, plus:

**Loading**
- Loading pictures and synchronised loading.
- Clean music around loads.
- Sounds stop at a level change.
- A 5-second minimum loading picture in single player.

**Window focus**
- Alt+Tab and minimise keep the game running, with sound and the renderer.
- Sound resyncs when you come back.
- The game runs at full speed in the background.

**Chat**
- Copy and paste, a 200-character limit, scrollback, history and lowercase text.
- Chat is hidden while loading and can be used in cutscenes.
- Chat works in single player, with local commands: `!gimmie`, `!nextarmor`, `!prevarmor`, `!class`, `!kill`, `!noclip`.

**Multiplayer**
- 60 updates per second from the server.
- A 15-second timeout when the server is lost.
- Quit disconnects first.
- The scoreboard shows while its key is held.
- Smooth cutscene cameras.
- Broken windows hide as soon as their shards appear.

All of these are listed in [CONFIGURATION.md](CONFIGURATION.md#client-kpcfixini) and can be turned off in `KPCFix.ini`.
