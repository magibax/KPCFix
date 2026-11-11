# KPCFix audit report (read-only, 2026-11-11)

**The shipped client is healthy, and the problems are on the server and co-op side.** `main` and `feature/coop-campaign` now have identical trees, so `main`'s tracked server DLL is the experimental co-op diagnostic build. Nothing in CI stops the next `server-tools-v*` tag from publishing it as stable Server Tools. No files were changed; the worktree and the game folder are as I found them. The full report and plan are saved at `C:\Users\magi\.claude\plans\kpcfix-comprehensive-imperative-perlis.md`.

## What I actually did
- **Git:** compared all branches and tags, diffed each release tag against HEAD, and listed CI runs and release assets with `gh` (read-only).
- **Code read in full:** the client C sources (`kpcfix.c`, `configs.c`, `hud.c`, `cinematic.c`, `ui.c`), four of the six client assembly hook files, `proxy.c`, six of the eight co-op headers (the allocator and model-state headers only in outline), both build scripts, the packaging tool, both workflows, all setup and launcher scripts, and the core docs.
- **Release checks:** downloaded the published client R4 and Server Tools R3 ZIPs to a temp folder (since deleted) and hashed their contents.
- **Game binaries:** hashed the installed files and decoded the original `object.lto` bytes at both campaign-trace patch sites by hand.
- **Tests run:** `coop/tools` passed 26/26 and `tools/` passed 6/6 (Python 3.10, Windows).
- **Not done:**
  - No client or server builds (no GCC or binutils installed).
  - No emulator runs (Unicorn not installed).
  - No C unit tests.
  - No Windows game testing.
- **Only skimmed:** `timing.S`, `ui_hooks.S`, `logging.S`, the region offset table in `profiles.h`, the test harness internals, the spawn analysis tools, and the long co-op research documents. This is not a complete audit of those.

## 1. Repository health
- **Client:** the published R4 `dinput.dll`, HEAD and your installed copy are the same file (`3d702051ÔÇª`). The INI defaults in the code match `CONFIGURATION.md`. Every patch checks file hashes and the original bytes first and rolls back on failure.
- **Server Tools R3:** the published DLL (`8de76e10ÔÇª`) is intact, and setup on HEAD still recognises it for upgrades.
- **Co-op:** the engineering is careful (hash gates, guarded memory reads, bounded searches, all off by default). The gaps are in isolation from releases and in evidence, not in unsafe code.

## 2. Confirmed defects and discrepancies

