#!/bin/sh
# Build only what changed: the client proxy (source/) and/or the server proxy (server-tools/).
#
# Client inputs are source/ and KPCFix.ini; server inputs are server-tools-source (see below).
# A component is rebuilt when the SHA-256 of its input files differs from the value recorded
# by the last successful build. Changes are detected from the working tree, so committed,
# staged, modified and untracked files all count. Build outputs (SYMBOLS.json, BUILD_RESULT.json,
# build/ and __pycache__) are excluded, so a build never triggers another build.
#
# Each build runs in a disposable copy under build/ with the pinned Docker image, as in
# CLAUDE.md. Outputs are then copied back to the same places the manual steps use:
#   client: out/dinput.dll -> dinput.dll, SYMBOLS.json + BUILD_RESULT.json -> source/
#   server: KPCServerTools.dll + SERVER_TOOLS_BUILD.json -> server-tools/
#
# Then it packages (always, also when nothing was rebuilt; skip with --no-package):
#   1. tools/refresh_manifests.py rewrites the hashes in SHA256SUMS and
#      server-tools/SHA256SUMS-Server-Tools from the working tree;
#   2. tools/package_release.py writes the full release ZIPs to dist/, each checked
#      file by file against its manifest:
#        dist/KPCFix-Server-Tools-1.0.0.zip  DLL, launcher and setup scripts, default INI, cfg,
#                                            maps, legacy patch data, README, licence
#        dist/KPCFix-Windows-1.13.zip        dinput.dll, KPCFix.ini, DDrawCompat-CLIENT.ini,
#                                            ddraw.dll, setup and check scripts, README, licences
#      (barebones: the lists are CLIENT_FILES / SERVER_FILES in package_release.py)
#      The client ZIP needs the pinned DDrawCompat ddraw.dll, which is not in git:
#      --ddraw <path>, else $KPC_DDRAW, else C:/Games/Psycho/ddraw.dll. Without it the
#      client ZIP is skipped (package_release.py checks its SHA-256).
#   3. dist/client/ gets the three files a test PC needs (dinput.dll, KPCFix.ini,
#      DDrawCompat-CLIENT.ini, with SHA256.txt), and dist/ gets the co-op test docs
#      (COOP-MVP.md, COOP-CONFIGURATION.md, UPGRADE-TEST-INSTALL.md).
# The ZIPs contain tracked files only: `git add` a new shipped file and add its
# line to the manifest first, or packaging stops with "Manifest/tree mismatch".
#
# Last it updates the test install (always; skip with --no-install): tools/install_game.py
# brings the game folder (--game-dir <path>, else $KPC_GAME_DIR, else C:/Games/Psycho) up to
# date with this checkout. It copies only files that differ, backs up everything it changes
# to C:/KPCTest/backup/, adds missing settings to KPCFix.ini and DDrawCompat-CLIENT.ini
# without changing existing ones, and never touches KPCServerTools.ini, server.original.dll
# or original game files. It refuses while client.exe or dedicated.exe is running.
# --dry-run-install only prints what it would change.
#
# Usage: tools/build-changed.sh [--force] [--client] [--server] [--no-package] [--ddraw <path>]
#                               [--no-install] [--dry-run-install] [--game-dir <path>]
#   (no component flag = both)   --force  rebuild even if nothing changed
#
# Requires: Docker Desktop running, git bash (sh, find, sha256sum, cygpath), python.

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
STATE_DIR="$(git -C "$ROOT" rev-parse --absolute-git-dir 2>/dev/null || echo "$ROOT/.git")/kpc-build-state"
CLIENT_IMAGE=kpc-client-gcc13:local
SERVER_IMAGE=kpc-gcc14:local

FORCE=0
DO_CLIENT=0
DO_SERVER=0
PACKAGE=1
DDRAW=${KPC_DDRAW:-}
INSTALL=1
DRY_INSTALL=
GAME_DIR=${KPC_GAME_DIR:-C:/Games/Psycho}
want_ddraw=0
want_game=0
for arg in "$@"; do
    if [ "$want_ddraw" = 1 ]; then DDRAW=$arg; want_ddraw=0; continue; fi
    if [ "$want_game" = 1 ]; then GAME_DIR=$arg; want_game=0; continue; fi
    case "$arg" in
        --no-install) INSTALL=0 ;;
        --dry-run-install) DRY_INSTALL=--dry-run ;;
        --game-dir) want_game=1 ;;
        --no-package) PACKAGE=0 ;;
        --ddraw) want_ddraw=1 ;;
        --force)  FORCE=1 ;;
        --client) DO_CLIENT=1 ;;
        --server) DO_SERVER=1 ;;
        -h|--help) sed -n '2,45p' "$0"; exit 0 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done
if [ "$DO_CLIENT" = 0 ] && [ "$DO_SERVER" = 0 ]; then DO_CLIENT=1; DO_SERVER=1; fi

if ! docker info >/dev/null 2>&1; then
    echo "error: Docker is not running; start Docker Desktop and retry" >&2
    exit 1
fi

# Hash of every input file under the given paths (relative to ROOT), excluding build outputs.
hash_inputs() {
    cd "$ROOT"
    find "$@" -type f \
        ! -name SYMBOLS.json ! -name BUILD_RESULT.json \
        ! -path '*/__pycache__/*' ! -path '*/build/*' \
        -print0 | sort -z | xargs -0 sha256sum | sha256sum | cut -d' ' -f1
}

# Run a docker command with a Windows-style host path mounted at /w.
# $1 = host dir, $2 = container mount target, $3 = image, $4 = shell command, $5 = log file
docker_build() {
    MSYS_NO_PATHCONV=1 docker run --rm \
        -v "$(cygpath -m "$1"):$2" -w "${2%%:*}" "$3" sh -euc "$4" > "$5" 2>&1
}

