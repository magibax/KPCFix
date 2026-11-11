# Co-op next-session handoff (2026-11-11, campaign session: M0 inventory + M1 campaign graph)

A self-contained resume point. The design and evidence live in:
* [CAMPAIGN-ARCHITECTURE.md](CAMPAIGN-ARCHITECTURE.md): milestones, protocol and UI designs;
* [CAMPAIGN-MAP-INVENTORY.md](CAMPAIGN-MAP-INVENTORY.md): all worlds and transitions;
* [COOP-MVP.md](COOP-MVP.md): sections C–H, the earlier verified features and pass 2.

## Where things stand

* **Branch:** `coop/mvp-lobby`, the head of PR #10 into `feature/coop-campaign`. The stable
  `main` is untouched. The local worktree branch is `coop/mvp-slice2`, tracking
  `origin/coop/mvp-lobby`.
* **This session:** commit `1524fb6` (M1 code) and the docs commit that follows it.
* **Current DLL:**
  * `KPCServerTools.dll` = `server.dll`
    `009eacd65ebc03627389a0bcac901193e33393b3188d1445d70d3d130546ab71`
  * manifest `d9b31fee880c1a447188cc68f3bcc35cb9a3e54958aaf4ab0517b37088fe6a82`
  * Built twice with byte-identical results in the pinned image.
  * It contains pass 2 unchanged, plus the opt-in campaign graph.
  * With `CoopCampaign` absent or 0, it behaves like pass 2 (`1b8816cb…`).
* **Installed:** in the disposable `C:\KPCTest\smoke3\Psycho` (port **42990**). Its INI has
  `CoopCampaign=1` and `TraceSpawners=1`. No server is left running.
* **Not touched:** `C:\KPCTest\smoke1\Psycho`. The user's `dedicated.exe` (PID 30564, port
  42972, DLL `e86bb352…`) was running all session and was neither stopped nor modified.
* **Backups** (in `C:\KPCTest\backup`):
  * `smoke3-1b8816cb-before-m1`;
  * `smoke1-e86bb352-before-pass2`;
  * `smoke3-13ca1d0a-before-pass2`;
  * older copies.

## Validation status

| Item | State |
| --- | --- |
| Transitions R1M2B → R1M2C → R1M3A, music, R1M3A intro, script cadence (`CoopScriptClock`) | **User-confirmed** (desktop + laptop) |
| Pass 2: `CoopSoundClock`, `CoopHealthBars`, `CoopSpawners`, `CoopStartScoreboard` | Implemented; unit/emulator/native-server tested; **two-PC test pending** |
| M0 world inventory (48 worlds; exits = WORLDORDER chain; R?M0 intros; Nightmare; loading backgrounds) | Done (static data/RE). The table matches the installed REZ |
| M1 campaign graph, `!campaign …`, `!abort`, completion → lobby, R?M0 intro exits | Implemented; 12 unit files × 64/32-bit, emulator 34/34, native Coop/Normal/PassThrough smoke. **No client has played it** |
| M2 loading screens, ready ACK; M3 15 s hold and freeze | Researched and designed (CAMPAIGN-ARCHITECTURE §4–6); **not implemented** |

## Tests run this session
* Unit tests (pinned image, `-Werror`, 64- and 32-bit): 24/24, including the new
  `test_coop_campaign.c`.
* `check_campaign_table.py` against `C:\Games\Psycho\PSYCHO.REZ`: PASS, 48 worlds.
* Emulator `check_proxy.py`: 34/34 modes, including `--campaign`,
  `--campaign --lobby=deathmatch/PM01`, `--campaign --no-start-scoreboard` and
  `--campaign --intro --test-start=R1M3A`.
* `package_release.py --component server`: PASS (55 files). The DLL and manifest were rebuilt
  twice with identical results.
* Native `smoke3` (port 42990, no clients):
  * Coop with `CoopCampaign=1`: PASS, exit 0, `effective CoopCampaign=1 …`.
  * Normal and PassThrough: PASS.

## Two-PC tests for the user (in this order)