| ID | Severity | Where | Problem | Fix |
|---|---|---|---|---|
| H1 | High | `server-tools/KPCServerTools.dll`; `release-integrity.yml:58-96,119-138`; `RELEASING.md:84-87` | `main` and the co-op branch have identical trees. `main`'s server DLL (43 008 bytes, "spawn-list-control-v8") contains the co-op code. The release workflow rebuilds and publishes whatever is tracked, so a server tag would ship it as stable. This contradicts `RELEASING.md`. | Restore the stable server on `main` with new commits, or add a publish guard. Either way, CI should refuse a stable server tag whose DLL contains `[KPCFix Co-op]`. |
| H2 | High | `proxy.c:812`; `coop/README.md:89-93,113`; `CAMPAIGN-SYNC-DESIGN.md:19` | Two different DLLs (`23dd5de7` and `7ea0dfa4`) both log the build name "spawn-list-control-v8". Your installed server DLL is the earlier v7 build (`7112b5ed`); the log shows only v6 and v7 runs and no campaign-trace lines. So the v8 checks and the campaign trace (`TraceCampaign`) are written but have never been run on Windows. | Give every build a unique marker (for example the commit SHA). Record the DLL hash with every Windows observation. |
| H3 | High | `coop-spawn-trace-build.yml:16-20,71-88,109-113`; `build_dll.py:70-100` | The co-op workflow builds with Ubuntu's GCC 13.3, not the pinned GCC 14.2 (`RELEASING.md` says GCC 13 doesn't reproduce the server DLL). The build overwrites the tracked DLL and its manifest, then checks the new DLL against the new manifest, which always passes. Testers download an artifact that may not be the reviewed binary. | Use the pinned container step from the release workflow, compare against the tracked files, and upload the tracked DLL. |
| M1 | Medium | No `.gitattributes`; `package_release.py:76-80` | Git converts line endings on Windows checkouts (171 files), so 150 of 152 checksum entries look stale. The checksums hash the LF versions in Git. Two hand-edited checksum commits on GitHub (`7c7481e`, `671aad6`) coincide with a run of failed CI builds. | Add a `.gitattributes` that matches what is stored today exactly. `git add --renormalize .` must then show no changes. |
| M2 | Medium | CI; `tests/check_proxy.py`; `verification/SUMMARY.json` | Main CI runs no client behaviour tests, no co-op tests, and never executes `proxy.c` (only the synthetic `logging.S` harness). `check_proxy.py` doesn't cover any `[Coop]` setting. The recorded emulator runs are for an older DLL (`7b15e046`), not the shipping one (`3d702051`). | Add the asset-free test jobs to the release workflow; add co-op on/off cases to the private checks; record emulator runs for the current DLL. |
| M3 | Medium | `coop_spawn.h:27`; `KPCServerTools-default.ini:14`; `SPAWN-ENGINE-TRACE.md:41,47` | The code comment says spawn slot 2 is untested; the INI and docs say it was tested live. The doc says the committed DLL was "not rebuilt", which is stale. It also says the trace needs only `TraceSpawns=1`, but `proxy.c:807-810` turns it on through four other keys too. | Correct the comments and docs. |
| M4 | Medium | `Start-KPC-Selected-Server.ps1:20-22`; `Setup-KPCServerTools.ps1:55` | The launcher stops if `AllowCheats` is missing from the INI, and setup never adds it to an existing INI. Three docs and a CI check describe the workaround instead. The rewrite also leaves mixed line endings and saves the file as ASCII. | Have the launcher add the key when it is missing, keeping line endings. |
| L1 | Low | `MAINTENANCE-AUDIT.md:27,32`; `README.md:43`; `README-Server-Tools.md:61-82`; `CONFIGURATION.md`; `kpcfix.c:549` | Documentation drift (details below). | Consolidate (section 5). |

L1 drift details:
- `MAINTENANCE-AUDIT.md` lists the old R2 server hash and says GCC 13.3 reproduced the server, which conflicts with `RELEASING.md`.
- The READMEs link co-op docs on the feature branch, although `main` already contains them.
- Only 3 of the 9 `[Coop]` keys are documented outside the INI comments, and `CONFIGURATION.md` has no `[Coop]` section.
- The client log says "7 enemy health HUD sites", but 8 edits are made.

**Checked and found correct by reading the bytes:**
- **Campaign exit hook:** at `0x2d450` the original is `mov eax,[0x1016de60]; mov edx,[eax+0x1d0]; test edx,edx`, so the hook's replay of that load is correct and flag-safe.
- **Campaign intro hook:** at `0x1045ab` the original is `lea ecx,[esp+0x230]; push ecx; call 0xc50c0; add esp,4`. The wrapper's one-argument signature is correct, and the argument can never be NULL.
- **Client conventions:**
  - the HUD assembly bridges;
  - the movement-probe stack frame;
  - the DirectDraw method indices used for movies.
- **Server conventions:** the calling conventions of both hooked spawn routines.
- **Files and catalogs:**
  - the 13 legacy object hashes agree between the restore and version-check scripts;
  - all 59 maps in `maps.json` pass the launcher's path check;
  - the client and server ZIPs share no file paths;
  - your `server.original.dll`, `object.lto` and `dedicated.exe` match the supported hashes, and `object.lto` is unmodified.

## 3. Suspected problems (need a test to confirm)

