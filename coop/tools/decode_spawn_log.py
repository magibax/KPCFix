#!/usr/bin/env python3
"""Decode KPCFix Co-op read-only spawn traces (x/y/z are IEEE-754 float bits).

Usage: python coop/tools/decode_spawn_log.py /path/to/KPCServerTools.log
"""
import argparse
from collections import defaultdict
import math
from pathlib import Path
import re
import struct

LINE = re.compile(
    r'\[KPCFix Co-op\] spawn position obj=(0x[0-9a-fA-F]{8}) '
    r'x=(0x[0-9a-fA-F]{8}) y=(0x[0-9a-fA-F]{8}) z=(0x[0-9a-fA-F]{8})'
)


def float_from_hex(value):
    return struct.unpack('<f', struct.pack('<I', int(value, 16)))[0]


def parse(text):
    for number, line in enumerate(text.splitlines(), start=1):
        match = LINE.search(line)
        if not match:
            continue
        obj, x, y, z = match.groups()
        xyz = tuple(float_from_hex(value) for value in (x, y, z))
        if not all(math.isfinite(v) for v in xyz):
            raise ValueError(f'Non-finite spawn coordinates at log line {number}')
        yield number, obj, xyz


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    events = list(parse(args.log.read_text(encoding='utf-8', errors='replace')))
    if not events:
        raise SystemExit('No KPCFix Co-op spawn traces found.')
    latest = {}
    for line, obj, xyz in events:
        print(f'{line}: player object {obj}, requested position {tuple(round(x, 2) for x in xyz)}')
        latest[obj] = xyz
    for a in sorted(latest):
        for b in sorted(latest):
            if a >= b:
                continue
            x, y = latest[a], latest[b]
            if all(abs(v - w) < 0.01 for v, w in zip(x, y)):
                print(f'OVERLAP CANDIDATE: {a} and {b} requested the same position')
    print('Note: these are position-setting call inputs, not collision-validated final transforms.')


if __name__ == '__main__':
    main()