build_client() {
    now=$(hash_inputs source KPCFix.ini)
    if [ "$FORCE" = 0 ] && [ -f "$STATE_DIR/client" ] && [ "$(cat "$STATE_DIR/client")" = "$now" ]; then
        echo "client: unchanged, skipping"
        return 0
    fi
    echo "client: source changed, building..."
    dir="$ROOT/build/client"
    rm -rf "$dir"; mkdir -p "$dir"
    cp -a "$ROOT/source" "$dir/source"
    log="$dir/build.log"
    if ! docker_build "$dir" /w "$CLIENT_IMAGE" \
        'python3 source/build.py --build-dir /w/out --output /w/out/dinput.dll' "$log"; then
        echo "client: BUILD FAILED, last lines of $log:" >&2
        tail -20 "$log" >&2
        return 1
    fi
    cp "$dir/out/dinput.dll" "$ROOT/dinput.dll"
    cp "$dir/out/SYMBOLS.json" "$dir/out/BUILD_RESULT.json" "$ROOT/source/"
    mkdir -p "$STATE_DIR"
    echo "$now" > "$STATE_DIR/client"
    sha256sum "$ROOT/dinput.dll" "$ROOT/source/SYMBOLS.json" "$ROOT/source/BUILD_RESULT.json"
    echo "client: built OK"
}

build_server() {
    now=$(hash_inputs server-tools/server-tools-source server-tools/legacy-object-patches.json server-tools/SERVER_TOOLS_SUPPORTED_BUILDS.json)
    if [ "$FORCE" = 0 ] && [ -f "$STATE_DIR/server" ] && [ "$(cat "$STATE_DIR/server")" = "$now" ]; then
        echo "server: unchanged, skipping"
        return 0
    fi
    echo "server: server-tools changed, building..."
    dir="$ROOT/build/server"
    rm -rf "$dir"; mkdir -p "$dir"
    cp -a "$ROOT/server-tools" "$dir/server-tools"
    rm -rf "$dir/server-tools/server-tools-source/build"
    log="$dir/build.log"
    if ! docker_build "$dir/server-tools" /w/server-tools "$SERVER_IMAGE" \
        'test "$(gcc -dumpfullversion)" = "14.2.0"; ld --version | head -1 | grep -F "2.44"; python3 server-tools-source/build_dll.py' "$log"; then
        echo "server: BUILD FAILED, last lines of $log:" >&2
        tail -20 "$log" >&2
        return 1
    fi
    cp "$dir/server-tools/KPCServerTools.dll" "$dir/server-tools/SERVER_TOOLS_BUILD.json" "$ROOT/server-tools/"
    mkdir -p "$STATE_DIR"
    echo "$now" > "$STATE_DIR/server"
    sha256sum "$ROOT/server-tools/KPCServerTools.dll" "$ROOT/server-tools/SERVER_TOOLS_BUILD.json"
    echo "server: built OK"
}

# Release ZIPs and test bundles in dist/ (see the header).
package() {
    cd "$ROOT"
    python tools/refresh_manifests.py || return 1
    mkdir -p "$ROOT/dist"
    if [ "$DO_SERVER" = 1 ]; then
        python tools/package_release.py --component server --output-dir dist || return 1
    fi
    if [ "$DO_CLIENT" = 1 ]; then
        bundle="$ROOT/dist/client"
        rm -rf "$bundle"; mkdir -p "$bundle"
        cp "$ROOT/dinput.dll" "$ROOT/KPCFix.ini" "$ROOT/DDrawCompat-CLIENT.ini" "$bundle/"
        (cd "$bundle" && sha256sum dinput.dll KPCFix.ini DDrawCompat-CLIENT.ini > SHA256.txt)
        echo "client: test bundle in dist/client"
        [ -n "$DDRAW" ] || { [ -f "C:/Games/Psycho/ddraw.dll" ] && DDRAW="C:/Games/Psycho/ddraw.dll"; } || true
        if [ -n "$DDRAW" ] && [ -f "$DDRAW" ]; then
            python tools/package_release.py --component client --ddraw "$DDRAW" --output-dir dist || return 1
        else
            echo "client: ZIP SKIPPED: no ddraw.dll (pass --ddraw <path> or set KPC_DDRAW)" >&2
            return 1
        fi
    fi
    cp "$ROOT/coop/COOP-MVP.md" "$ROOT/dist/COOP-MVP.md"
    cp "$ROOT/coop/CONFIGURATION.md" "$ROOT/dist/COOP-CONFIGURATION.md"
    cp "$ROOT/coop/UPGRADE-TEST-INSTALL.md" "$ROOT/dist/UPGRADE-TEST-INSTALL.md"
    echo "package: output in $ROOT/dist"
}

status=0
[ "$DO_CLIENT" = 1 ] && { build_client || status=1; }
[ "$DO_SERVER" = 1 ] && { build_server || status=1; }
if [ "$status" = 0 ] && [ "$PACKAGE" = 1 ]; then package || status=1; fi
if [ "$status" = 0 ] && [ "$INSTALL" = 1 ]; then
    flags=
    [ "$DO_CLIENT" = 1 ] && flags="$flags --client"
    [ "$DO_SERVER" = 1 ] && flags="$flags --server"
    if [ -d "$GAME_DIR" ]; then
        python "$ROOT/tools/install_game.py" --game-dir "$GAME_DIR" $flags $DRY_INSTALL || status=1
    else
        echo "install: no game folder at $GAME_DIR, skipped (use --game-dir or KPC_GAME_DIR)"
    fi
fi
exit $status