| ID | Severity | Where | Suspicion |
|---|---|---|---|
| S1 | Medium | `kpcfix.c:449-463` | The enemy-HUD patch sites lack the "already patched" check every other site has. If the game loaded `cshell.dll` a second time while it was still patched, the client would show a false error box. This didn't happen in any of the 6 logged Windows sessions. Confirm with an emulator test that loads it twice. |
| S2 | Low | `proxy.c:656-659` | The campaign exit trace writes a log line on every trigger call with no cap, so the log could grow without limit. |
| S3 | Low | `proxy.c:686-692,740-747` | The spawn-warning trim and the trial slot reset run only on first install. If the server reloads `object.lto` per map (unverified), they won't run again. |
| S4 | Low | `Start-KPC-Selected-Server.ps1:63-66` | The launcher waits a fixed 3 seconds before typing `start <map>`, which can fail on a slow host. |
| S5 | Low | `configs.c:85-94` | A leftover `.kpc-inputrate.tmp` file after a crash would permanently stop the profile migration (the runtime lock still works). |
| S6 | Low | `Check-KPCServerVersion.ps1:58` | If an older proxy is still installed, the user gets "files do not match" rather than "rerun setup". |

## 4. Code consolidation opportunities
- **Proxy upgrade list:** `build_dll.py:80-98` adds every hash ever seen in the old manifest, including untested dev builds, and setup trusts all of them. Replace it with a fixed, commented list in source.
- **Duplicated files:** the SHA-256 code, the Windows header, the ELF-to-COFF converter (confirmed identical) and the two version checkers exist in both packages. Keep them so each package builds on its own, but add a CI check that they stay identical.
- **One hard-coded address:** `ui.c:27` uses an absolute address where everything else is base-relative. This needs a client rebuild, so it lands with the next client release.
- **Unused helper:** `Assert-KPCServerVersion` in the client checker isn't called by current scripts. Removal was already deferred; it needs a decision on old-script support.
- **Splitting `proxy.c`:** if co-op stays on `main`, move the roughly 590 lines of diagnostics into their own file.
- **`.gitignore`:** add one for build folders and Python caches.

## 5. Documentation consolidation
- One status table in `coop/README.md`, classifying each item as static analysis, emulated, Windows-tested (with DLL hash) or proposed.
- `CONFIGURATION.md` becomes the single reference for `[Coop]` keys.
- Mark `MAINTENANCE-AUDIT.md` as historical (R2 era).
- Move the "Graphics research" roadmap out of the user-facing README.
- Point co-op links at wherever the docs live after the isolation decision.

## 6. Build and release integrity
- The client toolchain is pinned and enforced. The server toolchain is enforced only in the release workflow. The Debian image and Actions versions aren't pinned exactly, so a future update would make CI fail rather than silently drift.
- **Client R4:** published before the release workflow existed, without a checksum asset. Its DLL matches HEAD; only docs and checksums differ.
- **Server R3 vs HEAD:** R3 has 31 files; HEAD would ship 47, adding the co-op sources, tests and DLL.
- The DDrawCompat DLL is fetched from the R4 release with a pinned hash, so CI breaks if that asset is removed.

## 7. Co-op status and risks

| Area | Status | Evidence |
|---|---|---|
| Missing multiplayer starts and single-player fallback | Traced | Static analysis plus Windows log |
| Three fixed spawn positions on R1M2B | Works for 3 players in one session | Windows, v6/v7-era DLLs |
| Ray query layout | Verified | Static plus Windows comparison |
| Main-world collision box check | Passed 4 controls | Windows (v6/v7) |
| Model registry / type surveys | Diagnostic only | Windows (v6/v7) |
| v8 spawn-list check; campaign trace | Written | **Not run on Windows** |
| General spawn allocator | Policy and unit tests only | **Not connected to the game** |
| Disconnect, reconnect, slot release, world reset | **Not implemented** (slots are keyed by player pointer and never released) | ÔÇö |
| Second spawn path (`0xf15d0`) | **Not hooked**; respawns there could stack on the anchor | ÔÇö |
| Campaign scripts, cinematics, level transitions, sync | **Design proposal only** | ÔÇö |