**Install** (only after the user's own server is stopped):
```powershell
$d='C:\KPCTest\smoke1\Psycho'; $s='C:\Projects\KPCFix-audit\server-tools'
Get-Process dedicated -ErrorAction SilentlyContinue | Format-Table Id,Path   # nothing from smoke1
Get-NetUDPEndpoint -LocalPort 42972 -ErrorAction SilentlyContinue           # must be empty
Copy-Item "$s\KPCServerTools.dll" "$d\KPCServerTools.dll" -Force
Copy-Item "$s\KPCServerTools.dll" "$d\server.dll" -Force
Copy-Item "$s\SERVER_TOOLS_BUILD.json" "$d\SERVER_TOOLS_BUILD.json" -Force
(Get-FileHash "$d\server.dll").Hash      # 009EACD6...
```
Alternatively, use `smoke3` (already installed, port 42990): clients join port 42990.

**Rollback:** copy the three files back from `C:\KPCTest\backup\smoke1-e86bb352-before-pass2`.

**Test 1: pass 2 (unchanged recipe), with `CoopCampaign` absent.**
* INI: the existing `smoke1` keys, plus `TraceSpawners=1`.
* Start: `Start-KPC-Selected-Server.ps1 -Map "worlds\realm1\R1M2B" -Cheats Enabled -Coop`.
* Both machines `!ready`, the host `!start` (test start R1M3A), recording both screens with
  sound. Check:
  1. **Scoreboard:** up at `!start`; blank during the load (stock client); back after entry;
     closes at the first cinematic frame; no click.
  2. **Narration:** continues past "…useful ally for you here, as the way in is not as straight
     as it first seems".
  3. **spawn6 enemies:** appear after the smash, the same on both machines.
  4. **Enemy health bars:** shown on both screens and they disappear correctly.
  5. **Regression:** no scoreboard on natural exits; music and transitions as before.

**Test 2: M1 campaign.**
* INI: add `CoopCampaign=1`, `CoopLobbyWorld=worlds\realm1\R1M0`, `CoopIntro=1`, and remove
  `CoopTestStartWorld`.
* The launcher still starts R1M2B as the lobby world.
* Steps:
  1. `!campaign` on both machines: "Selected: full campaign (R1M0..R5M1B, 48 worlds)".
  2. The laptop types `!campaign realm fire`: refused (not host).
  3. Host: `!campaign full`, then both `!ready`, then host `!start`.
     * Expected: R1M0 loads via `maplist slot=… world=worlds\realm1\r1m0` (log).
     * The Water-realm opening intro plays (Madam Raven, ~79 s, `CoopSoundClock` keeps the
       narration whole).
     * The intro's exit then takes everybody to R1M1A (log `exit … activator=…`).
  4. Play R1M1A → R1M1B → R1M2A on natural exits.
     * Note anything that differs from single player: enemies, pickups, doors, hints, music,
       stuck spots.
  5. Host `!abort` → everybody returns to R1M0 as the lobby (no intro there); `!coop` shows
     `LOBBY R1M0`.
  6. Optional:
     * `!campaign from demon` + start: R4M0 intro (~72.5 s) → R4M1A;
     * `!campaign realm nightmare`: R5M1A.
* Send back the full `KPCServerTools.log`, the INI, recordings and notes.

## Open limitations
* **No world after R1M3A has been played in co-op.** The other intros (except R?M0 and R1M3A)
  are skipped unless `CoopIntroAll=1` (test only).
* **Lobby:** the launcher forces R1M2B as the co-op start world. There is no scoreboard in the
  lobby yet; the design is in §3.
* **Loading screens and readiness:**
  * the stock client shows black plus chat during loads;
  * the PCX needs the KPCFix client hook (§4, not implemented);
  * readiness is still spawn + 3 s settle. The spawn is client-originated, but the ACK of `0x69`
    is still missing (§5);
  * there is no 15 s hold and no freeze (§6).
* **Gameplay:**
  * no world-map screens between stages in multiplayer;
  * Nightmare loadout (`cheatscript`) not reproduced;
  * invulnerability and `disable_ai` still gated (setter `0x6bd20` usable; §7);
  * persistence, deterministic enemy targeting and difficulty scaling not designed.
* **Earlier limits still apply:** the pass-2 client-test items; the pre-existing
  `object.lto+0x87c41` crash on rapid empty-server nextlevels (not seen in this session's
  runs).

## NEXT SESSION
* **Resume:** `cd C:\Projects\KPCFix-audit; git fetch origin; git status -sb`, then read this
  file.
* **Next milestone: M3/M2 server side**, in this order:
  1. the server.dll paused-tick hook `0x2a292`;
  2. the pause flag via SetServerFlags;
  3. the OnMessage hook `0xfe40b` (observe and swallow during the hold);
  4. the echo-`0x17` ready ACK;
  5. the 15 000 ms wall-clock hold with chat status and music delivery while paused.

  Then the KPCFix client `PreLoadWorld` hook for the PCX loading screen, and the launcher's
  R1M0 lobby start.
* **First test to run:** the user's two-PC **Test 1** above (pass-2 validation), because every
  later step builds on those fixes.
* Tooling (outside the repo, in `C:\KPCTest\pass2`):
  * `build.sh <dir>`: disposable pinned build;
  * `matrix.sh <dir>`: emulator, 34 modes;
  * `unit.sh`: all C tests;
  * `sums.py`: regenerate `SHA256SUMS-Server-Tools` from the index.
* RE helpers in `C:\KPCTest\tools`: `ru.py`, `rez.py`, `wavenv.py`. Research notes are in
  `C:\KPCTest\scratch-*`. Never stop a user's server: check `Path` and port first.
