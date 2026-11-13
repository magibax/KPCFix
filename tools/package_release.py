#!/usr/bin/env python3
"""Package tracked KPCFix releases with strict SHA-256/source-tree checks.

The binary rebuild is checked separately by the release workflow. This tool
refuses stale manifests, unexpected tracked files or unpinned DLL dependencies.
"""
import argparse
import hashlib
from pathlib import Path, PurePosixPath
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
# Release packages are barebones: only the files a player or server host needs to
# install, run, check and remove the fix, plus the README and licences. Source,
# tests, build reports, verification runs and developer docs stay in the repository.
CLIENT_FILES = frozenset({
    'dinput.dll', 'KPCFix.ini', 'DDrawCompat-CLIENT.ini',
    'Setup-KPCFix.ps1', 'Launch-KPCFix.cmd', 'Check-KPCVersion.cmd', 'Check-KPCVersion.ps1',
    'SUPPORTED_BUILDS.json', 'README.md', 'LICENSE', 'LICENSES/DDrawCompat-0BSD.txt',
})
SERVER_FILES = frozenset({
    'KPCServerTools.dll', 'KPCServerTools-default.ini', 'SERVER_TOOLS_BUILD.json',
    'SERVER_TOOLS_SUPPORTED_BUILDS.json', 'Setup-KPCServerTools.cmd', 'Setup-KPCServerTools.ps1',
    'Restore-Original-Server.cmd', 'Restore-LegacyObject.ps1', 'legacy-object-patches.json',
    'Check-KPCServerVersion.ps1', 'KPC-Server-Launcher.cmd', 'KPC-Server-Launcher.ps1',
    'Start-KPC-Selected-Server.ps1', 'maps.json', 'server-template.cfg',
    'README-Server-Tools.md', 'LICENSE-Server-Tools.txt',
})
EXTERNAL_DDRAW_SHA256 = '94895cf6ede8fd0f74ee1447fdc08b7044a9c65a1a25a49a4df893a0370975a3'
PRODUCTS = {
    'client': ('SHA256SUMS', 'KPCFix-Windows-1.13.zip'),
    'server': ('server-tools/SHA256SUMS-Server-Tools', 'KPCFix-Server-Tools-1.0.0.zip'),
}

def digest(data):
    return hashlib.sha256(data).hexdigest()

def tracked():
    raw = subprocess.check_output(['git', '-C', str(ROOT), 'ls-files', '-z'])
    return {p.decode('utf-8') for p in raw.split(b'\0') if p}

def client_path(path):
    return path in CLIENT_FILES

def server_path(path):
    """Path relative to server-tools/."""
    return path in SERVER_FILES

def shipped(component):
    """Repository paths of the tracked files a package contains."""
    return {('server-tools/' + p) if component == 'server' else p
            for p in (SERVER_FILES if component == 'server' else CLIENT_FILES)}

def manifest_entries(text):
    result = {}
    folded = set()
    for line in text.splitlines():
        if not line.strip():
            continue
        sha, sep, name = line.partition('  ./')
        if not sep or len(sha) != 64 or any(c not in '0123456789abcdef' for c in sha):
            raise ValueError(f'Invalid checksum manifest entry: {line!r}')
        path = PurePosixPath(name)
        if not name or name.startswith('/') or '..' in path.parts or str(path) != name:
            raise ValueError(f'Unsafe package path: {name!r}')
        key = name.casefold()
        if key in folded:
            raise ValueError(f'Case-insensitive duplicate: {name!r}')
        folded.add(key)
        result[name] = sha
    return result

def make_package(component, out_dir, external_ddraw):
    manifest_path, archive_name = PRODUCTS[component]
    base = 'server-tools/' if component == 'server' else ''
    entries = manifest_entries((ROOT / manifest_path).read_text(encoding='utf-8'))
    tracked_paths = tracked()
    untracked = shipped(component) - tracked_paths
    if untracked:
        raise ValueError(f'Shipped files not tracked in git: {sorted(untracked)}')
    included = {p[len(base):] for p in shipped(component)}
    external = {'ddraw.dll'} if component == 'client' else set()
    missing = (included | external) - entries.keys()
    excess = entries.keys() - (included | external)
    if missing or excess:
        raise ValueError(f'Manifest/tree mismatch: missing={sorted(missing)} extra={sorted(excess)}')
    if component == 'client':
        if external_ddraw is None or not external_ddraw.is_file():
            raise ValueError('Client package requires a verified --ddraw DLL')
        if digest(external_ddraw.read_bytes()) != EXTERNAL_DDRAW_SHA256:
            raise ValueError('External DDrawCompat dependency does not match pinned SHA-256')
    payload = {}
    for name, expected in entries.items():
        data = external_ddraw.read_bytes() if name == 'ddraw.dll' else (ROOT / base / name).read_bytes()
        actual = digest(data)
        if actual != expected:
            raise ValueError(f'Stale checksum: {name}: expected {expected}, actual {actual}')
        payload[name] = data
    payload[Path(manifest_path).name] = (ROOT / manifest_path).read_bytes()
    out_dir.mkdir(parents=True, exist_ok=True)
    archive = out_dir / archive_name
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in sorted(payload.items()):
            item = zipfile.ZipInfo(name, (2020, 1, 1, 0, 0, 0))
            item.compress_type = zipfile.ZIP_DEFLATED
            item.external_attr = (0o100644 << 16)
            z.writestr(item, data, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None or set(z.namelist()) != set(payload):
            raise AssertionError('Failed ZIP integrity verification')
    (out_dir / (archive_name + '.sha256')).write_text(f'{digest(archive.read_bytes())}  {archive_name}\n')
    print(f'PASS {component}: {len(payload)} files, {archive.stat().st_size} bytes; {digest(archive.read_bytes())}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--component', choices=PRODUCTS, required=True)
    parser.add_argument('--output-dir', type=Path, default=Path('dist'))
    parser.add_argument('--ddraw', type=Path)
    args = parser.parse_args()
    make_package(args.component, args.output_dir, args.ddraw)

if __name__ == '__main__':
    main()
