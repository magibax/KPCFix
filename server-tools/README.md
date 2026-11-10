# KPCFix Server Tools

Windows dedicated server tools for supported official game version 1.13 installations. Includes the graphical map/rules launcher, chat logging, cheat announcements and live host control of noclip and invulnerability.

## Install

1. Stop the game and dedicated server for this update.
2. Extract **all files** beside `Psycho.exe`, `dedicated.exe`, `server.dll` and `object.lto` (usually `C:\Games\Psycho\`), replacing the previous Server Tools files. Existing recognized patches are upgraded automatically; do not restore the module manually.
3. Check the game version before launching:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Check-KPCVersion.ps1
   ```

4. Run **KPC-Server-Launcher.cmd**. Choose a map, rules and the initial **Allow cheats on startup** setting (default off), then start the server.

## Live host commands

Enter these directly at the dedicated server's `>` prompt:

```text
kpc_cheats 1
kpc_cheats 0
kpc_cheats
```

- `kpc_cheats 1` enables cheat permission immediately.
- `kpc_cheats 0` disables permission immediately and clears connected players' invulnerability. Clients with confirmed noclip receive a targeted OFF correction; late ON reports are also corrected without sending duplicate toggle responses.
- `kpc_cheats` prints the current permission setting in the server console.

No restart, file patch or client closure is needed for these console commands. Both startup modes include the live hooks, so enabling after starting disabled works too. The host command is registered in the dedicated application's command table; ordinary client chat cannot change it. Each actual permission change sends exactly **Cheats enabled** or **Cheats disabled** in-game to all connected clients. The console receives one copy prefixed `[KPCFix Server Tools]`. Repeating the current setting only prints console status, without another broadcast.

Rejected player requests remain silent: no cheat action, rejection notice or announcement. Normal chat is mirrored to the server console in both modes. While enabled, each player's own toggles remain individual and are announced in-game as `name toggled cheat ON/OFF`, with a prefixed console copy. Invulnerability uses the player flag; noclip uses the existing client-reported collision state, not a toggle counter. The host's disable command intentionally revokes active cheats for all connected players, with just the one host announcement.

The setting persists across map changes within the current server shell. A new server launch uses the checkbox selection again. The live console state is held in memory; it is not saved as a new startup preference.

## Hosting and configuration

All 59 maps remain selectable: PM01–PM10, campaign maps, M0 introductions and the tutorial. Campaign multiplayer can be buggy and may fall back to a single-player spawn point; this does not implement co-op. The GUI provides server/player/frag/time settings, teamplay, item retention, FOV permission and starting equipment. It generates `kpc-gui-server.cfg` and `kpc-gui-maps.txt` from `server-template.cfg`; the game's `server.cfg` is untouched. The server should appear in the in-game server browser.

Names and keyboard bindings are not changed. Players use the game's existing developer controls and their own bindings. Other developer commands retain their restrictions. Original engine console messages retain their original text; our logs preserve the prompt, pending command and insertion position.

## Installer and rollback

Initial installation/startup selection still changes `object.lto` on disk, while the game/server are closed. Live commands do not touch the file. A runtime DLL conversion that leaves the original module unchanged on disk has not been implemented.

The installer verifies known input hashes, exact normalization to the original module, target hashes and the replaced file, retaining the original backup `object.lto.before-noclip-test`. Its Windows PowerShell file-replacement correction is included. Known earlier cheat/chat and startup-switch builds are accepted.

- `Enable-ServerCheats.cmd`: install with initial cheats enabled.
- `Disable-ServerCheats.cmd`: install with initial cheats disabled, retaining live control and chat logging.
- `Restore-Original-Server.cmd`: remove every server binary patch and restore the exact original module.

Close the game/server before using these installer/restore commands. The launcher reapplies the selected initial mode on the next start; use the live console commands to change a running server.

## Verification

The supplied four region copies of original 1.13 `object.lto` are identical. Europe/Germany/Italy share a dedicated executable/server DLL profile; the US pair differs. Keep each official pair together. No proprietary game binaries are distributed.

x86 emulator checks passed for both dedicated profiles and both initial modes: native host-command registration/callback arguments, enabling/disabling, exact broadcast text, silent gates, two-player revocation, late-report deduplication, disconnect cleanup, command/cursor preservation and full patch rollback. The revised test executes the original object-list iterators and player conversion, includes a non-player entry, and guards opaque object memory against writes. It reproduces the old invalid-access bug and passes with this correction. Relocation was also exercised at a nonpreferred module address. PowerShell syntax checks passed. The user confirmed the corrected live-control/revocation build works on the European-family Windows setup. US native testing, broader two-client testing and map-change coverage remain outstanding.

Source and tests are included under `source/`. GNU binutils and Python build the patch manifest from an exact original module; the optional `unicorn` Python package runs emulator tests. `check_live.py` accepts the stock object path and dedicated executable path, plus `--disabled` for the off startup mode. `check_modes.py` validates mode hashes and rollback; `run_checks.py` covers packet and console hooks. See `LICENSE` for the MIT license.

The published Server Tools 0.1.0 ZIP predates these live-control updates. This folder contains the current main-branch implementation.
