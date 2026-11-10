import struct
import unittest
from decode_spawn_log import parse


def hx(value):
    return '0x%08x' % struct.unpack('<I', struct.pack('<f', value))[0]


class TestSpawnLog(unittest.TestCase):
    def test_parse(self):
        s = '[KPCFix Co-op] spawn position obj=0x000a1234 x=%s y=%s z=%s' % (
            hx(1600), hx(-256), hx(-2944))
        events = list(parse('unrelated line\n' + s))
        self.assertEqual(events, [(2, '0x000a1234', (1600., -256., -2944.))])

    def test_skip_other_logs(self):
        self.assertEqual(list(parse('Player joined.\n[Server Tools] Cheats enabled')), [])

    def test_invalid_float(self):
        s = '[KPCFix Co-op] spawn position obj=0x00000001 x=0x7fc00000 y=0x00000000 z=0x00000000'
        with self.assertRaises(ValueError):
            list(parse(s))


if __name__ == '__main__':
    unittest.main()
