# Co-op next-session handoff (2026-11-11, pass 2: narration, health bars, spawn6, scoreboard)

This is a self-contained resume point for the experimental co-op work. The details and
evidence are in [COOP-MVP.md](COOP-MVP.md) (sections C–H) and
[CAMPAIGN-SYNC-DESIGN.md](CAMPAIGN-SYNC-DESIGN.md).

## Where things stand

* **Branch:** `coop/mvp-lobby`, the head of PR #10 into `feature/coop-campaign`. The stable
  `main` is untouched. The local worktree branch is `coop/mvp-slice2`, tracking
  `origin/coop/mvp-lobby`.
* **Commits this pass:**
  * `64dbe5d` records the user's feedback;
  * `aa9949c` adds the code;
  * `4cc806f` makes the smoke script safe;
  * this docs commit follows.
* **New DLL:**
  * `KPCServerTools.dll` = `server.dll`
    `1b8816cbeafb48ca9e058bda48363847c807d47d1982bd17a4eb40615097f523`
  * manifest `SERVER_TOOLS_BUILD.json`
    `871695e56c490a2922cc0a6262aae18c8194275be7596728e9bb44674534fd7a`
  * Built twice with byte-identical results in the pinned image (GCC 14.2.0, binutils 2.44).
* **Installed:** only in the disposable copy `C:\KPCTest\smoke3\Psycho` (port **42990**). No
  server is left running there.
* **Not installed in `C:\KPCTest\smoke1\Psycho`:** the user's server (`dedicated.exe` from
  `smoke1`, port 42972) was running during this pass and was **not touched**. `smoke1` still
  has `e86bb352…`.
* **Backups** (in `C:\KPCTest\backup`):
  * `smoke1-e86bb352-before-pass2`: `server.dll`, `KPCServerTools.dll`, manifest, INI and log
    of the user-tested build;
  * `smoke3-13ca1d0a-before-pass2`: the previous `smoke3` state;
  * earlier: `smoke1-13ca1d0a-before-scriptclock` and `smoke1-e17c150f-before-intro`.
* **Server-only:** clients stay stock 1.13 plus the normal KPCFix client. No client patch was
  needed for anything implemented here.

## Live results so far (desktop + laptop, user-tested)

| Feature | Result |
| --- | --- |
| Natural transitions R1M2B → R1M2C → R1M3A | Work, without a scoreboard (`CoopIntermission=0`) |
| Original music (`CoopMusic=1`), including a late-joining laptop | Works; not beat-synchronised (accepted) |
| Original R1M3A intro (`CoopIntro=1`) | Camera, letterbox, subtitles and events work, and the two machines match |
| Subtitle/script cadence versus narration (`CoopScriptClock`, DLL `e86bb352`) | **FIXED** (confirmed). Server log: `script_ms=53005 wall_ms=53000`, engine 1.2456× |
| Narrator audio | **Open with `e86bb352`:** stops after "…will be a useful ally for you" |
| Enemy health bars | **Open with `e86bb352`:** never appear |
| Enemies after the window smash (`spawn6`) | **Open with `e86bb352`:** none appear |
| Scoreboard from `!start` until the cinematic | **Requested** |

## Pass 2: root causes and fixes (DLL `1b8816cb…`, NOT yet tested with clients)

| Issue | Root cause (evidence) | Fix (`[Coop]` key, default) |
| --- | --- | --- |
| Narrator cut-off | The stream is a **server-timed sound**: time left = 44.19 + 0.5 s, counted down by the **inflated engine dt**. When it reaches 0 the server drops it from every client's sound set: 44.69 / 1.2456 = 35.9 s of real time = "…ally for you". The WAV contains the full sentence to 43.7 s. Static analysis of `server.dll` `0x2c6e6`, `0x29f0b`/`0x2a1fc`, `0x1f94f` | `CoopSoundClock` (= `CoopScriptClock`): the sound timers get `min(engine dt, unpaid wall time)`. Hash-gated to the Europe-family `server.dll` |
| No enemy health bars | `object.lto 0xf8fe0` "is single player" gates all four paths of the bar manager (update, damage, remove, send). The server never builds or broadcasts `MID 0x56`. The stock client has no multiplayer gate | `CoopHealthBars` (1): the four gate calls proceed; Send limited to 10 Hz |
| No `spawn6` enemies | `CMonsterSpawner::ReadProp` forces "not in multiplayer" (`or al,0x10` at `0x93c5d`), so no monster spawner exists in multiplayer. `trigger_generic spawn6` then finds an empty group | `CoopSpawners` (1): NOP the forced bit; authored difficulty applies. Optional `CoopSkill` 0–4 (native server reports 2). Diagnostic `TraceSpawners` |
| `!start` scoreboard | The stock client shows the scoreboard on `0x48` (no click logic of its own) and closes it on `0x69`/re-entry. It cannot draw it while changing world (state 5) | `CoopStartScoreboard` (1, with `CoopIntermission=0`): `0x48` at `!start` with a 1 s hold, `0x48` again 250 ms after each spawn while the intro barrier holds, and `0x69` to all right before the script starts. Every failure path closes it, plus a 45 s failsafe |

