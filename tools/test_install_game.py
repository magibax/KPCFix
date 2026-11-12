import unittest

from install_game import merge_ini

REPO = '''; KPCFix settings
[Display]
; HUD scale in percent
HudScale=100
VSync=1

[Multiplayer]
; Seconds a chat line stays
ChatMessageSeconds=8
; New in this build
ConsoleKeyFix=1
'''


class MergeIni(unittest.TestCase):
    def test_identical_file_is_untouched(self):
        self.assertEqual(merge_ini(REPO, REPO), (REPO, []))

    def test_missing_key_is_added_with_its_comment_and_values_are_kept(self):
        mine = '[Display]\r\nHudScale=150\r\nVSync=0\r\n\r\n[Multiplayer]\r\nchatmessageseconds = 3\r\n'
        new, added = merge_ini(mine, REPO)
        self.assertEqual(added, ['[multiplayer] ConsoleKeyFix=1'])
        self.assertEqual(new, '[Display]\r\nHudScale=150\r\nVSync=0\r\n\r\n[Multiplayer]\r\nchatmessageseconds = 3\r\n'
                              '; New in this build\r\nConsoleKeyFix=1\r\n')

    def test_missing_section_is_appended(self):
        mine = '[Display]\nHudScale=200\nVSync=1\n'
        new, added = merge_ini(mine, REPO)
        self.assertEqual(added, ['section [multiplayer]'])
        self.assertTrue(new.startswith(mine))
        self.assertTrue(new.endswith('\n[Multiplayer]\n; Seconds a chat line stays\nChatMessageSeconds=8\n'
                                     '; New in this build\nConsoleKeyFix=1\n'))

    def test_key_is_inserted_inside_its_section_not_at_the_end(self):
        mine = '[Display]\nHudScale=100\n\n[Multiplayer]\nChatMessageSeconds=8\nConsoleKeyFix=0\n'
        new, added = merge_ini(mine, REPO)
        self.assertEqual(added, ['[display] VSync=1'])
        self.assertEqual(new, '[Display]\nHudScale=100\nVSync=1\n\n[Multiplayer]\nChatMessageSeconds=8\nConsoleKeyFix=0\n')

    def test_keys_without_a_section(self):
        new, added = merge_ini('FullscreenMode = borderless\n', 'FullscreenMode = exclusive\nVSync = on\n')
        self.assertEqual((new, added), ('FullscreenMode = borderless\nVSync = on\n', ['VSync = on']))

    def test_second_merge_changes_nothing(self):
        once, _ = merge_ini('[Display]\nHudScale=150\n', REPO)
        self.assertEqual(merge_ini(once, REPO), (once, []))


if __name__ == '__main__':
    unittest.main()
