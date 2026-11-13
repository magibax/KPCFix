# Next session prompts (playtest 2026-11-12, after commit `53b9751`)

Two independent sessions: **A (client, `source/`, `dinput.dll`)** and **B (server, `server-tools/`,
`KPCServerTools.dll`)**. They touch different DLLs and can run in parallel on branch `coop/mvp-lobby`
(each in its own worktree; merge in order). Read [NEXT-SESSION-HANDOFF.md](../NEXT-SESSION-HANDOFF.md)
first. Rules: never ship or modify original game assets; patches are in memory only, verified against
expected bytes; every change gets tests (client: `source/test_*.py` + `audit_sites.py`, run
`suite-coop.sh`; server: `unit-coop.sh`, `matrix-coop.sh`). Install for testing per
[UPGRADE-TEST-INSTALL.md](../UPGRADE-TEST-INSTALL.md). Commit on the branch; don't push.

**Status:** Prompt B is done (`5c8363f`, Server Tools `7dbba5eb`, installed; results go in the
handoff's test plan). Prompt A runs in worktree branch `claude/zealous-cori-401c64`.

---

## Prompt A: client (cshell Audio checkbox)

KPCFix co-op branch `coop/mvp-lobby` in `C:\Projects\KPCFix-coop`, client patch `source/` (the
`dinput.dll` that patches `cshell.dll` 703813a4 in memory). The Setup > Audio checkbox "Keep Running
When Minimised" (`source/src/ui.c`, `kpc_audio_*`, `keep_box_create`) works in game: toggling it switches
the `KeepAudioOnFocusLoss` console variable and the behaviour follows. Two fixes from the playtest
screenshot:

1. **The radio shows the wrong state.** With the setting ON (the default, behaviour active) the
   radio is drawn grey/unlit; the user has to make it look "disabled" to get the behaviour. Invert
   it so the lit/glowing radio means ON, like Enable Music. Today `keep_box_create` and
   `kpc_audio_draw` set the checkbox byte `+0x14c = !kpc_keep_running()` and the change callback
   `keep_box_changed` writes `!box[0x14c]`. That followed Enable Music (`0x7552`: `+0x14c = (0x7c70(...) == 0)`,
   callback `0x7080`), but the in-game result shows the opposite meaning, so re-derive which value of
   `+0x14c` draws `checkbox_checked.pcx` (images `+0x100`/`+0x104` from `0xd1b0`; draw = vtable `+0x0c`
   of class vtable `0xfd358`) and also re-check what `0x7c70` returns. Flip both the initial/draw
   state and the callback, and update `source/test_focus.py` (it currently asserts `+0x14c == 0` for ON).
   (The user's note also said "cycle player class" here; that looks like a separate request, which is
   Prompt B item 3. Confirm with the user if unsure.)
2. **Move the checkbox into the black gap above**, between Music Volume and Enable Sound FX. The stock
   positions are: Enable Music `(0xbc, 0x88)`, Music Volume slider `+0x9c`, Enable Sound FX `(0xbc, 0x100)`,
   3D Sound `0x13c`, Sound Quality `0x164`, Back `(0x21f, 0x1af)`. The gap is the hidden Music Type
   list (widget `+0x68`, class `0x2f000`). In the screenshot (2000×1125 for 640×480) the gap runs from
   about y 180 to y 250, so try y ≈ `0xd0`; x stays `0xbc`. Check that the hidden Music Type widget does
   not take the clicks there (the screen's own mouse-move `0x7d40` and event `0x7e70` run first; the
   box's event is tried first for event 1). Update the position assert in `test_focus.py`, rebuild,
   install, and ask the user for a screenshot.

---

## Prompt B: server (Server Tools / object.lto)

KPCFix co-op branch `coop/mvp-lobby` in `C:\Projects\KPCFix-coop`, server patch `server-tools/`
(`KPCServerTools.dll` proxy that hooks object.lto in memory; sources
`server-tools/server-tools-source/src/proxy.c`, `logging.S`, `coop_*.h`). Original binaries for RE are in
`C:\KPCTest\re` (capstone/pefile in `C:\KPCTest\re-venv`). Log evidence: `C:\Games\Psycho\KPCServerTools.log`.
Record findings in the playtest table of `coop/NEXT-SESSION-HANDOFF.md`. Items, roughly in priority order:

1. **Non-hosts don't trigger monster spawners / events.** In R4M1A the spawner room is triggered by
   walking through a threshold; as a non-host nothing happened, as the host it fired. Cause:
   `CoopHostSequences` (row 3, `71fb1df`) keeps every `CTriggerCommand` armed for the host only.
   Only triggers that lead to a level change (exit, victory/ending script → `trigger_generic CExitTrigger*`)
   should be host-gated. Restrict the gate to triggers whose command list (or the script it starts)
   reaches an exit, and let every player fire the rest. `proxy.c` already has `coop_commands_reach_exit`
   (~line 1428) and `coop_script_reaches_exit` (~1416); check why the gate doesn't use them for this
   trigger. Test with R4M1A and the R1M1A victory trigger.
2. **Cheats from the client's keybinds** (the user wants keybinds, not chat). The stock client already
   sends them: `GimmieGimmieGimmie` (action 98, cshell `0xc4c90`) and `NextArmor` (100, `0xc4120`) send
   message `0x3b` with cheat id `0x2f` / `0x39` (+ byte 1) through cshell `0xc3680`. Make Server Tools
   answer these per player (only for the sender, under `kpc_cheats 1` / the same permission as Invuln,
   which object.lto polls with `IsCommandOn(client, 93)` at `0xef1b4`). Find object.lto's handler for
   message `0x3b` and its multiplayer refusal. Keep `!gimmie`/`!nextarmor` working, or drop them if the
   user agrees.
3. **Cycle player class (Starbearer, Beast King, Celestial, Demon).** The stock developer cheat
   `CyclePlayerClass` (action 97 → cshell `0xc4cb0` → message `0x3b` cheat `0x31`) already does it in single
   player. Allow it per player in co-op via the keybind (same path as item 2), plus a chat command
   (for example `!class` / `!class <name>`). Weapons, ammo and armour must survive the class change (see item 4).
4. **Persistence.** Each player's weapons, armour pieces/shards, ammo and player class must persist
   through world changes and lobby resets; weapons, ammo and armour also through class cycles. This is
   row 8 (open): multiplayer respawns with a fresh inventory. Needs a per-member snapshot (keyed by the
   member, not the client slot) taken at world unload / before a class change / before a lobby reset,
   and re-given on spawn (the item give path `f3440` and has-item `f25f0` are already used by `!gimmie`;
   ammo give `f3530`; armour pieces `0x27`–`0x2c`). Map the player inventory layout first.
5. **Pickups respawn.** Weapons and armour shards (and other pickups) should respawn 15 s after
   being picked up and after the armour-shard pickup animation has finished, but only when no player
   stands in the pickup's spawn point (retry until the spot is clear). First check whether the stock
   deathmatch item-respawn code exists in object.lto and why it is inactive in co-op worlds (single-player
   pickups are removed). Reuse it if possible. Use the existing box-overlap helpers (`coop_aabb_overlap.h`,
   `BOX-OVERLAP-ABI.md`) for the "player in the spot" test.
6. **Powerup animation not visible to observers.** Since the teleport fix (row 1), only the player who
   picks up an armour piece sees the powerup animation; the others watching the cinematic don't.
   `CoopScriptEvents` routes `player teleport/animate/lookat/powerup` to the activator only
   (`proxy.c` ~line 1356, `c1a89` CCinematicEventPlayer::Fire). Find out whether `powerup`/`animate` is a
   per-client effect (message only to the activator's client) or a server animation on the activator's
   object that other clients don't receive (distance / PVS, or the activator's model being hidden for
   others during the cutscene). Make the effect visible to everyone without moving them.
7. **`!kill`**: kills the player who typed it (normal death and respawn rules; no cheat permission
   needed unless the user says otherwise).
8. **Chat-only "everything" cheat** (for example `!all`): gives the typing player only all armour, all
   weapons and full ammo of every type; needs `kpc_cheats 1`. Reuse the `!gimmie`/`!nextarmor` code
   (`COOP_GIMMIE_AMMO`, ids 3–14 weapons, `0x27`–`0x2c` armour; ammo needs the gauntlets) with maximum
   ammo instead of 50.
9. **`!changelevel <world>`, host only** (for example `!changelevel R1M2A`): changes every player to that
   campaign world from chat, in the lobby or during a campaign. Reuse the existing server-side world change
   (`coop_session.h` ~line 420, "Begin a server-side world change to route[target]") and the code that
   writes a world missing from the native 50-entry maplist into a free slot. Use the same loading
   picture / spawn hold / `!abort` handling as a natural exit.
   - **Accepted names:** match case-insensitively against the campaign table (`coop_campaign.h`);
     accept `R1M2A` and `worlds\realm1\R1M2A`.
   - **Errors:** reject unknown names with a chat reply listing the format; refuse from non-hosts
     ("Only the host…", like the exits).
   - **After the jump:** decide with the user whether the campaign carries on from there along
     WORLDORDER (natural exits lead to that world's successor), or whether it is a one-off.
     Carrying on is the likely expectation.
   - **Persistence:** item 4's snapshot must be taken as for any world change.

After building, run the unit tests and matrix, install per `UPGRADE-TEST-INSTALL.md` (Server Tools
setup refuses the previous branch proxy: move `server.dll` aside and rerun), and give the user
two-PC test steps for each item.
