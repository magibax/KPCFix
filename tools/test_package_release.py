"""Asset-free tests for strict release manifest validation."""
import unittest
from package_release import manifest_entries, client_path, server_path, digest, validate_version, archive_filename

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
        self.assertTrue(client_path('dinput.dll'))
        self.assertTrue(client_path('LICENSES/DDrawCompat-0BSD.txt'))
        self.assertFalse(client_path('source/src/kpcfix.c'))
        self.assertFalse(client_path('verification/SUMMARY.json'))
        self.assertFalse(client_path('TESTING.md'))
        self.assertFalse(client_path('server-tools/KPCServerTools.dll'))
        self.assertFalse(client_path('coop/README.md'))
        self.assertFalse(client_path('.github/workflows/release-integrity.yml'))
        self.assertTrue(server_path('KPCServerTools.dll'))
        self.assertTrue(server_path('legacy-object-patches.json'))
        self.assertFalse(server_path('server-tools-source/src/proxy.c'))
        self.assertFalse(server_path('server-tools-source/tests/check_proxy.py'))

    def test_semver_archive_names(self):
        self.assertEqual(archive_filename('client'), 'KPCFix-Windows-v1.1.2.zip')
        self.assertEqual(archive_filename('server'), 'KPCFix-Server-Tools-v1.0.1.zip')

    def test_semver_rejects_bad_versions(self):
        for value in ('1.13-r5', '1.0.0-r3', '01.2.3', '1.2', 'v1.2.3',
                      '../1.2.3', '1.2.3-beta.1', '', None):
            with self.subTest(value=value), self.assertRaises(ValueError):
                validate_version(value)

    def test_hash(self):
        self.assertEqual(digest(b'abc'), 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')

if __name__ == '__main__':
    unittest.main()
