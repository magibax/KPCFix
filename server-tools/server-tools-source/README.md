# Build and test

In a repository checkout, change into `server-tools/`. In an extracted Server Tools ZIP, start in its root. The release build uses Python 3, GCC 14.2.0 with 32-bit x86 support and GNU binutils 2.44 on Debian trixie. The builder writes the DLL and metadata in place, so use a disposable copy for verification.

```
python server-tools-source/build_dll.py
```

Output is `KPCServerTools.dll` plus `SERVER_TOOLS_BUILD.json`. Build intermediates are under `server-tools-source/build/`; never distribute that directory (it includes an import-library generation stub).

`src/proxy.c` implements engine forwarding, strict binary identification and guarded runtime patching. `logging.S` holds the existing object-relative server hooks. `legacy-object-patches.json` supports one-time restoration of known previous patched modules and the hook regression fixture; normal launches do not apply it to files.

Run the synthetic assembly harness with Unicorn 2.1.4 and GNU binutils:

```sh
python server-tools-source/tests/run_checks.py
python server-tools-source/tests/run_checks.py --usa
```

The following additional checks require local original 1.13 game binaries:

```
python server-tools-source/tests/check_proxy.py PATH_TO_EDITION_FOLDER
python server-tools-source/tests/check_proxy.py PATH_TO_EDITION_FOLDER --cheats
python server-tools-source/tests/check_proxy.py PATH_TO_EDITION_FOLDER --off
python server-tools-source/tests/check_proxy.py PATH_TO_EDITION_FOLDER --bad-object
python server-tools-source/tests/check_proxy.py PATH_TO_EDITION_FOLDER --no-server-frame
python server-tools-source/tests/check_live.py PATH_TO_OBJECT_LTO PATH_TO_DEDICATED_EXE
python server-tools-source/tests/check_live.py PATH_TO_OBJECT_LTO PATH_TO_DEDICATED_EXE --disabled
```

Run the proxy/live checks for both Europe and USA profiles. Tests load executable code in an emulator with mocked Windows/engine APIs; they do not launch a Windows game. Verify installation, first map loading, chat, two connected players, live revocation while airborne, and clean shutdown on Windows before release. Verify `object.lto` SHA256 remains `3a03f8796c482b1a7ad72755df33ee8f1f8056bb10957f6f700e64bd006b3e5b` before and after starting a server.
