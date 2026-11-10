# Build and test

Build the 32-bit Windows proxy with Python 3, GCC (including `-m32`) and GNU binutils:

```
python server-tools-source/build_dll.py
```

Output is `KPCServerTools.dll` plus `SERVER_TOOLS_BUILD.json`. Build intermediates are under `server-tools-source/build/`; never distribute that directory (it includes an import-library generation stub).

`src/proxy.c` implements engine forwarding, strict binary identification and guarded runtime patching. `logging.S` holds the existing object-relative server hooks. `legacy-object-patches.json` supports one-time restoration of known previous patched modules and the hook regression fixture; normal launches do not apply it to files.

With the optional Python `unicorn` package and original 1.13 binaries available locally:

```
python server-tools-source/tests/check_proxy.py PATH_TO_EDITION_FOLDER
python server-tools-source/tests/check_proxy.py PATH_TO_EDITION_FOLDER --cheats
python server-tools-source/tests/check_proxy.py PATH_TO_EDITION_FOLDER --off
python server-tools-source/tests/check_proxy.py PATH_TO_EDITION_FOLDER --bad-object
python server-tools-source/tests/check_live.py PATH_TO_OBJECT_LTO PATH_TO_DEDICATED_EXE
python server-tools-source/tests/check_live.py PATH_TO_OBJECT_LTO PATH_TO_DEDICATED_EXE --disabled
python server-tools-source/tests/run_checks.py
python server-tools-source/tests/run_checks.py --usa
```

Run the proxy/live checks for both Europe and USA profiles. Tests load executable code in an emulator with mocked Windows/engine APIs; they do not launch a Windows game. Verify installation, first map loading, chat, two connected players, live revocation while airborne, and clean shutdown on Windows before release. Verify `object.lto` SHA256 remains `3a03f8796c482b1a7ad72755df33ee8f1f8056bb10957f6f700e64bd006b3e5b` before and after starting a server.
