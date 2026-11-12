#!/usr/bin/env python3
"""Rewrite the SHA-256 of every entry in the two product manifests.

Hashes are taken from the working tree, which is what tools/package_release.py
verifies. Entries are neither added nor removed: a new shipped file must be
added to the manifest by hand (package_release.py reports it as missing).
The external ddraw.dll entry of the client manifest is left unchanged.

A file whose working copy differs from the git index only by line endings
would give a hash that fails in CI, so that case stops with an error.
"""
import hashlib
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MANIFESTS = (('SHA256SUMS', ''), ('server-tools/SHA256SUMS-Server-Tools', 'server-tools/'))
EXTERNAL = {'ddraw.dll'}


def git(*args):
    return subprocess.run(['git', '-C', str(ROOT), *args], capture_output=True)


def refresh(manifest, base):
    path = ROOT / manifest
    lines = path.read_bytes().decode('utf-8').splitlines(keepends=True)
    out, changed = [], []
    for line in lines:
        sha, sep, rest = line.partition('  ./')
        name = rest.rstrip('\r\n')
        if not sep or len(sha) != 64 or name in EXTERNAL:
            out.append(line)
            continue
        rel = base + name
        data = (ROOT / rel).read_bytes()
        indexed = git('show', ':' + rel)
        if indexed.returncode == 0 and indexed.stdout != data and git('diff', '--quiet', '--', rel).returncode == 0:
            sys.exit(f'error: {rel}: working copy differs from the git index only by line endings; '
                     'check it out with LF (see .gitattributes) before refreshing manifests')
        new = hashlib.sha256(data).hexdigest()
        if new != sha:
            changed.append(name)
        out.append(new + sep + rest)
    if changed:
        path.write_bytes(''.join(out).encode('utf-8'))
    print(f'{manifest}: {len(changed)} updated' + (': ' + ', '.join(changed) if changed else ''))


def main():
    for manifest, base in MANIFESTS:
        refresh(manifest, base)


if __name__ == '__main__':
    main()
