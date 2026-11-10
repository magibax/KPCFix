"""Synthetic signatures; no proprietary game files required for unit tests."""
import unittest
from check_spawn_site import inspect


class TestSpawnSignature(unittest.TestCase):
    def make_fixture(self):
        buf = bytearray(0x151000)
        buf[:2] = b'MZ'
        buf[0x3c:0x40] = (0x80).to_bytes(4, 'little')
        buf[0x80:0x84] = b'PE\0\0'
        fragments = {
            0x148A7C: b'Error!! Could not find any multiplayer start points.',
            0xF12F1: bytes.fromhex('687c8a1410'),
            0xF12BE: bytes.fromhex('e8adfdffff'),
            0xF1339: bytes.fromhex('e862f5ffff'),
            0xF132B: bytes.fromhex('8db9d0010000'),
            0xF1333: bytes.fromhex('c70700000000'),
            0xF1340: bytes.fromhex('891f'),
            0xF1355: bytes.fromhex('ff92d0010000'),
            0xF13A4: bytes.fromhex('e817fdffff'),
            0xF10C0: bytes.fromhex('81ec80000000'),
            0xF129D: bytes.fromhex('c20c00'),
            0xF13C4: bytes.fromhex('ff91e0010000'),
            0xF15F2: bytes.fromhex('e8a9f2ffff'),
            0xF167A: bytes.fromhex('e841faffff'),
            0xF16C7: bytes.fromhex('ff92e0010000'),
            0xF13CA: bytes.fromhex('83c408'),

            0xF2FAE: bytes.fromhex('ff9090000000'),
            0xF2FB4: bytes.fromhex('83c4083c01'),
            0xF2F5F: bytes.fromhex('c78424d000000005000000'),
            0xF2F6A: bytes.fromhex('c78424d400000030271010'),
            0xF2F75: bytes.fromhex('89b424d8000000'),
            0x3A60: bytes.fromhex('8bc133c9894824894828c3'),
            0x102730: bytes.fromhex('8b44240485c0750332c0c33b4424080f95c0c3'),
            0xEBC7: bytes.fromhex('ff909000000083c408'),
        }
        for off, raw in fragments.items():
            buf[off:off+len(raw)] = raw
        return buf

    def test_known_signatures(self):
        result = inspect(bytes(self.make_fixture()), require_hash=False)
        self.assertEqual(result['guards_passed'], 24)
        self.assertEqual(result['spawn_position_call_rva'], '0x0f13c4')

    def test_reject_wrong_patch_site(self):
        b = self.make_fixture()
        b[0xF13C4] = 0x90
        with self.assertRaisesRegex(ValueError, 'player placement'):
            inspect(bytes(b), require_hash=False)

    def test_reject_missing_fallback(self):
        b = self.make_fixture()
        b[0xF1333] = 0x90
        with self.assertRaisesRegex(ValueError, 'temporary zero'):
            inspect(bytes(b), require_hash=False)

    def test_reject_wrong_intersect_call(self):
        b = self.make_fixture()
        b[0xF2FAE] = 0x90
        with self.assertRaisesRegex(ValueError, 'intersect interface'):
            inspect(bytes(b), require_hash=False)

    def test_reject_wrong_query_layout(self):
        b = self.make_fixture()
        b[0xF2F75+1] = 0x90
        with self.assertRaisesRegex(ValueError, 'intersect filter userdata'):
            inspect(bytes(b), require_hash=False)

    def test_hash_gate(self):
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            inspect(bytes(self.make_fixture()), require_hash=True)

    def test_truncated(self):
        with self.assertRaisesRegex(ValueError, 'truncated'):
            inspect(bytes(4096), require_hash=False)


if __name__ == '__main__':
    unittest.main()
