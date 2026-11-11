# Co-op next-session handoff (2026-11-11, script clock session)

This is a self-contained resume point for the experimental co-op work. The details and
evidence are in [COOP-MVP.md](COOP-MVP.md) (sections C and D) and
[CAMPAIGN-SYNC-DESIGN.md](CAMPAIGN-SYNC-DESIGN.md).

## Where things stand

* **Branch:** `coop/mvp-lobby`, which is the head of PR #10 into `feature/coop-campaign`.
  The stable `main` is untouched. The local worktree branch was `coop/mvp-slice2`, tracking
  `origin/coop/mvp-lobby`.
* **Code commit:** `5843fa9` "hold the original script clock to wall time (CoopScriptClock)".
  This document was updated in the docs commit that follows it.
* **Current DLL:** `KPCServerTools.dll` = `server.dll`
  `e86bb352df4a726c2a6c0d689df93ce39414cd5cc8206b959892add58f2e257d`, manifest `a366de7e…`.
  It was built twice with byte-identical results in the pinned image (GCC 14.2.0, binutils 2.44).
* **Installed:** in the disposable test copy `…\KPCTest\smoke1\Psycho` (port 42972). No server
  was running at handoff. The normal game folder was not touched.
* **Rollback copies** (outside the repository, in the test install's backup folder):
  * `smoke1-13ca1d0a-before-scriptclock`: the previous intro DLL `13ca1d0a…`, user-tested with
    two clients, plus its INI and manifest;
  * `smoke1-e17c150f-before-intro`: the music-only DLL.
* **Server-only:** clients are stock 1.13 plus the normal KPCFix client.

## Verified by the user on Windows (2026-11-11, before this session)

| Feature | Result |
| --- | --- |
| Natural transitions R1M2B → R1M2C → R1M3A | Work, without a scoreboard (`CoopIntermission=0`) |
| Original music (`CoopMusic=1`), including a late-joining laptop | Works; not beat-synchronised (accepted) |
| Original R1M3A intro (`CoopIntro=1`, DLL `13ca1d0a`), desktop + laptop | Camera, letterbox, subtitles and events work, and the two machines match |
| Narration timing | **Bug:** voice trailed the subtitles on both machines; the 47.8 s window smash cut it off |

## Live feedback on `CoopScriptClock` (DLL `e86bb352…`, desktop + laptop, 2026-11-11)

| Item | User result |
| --- | --- |
| Subtitle/script cadence versus narration (the old ~1.25× lead) | **FIXED** in real gameplay. Keep the script clock governor |
| Narrator audio | **NEW / OPEN:** stops abruptly right after "…will be a useful ally for you". This is a playback-lifetime problem, not the old cadence bug |
| Enemy health bars | **OPEN:** never appear when enemies take damage in co-op |
| `47.9 trigger_generic spawn6` after the window smash | **OPEN:** smash seen, the expected enemies never appeared |
| Original scoreboard during the `!start` load and intro barrier | **REQUESTED:** show it from `!start` and close it automatically when the cinematic begins (no click) |

## Narration bug: root cause found, fix awaiting client validation

**Root cause (static analysis, then measured on the native server):**
* The script clock runs on the engine's frame time, not on wall time.
* The stock dedicated server runs a frame whenever `timeGetTime` has advanced ≥ 8 ms
  (`dedicated.exe 0x401c7b`), and clamps each frame's time to **at least 10 ms**
  (`server.dll 0x2a19c`).
* object.lto passes that dt to the script updater (`0xffba5` → `0xc5630` → `0xc6260`), which
  also drives camera paths and fog fades.
* When frames come faster than 100 Hz, script time outruns real time, by up to 1.25× with
  8 ms frames.
* The narration WAV is 44.19 s long and starts at script 1.0 s. At real-time rate it ends 2.6 s
  before the smash. At 1.25× the smash arrives 37.4 s into the clip.

| Measurement | Engine/wall |
| --- | --- |
| User's live intro run (client on the server PC): ≥ 53 s of script in 42.547 s | ≥ 1.246× |
| Native empty server, timer resolution 15.625 ms | 1.02× |
| Native empty server, 1 ms resolution held by a helper (as a local game client would) | 1.15× (a repeat: 1.02×) |

**Fix `[Coop] CoopScriptClock`** (default = `CoopIntro`):
* per frame, scripts get `min(engine dt, real time not yet given)`;
* only the script updater's argument changes, and engine game time is untouched;
* `CoopScriptClock=0` gives the old behaviour for A/B comparison.

On the native server, governed: `script_ms=9993` per `wall_ms=10000` while the engine credited
10 213. In the emulator, the authored 53 s ran in 53.04 s (stock: 42.44 s).

**Not yet verified:** that the narration now matches the subtitles and finishes before the
smash on real clients. Do not report this as fixed until the two-PC test below has been run.

## Pre-existing crash found during the smoke tests (not caused by this change)

* **Signature:** `dedicated.exe` access violation at `object.lto+0x87c41`, in the AI "Building
  path table." routine (`0x87be0`, called from a path-network object's staged update at
  `0x95259`).
