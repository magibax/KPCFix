# Claude audit — remediation and outstanding validation

Status: **2026-11-11, source and CI remediation applied; no new release tagged**.

Original audit: [KPCFix-audit-report.md](KPCFix-audit-report.md).
This is the action ledger; the original report is retained as a read-only
snapshot of the pre-remediation repository.

## Completed and verified by automated checks

| Audit topic | Remediation | Evidence |
| --- | --- | --- |
| H1: stable/experimental co-op mixing | Restored the **exact R3 Server Tools source/DLL/build manifest** to `main`, removed co-op source, tests and co-op workflow there; preserved the co-op work on its own branch (now `coop/mvp-lobby`) | [isolation commit](https://github.com/magibax/KPCFix/commit/c71159461131b7a3835cc07623b57a6d3d8c714b) |
| H1: accidental stable tags | Stable client/server tags must follow `main` history; CI checks stable server source/DLL for co-op markers on both branch workflow copies | [release workflow](.github/workflows/release-integrity.yml) |
| H3: non-reproduced experimental binaries | Co-op workflow uses the stable server's version-checked GCC 14.2/binutils 2.44 container, compares rebuilt DLL and JSON to **tracked** bytes, uploads only tracked outputs | [experimental workflow](https://github.com/magibax/KPCFix/blob/coop/mvp-lobby/.github/workflows/coop-spawn-trace-build.yml) |
| M1: Windows checkout hash drift | `.gitattributes` fixes LF for source and preserves exceptional checked-in CRLF blobs, `.gitignore` excludes build results. CI verifies `git add --renormalize .` is a no-op | [stable passing CI](https://github.com/magibax/KPCFix/actions/runs/42943941974) |
| M2: weak test gates | Stable branch now tests client/server reproducibility, server CPU harness, package manifests, shared C copies and Windows PowerShell INI migration; experimental branch runs asset-free Python/C regression tests and package verification | [stable CI](https://github.com/magibax/KPCFix/actions/runs/42944784869), [co-op CI](https://github.com/magibax/KPCFix/actions/runs/42944947551) |
| M3/L1: stale docs | Corrected R1M2B third slot and old rebuild statement, recorded evidence types, created canonical eight-key `[Coop]` config and moved graphics proposals out of install README | [co-op status](https://github.com/magibax/KPCFix/blob/coop/mvp-lobby/coop/README.md), [graphics research](GRAPHICS-RESEARCH.md) |
| M4: `AllowCheats` missing | Launcher inserts missing key under existing `[ServerTools]`, preserves newline style and drops a UTF-8 BOM, which hides `[ServerTools]` from `GetPrivateProfileIntW`; Windows CI parses every script in full and tests LF, CRLF, BOM removal read back through that API, Unicode and missing-section rejection | [test source](tools/Test-KPCServerIni.ps1) |
| S6: old proxy checker errors | Checker points to safe setup/backup recovery if official original `server.original.dll` is valid and active proxy is mismatched | [version checker](server-tools/Check-KPCServerVersion.ps1) |
| Upgrade hash accumulation | Co-op builder uses a fixed reviewed allowlist rather than silently importing arbitrary previous-manifest entries; tracked DLL and JSON still reproduce | [co-op builder](https://github.com/magibax/KPCFix/blob/coop/mvp-lobby/server-tools/server-tools-source/build_dll.py) |
| Product overlap and checksums | Separate product manifests refreshed **only for changed packaged files**. Stable CI still builds both products and packages independently. Published R4/R3 tags unchanged | [releasing guide](RELEASING.md) |

The backup branches created before remediation
(`backup/main-before-audit-remediation-20261111` and
`backup/coop-before-audit-remediation-20261111`) have since been removed
from the remote.

## Explicitly incomplete — runtime, emulator or manual gates

These require more than source review and must **not** be described as
fixed, live-validated or ready to ship:

- **H2 — provenance and v8 runtime tests.** Earlier v8 DLLs share a
  `spawn-list-control-v8` banner despite different SHA-256 hashes.
  Experimental CI now produces deterministic tracked binaries, and
  the [co-op evidence ledger](https://github.com/magibax/KPCFix/blob/coop/mvp-lobby/coop/README.md)
  requires a full DLL SHA-256 in every Windows observation. The current
  v8 spawn-list and `TraceCampaign` hooks still lack matching Windows
  runtime tests. A new embedded build-ID scheme requires a separately
  rebuilt and Windows-tested DLL.
- **M2 — current client emulator evidence.** The public CI does not
  contain the copyrighted original four-region client/server fixtures.
  The four-region Unicorn matrix and Windows/Wine client playthrough
  remain required for the currently shipped client and especially for
  any future modified `dinput.dll`.
- **S1 — enemy-HUD double-load idempotence.** Suspected only. Add a
  same-process double-load emulator regression and Windows test before
  changing the patch transaction.
- **S2 — unlimited campaign trace logging.** Rate-cap the experimental
  campaign exit hook in the next deliberately versioned/co-op DLL
  change; run a logging stress test and Windows regression.
- **S3 — object module/map reload.** Original module lifecycle and
  reservation reset are not proven; inspect and test before mutating
  spawn lifecycle hooks.
- **S4 — fixed startup delay.** Replace the launcher delay only after
  a reliable native console-readiness signal/retry strategy is verified;
  fixed-time observation alone is not evidence of failure.
- **S5 — stale Input Rate migration temp file.** Needs client build
  change, synthetic interrupted-write regression, four-region emulator
  run and Windows test.
- **Client `ui.c` base-relative address and HUD log site-count drift.**
  Group with a future `kpcfix-1.13-r5` source change and rebuild,
  not a documentation-only R4 update.
- **`proxy.c` split and stale helper.** Refactoring the experimental
  proxy and removing the older public checker helper both require
  explicit ABI/backward-compatibility decisions. Preserve the helper
  for old scripts unless incompatible use is proven.

## Release acceptance

No new client or Server Tools release tag has been created. Before a
future server release: execute the Windows migration/uninstall matrix
in [TESTING.md](TESTING.md), including v7/v8 -> stable R3, stock
`object.lto`, unknown active proxies, INI compatibility and disabled
pass-through. Record hashes, OS, map, client count, and run logs.

For client runtime changes, require the private original-fixture
Unicorn matrix and real Windows/Wine playtests. Never replace R4 or R3
assets or move their existing immutable tags.

For co-op, maintain independent builds from
`coop/mvp-lobby`; do not merge the stable Server Tools source
restoration into the feature branch wholesale. General automatic
spawning, lifecycle and campaign sync remain outside stable releases.
