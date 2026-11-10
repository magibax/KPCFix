# KPCFix Server Tools 0.1.0

Windows graphical launcher and optional server cheats for KISS: Psycho Circus game version 1.13. Distributed separately from the KPCFix client fixes in the same repository.

## Install and host

1. Extract the entire ZIP directly beside `Psycho.exe`, `dedicated.exe` and `object.lto` (normally `C:\Games\Psycho\`).
2. **For invulnerability and noclip support:** close the game and server, then run **`Enable-ServerCheats.cmd`**. This patches the host’s `object.lto` and creates a verified backup. Skip this step if you only want the graphical launcher.
3. Run **`KPC-Server-Launcher.cmd`**. Use its graphical controls to select any map, set the server name, UDP port and player limit, and adjust frag/time limits, teamplay, item retention, FOV changes and starting weapons/armour.
4. Start the server and keep its console open while hosting. Defaults are eight players, 20 frags and 30 minutes. Zero frag/time limits request unlimited rounds. Close the console or use the server quit command to stop hosting. Run only one server instance at a time.

The newly added rule controls await native Windows validation. To remove the optional cheat patch, stop the game/server and run **`Disable-ServerCheats.cmd`**; it restores the original `object.lto`. Enabling/disabling currently requires a restart. Live cheat controls are not yet implemented.

All 59 listed maps are selectable, including PM01-PM10, campaign maps, M0 introductions and the tutorial. The chosen map is the single-entry round rotation. Campaign hosting is experimental: maps can lack multiplayer spawn points and fall back to a single-player start; missing entity classes and campaign scripting may affect play. This does not implement campaign co-op or new spawn points.

Connect through the in-game server browser or directly using the host’s IP address and chosen UDP port (default 42972). The launcher uses the original dedicated server, REZ archives and other installed game assets; none are redistributed.

## Optional cheats

Cheats remain stock until you run Enable-ServerCheats.cmd. Stop the game and server first. This enables invulnerability and noclip requests from connected clients using Developer=1 and the original developer key bindings. Each action applies to the requesting player, not everyone. Code inspection establishes targeted state/message handling; an independent two-client isolation test is still outstanding.

The server logs invulnerability ON/OFF and accepted noclip toggles with the name supplied by that client. A noclip log confirms the toggle request, not a server-tracked ON/OFF state. Example:

    [KPCFix Server Tools] Invulnerability ON for Player
    [KPCFix Server Tools] Noclip toggle accepted for Player

Run Disable-ServerCheats.cmd with game/server stopped to restore original object.lto and disable both cheats and their logging. The patcher makes a verified original backup named object.lto.before-noclip-test and accepts only the exact supported original or known development patches. It changes the host object.lto on disk, so local games in the same installation also use it until disabled. Other developer commands retain their original restrictions. No keyboard bindings or player names are changed.

## Supported server files

The UK/European-family dedicated server setup has been tested by the user, including map selection, invulnerability, noclip and client-name logging. The US 1.13 installation uses a different dedicated.exe and server.dll, but its object.lto is byte-identical, so the same cheat patch targets apply. US runtime testing remains outstanding. Keep each official dedicated.exe/server.dll pair together; the checker accepts both supported pairs and exact patch hashes. German and Italian 1.13 server pairs match the European pair in the supplied files.

Europe-family dedicated.exe: 69,632 bytes; server.dll: 323,584 bytes.
US dedicated.exe: 81,920 bytes; server.dll: 335,872 bytes.
Both display Server Version 3. The US executable adds -logfile and -debuginfo switches; its manual documents them. Both contain different executable code, so further gameplay differences cannot be inferred merely from size or the shared banner. Their server DLLs both pass packet lengths through their send/receive wrappers; cross-edition network play needs live validation.

Common original object.lto SHA256: 3a03f8796c482b1a7ad72755df33ee8f1f8056bb10957f6f700e64bd006b3e5b

## Validation and source

This 0.1.0 package retains the working gameplay changes and cleans up their console output: consistent prefix, no duplicate original invulnerability diagnostic, and no launcher status line after console-input submission. Those final output changes have static validation and await native confirmation. Earlier runtime tests succeeded with the prior logging build. The original engine's own messages retain their original text.

patch.json describes every changed byte; source/logging.S contains the additional console hooks. The patcher checks input and output hashes and rollback normalization. The launcher requires Windows PowerShell and Windows Forms, already supplied with supported Windows installations. See LICENSE for the MIT license. KPCFix client configuration is separate.

## Configuration files

The launcher reads server-template.cfg and writes its own kpc-gui-server.cfg plus kpc-gui-maps.txt. It explicitly executes that generated config; the shipped server.cfg is not edited. GUI values override the corresponding template values for that launch. Other template values remain available for manual editing, including hostname location/MOTD, gib detail, friendlykill and suicide multipliers. Teamplay is the original teamplay flag; the game type remains regular deathmatch (1). Spawn type remains the established value 0; the duplicate spawntype=2 line from the stock config is omitted. Starting equipment follows the stock defaults (gauntlets and belt). Configuration is regenerated each time you start a server; edit the template for settings not exposed in the UI.