## Tests run this pass

| Test | Result |
| --- | --- |
| C unit tests, 11 files × 64/32-bit, `-Werror`, pinned image (new: `test_coop_scoreboard.c`) | 22/22 PASS |
| Emulator `check_proxy.py`, compiled DLL on the stock Europe fixtures, 30 modes | 30/30 PASS. Covers pass-through, bad object, all session modes, `--no-sound-clock`, `--no-start-scoreboard`, `--no-health-bars`, `--no-spawners`, `--trace-spawners --skill=3`, `--skill=9`, intro overrun with both clocks off, `CoopIntermission=1` with an intro |
| `run_checks.py` (logging harness), Europe and USA layouts, in the pinned image | PASS |
| `tools/package_release.py --component server`, `tools/test_package_release.py` | PASS (53 files) |
| Native `dedicated.exe`, `smoke3`, port 42990, no clients: Coop (three console nextlevels) | PASS (15 checks), exit 0 |
| Native Hold with `TraceSpawners=1`, nextlevels 15 s apart | PASS, exit 0. Skill 2; R1M3A 42 spawner filter calls accepted, 15 rejected |
| Native Normal (PM01) and PassThrough | PASS. Sound hook not installed |
| Desktop + laptop | **NOT RUN** (needs the user) |

The smoke script used to refuse to run while any server was running, and on a quit timeout it
stopped *every* `dedicated.exe`. It now refuses only a server from its own folder or an occupied
port, and stops only its own server (`4cc806f`).

## Install the new DLL in `smoke1` (after the user's server is stopped)

```powershell
$d='C:\KPCTest\smoke1\Psycho'; $s='C:\Projects\KPCFix-audit\server-tools'
Get-NetUDPEndpoint -LocalPort 42972 -ErrorAction SilentlyContinue   # must be empty
Copy-Item "$s\KPCServerTools.dll" "$d\KPCServerTools.dll" -Force
Copy-Item "$s\KPCServerTools.dll" "$d\server.dll" -Force
Copy-Item "$s\SERVER_TOOLS_BUILD.json" "$d\SERVER_TOOLS_BUILD.json" -Force
(Get-FileHash "$d\server.dll").Hash      # 1B8816CB...
```

The existing `smoke1` INI already has `CoopSession=1`, `CoopMinPlayers=1`, `CoopIntro=1`,
`CoopTestStartWorld=R1M3A` and the traces. The new keys default to on. For evidence, add:
```ini
TraceSpawners=1
```

**Rollback:** copy the three files back from `C:\KPCTest\backup\smoke1-e86bb352-before-pass2`.
To disable one fix only, set its key to 0, for example `CoopSoundClock=0`, `CoopSpawners=0`,
`CoopHealthBars=0` or `CoopStartScoreboard=0`. Restart the server after any change.

## Two-PC acceptance test (desktop + laptop)

Start the server:
* `powershell -File Start-KPC-Selected-Server.ps1 -Map "worlds\realm1\R1M2B" -Cheats Enabled -Coop`
  in `smoke1`.
* The log must show:
  * `sha256=1b8816cb…`;
  * `effective CoopScriptClock=1`, `CoopSoundClock=1`, `CoopStartScoreboard=1`,
    `CoopHealthBars=1`, `CoopSpawners=1`;
  * `sound clock hook installed (server.dll 2a1fc -> 29f0b)`.

Both machines: `Psycho.exe` → Play, join, and type `!ready`; the host types `!start`. Record
both screens with sound (a phone video per screen is enough). Then check:

