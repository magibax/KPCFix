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
another commit.** Make a new SemVer tag instead. This preserves the
relationship between source, installer and reproducible build evidence.

## Semantic versioning, tags and asset names

The client and Server Tools are independent products with independent
`MAJOR.MINOR.PATCH` versions. Breaking compatibility increments MAJOR,
backwards-compatible features increment MINOR, and compatible bug fixes
increment PATCH. The official game's **1.13** version is a compatibility
requirement, *not* either KPCFix product's release version.

| Product | New tag format | New installable ZIP |
| --- | --- | --- |
| Windows client | `kpcfix-vMAJOR.MINOR.PATCH` | `KPCFix-Windows-vMAJOR.MINOR.PATCH.zip` |
| Server Tools | `server-tools-vMAJOR.MINOR.PATCH` | `KPCFix-Server-Tools-vMAJOR.MINOR.PATCH.zip` |

The current pinned versions are **`kpcfix-v1.1.2`** and
**`server-tools-v1.0.1`**. These names are pinned in
`tools/release_versions.json`; they are **not published merely by editing main**.
A future release must increment the relevant pinned version in that file
*before* the verified, annotated tag is pushed. CI rejects tags not matching
the pinned version, and produces candidate ZIPs using the pinned version.

Current releases (barebones packages, published 2026-11-13 from `main`):

