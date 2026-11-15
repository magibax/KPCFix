"""Synthetic DAT fixtures; no proprietary game files are needed to run these tests."""
import struct
import unittest
from analyze_spawn_points import MapFormatError, inspect_dat, game_type


def prop(key, kind, payload):
    k = key.encode('ascii')
    return struct.pack('<H', len(k)) + k + struct.pack('<IBH', kind, 0, len(payload)) + payload


def text_value(value):
    b = value.encode('ascii')
    return struct.pack('<H', len(b)) + b


def obj(cls, name, typ, position):
    fields = [
        prop('Name', 0, text_value(name)),
        prop('Pos', 1, struct.pack('<3f', *position)),
        prop('GameType', 6, struct.pack('<f', typ)),
    ]
    label = cls.encode('ascii')
    return struct.pack('<HH', 223, len(label)) + label + struct.pack('<I', len(fields)) + b''.join(fields)


def world():
    comment = b'Test World\x00'
    start = 16 + len(comment) + 36
    section = struct.pack('<I', 3) + obj('PlayerStart', 'first', 0., (8., 16., 32.)) + obj('PlayerStart', 'second', 1., (64., 16., 32.)) + obj('Light', 'bulb', 0., (0., 0., 0.))
    return struct.pack('<4I', 127, start, start + len(section), len(comment)) + comment + bytes(36) + section + b'geometry'


class TestWorldReader(unittest.TestCase):
    def test_counts(self):
        result = inspect_dat(world(), 'synthetic.dat')
        self.assertEqual(result['objects'], 3)
        self.assertEqual(result['player_start_single'], 1)
        self.assertEqual(result['player_start_multi'], 1)
        self.assertEqual(result['start_points'], None)

    def test_positions(self):
        result = inspect_dat(world(), 'synthetic.dat', True)
        self.assertEqual(result['start_points'][1]['pos'], [64.0, 16.0, 32.0])
        self.assertEqual(result['start_points'][1]['game_type'], 1)

    def test_invalid_world(self):
        bad = bytearray(world())
        bad[0:4] = (999).to_bytes(4, 'little')
        with self.assertRaises(MapFormatError):
            inspect_dat(bad, 'invalid.dat')
        with self.assertRaises(MapFormatError):
            inspect_dat(world()[:70], 'truncated.dat')
        bad = bytearray(world())
        end = int.from_bytes(bad[8:12], 'little')
        bad[8:12] = (end + 1).to_bytes(4, 'little')
        with self.assertRaises(MapFormatError):
            inspect_dat(bad, 'bad-end.dat')

    def test_bad_game_type(self):
        self.assertIsNone(game_type(5, b'\x01'))


if __name__ == '__main__':
    unittest.main()
