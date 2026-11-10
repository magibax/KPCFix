# Verification

From the repository root build the client with the command in [source/BUILD.md](source/BUILD.md). Use a new output path. Compare both dinput.dll and SYMBOLS.json against their tracked counterparts. Build the server in a disposable copy of server-tools using its [build instructions](https://github.com/magibax/KPCFix/blob/main/server-tools/server-tools-source/README.md). Its builder writes the DLL and build report into the package. Compare both outputs before deploying.

## Automated checks

- With original patched and unpatched fixtures for all four regions and Unicorn 2.1.4, run source/verify.py as documented in the client build guide. It exercises region selection, rejection of altered files, timing, configuration I/O, networking, UI, enemy HUD and cinematics. The recorded verification/SUMMARY.json belongs to the earlier DLL identified by its own hash.
- Run the server assembly harness in both default and --usa modes. It needs Unicorn and GNU binutils and uses synthetic fixtures. Build the server first for check_live.py, which loads build/hooks.bin. Exercise check_proxy.py and check_live.py with both dedicated-server layouts, enabled and disabled paths and altered-object rejection.
- Stage the client and Server Tools separately. Check SHA256SUMS from the client extraction root and SHA256SUMS-Server-Tools from the server extraction root. The client checksum includes the external DDrawCompat ddraw.dll provided in the release. Verify distinct case-insensitive file paths before extracting both into one game directory.

## Manual regression checks

| Environment | Procedure and expected observation |
|---|---|
| Windows, each client shell layout | Run the checker, launch twice, confirm detection in KPCFix.log, then launch directly. Resolution and bindings persist. Test an unsupported input on a disposable installation and verify the refusal. |
| Windows and Wine, client | Compare capped 60 FPS and high refresh during narration, panning, animations, movement and key release. Toggle StockMouse. Check aiming, the Input Rate slider and saved bindings. Listen for stutter and stereo balance. |
| 1080p and 1440p | Compare HUD scales 100, 150 and 200. Check enemy bars and labels, cursor hotspot, tooltips at screen edges and 4:3 movies with filtering on and off. |
| Multiplayer | Query and join by direct IP immediately after a fresh launch. Repeat with two players and both server layouts. Check chat, cheat permission, revocation while airborne and clean shutdown. |
| Server installation and migration | Test clean install, recognised legacy object restore, existing proxy upgrade, repeat setup and uninstall. Compare stock object and backend hashes throughout. Confirm an existing active INI survives setup. |

Record toolchain, DLL hashes, regional files, OS or Wine version, renderer and observed results. Emulator assertions, byte-identical builds and native playtests answer different questions.

## Release package checks

From the repository root run `python3 -m unittest discover -s tools -p 'test_*.py'`. Build both archives using the [release procedure](https://github.com/magibax/KPCFix/blob/main/RELEASING.md). Verify the internal manifests, compare ZIP member paths case-insensitively across products and record the ZIP hash with the playtest results.

For an upgrade, test with the official engine already preserved as `server.original.dll`. A recognised active proxy should upgrade in place. An unrecognised active `server.dll` should stop setup without replacing it. With a verified original backup, move the active DLL aside and rerun setup. Confirm the original engine hash and active INI survive both paths.
