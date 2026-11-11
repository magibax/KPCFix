# Co-op next-session handoff (end of 2026-11-11)

This is a self-contained resume point for the experimental co-op work. The details and
evidence are in [COOP-MVP.md](COOP-MVP.md) and [CAMPAIGN-SYNC-DESIGN.md](CAMPAIGN-SYNC-DESIGN.md).

## Where things stand

* **Branch:** `coop/mvp-lobby`, which is the head of PR #10 into `feature/coop-campaign`.
  The stable `main` is untouched.
* **Baseline:** commit `c61aafe` was the branch head when this handoff was written. The last
  code commit is `23c68f4`. This document was added in the commit after `c61aafe`.
* **Tested DLL:** `KPCServerTools.dll` = `server.dll`
  `13ca1d0a7c17986dcbdc3f360bf82f92084584864824b623854fb57b1f10364b`. It was built twice in
  the pinned GCC 14.2 / binutils 2.44 image with byte-identical results, and CI reproduced it.
  The SHA-256 was verified in the test install at handoff time.
* **Previous music-only build:** `e17c150ff920aa9865fe06c8e0109bad9426215cfb1fdf943115af67be7dbf27`.
  It is kept as a rollback copy outside the repository, in the test install's backup folder
  (`smoke1-e17c150f-before-intro`).
* **Test install:** a disposable copy of the game, `…\KPCTest\smoke1\Psycho`. Only that copy
  is modified; the original game folder is never patched.
* **Server:** port 42972. No server was running at handoff time; start one as described below.
* **Server-only:** everything below runs in the dedicated server's proxy DLL. Clients are
  stock 1.13 plus the normal KPCFix client. No client change was needed for any of it.

## Verified by the user on Windows (2026-11-11)

| Feature | Result |
| --- | --- |
| Natural transitions R1M2B → R1M2C (hallway) and R1M2C → R1M3A (golden mirror) | Work. No scoreboard and no fire/use click needed (`CoopIntermission=0`) |
| Original campaign music (song5 on R1M2B/R1M2C, song1 on R1M3A) | Plays through the client's own DirectMusic (`CoopMusic=1`, per-client send after spawn). A second laptop joining a running game also got music. Start positions are **not** beat-synchronised across clients; accepted for now |
| Original R1M3A opening cinematic (`mapdata\realm1\r1m3\r1m3a\cathedral1start.scr`) with a desktop and a laptop | Works through `CoopIntro=1`: camera pans, letterbox, scripted events and coordination. The two machines matched each other exactly by eye (not measured) |
| **Narration timing (open bug)** | On **both** machines, the narrator's speech trails the subtitles by about two seconds, and the scripted window smash near the end cuts off narration still playing. See below |

Server log of that run (`TraceTiming=1`, ms since the session was armed):
```text
t=351844 e=1 INTRO_PENDING intro-armed mapdata\realm1\r1m3\r1m3a\cathedral1start.scr
t=353141 spawn johnny (slot 0)        t=353469 spawn Player (slot 1)
t=354641 / 354969 music-send song1 to each client
t=356469 e=1 INTRO intro-start …   intro-playing "[Co-op] R1M3A intro starting (epoch 1, 2/2 players)"
t=399016 e=1 PLAYING intro-finished …
```

## Open bug: narration lags behind subtitles and the window smash

* **Observed:**
  * The voice-over (`1 streamsound player mapdata\realm1\r1m3\r1m3a\r1m3-intro.wav local`)
    trails the subtitles (`1`, `8.5`, `20`, `31.2`, `38.9 subtitle …`) by about 2 s.
  * The `47.8 window_break` / glass sound interrupts narration that is still playing.
  * The lag was about the same on both clients.
* **Not known:** the root cause. Candidates, none confirmed:
  * the streamed sound starts late, or the client buffers or schedules audio late;
  * voice queueing on the client;
  * the server's script clock running at a different rate from real time;
  * an interaction with frame rate or KPCFix's client timing fixes.
* **Lead from the server log, not a conclusion:** `intro-start` → `intro-finished` took
  **42.5 s of wall clock** (`GetTickCount`). The script's last timed event is at 53 s
  (`player invulnerable false`), and its release events are at 51.7 s. If the session's "finished" check (`0xc55b0`) only turns false
  after the last event, the server's script time advanced about 1.25× faster than wall clock.
  That would put subtitles and the smash (script-timed) ahead of a real-time audio stream.
  Unverified: when exactly `0xc55b0` turns false, and whether single player shows the same rate.