| Product | Tag | Installable asset |
| --- | --- | --- |
| Windows client | [`kpcfix-v1.1.2`](https://github.com/magibax/KPCFix/releases/tag/kpcfix-v1.1.2) | `KPCFix-Windows-v1.1.2.zip` |
| Server Tools | [`server-tools-v1.0.1`](https://github.com/magibax/KPCFix/releases/tag/server-tools-v1.0.1) | `KPCFix-Server-Tools-v1.0.1.zip` |

Earlier releases:

| Product | Tag | Installable asset |
| --- | --- | --- |
| Server Tools | [`server-tools-v1.0.0-r3`](https://github.com/magibax/KPCFix/releases/tag/server-tools-v1.0.0-r3) | `KPCFix-Server-Tools-1.0.0.zip` (kept as the fallback for v1.0.1) |

The GitHub Releases of `kpcfix-v1.1.1`, `kpcfix-v1.1.0` and `kpcfix-1.13-r4`
were removed; their tags remain. All three shipped the same `dinput.dll`
functionality line that v1.1.2 continues (v1.1.0 to v1.1.2 are the same DLL).

Server Tools R3 was published from `a034fa6`. Its ZIP SHA-256 is
`bb518c27864b59a41e9425e015071bc99cbf26d1a724532edf5fe944307c6646`.
Later changes to the upgrade guidance on main postdate that ZIP.

After manual verification of the exact `main` commit, create an annotated tag
for the appropriate product (not both unless both have passed acceptance):

```sh
git fetch origin main
git tag -a kpcfix-v1.1.2 origin/main -m "KPCFix v1.1.2"
git push origin kpcfix-v1.1.2

# Or, after separate Server Tools acceptance:
git tag -a server-tools-v1.0.1 origin/main -m "Server Tools v1.0.1"
git push origin server-tools-v1.0.1
```

The tag workflow validates that the tag version matches the pinned version,
rebuilds the relevant product and creates the GitHub Release with a versioned
ZIP and adjacent `.zip.sha256` attachment. Update release links after
publication; update any checksum manifest affected by documentation changes.
Do not move old tags, relabel the older ZIPs, or use an experimental co-op
branch to publish stable assets.

## Reproducibility and dependency boundary

- **Client toolchain:** Ubuntu 24.04, GCC 13.3.0, GNU binutils 2.42.
- **Server toolchain:** Debian trixie container, GCC 14.2.0 and GNU binutils
  2.44, matching the Windows-tested Server Tools beta build. The workflow
  validates the versions inside the container and rejects an altered build.
  These tools intentionally use different compilers because otherwise their
  tracked PE32 binaries do not reproduce byte-for-byte.
- **Client:** `source/build.py` must reproduce tracked `dinput.dll`,
  `source/SYMBOLS.json` and `source/BUILD_RESULT.json`.
- **Server:** `server-tools/server-tools-source/build_dll.py` must reproduce
  tracked `KPCServerTools.dll` and `SERVER_TOOLS_BUILD.json`.
- **Third-party graphics dependency:** `ddraw.dll` is not kept in Git. The
  workflow temporarily extracts the known DDrawCompat binary from the newest
  published `kpcfix-v*` release and refuses any SHA-256 other than
  `94895cf6ede8fd0f74ee1447fdc08b7044a9c65a1a25a49a4df893a0370975a3`.
  Replace this provenance source only as an explicit, independently tested
  dependency upgrade.
- **Package contents:** packages are barebones. `tools/package_release.py`
  ships only the files listed in its `CLIENT_FILES` / `SERVER_FILES` (binaries,
  INIs, install/run/check scripts and their data, README and licences), and for
  the client the pinned `ddraw.dll`. Source, tests, build reports, verification
  runs and developer docs stay in the repository. It
  compares every file with the product's manifest, checks case-insensitive
  collisions and path traversal, then writes a stable ZIP and ZIP checksum.
- **Original game assets:** Never add `dedicated.exe`, `client.exe`,
  `server.original.dll`, `object.lto`, campaign maps or copyrighted game
  data to either package. Tests requiring the originals run privately.
- **Independent packages:** Extracting both into one game folder must not
  cause overlapping file paths. Client and Server Tools have separate README,
  licenses, config and checksum filenames.

## Manual packaging or recovery

From a checkout of the verified release commit, with the pinned product versions
in `tools/release_versions.json` and the exact external DDrawCompat DLL at a
known path:

```sh
python3 tools/package_release.py --component client --ddraw /path/to/ddraw.dll --output-dir dist/client
python3 tools/package_release.py --component server --output-dir dist/server
```

This packaging command **validates the tracked compiled DLL** but does **not**
rebuild it: use the release-integrity workflow or the product build instructions
first. If it refuses to package, fix the source/manifest discrepancy and rerun
CI; do not edit SHA-256 values to match an untested substitute binary.

The release workflow creates new releases on matching tag pushes, but does not
overwrite existing releases or retag history. If an action fails, inspect its logs and correct the cause before attempting
publication again from a new verified commit and unused SemVer tag. An experimental
co-op build must stay on its feature branch and must not trigger a stable
release tag.

## Stable / experimental isolation (2026-11-11)

Production Server Tools source, DLL and build metadata on `main` were
restored from the exact published `server-tools-v1.0.0-r3` baseline.
New stable changes may be committed to `main` only after pinned-toolchain
reproduction, package checksum validation and the manual tests above.

All ongoing co-op code, diagnostics, experimental Windows ZIPs and
co-op research remain on `coop/mvp-lobby`. The experimental
workflow uses the same GCC 14.2/binutils 2.44 environment as stable CI,
and it must compare the rebuilt DLL and manifest with the **tracked**
version before uploading those tracked files. A synthetic pass never
authorizes general automatic spawning.

The stable release workflow rejects binaries/source with experimental
co-op signatures and requires stable client/server tags to reference
commits on `main` history. Keep tag protections in both branch copies
of the workflow. Do **not** merge a stable server-tree restoration or
revert directly into the experimental branch: that would remove
co-op implementation. Cherry-pick narrowly reviewed fixes instead.

An existing stable tag remains immutable, even if a later stable main
commit updates launcher scripts or checksum manifests; such fixes
require a **new** SemVer release after Windows acceptance.

See [2026-11-11 audit remediation and unverified runtime gates](AUDIT-REMEDIATION.md) before publishing another release.
