"""Check the compiled-in campaign table (server-tools-source/src/coop_campaign.h)
against WORLDS\\WORLDORDER.TXT inside the user's own PSYCHO.REZ (read-only).

Usage: python check_campaign_table.py C:\\Games\\Psycho\\PSYCHO.REZ
No game data is copied or written; exit code 0 means identical order, realms,
stages and INTRO/WORLDMAP/first-map flags.
"""
import re
import struct
import sys
from pathlib import Path

HEADER = Path(__file__).resolve().parents[2] / 'server-tools' / 'server-tools-source' / 'src' / 'coop_campaign.h'


def rez_file(path, want):
    """Return the bytes of one entry of a LithTech REZ v1 archive."""
    with open(path, 'rb') as f:
        head = f.read(200)
        _, root, size = struct.unpack_from('<III', head, 2 + 60 + 2 + 60 + 2 + 1)

        def walk(pos, length, prefix):
            f.seek(pos)
            data = f.read(length)
            o = 0
            while o < len(data):
                kind, = struct.unpack_from('<I', data, o); o += 4
                if kind == 1:
                    p, s, _ = struct.unpack_from('<III', data, o); o += 12
                    e = data.index(b'\0', o); name = data[o:e].decode('latin1'); o = e + 1
                    here = f.tell()
                    found = walk(p, s, prefix + name + '\\')
                    if found is not None:
                        return found
                    f.seek(here)
                else:
                    p, s, _, _, typ, keys = struct.unpack_from('<IIIIII', data, o); o += 24
                    e = data.index(b'\0', o); name = data[o:e].decode('latin1'); o = e + 1
                    e = data.index(b'\0', o); o = e + 1 + 4 * keys
                    ext = struct.pack('<I', typ).rstrip(b'\0')[::-1].decode('latin1')
                    if (prefix + name + ('.' + ext if ext else '')).lower() == want.lower():
                        f.seek(p)
                        return f.read(s)
            return None
        return walk(root, size, '')


def worldorder(rez):
    text = rez_file(rez, 'WORLDS\\WORLDORDER.TXT')
    if text is None:
        raise SystemExit('WORLDS\\WORLDORDER.TXT not found in ' + rez)
    rows = []
    for line in text.decode('latin1').splitlines():
        line = line.split('//')[0].strip()
        if not line:
            continue
        f = [x.strip() for x in line.split(':')]
        if len(f) < 5 or not f[0].lower().startswith('worlds/'):
            continue
        flags = (1 if f[1] == 'INTRO' else 0) | (2 if f[1] == 'WORLDMAP' else 0) | (4 if f[4] == '1' else 0)
        rows.append((f[0].lower()[len('worlds/'):], int(f[2]) + 1, int(f[3]), flags))
    return rows


def table():
    src = HEADER.read_text()
    rows = []
    for m in re.finditer(r'CW\("([^"]+)","([^"]+)",(\d),(\d),([^)]*)\)', src):
        path, label, realm, stage, fl = m.groups()
        flags = 1 if re.search(r'COOP_W_INTRO(?!_OK)', fl) else 0
        flags |= (2 if 'COOP_W_WORLDMAP' in fl else 0) | (4 if 'COOP_W_STAGE1ST' in fl else 0)
        rows.append(('realm%s/%s' % (realm, path), int(realm), int(stage), flags))
        if label.lower() != path:
            raise SystemExit('label %s does not match %s' % (label, path))
    return rows


def main():
    expected = worldorder(sys.argv[1])
    actual = table()
    if expected != actual:
        for i, (e, a) in enumerate(zip(expected, actual)):
            if e != a:
                print('first difference at %d: WORLDORDER %r, table %r' % (i, e, a))
                break
        raise SystemExit('MISMATCH: %d WORLDORDER rows, %d table rows' % (len(expected), len(actual)))
    print('PASS campaign table matches WORLDORDER.TXT: %d worlds, realms %s' % (
        len(actual), sorted({r for _, r, _, _ in actual})))


if __name__ == '__main__':
    main()
