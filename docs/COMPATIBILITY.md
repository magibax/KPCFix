# Supported game builds

KPCFix supports game version **1.13** in four editions: Europe/UK, USA, Germany and Italy (Covermount). Disc versions (before 1.13) are refused.

All findings here come from the reference archive `KPCFix-region-research-final.tar.gz` (SHA-256 `e6edddf47e9ee8d20c05ef61d8b2c7b29b99ce352d187cbf9a0e7adeeaf76d89`). It holds the four disc installs and the same four updated to 1.13. Per-file hashes are in [region-files.json](region-files.json). The fingerprints the DLLs and scripts check are in `SUPPORTED_BUILDS.json` and `server-tools/SERVER_TOOLS_SUPPORTED_BUILDS.json`.

## Client

All four 1.13 editions have the same `client.exe` (720,948 bytes) and the same renderer, `d3d.ren`. Because of that, the timing fix, animation carries, input clock, Input Rate lock and multiplayer bind fix run the same code in every edition. Each disc edition has its own `client.exe`, and all four are refused.

The edition-specific code lives in `cshell.dll`:

| 1.13 edition | Shell size (bytes) | SHA-256 | HUD panel draw call (shell offset) |
|---|---:|---|---|
| Europe/UK | 1,282,100 | `703813a4dff8…` | `0x9F95B` |
| USA | 1,282,100 | `703813a4dff8…` | `0x9F95B` |
| Germany | 1,282,114 | `513f468dca85…` | `0x9F90B` |
| Italy Covermount | 1,192,001 | `a3f27e7bdf36…` | `0x9F8DB` |

Europe and the USA share every hook. Germany and Italy each have their own table of shell offsets in `source/src/profiles.h`. The tables cover:
- ground contact;
- HUD panels and glyphs;
- inventory carousel counts;
- crosshair anchoring;
- the settings menu;
- the cursor and tooltips.

The offsets are relative to the loaded shell, so they still work when Windows relocates it.

The Italian shell is not just a differently placed copy:
- It imports several C runtime functions that the other shells link in.
- Its Input Rate settings function reserves a 148-byte stack frame, where the others reserve 144. The Italian hook reproduces that frame exactly.
- Its menu layout writes widget positions through ESI where the others use EBP.

Adding the Italian hash to the UK table would not have worked.

`Psycho.exe`, `cres.dll` and `sres.dll` differ in every edition, but KPCFix doesn't depend on them. Leave your edition's copies in place. The Italian folders in the archive lack `ima.dll`; no hook needs it.

## Dedicated server

| 1.13 install | `dedicated.exe` (bytes) | `server.dll` (bytes) | Built |
|---|---:|---:|---|
| Europe, Germany, Italy | 69,632 | 323,584 | 24 Aug 2000 |
| USA | 81,920 | 335,872 | 21 Feb 2001 |

All editions use the same `object.lto` (`3a03f879…`).

The USA server is a later build. It also accepts `-logfile` and `-debuginfo`, which its manual documents.

**Server frame timing (`ServerFrameMs`) works on the Europe-family `server.dll` only (`83b7bce8…`).** In that build:
- the interface Update at `0x2358c` calls the frame function `0x2a05f(flags, time)` at `0x235b0`;
- `0x2a05f` clamps each step to at least 10 ms (`0x2a19c`–`0x2a1d0`);
- the server object keeps the time base at `+0x1f8` and the last frame time at `+0x218`.

The USA `server.dll` (`93178237…`) has different code, and the matching call, function and offsets have not been found yet. To add support:
1. Find those addresses.
2. Add a second profile in `install_server_frame` (`server-tools/server-tools-source/src/proxy.c`).
3. Extend the `ServerFrameMs` checks in `tests/check_proxy.py` to the USA fixture.

The 1.13 `server.cfg` and `server_bighead.cfg` are the same in every edition. The first selects game type 1 (deathmatch), the second type 2 (Conquer). The Server Tools launcher writes its own config and doesn't use either.

## Notes from the game's own documents

- **HUD.** `changes.txt` puts the HUD change in Beta 1: the developers stopped enlarging the HUD above 640×480. No later patch changes HUD scaling, which is why the native HUD is so small at high resolutions.
- **Input Rate.** The readmes describe the original trade-off: low settings feel tighter but less steady, high settings smoother but less responsive. The German disc `default.cfg` sets `InputRate 15.0`; the German 1.13 defaults don't.
- **Defaults.** At 1.13, Europe and the USA share one `default.cfg`. Germany and Italy share another; it lacks the UseRageOrb action and a joystick binding. The German and Italian 1.13 text files are identical and written in Italian.
- **Network.** The 1.13 notes mention a UDP packet-length fix and a dedicated-server crash fix. KPCFix's join fix is a separate client-side problem: an empty local IP now binds to the wildcard address. No match between servers of different editions has been tested.