* **When:** during the native Coop smoke, which types three console `nextlevel`s in quick
  succession on an empty server.
* **Rate:** 3 of 4 runs with `e86bb352` and 1 of 3 with the previous `13ca1d0a`, at the same
  offset and step.
* **Not yet known:** whether stock pass-through has it too, and whether it can occur with
  players. It is probably the same as the "silent exit during a world start" noted earlier.
  Investigate separately; it is not part of the cadence task.

## Known limitations (deferred)
* **Invulnerability:** the script's `player invulnerable` is gated off in multiplayer.
* **Join during the cinematic:** the late joiner sees the remaining broadcasts only.
* **Readiness:** spawn-based (+3 s settle), not a client acknowledgement.
* **Not run yet:** the two-player combat, death, respawn and reconnect regression.
* **Test mode only:** `CoopTestStartWorld=R1M3A`. The intro allowlist is R1M3A only.
* **Engine game time:** objects, AI and physics still run on the inflated engine time. That is
  stock dedicated-server behaviour and deliberately not changed.

## Settings in the test install (`KPCServerTools.ini`)
```ini
[ServerTools]
Enabled=1
AllowCheats=1          ; disposable test server only
[Coop]
CoopSession=1
CoopMinPlayers=1
CoopMaxPlayers=3
CoopIntermission=0
CoopMusic=1
CoopIntro=1            ; CoopScriptClock is absent, so it follows CoopIntro (=1)
CoopTestStartWorld=R1M3A
TraceTiming=1          ; adds clock-calibration, intro-event and intro-clock lines
TraceMusic=1
TraceCampaign=1
```
For an A/B run, add `CoopScriptClock=0` under `[Coop]` and restart the server.

## Two-PC validation test (next step)
1. Start the test server from the test install:
   `powershell -File Start-KPC-Selected-Server.ps1 -Map "worlds\realm1\R1M2B" -Cheats Enabled -Coop`.
   * First check that nothing else is listening on port 42972.
   * Check that the log shows `sha256=e86bb352…` and `effective CoopScriptClock=1`.
2. On the desktop and the laptop, start `Psycho.exe` → **Play** and join port 42972. Both type
   `!ready`; the host types `!start`.
3. Record both screens **with sound**. A phone video of each screen is enough. For each machine,
   note the time from the first letterbox frame to each of:
   * the start of the narration;
   * the subtitles at authored 8.5 s, 20 s, 31.2 s and 38.9 s, and whether the voice matches
     each one;
   * the end of the narration;
   * the window smash (authored 47.8 s; the narration should end about 2.6 s before it).
4. Keep the server log lines `intro-start`, every `intro-event`, `intro-clock` and
   `clock-calibration`.
   * `intro-event … at_ms=47800 … wall_ms=…` should be about 47 000–48 000 after the start.
   * `intro-clock … script_ms` should be close to `wall_ms`, while `engine_ms` may be larger.
5. Optional A/B: repeat with `CoopScriptClock=0`. The smash should then come early again.

## Resume steps
1. Run `git fetch origin` and check that `origin/coop/mvp-lobby` contains `5843fa9` and this file.
2. Check the installed `server.dll` SHA-256 (`e86bb352…`) and that port 42972 is free.
   Extracting a Server Tools ZIP resets `server-template.cfg` to port 42972.
3. Builds use the Docker image `kpc-gcc14:local` (start Docker Desktop first). Build twice in
   disposable copies and compare the DLL and the JSON manifest. Under Git Bash use
   `MSYS_NO_PATHCONV=1`, with bounded timeouts.
4. The emulator matrix uses `check_proxy.py <fixtures> --coop-session --intro
   --test-start=R1M3A [--trace-timing] [--no-script-clock]` plus the earlier modes.
