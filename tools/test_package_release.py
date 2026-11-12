"""Asset-free tests for strict release manifest validation."""
import unittest
from package_release import manifest_entries, client_path, digest

H = 'a' * 64

class ManifestTests(unittest.TestCase):
    def test_valid_entry(self):
        self.assertEqual(manifest_entries(H + '  ./source/build.py\n'), {'source/build.py': H})

    def test_rejects_parent_traversal(self):
        with self.assertRaises(ValueError):
            manifest_entries(H + '  ./../object.lto\n')

    def test_rejects_duplicate_case_insensitive_paths(self):
        with self.assertRaises(ValueError):
            manifest_entries(H + '  ./README.md\n' + H + '  ./readme.md\n')

    def test_rejects_invalid_hash(self):
        with self.assertRaises(ValueError):
            manifest_entries('not-a-sha  ./README.md\n')

    def test_source_packaging_boundary(self):
        self.assertTrue(client_path('source/src/kpcfix.c'))
        self.assertTrue(client_path('verification/SUMMARY.json'))
        self.assertFalse(client_path('server-tools/KPCServerTools.dll'))
        self.assertFalse(client_path('coop/README.md'))
        self.assertFalse(client_path('.github/workflows/release-integrity.yml'))

    def test_hash(self):
        self.assertEqual(digest(b'abc'), 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')

if __name__ == '__main__':
    unittest.main()
