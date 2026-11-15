#!/usr/bin/env python3
"""Read-only inventory of player start objects in official KPC v127 DAT worlds.

Input: local DAT file or a ZIP containing WORLDS/**/*.DAT. Nothing is extracted
or written. The default report has no map geometry or other asset data.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
from zipfile import ZipFile


class MapFormatError(ValueError):
    pass


def read_u16(data, pos):
    if pos + 2 > len(data):
        raise MapFormatError(f"Truncated u16 at offset {pos}")
    return struct.unpack_from('<H', data, pos)[0]


def read_u32(data, pos):
    if pos + 4 > len(data):
        raise MapFormatError(f"Truncated u32 at offset {pos}")
    return struct.unpack_from('<I', data, pos)[0]


def read_text(data, pos, length):
    if length > 32768 or pos + length > len(data):
        raise MapFormatError(f"Truncated or oversized string at {pos}")
    return data[pos:pos + length].decode('latin-1')


def game_type(prop_type, payload):
    # GameType in these map files is serialized as type 6, 32-bit float.
    if prop_type == 6 and len(payload) == 4:
        value = struct.unpack('<f', payload)[0]
        return int(value) if value in (0.0, 1.0) else value
    return None


def inspect_dat(data, name, include_positions=False):
    if len(data) < 64:
        raise MapFormatError(f'{name}: file too short')
    version, start_offset, end_offset, comment_length = struct.unpack_from('<4I', data)
    if version != 127:
        raise MapFormatError(f'{name}: unsupported DAT version {version}')
    if comment_length > 32768 or not 16 + comment_length + 36 <= start_offset < end_offset <= len(data):
        raise MapFormatError(f'{name}: invalid world offsets')
    pos = start_offset
    object_count = read_u32(data, pos)
    pos += 4
    if object_count > 50000:
        raise MapFormatError(f'{name}: unreasonable object count')
    starts = []
    for obj_index in range(object_count):
        _flags = read_u16(data, pos)
        pos += 2
        name_length = read_u16(data, pos)
        pos += 2
        object_class = read_text(data, pos, name_length)
        pos += name_length
        count = read_u32(data, pos)
        pos += 4
        if count > 2048:
            raise MapFormatError(f'{name}: excessive properties on {object_class}')
        is_spawn = object_class in ('PlayerStart', 'GameStartPoint')
        spawn = {'class': object_class, 'object_index': obj_index} if is_spawn else None
        for _ in range(count):
            key_len = read_u16(data, pos)
            pos += 2
            key = read_text(data, pos, key_len)
            pos += key_len
            dtype = read_u32(data, pos)
            pos += 4
            if pos + 3 > len(data):
                raise MapFormatError(f'{name}: truncated property descriptor')
            _property_flags = data[pos]
            pos += 1
            payload_size = read_u16(data, pos)
            pos += 2
            if pos + payload_size > len(data):
                raise MapFormatError(f'{name}: truncated property {key}')
            payload = data[pos:pos + payload_size]
            pos += payload_size
            if not is_spawn:
                continue
            if key == 'GameType':
                spawn['game_type'] = game_type(dtype, payload)
            elif key == 'Name' and dtype == 0 and payload_size >= 2:
                n = read_u16(payload, 0)
                if n == payload_size - 2:
                    spawn['name'] = read_text(payload, 2, n)
            elif include_positions and key == 'Pos' and dtype == 1 and payload_size == 12:
                spawn['pos'] = list(struct.unpack('<fff', payload))
            elif include_positions and key == 'Rotation' and dtype == 7 and payload_size == 16:
                spawn['rotation'] = list(struct.unpack('<ffff', payload))
        if spawn is not None:
            starts.append(spawn)
        if pos > end_offset:
            raise MapFormatError(f'{name}: object data exceeds object-section boundary')
    if pos != end_offset:
        raise MapFormatError(f'{name}: object table ended at {pos}, expected {end_offset}')
    return {
        'map': name,
        'sha256': hashlib.sha256(data).hexdigest(),
        'dat_version': version,
        'objects': object_count,
        'player_start_single': sum(s['class'] == 'PlayerStart' and s.get('game_type') == 0 for s in starts),
        'player_start_multi': sum(s['class'] == 'PlayerStart' and s.get('game_type') == 1 for s in starts),
        'player_start_other': sum(s['class'] == 'PlayerStart' and s.get('game_type') not in (0, 1) for s in starts),
        'game_start_point': sum(s['class'] == 'GameStartPoint' for s in starts),
        'start_points': starts if include_positions else None,
    }


def inspect_file(path, include_positions=False, match=None):
    match = [s.upper() for s in (match or [])]
    if path.suffix.lower() == '.zip':
        with ZipFile(path) as archive:
            entries = sorted((item for item in archive.infolist()
                              if item.filename.upper().startswith('WORLDS/')
                              and item.filename.upper().endswith('.DAT')),
                             key=lambda item: item.filename)
            for entry in entries:
                if match and not any(m in entry.filename.upper() for m in match):
                    continue
                if entry.file_size > 32_000_000:
                    raise MapFormatError(f'{entry.filename}: oversized DAT')
                yield inspect_dat(archive.read(entry), entry.filename, include_positions)
    else:
        if path.suffix.lower() != '.dat':
            raise MapFormatError('Input must be a .DAT world or a .ZIP archive')
        if not match or any(m in path.name.upper() for m in match):
            yield inspect_dat(path.read_bytes(), path.name, include_positions)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path, help='Local MAPDATA.zip or a DAT file')
    parser.add_argument('--map', action='append', default=[], help='Filter by substring (repeatable)')
    parser.add_argument('--positions', action='store_true', help='Include player-start names and coordinates')
    parser.add_argument('--json', action='store_true', help='Emit JSON instead of a table')
    args = parser.parse_args()
    results = list(inspect_file(args.input, args.positions, args.map))
    if args.json:
        print(json.dumps(results, indent=2))
    else:
        print(f'{"Map":<24} {"Objects":>7} {"SP":>4} {"MP":>4} {"Other":>5} {"GSP":>4}')
        for row in results:
            print(f'{Path(row["map"]).stem:<24} {row["objects"]:>7} '
                  f'{row["player_start_single"]:>4} {row["player_start_multi"]:>4} '
                  f'{row["player_start_other"]:>5} {row["game_start_point"]:>4}')
            if args.positions:
                for s in row['start_points']:
                    print('   ', s)
        print(f'Parsed {len(results)} world(s) successfully.')


if __name__ == '__main__':
    main()
