# KPCFix compatibility notes for game version 1.13

The reference is `KPCFix-region-research-final.tar.gz`, SHA-256 `e6edddf47e9ee8d20c05ef61d8b2c7b29b99ce352d187cbf9a0e7adeeaf76d89`. It contains four disc installations and four installations updated to 1.13. The analysis covered all 40 TXT and CFG files across the eight folders. The per-file hashes are in `verification/reference-files.json`.

## Which game files determine support?

All four 1.13 installations contain the same 720,948-byte `client.exe` as the working UK reference. The renderer (`d3d.ren`) is also identical across all four. This lets the timing fix, animation carries, input clock, console InputRate lock and fresh-start network bind correction run the same client instructions in every edition. The original disc client hash is different in each region. All four disc clients are rejected.

| 1.13 edition | Shell bytes | Shell SHA-256 prefix | HUD panel draw call, relative to the shell |
|---|---:|---|---|
| Europe / UK reference | 1,282,100 | `703813a4dff8` | `0x9F95B` |
| US | 1,282,100 | `703813a4dff8` | `0x9F95B` |
| Germany | 1,282,114 | `513f468dca85` | `0x9F90B` |
| Italy Covermount | 1,192,001 | `a3f27e7bdf36` | `0x9F8DB` |

Europe and the US can share every hook. Germany and Italy each have a separate table of shell addresses for ground contact, HUD panels and glyphs, carousel counts, crosshair anchoring, the settings menu, cursor and tooltips. The addresses are offsets from the loaded module, so the table also works when Windows relocates the shell. The DLL checks the original instructions before writing the hooks. Full fingerprints are in `SUPPORTED_BUILDS.json`.

The Italian shell imports the Microsoft C runtime for several functions that the English and German shells link into their own binaries. Its Input Rate slider calls a settings function with a **148-byte** local stack frame. The English and German functions reserve **144 bytes**. The Italian hook reproduces that allocation exactly and resumes the original function. Its menu layout also uses ESI where the other builds use EBP to write widget positions. Each profile preserves the original register and changes only the vertical position. This is why an Italian hash added to the UK table would have been insufficient.

`Psycho.exe`, `cres.dll` and `sres.dll` each have four different hashes at 1.13. Leave the launcher and resource DLLs from the installed region in place. The supplied Italian folders omit `ima.dll`. That is an observation about the archive contents. Neither the client nor shell hooks depend on that file.

## HUD and input in the game documentation

The shared `changes.txt` puts the HUD change in the **Beta 1** history. The developers stopped enlarging the HUD above 640×480 and added clipping for long names on the multiplayer scoreboard. Later notes describe clearing the scoreboard between sessions. The entries for 1.12 and 1.13 list no further HUD scaling change. This is consistent with the small native-resolution HUD in the screenshots. KPCFix scales the panel destinations and bitmap glyphs and keeps the count labels anchored to the projected inventory items.

The same history says mouse-wheel binding was added late and was absent from an early German release. The corrected German **1.13** mouse menu has the slider and widget paths used by the fix. The German disc `default.cfg` explicitly sets InputRate to 15.0. The supplied German 1.13 defaults omit that line. The setup script edits screen dimensions in `autoexec.cfg` and existing profiles. The optional input normalizer edits InputRate there when the lock is enabled.

At 1.13, Europe and the US share one `default.cfg`. It contains the UseRageOrb action and a joystick Z-axis binding for NextArmor that are absent from the German/Italian defaults. The German and Italian 1.13 `default.cfg`, `readme.txt` and `help.txt` are byte-identical to each other in this archive. Both sets of text documents are in Italian, while the original German disc documents are in German. The English disc README mentions US keyboard layout limitations. Its replacement discusses missed input events under Windows 2000 instead. These documents cannot identify a binary build.

Across the help files, “Disable Optimized Surfaces” is a troubleshooting option. The English text specifically warns of a slowdown when the HUD or text is drawn. Its newer edition also revises the TNT2 driver notes. The readmes describe the original Input Rate tradeoff: low settings feel tighter but less steady, high settings smoother but less responsive. Their claim that a dedicated server is forthcoming predates the dedicated executable and its separate manual.

## Dedicated server

| 1.13 install | `dedicated.exe` | `server.dll` | Date in the executable |
|---|---:|---:|---|
| Europe, Germany, Italy | 69,632 bytes | 323,584 bytes | 24 Aug 2000, 16:18:18 |
| US | 81,920 bytes | 335,872 bytes | 21 Feb 2001, 13:55:24 |

Both dedicated executables pass **3** to the “Server Version” banner. They contain different executable code. The US build parses `-logfile` and `-debuginfo`; its manual adds those two switches. The three other manuals and server pairs match exactly, and the switches are absent from their executables. All four use the same `object.lto`. The server launcher checks for a supported set before starting.

The common patch notes list a 1.12 server crash when a jack-in-the-box exploded after its owner left, and a 1.13 UDP packet-length fix, dedicated-server diagnostics, and console access to variables such as UpdateRate. The US and European-family `server.dll` both pass the caller's packet length to the send and receive wrappers. The changelog alone cannot establish which internal packet-construction paths differ. KPCFix's fresh-start browser fix addresses a separately identified **client-side** problem: an empty local IP now binds to the wildcard address. A live cross-region match is still needed to establish network interoperability.

`server.cfg` and `server_bighead.cfg` match across all four 1.13 folders. The standard file chooses game type 1 (regular deathmatch). The file named “bighead” chooses type 2 (Conquer deathmatch). Both contain two spawntype assignments. The separate Server Tools launcher writes its own configuration with one spawn-type assignment. The dedicated manual distinguishes the interactive `start` command from variables accepted in a config file. Hosting scripts are supplied only in the separate Server Tools package. Its graphical launcher loads its generated configuration and queues the selected first map in the console.

## Validation

The tests load the shipped DLL and execute original 1.13 x86 routines with mocked Windows, graphics and socket services. They exercise the slider and its stock switch, both cursor images and tooltip bounds, ground movement, fractional animation timing, each of the 13 HUD redirects, the original panel loop, projected inventory counts and the direct-IP address builder. The compiled file reader and SHA-256 accept each stock 1.13 pair. A same-size shell with one altered byte is rejected. The disc files are also exercised as rejected inputs. See `verification/SUMMARY.json` for the per-region results.

Native visual presentation through DDrawCompat, actual mouse feel, Windows launcher execution and live multiplayer between server editions remain playtest items.

The summary and logs under verification/runs are retained from the first KPCFix release and identify its DLL by hash. R2 adds checks for enemy bar geometry, edition-specific label placement, fullscreen movie placement and the movie filter fallback. The original client is shared across regions. Enemy HUD hooks use the separate shell layouts in profiles.h.

