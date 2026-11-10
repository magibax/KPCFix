# Runtime DLL feasibility

The supplied binaries support moving these patches into memory and keeping the original `object.lto` unchanged on disk. This archive still uses an on-disk installer; a runtime DLL has not been implemented. Live cheat permission now changes in memory through the host console, independently of the proposed DLL conversion.

## Verified loading path

- Both dedicated executables load `server.dll` and resolve `CreateServer` and `DeleteServer` dynamically.
- Both official server DLLs export those two named functions, although their internal code/layout differs.
- The engine loads `object.lto`, resolves `ObjectDLLSetup` through `GetProcAddress`, and calls it during initialization. This is an early interception point, before normal player and chat callbacks.
- The four supplied 1.13 region copies of `object.lto` are identical. Existing gameplay hooks therefore have one common profile; engine loader integration needs separate checked Europe/US profiles.
- Existing hooks already calculate addresses relative to the loaded object module, so they can be reused at its actual load address.

For reference, in the supplied Europe `server.dll`, RVA `0x1e1c` references `ObjectDLLSetup`, `0x1e24` resolves it, and `0x1e50` calls it. These are evidence for a loading boundary, not universal patch addresses for other engine builds.

## Recommended design

A small `KPCServerTools.dll` loaded by a dedicated server launcher would install a checked hook at the engine's object-loading boundary. Once the original object is loaded and identified, it would temporarily change memory protection, apply the verified hooks in memory, restore protection and flush the instruction cache. It should fail closed on unknown builds. Disabling it would be a launcher/config choice followed by restarting the server.

This keeps `dedicated.exe`, `server.dll` and `object.lto` unchanged on disk, and avoids permanently enabling the server changes in local single-player sessions. The launcher needs a deliberate DLL-loading mechanism: Windows will not automatically load a DLL merely because it is beside the game. A deterministic loading boundary is preferable to a background thread polling for the module.

A second option is a `server.dll` proxy exporting `CreateServer` and `DeleteServer`, forwarding to the matching original engine under another filename. That needs a one-time rename of the original server DLL, but leaves its bytes and `object.lto` intact. It would still need checked runtime hook installation, shutdown handling and engine compatibility tests. An installer would also update the version checker to recognize this layout.

## Important implementation differences

An in-memory installer must validate relocated instruction operands rather than compare all mapped bytes directly to the file image. In particular, the original noclip gate contains a relocated absolute address. The current file patch removes its relocation entry because the replacement instruction no longer uses it; a runtime patch runs after relocation and must not apply that file-relocation edit. File offsets also differ from RVAs in later sections. PE header and relocation-table edits from `patch.json` must not simply be copied wholesale into memory.

A production DLL conversion therefore needs its own installer, lifecycle and tests. The identified entry points establish feasibility; they do not constitute a Windows-tested DLL implementation.
