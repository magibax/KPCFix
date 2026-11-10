# Building the Windows proxy

From the package root, run:

```sh
python3 source/build.py --build-dir build --output build/dinput.dll
```

Use Python 3, GCC with 32-bit x86 support, and GNU binutils with the i386 PE target. The build creates a freestanding PE32 Windows DLL and its symbol map. The output must be a new file. The temporary Windows import-library stubs in the build directory are build tools only. Deploy the finished `dinput.dll`.

`src/profiles.h` contains the exact 1.13 client expectations and the three shell layouts. `src/kpcfix.c` validates the files, chooses a layout and installs the instruction edits. `hud.c` and `ui.c` use that layout for calls back into the game. The assembly files preserve the x86 calling conventions, including the different Italian stack allocation. `SUPPORTED_BUILDS.json` is the corresponding readable fingerprint list used by the setup scripts.

## Replaying the supported-build checks

The Python checks use Unicorn 2.1.4 plus local copies of the original game files. To replay them from the game research archive, run this from the package root. The script takes the two game binaries for each test from the archive, extracts them to a temporary directory, then removes the temporary files at exit. With Unicorn already installed, the dependency directory argument is optional.

```sh
python3 source/verify.py --archive KPCFix-region-research-final.tar.gz --output verification-local
```

The checked DLL is the package-root `dinput.dll`; the matching symbol map is `source/SYMBOLS.json`. To check a new build, copy both of those files from that build before running verification. The disassembly audit additionally requires `objdump`. Checks execute the proxy and original game code in an x86 emulator; Windows I/O, presentation, and socket services are simulated.

The R2 client hook fits fullscreen Smacker playback to 4:3 and optionally smooths the movie. The edition-specific shell hooks scale the enemy frame, fill and label. At HUD scales other than 100%, the label uses the row midpoint and font height.
