#!/usr/bin/env python3
"""Update a KPCFix test install (a real game folder) from this checkout.

Follows coop/UPGRADE-TEST-INSTALL.md:
  * refuses to run while client.exe, dedicated.exe or Psycho.exe is running;
  * backs up every file it is about to change;
  * copies only files that differ;
  * never touches original game files, server.original.dll or the user's
    KPCServerTools.ini (the DLL has a built-in default for every missing key).

Client (--client): the files of the client release package (dinput.dll, scripts,
README, licences and SHA256SUMS) are replaced when they differ. KPCFix.ini and DDrawCompat-CLIENT.ini are the user's
settings: they are never replaced. Sections and keys that the repository
version has and the installed file lacks are added (with their comment lines);
every existing line and value stays as it is.

Server (--server): the files of the Server Tools release package are copied to
the game folder (source and tests are not). Server Tools run as winmm.dll beside the
unchanged server.dll: winmm.dll is replaced by the new KPCServerTools.dll only when
it is a Server Tools build already (it carries the "[KPCFix Server Tools]" marker).
An earlier install (proxy as server.dll) or a folder without Server Tools is left
alone and you are told to run Setup-KPCServerTools.cmd.
"""
import argparse
import datetime
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MERGED_INIS = ('KPCFix.ini', 'DDrawCompat-CLIENT.ini')
sys.path.insert(0, str(Path(__file__).resolve().parent))
import package_release  # noqa: E402  (shipped-file lists)
CLIENT_FILES = tuple(sorted(package_release.CLIENT_FILES - set(MERGED_INIS))) + ('SHA256SUMS',)
SERVER_FILES = tuple(sorted(package_release.SERVER_FILES)) + ('SHA256SUMS-Server-Tools',)
NEVER = {'kpcservertools.ini', 'server.original.dll', 'object.lto', 'client.exe', 'cshell.dll', 'dedicated.exe'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def running_game():
    try:
        out = subprocess.run(['tasklist', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout.lower()
    except OSError:
        return []
    return [name for name in ('client.exe', 'dedicated.exe', 'psycho.exe') if f'"{name}"' in out]


# ---- INI merge: add what is missing, keep everything that is there ----------

def is_comment(line):
    text = line.strip()
    return not text or text[0] in ';#'


def section_name(line):
    text = line.strip()
    return text[1:text.index(']')].strip().casefold() if text.startswith('[') and ']' in text else None


def key_name(line):
    return None if is_comment(line) or section_name(line) is not None or '=' not in line else line.split('=', 1)[0].strip().casefold()


def parse_ini(lines):
    """Return [(section, header_index or None, [line indexes])] in file order."""
    sections = [('', None, [])]
    for index, line in enumerate(lines):
        name = section_name(line)
        if name is not None:
            sections.append((name, index, []))
        else:
            sections[-1][2].append(index)
    return sections


def key_blocks(lines, indexes):
    """[(key, [lines: its comment block + the key line])] for one section."""
    blocks, pending = [], []
    for index in indexes:
        line = lines[index]
        key = key_name(line)
        if key is None:
            pending = pending + [line] if line.strip() else []
            continue
        blocks.append((key, pending + [line]))
        pending = []
    return blocks


def merge_ini(installed_text, repo_text):
    """Return (new_text, [added descriptions])."""
    eol = '\r\n' if '\r\n' in installed_text else '\n'
    mine = installed_text.splitlines()
    theirs = repo_text.splitlines()
    my_sections = parse_ini(mine)
    have = {}
    for name, _header, indexes in my_sections:
        have.setdefault(name, set()).update(k for k in (key_name(mine[i]) for i in indexes) if k)
    inserts, appended, added = {}, [], []
    for name, header, indexes in parse_ini(theirs):
        blocks = key_blocks(theirs, indexes)
        if name not in have:
            if header is None and not blocks:
                continue
            body = ([theirs[header]] if header is not None else []) + [theirs[i] for i in indexes]
            while body and not body[-1].strip():
                body.pop()
            appended.append(body)
            added.append(f'section [{name}]')
            continue
        missing = [(key, block) for key, block in blocks if key not in have[name]]
        if not missing:
            continue
        # After the last non-blank line of the installed section (its first occurrence).
        _n, my_header, my_indexes = next(s for s in my_sections if s[0] == name)
        last = my_header if my_header is not None else -1
        for i in my_indexes:
            if mine[i].strip():
                last = i
        for key, block in missing:
            inserts.setdefault(last, []).extend(block)
            added.append(f'[{name}] {block[-1].strip()}' if name else block[-1].strip())
    if not added:
        return installed_text, []
    out = list(inserts.get(-1, []))
    for index, line in enumerate(mine):
        out.append(line)
        out.extend(inserts.get(index, []))
    for body in appended:
        if out and out[-1].strip():
            out.append('')
        out.extend(body)
    return eol.join(out) + eol, added


# ---- plan and apply ---------------------------------------------------------

def is_proxy(path):
    """True for any Server Tools build: every one carries its log prefix."""
    return b'[KPCFix Server Tools]' in path.read_bytes()


def plan(game, client, server):
    """Return (copies [(src, dst, why)], merges [(dst, new_text, added)], notes)."""
    copies, merges, notes = [], [], []

    def copy_if_differs(src, dst, why):
        if dst.name.casefold() in NEVER:
            return
        if not dst.is_file() or sha(dst) != sha(src):
            copies.append((src, dst, why if dst.is_file() else why + ' (new)'))

    if client:
        for name in CLIENT_FILES:
            copy_if_differs(ROOT / name, game / name, 'client')
        for name in MERGED_INIS:
            src, dst = ROOT / name, game / name
            if not dst.is_file():
                copies.append((src, dst, 'client settings (new)'))
                continue
            old = dst.read_bytes().decode('utf-8', errors='surrogateescape')
            new, added = merge_ini(old, src.read_bytes().decode('utf-8'))
            if added:
                merges.append((dst, new, added))
    if server:
        for name in SERVER_FILES:
            copy_if_differs(ROOT / 'server-tools' / name, game / name, 'server')
        new_dll = ROOT / 'server-tools' / 'KPCServerTools.dll'
        # Server Tools load as winmm.dll beside the unchanged server.dll. An
        # earlier install (proxy as server.dll, engine as server.original.dll)
        # is moved over only by Setup-KPCServerTools.cmd.
        loader, active = game / 'winmm.dll', game / 'server.dll'
        if is_proxy(active):
            notes.append('winmm.dll NOT installed: server.dll is an earlier Server Tools install; run Setup-KPCServerTools.cmd in the game folder to move it to winmm.dll.')
        elif not loader.is_file():
            notes.append('winmm.dll NOT installed: Server Tools are not set up in this folder; run Setup-KPCServerTools.cmd in the game folder.')
        elif sha(loader) == sha(new_dll):
            pass
        elif not is_proxy(loader):
            notes.append('winmm.dll NOT updated: the winmm.dll in the game folder is not a Server Tools build.')
        else:
            copies.append((new_dll, loader, 'Server Tools (winmm.dll)'))
    return copies, merges, notes


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--game-dir', type=Path, required=True)
    parser.add_argument('--backup-root', type=Path, default=Path('C:/KPCTest/backup'))
    parser.add_argument('--client', action='store_true')
    parser.add_argument('--server', action='store_true')
    parser.add_argument('--dry-run', action='store_true', help='print what would change; write nothing')
    args = parser.parse_args()
    game = args.game_dir
    if not (game / 'client.exe').is_file():
        sys.exit(f'install: {game} is not a game folder (no client.exe); nothing done')
    copies, merges, notes = plan(game, args.client, args.server)
    for note in notes:
        print('install: ' + note)
    if not copies and not merges:
        print(f'install: {game} is up to date')
        return
    for src, dst, why in copies:
        print(f'install: {"would copy" if args.dry_run else "copy"} {dst.relative_to(game)}  [{why}]')
    for dst, _new, added in merges:
        for item in added:
            print(f'install: {"would add" if args.dry_run else "add"} to {dst.name}: {item}')
    if args.dry_run:
        return
    busy = running_game()
    if busy:
        sys.exit(f'install: {", ".join(busy)} is running; close it and run again. Nothing was changed.')
    tag = sha(ROOT / 'server-tools' / 'KPCServerTools.dll')[:8] if args.server else sha(ROOT / 'dinput.dll')[:8]
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    root = args.backup_root if args.backup_root.is_dir() else game / 'KPCFix-backups'
    backup = root / f'{game.name.lower()}-{stamp}-before-{tag}'
    saved = 0
    for dst in [d for _s, d, _w in copies] + [d for d, _n, _a in merges]:
        if dst.is_file():
            target = backup / dst.relative_to(game)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(dst, target)
            saved += 1
    if saved:
        print(f'install: {saved} file(s) backed up to {backup}')
    for src, dst, _why in copies:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
    for dst, new, _added in merges:
        dst.write_bytes(new.encode('utf-8', errors='surrogateescape'))
    print(f'install: {game} updated ({len(copies)} copied, {len(merges)} settings file(s) extended)')


if __name__ == '__main__':
    main()
