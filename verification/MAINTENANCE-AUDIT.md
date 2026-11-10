# Maintenance audit of the 1.13 packages

Baseline: main at 3cb17e94287d5c07ecb0344eb2d92ad5d668c36f. The tree had 133 files and the visible history had 33 commits. Its latest changes split server hosting from the client and renamed colliding package files. The release tags point to earlier commits. A tag checkout and a subsequently replaced release attachment can have different contents.

## Findings and disposition

| Subsystem | Finding and decision | Regression risk and verification |
|---|---|---|
| Packaging | **Update.** The root checksum list retained the whole server subtree after the split. Give each package its own file list. The external DDrawCompat DLL belongs to the client package. | Low. Check separate stages and both combined installation orders, including case-insensitive paths. |
| Client verification | **Update.** The regional suite stopped on a log string that the current binary does not emit. Require the full 13-site fixture and decode every installed branch destination. | Low. The change affects the test harness. Repeat the four-region matrix and both load addresses. |
| Configuration | **Document.** Record the HUD's shipped and missing-key values, input dependencies, setup-only ForceDeveloper, renderer overlay precedence and the server GUI's startup policy. | Low. Trace each owner from its entry point. |
| Client and server documentation | **Update.** The extracted client README referenced a guide under a separate package tree. Link the hosted guide and provide contributor references for both packages. | Low. Inspect README links and standalone package contents. |
| Historical evidence | **Retain.** verification/SUMMARY.json identifies an earlier DLL. The original runs remain evidence for that binary. The CPU checks for cinematics and enemy HUD postdate it. | Mistaking old evidence for current test results obscures regressions. Compare binary hashes when citing a run. |
| Legacy object data | **Retain.** The restoration script consumes stock bytes, the server builder extracts a trampoline, and check_live.py uses the legacy states as fixtures. | High. Removal would affect builds, migration and live-hook verification. The checker and restore script recognise the same 13 legacy hashes. |
| Package-local C headers, SHA and fingerprint lists | **Retain.** The shared portions match today. Each package's build and checks use its own copies after extraction. | Cross-package relative paths would break standalone workflows. Compare the copies and both resulting DLLs on future changes. |
| Client server-check helper | **Defer.** The current client entry points call only Assert-KPCGameVersion. The independently packaged launcher calls its local server checker. Older dot-sourced scripts could still call the client copy. | Confirm the intended legacy-script support policy before deleting a callable function. |
| Patch lifecycle | **Defer.** The client has separate startup and late-loaded shell transactions. The dedicated proxy has different fatal and pass-through paths. | High. Combining installers would alter failure and loader behaviour. Add targeted failure injection and native testing before changing them. |

## Change inventory

The branch changes the client manifest, one regional harness assertion, the ForceDeveloper INI comment and the README. It adds ARCHITECTURE.md, CONFIGURATION.md, TESTING.md and this report. Removed material consists of server entries in the client file list and the obsolete log wording assertion. Runtime sources, patch offsets, DLLs and shipped setting values retain their baseline Git blob identities.

## Verification record

The client and server rebuilt as PE32 DLLs with GCC 13.3.0 and GNU binutils 2.42. No compiler warnings were observed. The builds reproduced the tracked DLLs, client symbol map and server build report byte-for-byte.

| Output | SHA-256 |
|---|---|
| dinput.dll | 3d7020511dd1b223cab2ed05a0180879ae7271d549a3a9324e9ef0a6b82a56e3 |
| KPCServerTools.dll | bac7696bc79f10b637134b7bbffea1ead01e67c8207452f5c3f339a8b5a353d4 |
| Original regional fixture archive | e6edddf47e9ee8d20c05ef61d8b2c7b29b99ce352d187cbf9a0e7adeeaf76d89 |

- **Client emulation.** Unicorn 2.1.4 passed 15 suite invocations on each of Europe, USA, Germany and Italy-Covermount, plus the altered German shell rejection (61 total). Coverage included startup at two load addresses, independent timing carries, movement and mouse intervals, HUD and carousel drawing, enemy labels, movie blit fallback, profile I/O, direct-IP address construction and the compiled file-hash gate.
- **Server emulation.** On Europe and USA, the default and --usa synthetic harnesses passed. Four proxy runs per layout covered normal, cheats, disabled and altered-object configurations. Two live-control runs per layout covered enabled and disabled policy. All 14 invocations passed.
- **Static and component checks.** All seven PowerShell scripts parsed, Python sources compiled, and JSON files parsed. The three shell fingerprints and common client hash match the C table. All 59 distinct map paths pass the worker's selection pattern. The C SHA implementation passed 33 comparisons with Python hashlib, including padding and chunk boundaries.
- **Packaging.** Both installation orders were staged from manifests with a hash-matching copy of external ddraw.dll. Every listed file verified and no files collided. GitHub release asset metadata was inspected. The staged packages are working-tree reconstructions.
- **Runtime limits.** Native Windows and Wine startup, installation and restoration, input feel, audio output, DDrawCompat presentation and live cross-region multiplayer require playtesting. See [TESTING.md](../TESTING.md). Separate packages still lack a tracked end-to-end release assembly recipe, and the published ZIP bytes were not independently replayed in this audit.
