# Architecture

## Client startup

The local 32-bit dinput.dll exports DirectInput factories and forwards calls to the Windows system implementation. DllMain records the module and disables thread notifications. The first factory call resolves the backend and initiates game setup. Separate one-time states distinguish backend availability from game-patch success. Non-game hosts and disabled game fixes can use forwarding. The proxy reports S_FALSE from DllCanUnloadNow. Installed hooks depend on its code remaining resident.

Check-KPCVersion.ps1 reads SUPPORTED_BUILDS.json for the install-time check. The DLL independently hashes client.exe and the on-disk cshell.dll, checks file and PE layout, and checks original instructions before writing. All four editions share the client. Europe and USA share a shell layout, while Germany and Italy each have a separate layout in source/src/profiles.h. The loaded shell can already exist or arrive through the client's LoadLibraryA hook. Preserve both paths, expected instruction bytes, calling conventions and edition-specific stack layouts.

## Runtime responsibilities

| Area | Implementation and boundary |
|---|---|
| Timing and movement | kpcfix.c selects edit sites, timing.S supplies the clock guard and eight independent fractional carries, and ground.S handles shell movement. Sharing accumulators would couple unrelated animation sites. minstep_hook.S replaces the clock guard in the same client.exe slot (0x664b2) when LocalServerMinStepMs is set: the local server skips frames shorter than the minimum and carries their time into the next, because the clock guard removes the engine's 10 ms floor and some AI stalls on shorter steps. |
| Console | console.c drops the character the console toggle key types into the console (client.exe 0x6dc9 key-down and 0x700a WM_CHAR test). |
| Mouse and profile settings | input_interval.S handles input event intervals. kpcfix.c owns sensitivity constants and lock policy, ui.c and its assembly bridges handle menu controls, and configs.c handles persistent InputRate edits and backups. |
| HUD, menus and movies | hud.c, ui.c and their assembly bridges call the detected shell layout. Enemy bars and labels have edition-specific sites. cinematic.c and cinematic_hook.S handle fullscreen movies and the filtered blit fallback. |
| Rendering and audio stutter | The separately distributed DDrawCompat wrapper reads DDrawCompat-CLIENT.ini. Its presentation, scheduling and filtering settings are independent of the game patch installer. |
| Multiplayer client | The empty local bind address fix allows an initial wildcard bind. Optional NetTrace instruments socket calls for diagnostics. The active implementation retains the browser clock. |

The client stages guarded edits and has protection restoration, instruction-cache flushing and rollback paths. Client and shell installation are separate operations. A failure in the later shell stage must be diagnosed separately from earlier client edits.

## Dedicated server

Start-KPC-Selected-Server.ps1 imports the package-local Check-KPCServerVersion.ps1 and calls both its game and server checks. Running the checker file directly executes only its client and shell check. The client checker still defines a server-check helper that current client entry points do not call. Older dot-sourced launch scripts may refer to it.

The separate setup script verifies the installation, preserves the engine as server.original.dll and deploys KPCServerTools.dll as server.dll. Restore-LegacyObject.ps1 recognises historical disk patches. The legacy manifest also supplies trampoline bytes at build time and test data for check_live.py.

CreateServer initialises the proxy and forwards to the original engine. Known dedicated hosts must match the corresponding backend. Other hosts can forward through a recognised backend. For an active dedicated host the proxy intercepts backend lookup of ObjectDLLSetup, checks the object and installs the assembled hooks in memory. DeleteServer forwards to the backend. Missing or incompatible inputs can terminate the server through its fatal path. logging.S implements chat, console handling and player cheat state. R3 trims the extra line break in the missing multiplayer start warning in the loaded object module. On the Europe-family server.dll the proxy also redirects the engine's frame call (0x235b0 to 0x2a05f) and skips frames shorter than ServerFrameMs, so the server simulates real time in steps of at least 16 ms. The USA server.dll is not supported yet and keeps its original timing (see COMPATIBILITY-NOTES.md). On the co-op branches the same hook is `[Coop] CoopServerFrameMs` and applies only during co-op sessions. The GUI generates its own server configuration and queues the first map.

## Build and distribution

Each package builds separately with Python, GCC and GNU binutils. The builders convert freestanding x86 objects to COFF and link Windows PE32 DLLs. Generated import stubs are build intermediates. The client symbol map and server build metadata must match the DLL under test. The two copies of the SHA implementation and lightweight Windows headers allow either source package to build after separate extraction. Keep matching fingerprint data and shared code in sync when changing either package.

Both targets are Windows DLLs. Building them on Linux exercises the cross-build toolchain. Running the game under Wine also requires runtime verification of DLL selection, rendering, input and audio.