* **Do not:** apply a fixed offset, edit the copyrighted `.SCR` timestamps, or add blanket delays.

### Investigation plan (in this order)
1. **Baseline:** in stock single player, record R1M3A's narration, subtitle and window-smash
   timing at a fixed frame rate (screen and audio capture).
2. **Timestamps along the chain:** server script dispatch time per event versus client receipt
   and start of the voice clip, subtitle display and the smash.
   * Compare early, middle and late events. A constant offset suggests start latency; a growing
     offset suggests a clock-rate difference; per-clip jumps suggest queueing.
   * Add the server side as bounded opt-in tracing first, then do the client side.
3. **Trace the voice path:** how the original `streamsound player … local` event is dispatched
   to clients (a broadcast or a single "player" target) and how the client starts and queues it.
   Treat KPCFix client timing hooks and 60 vs 165 FPS as hypotheses to test, not causes.
4. **Fix:** prefer a generic engine or scheduling correction. Keep the original camera, world
   events, music and once-only server-side script effects exactly as they are.
5. **Validate** on both Windows clients and record any remaining difference. Only then do the
   broad two-player gameplay regression.

## Known limitations (deferred; not to be solved in the cadence task)
* **Invulnerability:** the script's `player invulnerable` lines are gated off by the original
  code in multiplayer (`Player` event `0xc1a80`), so players are input-locked but not protected.
* **Join during a cinematic:** a late joiner sees the remaining broadcasts and nothing restarts.
  The policy is not designed or tested beyond that.
* **Readiness:** the barrier is "every present member spawned in the new world, plus 3 s",
  with a 30 s timeout. That is not a client acknowledgement.
* **Not yet run:** a two-player combat, death, respawn and reconnect regression.
* **Test mode only:** `CoopTestStartWorld=R1M3A` makes `!start` load R1M3A directly. The normal
  route start is R1M2B, and the intro allowlist is R1M3A only.

## Settings used for the successful run (`KPCServerTools.ini` in the test install)
```ini
[ServerTools]
Enabled=1
AllowCheats=1          ; disposable test server only
[Coop]
CoopSession=1          ; written by the launcher worker with -Coop
CoopMinPlayers=1
CoopMaxPlayers=3
CoopIntermission=0
CoopMusic=1
CoopIntro=1
CoopTestStartWorld=R1M3A
TraceTiming=1
TraceMusic=1
TraceCampaign=1
```
Server config: `maxclients 3`, `fraglimit 0`, `timelimit 0`, `keepitems 1`, port 42972. Map list:
`worlds\realm1\R1M2B`, `worlds\realm1\R1M2C`, `worlds\realm1\R1M3A`.

## Resume steps
1. In the worktree, run `git fetch origin` and check that `origin/coop/mvp-lobby` contains this
   file. The local branch may be named differently (it was `coop/mvp-slice2`, tracking
   `origin/coop/mvp-lobby`).
2. Check that the installed `server.dll` in the test install still has the SHA-256 above, and that
   no other dedicated server uses port 42972.
   * Extracting a Server Tools ZIP overwrites `server-template.cfg`, which resets the port to
     42972. An accidental second server on the same port probably stopped a running test server
     on 2026-11-11.
3. Builds use the Docker image `kpc-gcc14:local`. Build twice and compare both the DLL and the
   JSON manifest. Use `MSYS_NO_PATHCONV=1` under Git Bash, and bounded timeouts.
4. Start the test server with the launcher worker in co-op mode with cheats, for example:
   `powershell -File Start-KPC-Selected-Server.ps1 -Map "worlds\realm1\R1M2B" -Cheats Enabled -Coop`
   from the test install.

## Desktop + laptop test recipe
1. On both machines, start the game with `Psycho.exe` → **Play**. Join the server PC's address on
   port 42972 (`127.0.0.1` on the server PC itself). Optionally type `c_DebugMessages 1` in the
   client console (`~`).
2. Both players type `!ready`, then the host types `!start`. Expect R1M3A with no scoreboard,
   then "R1M3A intro starting (epoch 1, 2/2 players)".
3. Watch and listen on both machines: the camera pan and letterbox, the subtitle-versus-voice
   timing at each subtitle, and the window smash at script time 47.8 s. Expect "Intro finished"
   at the end.
4. Keep the server's `KPCServerTools.log` lines with `intro-` and `music-send`, plus screen or
   audio recordings of both clients.
