# Releasing KPCFix

The installable client and Server Tools ZIPs are **different products** with
separate release tags. GitHub's built-in **Source code (ZIP/tar.gz)** is the
*entire repository at the tag*, not a ZIP of the installable files. Neither
GitHub-generated archive is derived from an uploaded binary attachment.

## Rule: a release is an immutable snapshot

1. Land reviewed source, updated DLL, generated symbol/build metadata and the
   corresponding product SHA256SUMS manifest in `main`.
2. Run the [release-integrity workflow](.github/workflows/release-integrity.yml)
   on `main`, and review its client/server candidate archives and hashes.
   The workflow rebuilds from the checked-out commit and **refuses** nonmatching
   tracked DLLs, metadata, stale manifests or missing files.
3. Complete product-specific Windows/Wine manual checks from [TESTING.md](TESTING.md).
   Automated CPU and build checks alone are not evidence of live compatibility.
4. Create and push a **new, annotated tag on the verified `main` commit**.
   The tag push rebuilds and validates the relevant product again, then creates
   its GitHub Release and uploads its deterministic ZIP and .sha256.
5. Read the release's commit and tag SHA, check the ZIP SHA-256 and compare the
   built-in source archive's commit against the tagged commit.

**Never move an existing release tag or replace its binaries with a build from
another commit.** Make a new revision tag instead. This preserves the
relationship between source, installer and reproducible build evidence.

## Tag naming and trigger

| Product | Existing stable | Next suitable tag (after verification) | Release asset |
| --- | --- | --- | --- |
| Client | `kpcfix-1.13-r4` | `kpcfix-1.13-r5` (only when client changes warrant it) | `KPCFix-Windows-1.13.zip` |
| Server Tools | `server-tools-v1.0.0-r2` | `server-tools-v1.0.0-r3` | `KPCFix-Server-Tools-1.0.0.zip` |

For the current completed server beta integration, only a **new Server Tools
release** is needed. The existing Windows client release remains valid if the
client executable and its distributed assets have not changed.

To create the server revision after the workflow and live checks succeed:

```sh
git fetch origin
git switch main
git pull --ff-only origin main
git tag -a server-tools-v1.0.0-r3 -m "KPCFix Server Tools 1.0.0 R3"
git push origin server-tools-v1.0.0-r3
```

Inspect [Actions](https://github.com/magibax/KPCFix/actions) and the new
[Releases](https://github.com/magibax/KPCFix/releases) entry. Do not push a
tag until the commit to be released has passed validation. No tag or GitHub
Release is created merely by merging a feature branch or opening a PR.

## Reproducibility and dependency boundary

- **Toolchain:** GitHub Actions uses Ubuntu 24.04, GCC 13.3.0 and GNU
  binutils 2.42. The runner checks these versions before building. Source
  compilation with a different toolchain can succeed but yield different DLL
  bytes. Byte-for-byte comparisons are mandatory.
- **Client:** `source/build.py` must reproduce tracked `dinput.dll`,
  `source/SYMBOLS.json` and `source/BUILD_RESULT.json`.
- **Server:** `server-tools/server-tools-source/build_dll.py` must reproduce
  tracked `KPCServerTools.dll` and `SERVER_TOOLS_BUILD.json`.
- **Third-party graphics dependency:** `ddraw.dll` is not kept in Git. The
  workflow temporarily extracts the known DDrawCompat binary from the older
  `kpcfix-1.13-r4` release and refuses any SHA-256 other than
  `94895cf6ede8fd0f74ee1447fdc08b7044a9c65a1a25a49a4df893a0370975a3`.
  Replace this provenance source only as an explicit, independently tested
  dependency upgrade.
- **Package contents:** `tools/package_release.py` includes only the
  product's tracked files, and for the client the pinned `ddraw.dll`. It
  compares every file with the product's manifest, checks case-insensitive
  collisions and path traversal, then writes a stable ZIP and ZIP checksum.
- **Original game assets:** Never add `dedicated.exe`, `client.exe`,
  `server.original.dll`, `object.lto`, campaign maps or copyrighted game
  data to either package. Tests requiring the originals run privately.
- **Independent packages:** Extracting both into one game folder must not
  cause overlapping file paths. Client and Server Tools have separate README,
  licenses, config and checksum filenames.

## Manual packaging or recovery

From a checkout of the verified release commit, with the exact external
DDrawCompat DLL at a known path:

```sh
python3 tools/package_release.py --component client --ddraw /path/to/ddraw.dll --output-dir dist/client
python3 tools/package_release.py --component server --output-dir dist/server
```

This packaging command **validates the tracked compiled DLL** but does **not**
rebuild it: use the release-integrity workflow or the product build instructions
first. If it refuses to package, fix the source/manifest discrepancy and rerun
CI; do not edit SHA-256 values to match an untested substitute binary.

The release workflow creates new releases on matching tag pushes, but does not
overwrite existing releases or retag history. If an action fails, inspect its
logs and correct the cause before choosing a fresh release tag. An experimental
co-op build must stay on its feature branch and must not trigger a stable
release tag.