All co-op paths are off by default and gated on exact game hashes. With them off, normal multiplayer only gets extra original-byte checks at the spawn sites, which are harmless. The real risk is H1 (shipping it by accident), not runtime behaviour.

## 8. Plan for the next session
- **Phase 0 ÔÇö your decision:** see the question at the end.
- **Phase 1 ÔÇö no binary changes:**
  - Add `.gitattributes` (renormalising must show no changes) and `.gitignore`.
  - Make the documentation fixes.
  - Fix the co-op workflow (pinned container, compare against tracked files).
  - Add the asset-free test jobs to the release workflow.
  - Add a guard so a stable server tag can't contain co-op code, plus a check that duplicated files stay identical.
  - Refresh checksums only for files that actually changed, with the reason in the commit.
- **Phase 2 ÔÇö restore the stable server (if you choose Option A):**
  - Rebuild in the pinned container; the DLL should match R3 (`8de76e10ÔÇª`) or have a documented new hash.
  - Keep `7112b5ed`, `23dd5de7` and `7ea0dfa4` in the upgrade list so testers can switch back.
  - Run the synthetic checks for both regions and the four private proxy checks.
  - Test a Windows upgrade from v7 to stable, and an uninstall.
- **Phase 3 ÔÇö behaviour changes (each needs a rebuild, emulator run and Windows test before tagging):**
  - Fix S1 with a new double-load test.
  - Have the launcher add `AllowCheats`, then drop the workaround CI checks.
  - Clean up the stale temp file (S5).
  - Cap the campaign trace log (feature branch).
  - Make the `ui.c` address base-relative.
  - Ship the client fixes together as `kpcfix-1.13-r5` after the four-region emulator run and Windows playtests.
- **Phase 4 ÔÇö tidy-up:**
  - Replace the accumulated upgrade-hash history with a fixed list.
  - Split the co-op diagnostics out of `proxy.c` on the feature branch.
  - Decide on the unused client helper.
- **Phase 5 ÔÇö co-op baseline (feature branch):**
  - Rebuild with unique build markers.
  - Run the v8 checks and the campaign trace on Windows, recording the DLL hash each time.
  - Fill in the status table.

## 9. Testing strategy
- **Every push (CI, no game files):** byte-for-byte rebuilds of both DLLs, all asset-free Python and C tests, the synthetic server checks for both regions, checksum validation, a path-collision check, a duplicate-file check, and the stable-tag co-op guard.
- **Private (needs your game files):** the four-region client emulator run for every new `dinput.dll`, recorded against its hash, plus the server proxy and live checks with co-op settings on and off.
- **Windows:** the tables in `TESTING.md`. Every co-op observation must record the DLL hash and the build name from the log.

## 10. Readiness to resume co-op work
Not yet ready for feature work (automatic spawning, player lifecycle, campaign sync). Read-only diagnostic research can resume after Phase 0 and the Phase 1 CI fixes. Feature work should wait until:
1. co-op can't reach a stable release;
2. co-op CI builds and tests the tracked binary with the pinned toolchain;
3. each diagnostic build is uniquely identified, and the v8 and campaign traces have hash-tagged Windows results;
4. the disconnect/world-reset event and the second spawn path are traced.

## Decision needed before implementation
Which way should co-op be kept out of stable releases?
- **Option A (recommended, matches `RELEASING.md`):** restore the stable server code and DLL on `main` with new commits, and keep co-op on `feature/coop-campaign`. The catch is that any later merge of `main` into the feature branch will carry the revert, so co-op must be re-applied on purpose there.
- **Option B:** keep co-op on `main` behind a hard publish guard with a separate stable build output. This is a bigger architectural change.
