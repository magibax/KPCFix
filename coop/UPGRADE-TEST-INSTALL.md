# Upgrading a co-op test install

How to put a new `coop/mvp-lobby` client and Server Tools build onto the test PCs. This is for
branch test builds. For released builds, see the [Server Tools README](../server-tools/README-Server-Tools.md)
(section "Update an existing DLL installation") and the [main README](../README.md).

Installs used for testing:

| Install | Role | Gets |
| --- | --- | --- |
| `C:\Games\Psycho` | desktop: game client, host and dedicated server | client and server |
| `C:\KPCTest\smoke1\Psycho` | disposable server for `Invoke-CoopServerSmoke.ps1` | server |
| laptop | second player | client, through a package folder |

## 1. Build and commit

* **Client:** run `C:\KPCTest\clientbuild\build-coop.sh <dir>`. Copy `out\dinput.dll` to the repository root,
  and `out\SYMBOLS.json` and `out\BUILD_RESULT.json` to `source\`. Then run `suite-coop.sh` and
  `audit_sites.py` (the audit needs `objdump`, so run it in the `kpc-client-gcc13:local` container).
* **Server:** run `C:\KPCTest\pass2\build-coop.sh <dir>`. It builds a disposable copy in
  `C:\KPCTest\pass2\<dir>\server-tools`; copy the new `KPCServerTools.dll` and `SERVER_TOOLS_BUILD.json`
  (and `SHA256SUMS-Server-Tools` if it changed) back into `server-tools\`. Then run `unit-coop.sh` and
  `matrix-coop.sh <dir>`.
* **Hashes:** update the hashes of the changed files in the root `SHA256SUMS`. Hash the committed (LF)
  contents, not the Windows working copy: a CRLF checkout gives different hashes for text files.
* **Commit:** commit on `coop/mvp-lobby`. Install only committed builds, so every installed hash can be
  traced to a commit.

The build scripts copy from `C:\Projects\KPCFix-coop`, the main checkout. When you build from another
worktree, use a copy of the script that points at that worktree.

## 2. Before installing

1. Close the game and every dedicated server that uses the folder (check Task Manager for `client.exe`
   and `dedicated.exe`).
2. Compare the files: list what differs between the repository and the install, and copy only those.
   * Client: `dinput.dll` and `KPCFix.ini`.
   * Server: the files in `server-tools\` and `server-tools\server-tools-source\`.
   * Never `KPCServerTools.ini` (the user's settings) or `server.original.dll` (the official engine).
3. Check `KPCFix.ini` before you replace it. If the installed copy matches a previous repository
   version (its hash is in an earlier `SHA256SUMS`), it is unmodified and can be replaced. If not, the
   user changed settings: carry them over by hand.
4. Back up everything you will replace to `C:\KPCTest\backup\<install>-<date>-before-<short hash of the
   new dinput.dll or server DLL>\`:
   * `dinput.dll`, `KPCFix.ini`, `server.dll` and `KPCServerTools.ini`;
   * every Server Tools file you will overwrite, and `server-tools-source\`.

## 3. Install the client

Copy `dinput.dll` and `KPCFix.ini` from the repository root into the game folder. Nothing else is
needed; KPCFix checks the game binaries at the next start. `DDrawCompat-CLIENT.ini` changes only when
the repository changes it.

## 4. Install the server

1. Copy the changed files from `server-tools\` into the game folder:
   * usually `KPCServerTools.dll`, `SERVER_TOOLS_BUILD.json`, `SHA256SUMS-Server-Tools`,
     `KPCServerTools-default.ini` and the launcher scripts;
   * replace `server-tools-source\` as a whole.
2. Run `Setup-KPCServerTools.ps1` (or `.cmd`) in the game folder. It installs the proxy as `server.dll`,
   keeps the official engine as `server.original.dll`, restores a stock `object.lto` if needed, and checks
   the installed hash against `SERVER_TOOLS_BUILD.json`.
3. **Expected on branch builds:** setup stops with "Unrecognized active server.dll". Setup only upgrades in
   place from proxies on the reviewed allowlist (`previous_proxy_hashes`), and branch test builds are not
   on it. First confirm the backup's `server.dll` matches the active one. Then move the active
   `server.dll` into the backup folder (leave `server.original.dll` where it is) and run setup again. It
   reports "DLL installed. Original engine preserved; object.lto is stock."

Do not add branch hashes to `previous_proxy_hashes` to avoid this step. That list requires a reviewed
compatibility decision (see [README.md](README.md), "Reviewed proxy upgrade hash policy").

## 5. Verify

* `dinput.dll` in the game folder is byte-identical to the repository's, and so is `KPCFix.ini`.
* `server.dll` is byte-identical to `server-tools\KPCServerTools.dll`.
* `Check-KPCServerVersion.ps1` and `Check-KPCVersion.ps1` pass.
* Optional: after the first start, `KPCFix.log` shows the expected feature lines (for example
  "Chat typing: no command ...", "Focus loss: KeepAudioOnFocusLoss default=..."), and
  `KPCServerTools.log` shows the hooks installed.

## 6. Laptop package

Create `C:\KPCTest\pkg-client-<short dinput hash>\` with `dinput.dll`, `KPCFix.ini` and the desktop's
`DDrawCompat-CLIENT.ini`, plus a `SHA256.txt` (`sha256sum dinput.dll KPCFix.ini DDrawCompat-CLIENT.ini`).
On the laptop, close the game, back up the same three files, copy the package into the game folder, and
check the hashes.

## 7. Record it

In [NEXT-SESSION-HANDOFF.md](NEXT-SESSION-HANDOFF.md), "Where things stand", note the installed client
and server hashes, the backup folder and the package folder.

## Roll back

* **Client:** copy `dinput.dll` and `KPCFix.ini` back from the backup folder.
* **Server:** copy the backup's Server Tools files back, then put the moved `server.dll` back in place
  (the game and server must be closed).
* **Remove Server Tools entirely:** run `Restore-Original-Server.cmd`, which restores the official engine.
