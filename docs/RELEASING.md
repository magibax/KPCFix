# Releasing

The client and Server Tools are separate products with separate versions, tags and ZIPs. Releases are made only from `master`.

| Product | Tag | ZIP |
|---|---|---|
| Client | `kpcfix-vX.Y.Z` | `KPCFix-Windows-vX.Y.Z.zip` |
| Server Tools | `server-tools-vX.Y.Z` | `KPCFix-Server-Tools-vX.Y.Z.zip` |

Versions follow SemVer: MAJOR for breaking changes, MINOR for features, PATCH for fixes. The game's "1.13" is a compatibility requirement, not a KPCFix version.

The next versions are pinned in `tools/release_versions.json`. CI refuses a tag that doesn't match the pin.

## Published releases

| Product | Tag | Notes |
|---|---|---|
| Client | `kpcfix-v1.1.3` | Current. `dinput.dll` `288bce4e…` |
| Server Tools | `server-tools-v1.0.1` | Current. Installs as `server.dll` + `server.original.dll` |

Pinned next: Server Tools **v1.1.0**. It installs as `winmm.dll` and adds `ServerFrameMs`. It needs the in-game checks in [DEVELOPMENT.md](DEVELOPMENT.md#in-game-checks-before-a-release) before it is tagged.

Older GitHub Releases were deleted; their tags remain. Never move or reuse a tag. A fix always gets a new version.

## Steps

1. Land the source, the rebuilt DLL, its build report and the refreshed manifest on `master` (`tools/refresh_manifests.py`).
2. Let the **release-integrity** workflow pass on that commit. It:
   - rebuilds both DLLs with the pinned toolchains and compares them byte for byte;
   - checks that `git add --renormalize .` changes nothing (LF everywhere);
   - checks that the shared `sha256.c`, `sha256.h` and `winmini.h` are identical in both products;
   - checks that the Server Tools DLL and source contain no co-op markers;
   - runs the server assembly harness, the packaging tests and the PowerShell parse and INI tests on Windows;
   - builds candidate ZIPs as artifacts.
3. Run the in-game checks for that product, using the candidate ZIP.
4. Bump `tools/release_versions.json` if it isn't already. The bump is a commit of its own: wait for **release-integrity** to pass on it, then tag that commit, not an earlier one:
   ```sh
   git tag -a server-tools-v1.1.0 <commit> -m "KPCFix Server Tools v1.1.0"
   git push origin server-tools-v1.1.0
   ```
5. The tag push rebuilds and checks the product again and runs the Windows script tests. Only if both pass does it create the GitHub Release with the ZIP and a `.zip.sha256` file. Check the release's commit and ZIP hash. If the tag's run fails, nothing is published: fix the cause on `master` and release under the next version.

## Packages

`tools/package_release.py` builds a ZIP from the files listed in its `CLIENT_FILES` or `SERVER_FILES` and nothing else. It never rebuilds a DLL; it only checks the tracked one. It refuses to package if:
- a file doesn't match the product's manifest (`SHA256SUMS` or `server-tools/SHA256SUMS-Server-Tools`);
- two paths collide when case is ignored;
- a path escapes the package.

To ship a new file:
1. Track it in git.
2. Add it to the file list.
3. Add a line for it to the manifest.

`README.md` and `README-Server-Tools.md` are shipped, so editing them changes the manifests.

Manual packaging, from a checkout of the release commit:

```sh
python3 tools/package_release.py --component client --ddraw /path/to/ddraw.dll --output-dir dist/client
python3 tools/package_release.py --component server --output-dir dist/server
```

**`ddraw.dll`** is DDrawCompat, a third-party DLL that is not kept in git. CI downloads it from the newest published `kpcfix-v*` release and refuses any file other than SHA-256 `94895cf6ede8fd0f74ee1447fdc08b7044a9c65a1a25a49a4df893a0370975a3`, so always keep at least one client release published. Upgrading DDrawCompat is a deliberate change, tested on its own.

Never put original game files in a package (`dedicated.exe`, `client.exe`, `server.dll`, `object.lto`, `cshell.dll`, maps, `.rez`).