1. **Scoreboard.**
   * The stock scoreboard appears on both machines immediately at `!start`, without clicking.
   * During the world load the screen may be blank or show the loading screen: the stock client
     cannot draw the scoreboard then (section H).
   * After each machine is in R1M3A, the scoreboard comes back within about 0.25 s and stays up
     during the 3 s settle.
   * It closes on both at the first cinematic frame, and the letterbox and camera then run as
     before.
   * Note any flicker or a scoreboard that does not close. Pressing fire must not be needed.
   * Log: `scoreboard show …`, then `close … msg=0x69 sent` just before `intro-start`.
2. **Narration.**
   * The voice must continue past "…will be a useful ally for you **here, as the way in is not
     as straight as it first seems**" and finish about 2.6 s before the window smash.
   * Note whether both machines behave the same.
   * Log: `intro-clock …` and `sound-clock … sound_ms≈wall_ms`.
3. **spawn6.**
   * After the smash, Headless (10) and Gasbag (3) enemies arrive, the same on both machines,
     once.
   * Log: `world-content skill=2 monster-spawners=authored accepted=… rejected=…` for R1M3A.
   * Note whether enemies attack during the last ~4 s of the letterbox (`disable_ai` and
     invulnerability are gated off in multiplayer).
4. **Health bars.**
   * Damage an enemy from each machine. The original bar and name appear on **both** screens
     for the damaged enemy, track its health, and disappear on death or after the timeout.
   * Player health HUD unchanged. Log: `health-bar first monster damage in this epoch`.
5. **Regression.**
   * Natural exits R1M2B → R1M2C → R1M3A with `CoopTestStartWorld` removed: no scoreboard on
     the exits, music as before.
   * Optionally repeat with `CoopSoundClock=0` (the cut-off should return), and with
     `CoopScriptClock=0`.
6. **Reconnect / late join.**
   * The laptop disconnects and rejoins during the 3 s barrier: it should get its own
     scoreboard and see it close with everyone.
   * After the intro: no new scoreboard, no second spawn6.

Send back the full `KPCServerTools.log`, the effective INI, both recordings, and notes for steps
1–6.

## Open limitations

* **Client-only gap:** the stock client cannot show the scoreboard while it changes world.
  * Candidate client patch: cshell `0xcdb6f` `32 c0` → `b0 01` (untested, edition-specific).
  * Each client also shows the new world briefly between its re-entry and the second
    scoreboard.
* **Monster spawners:** enabled on **all** route worlds. They are the single-player content,
  so R1M2B/R1M2C get more enemies than in the earlier co-op tests.
  * `AttackActivator` targets the most recently joined player.
  * `disable_ai` and `player invulnerable` stay gated off in multiplayer.
* **Health bars:** global (everyone sees every damaged monster). Bar timeouts age with engine
  time.
* **USA `server.dll`:** no sound clock (site not verified); logged as unsupported.
* **Engine game time:** still the stock inflated time for objects, AI and physics.
* **Earlier limits still apply:**
  * join during the cinematic sees the rest only;
  * readiness is spawn-based;
  * R1M3A-only intro;
  * two-player combat/death/respawn regression not yet run.

## Pre-existing crash found during the earlier smoke tests (not caused by these changes)

* **Signature:** `dedicated.exe` access violation at `object.lto+0x87c41`, in the AI "Building
  path table." routine (`0x87be0`, called from `0x95259`).
* **When:** during the native Coop smoke with three rapid console `nextlevel`s.
* **Rate:** 3/4 runs with `e86bb352` and 1/3 with `13ca1d0a`. It did not occur in the two
  `1b8816cb` Coop runs of this pass, which is too few to conclude anything.
* **Not yet checked under pass-through.**

## Resume steps
1. Run `git fetch origin` and check that `origin/coop/mvp-lobby` contains `aa9949c` and this
   file.
2. Check whether the user's test server is still running (`Get-Process dedicated` with its
   `Path`). Never stop it. Use `smoke3` (port 42990) for native tests.
   `Invoke-CoopServerSmoke.ps1` now coexists with it.
3. Builds: `C:\KPCTest\pass2\build.sh <dir>` copies `server-tools` and runs the image
   `kpc-gcc14:local`. Then use `matrix.sh <dir>` (emulator, 30 modes) and `unit.sh` (C tests).
4. After installing in `smoke1`, run the two-PC test above and record the results in
   COOP-MVP.md sections E–H.
