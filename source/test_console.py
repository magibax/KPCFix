"""Console key character (ConsoleKeyFix): the toggle key-down call (client.exe
0x6dc9 -> 0x287a0) is timestamped and the WM_CHAR backtick test (0x700a) also
drops the next character within 250 ms, so the UK apostrophe that opens the
console is not typed into it. Helpers are driven directly; the two sites are
checked byte for byte."""
import json
import struct
import sys
import test_runtime

sys.argv += ['--expose-rig']
Rig, _, _, uc, x, args = test_runtime.main()
syms = json.loads(args.symbols.read_text())
pack = lambda *v: struct.pack('<' + 'I' * len(v), *[i & 0xffffffff for i in v])
read = lambda u, a: struct.unpack('<I', u.mem_read(a, 4))[0]
CLIENT = 0x1ca0000


def target(rig, site):
    code = bytes(rig.u.mem_read(CLIENT + site, 5))
    assert code[0] == 0xe8, (hex(site), code.hex())
    return (CLIENT + site + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff


rig = Rig(settings={'Multiplayer.SinglePlayerChat': 0}); rig.factory(); assert not rig.messages, rig.messages   # single-player chat: its own rig below
assert target(rig, 0x6dc9) == rig.dllbase + syms['_kpc_console_key_entry']
assert target(rig, 0x700a) == rig.dllbase + syms['_kpc_console_char_entry']
assert b'Console key: the character typed by the console toggle key' in bytes(rig.logs)
key, drop = rig.dllbase + syms['_kpc_console_key_down'], rig.dllbase + syms['_kpc_console_char_drop']
rig.u.mem_write(0x1d4a60c, pack(1))
rig.tick = 10000
assert rig.invoke_address(key, ()) == 1                     # returns [0x1d4a60c] like 0x287a0
rig.tick = 10050; assert rig.invoke_address(drop, ()) == 1  # the toggle's own character: dropped
rig.tick = 10060; assert rig.invoke_address(drop, ()) == 0  # the next one is typed
rig.tick = 20000; rig.invoke_address(key, ())
rig.tick = 20400; assert rig.invoke_address(drop, ()) == 0  # a character long after the key: typed
assert rig.invoke_address(drop, ()) == 0
import fixture_layout
if fixture_layout.REGION not in ('Europe', 'USA'):
    # The chat-typing and window hooks below exist only for the Europe/USA
    # shell layout; other editions log that and keep the original behaviour.
    assert b'Chat typing: not verified for this cshell edition' in bytes(rig.logs)
    assert b'Chat typing: key-bound actions' not in bytes(rig.logs)
    assert b'Scoreboard hold: not verified for this cshell edition' in bytes(rig.logs)
    rig0 = Rig(settings={'ConsoleKeyFix': 0}); rig0.factory(); assert not rig0.messages
    assert bytes(rig0.u.mem_read(CLIENT + 0x6dc9, 5)) == bytes(rig0.initial_client[0x6dc9:0x6dce])
    assert bytes(rig0.u.mem_read(CLIENT + 0x700a, 5)) == bytes.fromhex('83f8607445')
    print(args.fixtures.name, 'passed console key; chat-typing and window hooks are Europe/USA only and stay off here')
    sys.exit(0)
# Chat typing (ChatBlocksActions): OnCommandOn vtable slot (cshell 0xff724)
# drops every action while the message editor (shell+0x8c) is typing (+0x12ed).
SHELLBASE, SHELL = 0x20000000, 0x5a0000
assert read(rig.u, SHELLBASE + 0xff724) == rig.dllbase + syms['_kpc_command_on']
assert b'Chat typing: key-bound actions' in bytes(rig.logs)
actions = []
def command_on(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP); actions.append((u.reg_read(x.UC_X86_REG_ECX), read(u, sp + 4)))
    u.reg_write(x.UC_X86_REG_ESP, sp + 8); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
rig.u.hook_add(uc.UC_HOOK_CODE, command_on, begin=SHELLBASE + 0xc9d80, end=SHELLBASE + 0xc9d80)
command = rig.dllbase + syms['_kpc_command_on']
rig.invoke_address(command, (90,), this=SHELL); assert actions == [(SHELL, 90)], actions      # not typing: NoClip passes
rig.u.mem_write(SHELL + 0x8c + 0x12ed, b'\x01'); actions.clear()
for a in (90, 93, 0, 42): rig.invoke_address(command, (a,), this=SHELL)
assert actions == [], actions                                                            # typing: all dropped
rig.u.mem_write(SHELL + 0x8c + 0x12ed, b'\x00')
rig.invoke_address(command, (2,), this=SHELL); assert actions == [(SHELL, 2)], actions
# Engine command state (sent to the server, polled there by IsCommandOn):
# client.exe 0x23c84 "mov ecx,[eax+0x1014]" (input enabled) -> gate entry.
site = bytes(rig.u.mem_read(CLIENT + 0x23c84, 6))
assert site[5] == 0x90 and target(rig, 0x23c84) == rig.dllbase + syms['_kpc_input_state_entry'], site.hex()
assert b'Chat typing: no command' in bytes(rig.logs)
CM = 0x5b0000
entry = rig.dllbase + syms['_kpc_input_state_entry']
def gate(enabled):
    """The entry as called from 0x23c84: EAX = cm; returns ECX, EAX, EDX after."""
    rig.u.mem_write(CM + 0x1014, pack(enabled))
    rig.u.reg_write(x.UC_X86_REG_EAX, CM); rig.u.reg_write(x.UC_X86_REG_EDX, 0x1234); rig.u.reg_write(x.UC_X86_REG_ECX, 0xdead)
    rig.invoke_address(entry, ())
    return rig.u.reg_read(x.UC_X86_REG_ECX), rig.u.reg_read(x.UC_X86_REG_EAX), rig.u.reg_read(x.UC_X86_REG_EDX)
rig.u.mem_write(SHELLBASE + 0x1c8de8, pack(SHELL))
rig.u.mem_write(SHELL + 0x8c + 0x12ed, b'\x00')
assert gate(1) == (1, CM, 0x1234) and gate(0) == (0, CM, 0x1234)  # not typing: ECX = [cm+0x1014], EAX/EDX kept
rig.u.mem_write(SHELL + 0x8c + 0x12ed, b'\x01')
assert gate(1) == (0, CM, 0x1234)                                 # typing: the frame's commands are cleared
assert gate(0) == (0, CM, 0x1234)
rig.u.mem_write(SHELLBASE + 0x1c8de8, pack(0))
assert gate(1) == (1, CM, 0x1234)                                 # no shell object: stock
rig.u.mem_write(SHELLBASE + 0x1c8de8, pack(SHELL)); rig.u.mem_write(SHELL + 0x8c + 0x12ed, b'\x00')
# Scoreboard held (ScoreboardHold): the key scoreboard (screen type 4 of the
# manager at [shell+0x138c]) is never closed by a press and closes on release
# (OnCommandOff slot 0xff728, original 0xcab60 "ret 4"; close 0xad3c0).
MGR = 0x5a4000
assert read(rig.u, SHELLBASE + 0xff728) == rig.dllbase + syms['_kpc_command_off']
assert b'Scoreboard hold: the scoreboard shows while its key is held' in bytes(rig.logs)
closes = []
def close(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP); closes.append(u.reg_read(x.UC_X86_REG_ECX))
    u.reg_write(x.UC_X86_REG_ESP, sp + 4); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
rig.u.hook_add(uc.UC_HOOK_CODE, close, begin=SHELLBASE + 0xad3c0, end=SHELLBASE + 0xad3c0)
off = rig.dllbase + syms['_kpc_command_off']
rig.u.mem_write(SHELL + 0x138c, pack(MGR))
def screen(kind): rig.u.mem_write(MGR + 4, struct.pack('<i', kind))
screen(-1); actions.clear()
rig.invoke_address(command, (41,), this=SHELL); assert actions == [(SHELL, 41)], actions     # nothing up: the stock show
screen(4); actions.clear()
rig.invoke_address(command, (41,), this=SHELL); assert actions == [], actions               # showing: a press keeps it
rig.invoke_address(off, (40,), this=SHELL); assert closes == []                             # another action released
rig.invoke_address(off, (41,), this=SHELL); assert closes == [MGR], closes                  # release: closed
screen(2); closes.clear(); actions.clear()
rig.invoke_address(off, (41,), this=SHELL); assert closes == []                             # server scoreboard: untouched
rig.invoke_address(command, (41,), this=SHELL); assert actions == [(SHELL, 41)]             # stock (ignores it while type 2)
rig.u.mem_write(SHELL + 0x138c, pack(0)); rig.invoke_address(off, (41,), this=SHELL); assert closes == []
# Chat clipboard keys (ChatClipboard): the message editor's OnKeyDown slot
# (0x1004ec -> 0xd8040). Ctrl+A selects (the next key replaces the line),
# Ctrl+V pastes the first clipboard line (allowed characters), Ctrl+C copies,
# Ctrl+letters type nothing, AltGr and plain keys reach the editor.
assert read(rig.u, SHELLBASE + 0x1004ec) == rig.dllbase + syms['_kpc_chat_key']
assert b'Chat clipboard keys: Ctrl+A selects the line' in bytes(rig.logs)
ED, CLIPMEM, STUBS = 0x5b8000, 0x5bc000, 0x62000800
editor_keys, layouts, pressed, clip = [], [], set(), {'text': b'', 'set': None}
def key_hook(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP); editor_keys.append((read(u, sp + 4), read(u, sp + 8)))
    u.reg_write(x.UC_X86_REG_EAX, 1); u.reg_write(x.UC_X86_REG_ESP, sp + 12); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
rig.u.hook_add(uc.UC_HOOK_CODE, key_hook, begin=SHELLBASE + 0xd8040, end=SHELLBASE + 0xd8040)
def layout_hook(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP); layouts.append(u.reg_read(x.UC_X86_REG_ECX))
    u.reg_write(x.UC_X86_REG_ESP, sp + 4); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
rig.u.hook_add(uc.UC_HOOK_CODE, layout_hook, begin=SHELLBASE + 0xd7aa0, end=SHELLBASE + 0xd7aa0)
def api(index, nargs, fn):
    at = STUBS + index * 0x10; rig.u.mem_write(at, b'\xc3')
    def run(u, a, size, user):
        sp = u.reg_read(x.UC_X86_REG_ESP); args = [read(u, sp + 4 + 4 * k) for k in range(nargs)]
        u.reg_write(x.UC_X86_REG_EAX, fn(args) & 0xffffffff); u.reg_write(x.UC_X86_REG_ESP, sp + 4 + 4 * nargs); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
    rig.u.hook_add(uc.UC_HOOK_CODE, run, begin=at, end=at)
    return at
def set_data(a):
    clip['set'] = (a[0], bytes(rig.u.mem_read(a[1], 64)).split(b'\0')[0]); return a[1]
table = [1,
         api(0, 1, lambda a: 0x8000 if a[0] in pressed else 0),              # GetKeyState
         api(1, 1, lambda a: 1), api(2, 0, lambda a: 1), api(3, 0, lambda a: 1),  # Open, Close, Empty
         api(4, 1, lambda a: (rig.u.mem_write(CLIPMEM, clip['text'] + b'\0'), CLIPMEM)[1]),   # GetClipboardData
         api(5, 2, set_data),                                                 # SetClipboardData
         api(6, 2, lambda a: CLIPMEM + 0x1000), api(7, 1, lambda a: a[0]), api(8, 1, lambda a: 1)]   # GlobalAlloc/Lock/Unlock
rig.u.mem_write(rig.dllbase + syms['_clip'], struct.pack('<10I', *table))
chat_key = rig.dllbase + syms['_kpc_chat_key']
def editor(text, cursor=None):
    rig.u.mem_write(ED, bytes(0x1300)); rig.u.mem_write(ED + 0x178, text + b'\0')
    rig.u.mem_write(ED + 0x1244, pack(len(text) if cursor is None else cursor))
    rig.u.mem_write(ED + 0x1265, b'\x01\x01')                                 # editing, insert mode
    rig.u.mem_write(ED + 0x1267, b' abcdefghijklmnopqrstuvwxyz!?\0')
def text(): return bytes(rig.u.mem_read(ED + 0x178, 0x100)).split(b'\0')[0]   # text + former backup (long lines)
def key(vk, ctrl=False, alt=False):
    pressed.clear(); pressed.update(([0x11] if ctrl else []) + ([0x12] if alt else []))
    return rig.invoke_address(chat_key, (vk, 0), this=ED) & 0xff
editor(b'hello'); editor_keys.clear()
assert key(0x41) == 1 and editor_keys == [(0x41, 0)]                          # plain 'A': typed by the editor
editor_keys.clear(); assert key(0x41, ctrl=True) == 0 and editor_keys == []     # Ctrl+A: selected
assert rig.u.mem_read(ED + 0x1264, 1)[0] == 1
assert key(0x42) == 1 and editor_keys == [(0x42, 0)] and text() == b''           # typing replaces the line
editor(b'hello'); key(0x41, ctrl=True); editor_keys.clear(); layouts.clear()
assert key(0x08) == 0 and text() == b'' and editor_keys == [] and layouts == [ED]   # Backspace clears it
editor(b'say ', cursor=4); clip['text'] = b'Hi there\tYOU\r\nsecond line'; layouts.clear()
assert key(0x56, ctrl=True) == 0 and text() == b'say i there ' and layouts == [ED]   # allowed characters, first line, tab -> space
assert read(rig.u, ED + 0x1244) == 12
editor(b'x' * 198); clip['text'] = b'abcdef'
key(0x56, ctrl=True); assert text() == b'x' * 198 + b'ab', (len(text()), text()[-8:])   # 200 characters at most (ChatLongMessages)
editor(b'copy me'); key(0x43, ctrl=True); assert clip['set'] == (1, b'copy me'), clip['set']   # CF_TEXT
editor(b'cut'); key(0x58, ctrl=True); assert clip['set'] == (1, b'cut') and text() == b''
editor(b'z'); editor_keys.clear()
assert key(0x51, ctrl=True) == 0 and editor_keys == []                         # other Ctrl+letters type nothing
assert key(0x51, ctrl=True, alt=True) == 1 and editor_keys == [(0x51, 0)]      # AltGr reaches the editor
# Chat scrollback (ChatScrollback): server messages added by the 0x45 handler
# (call 0xcb441 -> add 0xd56d0) are kept; Page Up in the chat box re-adds the
# previous page through the stock add, Page Down the next one.
code = bytes(rig.u.mem_read(SHELLBASE + 0xcb441, 5))
assert code[0] == 0xe8 and (SHELLBASE + 0xcb441 + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == rig.dllbase + syms['_kpc_chat_add']
assert b'Chat scrollback: Page Up / Page Down' in bytes(rig.logs)
LIST, MSG = SHELLBASE + 0x1d24b8, 0x5bd000
added = []
def add_hook(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP)
    added.append((u.reg_read(x.UC_X86_REG_ECX), rig.cstr(read(u, sp + 4)), struct.unpack('<f', bytes(u.mem_read(sp + 8, 4)))[0], read(u, sp + 12), read(u, sp + 16)))
    u.reg_write(x.UC_X86_REG_ESP, sp + 20); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
rig.u.hook_add(uc.UC_HOOK_CODE, add_hook, begin=SHELLBASE + 0xd56d0, end=SHELLBASE + 0xd56d0)
chat_add = rig.dllbase + syms['_kpc_chat_add']
def incoming(text, flags=0x46, colour=5):
    rig.u.mem_write(MSG, text.encode() + b'\0')
    rig.invoke_address(chat_add, (MSG, struct.unpack('<I', struct.pack('<f', 5.0))[0], flags, colour), this=LIST)
for i in range(10): incoming('line %d' % i)
incoming('##exploit', colour=3)
incoming('hidden', flags=0x10)                                                  # not added to the list: not kept
assert len(added) == 12 and added[0] == (LIST, 'line 0', 5.0, 0x46, 5)            # all forwarded unchanged
editor(b''); added.clear(); editor_keys.clear()
assert key(0x21) == 0 and editor_keys == []                                    # Page Up: handled
assert [a[1] for a in added] == ['line 5', 'line 6', 'line 7', 'line 8', 'line 9', '# exploit'], added   # newest page first
assert added[-1][2:] == (8.0, 0, 3)                                            # 8 s, no flags, its colour
added.clear(); key(0x21); assert [a[1] for a in added] == ['line 0', 'line 1', 'line 2', 'line 3', 'line 4'], added
added.clear(); key(0x21); assert [a[1] for a in added] == ['line 0', 'line 1', 'line 2', 'line 3', 'line 4'], added   # oldest page stays
added.clear(); key(0x22); assert [a[1] for a in added] == ['line 5', 'line 6', 'line 7', 'line 8', 'line 9', '# exploit'], added
incoming('new'); added.clear(); key(0x21)                                        # a new message resets the view
assert [a[1] for a in added] == ['line 6', 'line 7', 'line 8', 'line 9', '# exploit', 'new'], added
rig.u.mem_write(ED + 0x1265, b'\x00'); added.clear(); editor_keys.clear()
assert key(0x21) == 1 and added == [] and editor_keys == [(0x21, 0)]           # chat box closed: stock
rigsb = Rig(settings={'Multiplayer.ChatScrollback': 0, 'Multiplayer.ChatLongMessages': 0}); rigsb.factory(); assert not rigsb.messages   # long lines also guard the add call
assert bytes(rigsb.u.mem_read(SHELLBASE + 0xcb441, 5)) == bytes(rigsb.initial_shell[0xcb441:0xcb446])
# Long chat lines (ChatLongMessages): the insert call 0xd81da goes to
# kpc_chat_insert (200 characters, allowed characters, insert mode), Esc
# (slot 0x1004f0) clears, the scroll cap 0xd7ce1 is gone and the width is 640.
code = bytes(rig.u.mem_read(SHELLBASE + 0xd81da, 5))
assert code[0] == 0xe8 and (SHELLBASE + 0xd81da + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == rig.dllbase + syms['_kpc_chat_insert']
assert bytes(rig.u.mem_read(SHELLBASE + 0xd7ce1, 5)) == b'\x90' * 5 and bytes(rig.u.mem_read(SHELLBASE + 0xca916, 6)) == bytes.fromhex('bf8002000090')
assert read(rig.u, SHELLBASE + 0x1004f0) == rig.dllbase + syms['_kpc_chat_escape']
assert b'Long chat lines: up to 200 characters' in bytes(rig.logs)
chars = {}
def key_char(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP); u.reg_write(x.UC_X86_REG_EAX, chars.get(read(u, sp + 4), 0))
    u.reg_write(x.UC_X86_REG_EIP, read(u, sp)); u.reg_write(x.UC_X86_REG_ESP, sp + 4)
rig.u.hook_add(uc.UC_HOOK_CODE, key_char, begin=SHELLBASE + 0xe4480, end=SHELLBASE + 0xe4480)
insert = rig.dllbase + syms['_kpc_chat_insert']
chars.update({0x41: ord('a'), 0x42: ord('B'), 0x43: ord('c')})
editor(b'x' * 150); rig.invoke_address(insert, (0x41,), this=ED); assert text() == b'x' * 150 + b'a'   # past the stock 127
editor(b'x' * 200); rig.invoke_address(insert, (0x41,), this=ED); assert text() == b'x' * 200          # 200 at most
editor(b'ac', cursor=1); rig.invoke_address(insert, (0x43,), this=ED); assert text() == b'acc' and read(rig.u, ED + 0x1244) == 2
editor(b'ac'); rig.invoke_address(insert, (0x42,), this=ED); assert text() == b'ac'                    # 'B' not allowed
rig.invoke_address(rig.dllbase + syms['_kpc_chat_escape'], (), this=ED); assert text() == b''
# Selection highlight and placement: Draw (slot 0x1004cc) shows the selected
# text with state flag +0x24 and keeps the bar below the list's bottom.
assert read(rig.u, SHELLBASE + 0x1004cc) == rig.dllbase + syms['_kpc_chat_draw']
draws = []
def draw_hook(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP); draws.append((read(u, sp + 4), u.mem_read(ED + 0x24, 1)[0], read(u, ED + 0x2c)))
    u.reg_write(x.UC_X86_REG_ESP, sp + 8); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
rig.u.hook_add(uc.UC_HOOK_CODE, draw_hook, begin=SHELLBASE + 0xd8200, end=SHELLBASE + 0xd8200)
chat_draw = rig.dllbase + syms['_kpc_chat_draw']
editor(b'hello'); rig.u.mem_write(ED + 0x2c, pack(410)); key(0x41, ctrl=True)
rig.invoke_address(chat_draw, (0x777,), this=ED); assert draws[-1] == (0x777, 1, 410) and rig.u.mem_read(ED + 0x24, 1)[0] == 0
key(0x42); rig.invoke_address(chat_draw, (0x777,), this=ED); assert draws[-1][1] == 0   # typing ended the selection
list_update = rig.dllbase + syms['_kpc_list_update']
def list_hook(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP); u.reg_write(x.UC_X86_REG_EIP, read(u, sp)); u.reg_write(x.UC_X86_REG_ESP, sp + 4)
rig.u.hook_add(uc.UC_HOOK_CODE, list_hook, begin=SHELLBASE + 0xd50c0, end=SHELLBASE + 0xd50c0)
rig.u.mem_write(LIST + 0x3c, pack(286)); rig.u.mem_write(LIST + 0x44, pack(430))       # list drawn lower than the bar
rig.invoke_address(list_update, (), this=LIST)
rig.u.mem_write(ED + 0x2c, pack(410)); rig.invoke_address(chat_draw, (0x777,), this=ED)
assert draws[-1][2] == 432 and b'Chat layout: entry bar y 410 moved below the chat lines (list 286-430' in bytes(rig.logs)
rig.u.mem_write(LIST + 0x44, pack(386)); rig.invoke_address(list_update, (), this=LIST)
rig.u.mem_write(ED + 0x2c, pack(410)); rig.invoke_address(chat_draw, (0x777,), this=ED); assert draws[-1][2] == 410   # already below
# Chat history (ChatHistory): Enter keeps the sent line; Up / Down recall them
# (no stray '&' / '('), past the newest the draft returns.
editor(b'first'); key(0x0d); editor(b'second'); key(0x0d); editor(b'second'); key(0x0d)   # repeat not kept
editor(b'draft'); editor_keys.clear(); layouts.clear()
assert key(0x26) == 0 and text() == b'second' and editor_keys == [] and layouts
assert key(0x26) == 0 and text() == b'first'
assert key(0x26) == 0 and text() == b'first'                                   # oldest stays
assert key(0x28) == 0 and text() == b'second'
assert key(0x28) == 0 and text() == b'draft' and read(rig.u, ED + 0x1244) == 5   # back to the draft
# Hidden while loading (ChatHideOnLoading): with a loading screen (type 0) up the
# bar ignores keys and is not drawn; the list draw (0xcdba8) is skipped and its
# oldest line's timer restarts when it shows again.
code = bytes(rig.u.mem_read(SHELLBASE + 0xcdba8, 5))
assert code[0] == 0xe8 and (SHELLBASE + 0xcdba8 + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == rig.dllbase + syms['_kpc_list_draw']
SCR = 0x5be000
rig.u.mem_write(SHELLBASE + 0x1c8de8, pack(SHELL)); rig.u.mem_write(SHELL + 0x138c, pack(SCR)); rig.u.mem_write(SCR + 4, struct.pack('<i', 0))
rig.u.mem_write(SHELL + 0x1398, pack(1))
editor(b'typing'); editor_keys.clear(); draws.clear()
assert key(0x41) == 0 and editor_keys == [] and text() == b'typing'
rig.invoke_address(chat_draw, (0x777,), this=ED); assert draws == []
lists = []
def list_draw_hook(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP); lists.append(read(u, sp + 4))
    u.reg_write(x.UC_X86_REG_ESP, sp + 8); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
rig.u.hook_add(uc.UC_HOOK_CODE, list_draw_hook, begin=SHELLBASE + 0xd5d80, end=SHELLBASE + 0xd5d80)
ENTRY, TIMEFN, APIOBJ = 0x5bf000, 0x62000600, 0x5bf800
rig.u.mem_write(LIST + 0xc, pack(ENTRY)); rig.u.mem_write(ENTRY + 0x10, struct.pack('<f', 5.0)); rig.u.mem_write(ENTRY + 0x14, struct.pack('<f', 1.0))
rig.u.mem_write(ENTRY + 0x121, b'\x01'); rig.u.mem_write(ENTRY + 0x138, pack(0))
rig.u.mem_write(TIMEFN, b'\xd9\x05' + pack(TIMEFN + 0x10) + b'\xc3'); rig.u.mem_write(TIMEFN + 0x10, struct.pack('<f', 100.0))
saved_api = read(rig.u, SHELLBASE + 0x1c8dfc); rig.u.mem_write(SHELLBASE + 0x1c8dfc, pack(APIOBJ)); rig.u.mem_write(APIOBJ + 0x210, pack(TIMEFN))
list_draw = rig.dllbase + syms['_kpc_list_draw']
rig.invoke_address(list_draw, (0x555,), this=LIST); assert lists == []          # hidden
rig.u.mem_write(SCR + 4, struct.pack('<i', -1))                                  # picture closed
rig.invoke_address(list_draw, (0x555,), this=LIST); assert lists == [0x555]
assert struct.unpack('<f', bytes(rig.u.mem_read(ENTRY + 0x14, 4)))[0] == 105.0 and rig.u.mem_read(ENTRY + 0x121, 1)[0] == 0
rig.u.mem_write(SHELLBASE + 0x1c8dfc, pack(saved_api))
assert key(0x41) == 1                                                            # keys again
# Label hint: Init (slot 0x100500) gets the short "MSG (PgUp/Dn, Up/Dn):" and an empty text;
# then (ChatLowercaseFont) the three bar fonts are reloaded from Arial 7 (white, yellow selection) and the bar rebuilt.
assert read(rig.u, SHELLBASE + 0x100500) == rig.dllbase + syms['_kpc_chat_init']
inits = []
def init_hook(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP); inits.append((rig.cstr(read(u, sp + 4)), rig.cstr(read(u, sp + 8)), read(u, sp + 12)))
    u.reg_write(x.UC_X86_REG_ESP, sp + 24); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
rig.u.hook_add(uc.UC_HOOK_CODE, init_hook, begin=SHELLBASE + 0xd7620, end=SHELLBASE + 0xd7620)
fontops = []
def font_term_hook(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP); fontops.append(('term', u.reg_read(x.UC_X86_REG_ECX) - ED))
    u.reg_write(x.UC_X86_REG_ESP, sp + 4); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
def font_init_hook(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP)
    fontops.append(('init', u.reg_read(x.UC_X86_REG_ECX) - ED, read(u, sp + 4), read(u, sp + 8), rig.cstr(read(u, sp + 12)), read(u, sp + 16)))
    u.reg_write(x.UC_X86_REG_EAX, 1); u.reg_write(x.UC_X86_REG_ESP, sp + 20); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
rig.u.hook_add(uc.UC_HOOK_CODE, font_term_hook, begin=SHELLBASE + 0xb24a0, end=SHELLBASE + 0xb24a0)
rig.u.hook_add(uc.UC_HOOK_CODE, font_init_hook, begin=SHELLBASE + 0xb2550, end=SHELLBASE + 0xb2550)
layouts.clear()   # the bar rebuild 0xd7aa0 is the layout stub above
saved_api = read(rig.u, SHELLBASE + 0x1c8dfc); rig.u.mem_write(SHELLBASE + 0x1c8dfc, pack(APIOBJ)); rig.u.mem_write(ED + 0x278, pack(0x4242))
rig.u.mem_write(MSG, b'MSG:\0')
rig.invoke_address(rig.dllbase + syms['_kpc_chat_init'], (MSG, MSG + 0x100, 640, 5, 0), this=ED)
assert inits == [('MSG (PgUp/Dn, Up/Dn):', '', 640)], inits
assert fontops == [('term', 0x27c), ('init', 0x27c, APIOBJ, 0x4242, 'interface/arial7_white.pcf', 1),
                   ('term', 0x7b8), ('init', 0x7b8, APIOBJ, 0x4242, 'interface/arial7_white.pcf', 1),
                   ('term', 0xcf4), ('init', 0xcf4, APIOBJ, 0x4242, 'interface/arial7_yellow.pcf', 1)], fontops
assert layouts == [ED], layouts
assert b'Chat bar: lowercase fonts (Arial 7 white, yellow selection) loaded.' in bytes(rig.logs)
rig.u.mem_write(SHELLBASE + 0x1c8dfc, pack(saved_api))
# Chat in cinematics: Space while typing goes only to the bar (no skip), other keys to the shell.
assert read(rig.u, SHELLBASE + 0xff72c) == rig.dllbase + syms['_kpc_shell_key']
shell_keys = []
def shell_key_hook(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP); shell_keys.append(read(u, sp + 4))
    u.reg_write(x.UC_X86_REG_ESP, sp + 12); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
rig.u.hook_add(uc.UC_HOOK_CODE, shell_key_hook, begin=SHELLBASE + 0xccab0, end=SHELLBASE + 0xccab0)
EDV = 0x5c0800; rig.u.mem_write(SHELL + 0x8c, pack(EDV)); rig.u.mem_write(EDV + 0x2c, pack(SHELLBASE + 0xd8040))
rig.u.mem_write(SHELL + 0x8c + 0x12ed, b'\x01'); editor_keys.clear()
rig.invoke_address(rig.dllbase + syms['_kpc_shell_key'], (0x20, 0), this=SHELL)
assert shell_keys == [] and editor_keys == [(0x20, 0)], (shell_keys, editor_keys)
rig.invoke_address(rig.dllbase + syms['_kpc_shell_key'], (0x41, 0), this=SHELL); assert shell_keys == [0x41]
rig.u.mem_write(SHELL + 0x8c + 0x12ed, b'\x00')
rig.invoke_address(rig.dllbase + syms['_kpc_shell_key'], (0x20, 0), this=SHELL); assert shell_keys == [0x41, 0x20]
# Spawn lightning off (SpawnLightning=0, default): "jl" -> "jmp" over the bolt at 0x7b5b0.
assert bytes(rig.u.mem_read(SHELLBASE + 0x7b5b0, 2)) == b'\xeb\x20' and b'Spawn lightning: off' in bytes(rig.logs)
rigsl = Rig(settings={'Multiplayer.SpawnLightning': 1, 'Multiplayer.ChatLongMessages': 0}); rigsl.factory(); assert not rigsl.messages
assert bytes(rigsl.u.mem_read(SHELLBASE + 0x7b5b0, 2)) == b'\x7c\x20'
assert bytes(rigsl.u.mem_read(SHELLBASE + 0xd81da, 5)) == bytes(rigsl.initial_shell[0xd81da:0xd81df])
rigc0 = Rig(settings={'Multiplayer.ChatClipboard': 0, 'Multiplayer.ChatScrollback': 0, 'Multiplayer.ChatLongMessages': 0, 'Multiplayer.ChatHistory': 0, 'Multiplayer.ChatHideOnLoading': 0}); rigc0.factory(); assert not rigc0.messages
assert read(rigc0.u, SHELLBASE + 0x1004ec) == SHELLBASE + 0xd8040
rigs = Rig(settings={'Multiplayer.ScoreboardHold': 0}); rigs.factory(); assert not rigs.messages
assert read(rigs.u, SHELLBASE + 0xff728) == SHELLBASE + 0xcab60
assert read(rigs.u, SHELLBASE + 0xff724) == rigs.dllbase + syms['_kpc_command_on']   # chat typing still wraps it
rigc = Rig(settings={'Multiplayer.ChatBlocksActions': 0}); rigc.factory(); assert not rigc.messages
assert read(rigc.u, SHELLBASE + 0xff724) == rigc.dllbase + syms['_kpc_command_on']   # the scoreboard still needs it
rigc = Rig(settings={'Multiplayer.ChatBlocksActions': 0, 'Multiplayer.ScoreboardHold': 0, 'Multiplayer.SinglePlayerChat': 0}); rigc.factory(); assert not rigc.messages
assert read(rigc.u, SHELLBASE + 0xff724) == SHELLBASE + 0xc9d80
assert bytes(rigc.u.mem_read(CLIENT + 0x23c84, 6)) == bytes.fromhex('8b8814100000')
# Disabled: both sites keep their original bytes.
rig0 = Rig(settings={'ConsoleKeyFix': 0}); rig0.factory(); assert not rig0.messages
assert bytes(rig0.u.mem_read(CLIENT + 0x6dc9, 5)) == bytes(rig0.initial_client[0x6dc9:0x6dce])
assert bytes(rig0.u.mem_read(CLIENT + 0x700a, 5)) == bytes.fromhex('83f8607445')
# Window-shard diagnostic (LogWindowBreaks): the shard manager's handler slot
# (cshell vtable 0xfe184 -> 0x5bd30, thiscall, ret 0xc) logs whether the
# window object reached this client, then runs the original handler.
assert read(rig.u, SHELLBASE + 0xfe184) == rig.dllbase + syms['_kpc_window_break']
handled = []
def window_handler(u, at, size, user):
    sp = u.reg_read(x.UC_X86_REG_ESP)
    handled.append((u.reg_read(x.UC_X86_REG_ECX),) + tuple(read(u, sp + 4 + 4 * i) for i in range(3)))
    u.reg_write(x.UC_X86_REG_EIP, read(u, sp)); u.reg_write(x.UC_X86_REG_ESP, sp + 16)
rig.u.hook_add(uc.UC_HOOK_CODE, window_handler, begin=SHELLBASE + 0x5bd30, end=SHELLBASE + 0x5bd30)
# HideBrokenWindow: after the shards, FLAG_VISIBLE is cleared through the client
# API [cshell+0x1c8dfc] +0x290 GetObjectFlags / +0x294 SetObjectFlags (cdecl).
API, APICODE, flag_calls = 0x5ba000, 0x5bb000, []
def api_stub(slot, nargs, result):
    def run(u, at, size, user):
        sp = u.reg_read(x.UC_X86_REG_ESP)
        flag_calls.append((slot,) + tuple(read(u, sp + 4 + 4 * i) for i in range(nargs)))
        u.reg_write(x.UC_X86_REG_EAX, result); u.reg_write(x.UC_X86_REG_EIP, read(u, sp)); u.reg_write(x.UC_X86_REG_ESP, sp + 4)
    rig.u.mem_write(API + slot, pack(APICODE + slot)); rig.u.mem_write(APICODE + slot, bytes([0xc3]))
    rig.u.hook_add(uc.UC_HOOK_CODE, run, begin=APICODE + slot, end=APICODE + slot)
api_stub(0x290, 1, 0x2001 | 0x40); api_stub(0x294, 2, 0)
saved_api = read(rig.u, SHELLBASE + 0x1c8dfc); rig.u.mem_write(SHELLBASE + 0x1c8dfc, pack(API))
REF, MANAGER = 0x5b9000, SHELLBASE + 0x19e6c0
rig.u.mem_write(REF, pack(0x1234))
rig.invoke_address(rig.dllbase + syms['_kpc_window_break'], (REF, 0x4b, 0x777), this=MANAGER)
rig.u.mem_write(REF, pack(0))
rig.invoke_address(rig.dllbase + syms['_kpc_window_break'], (REF, 0x4b, 0x777), this=MANAGER)
assert handled == [(MANAGER, REF, 0x4b, 0x777)] * 2, handled
assert flag_calls == [(0x290, 0x1234), (0x294, 0x1234, 0x2040)], flag_calls   # only the present window, visible bit cleared
rig.u.mem_write(SHELLBASE + 0x1c8dfc, pack(saved_api))
assert b'Window break message 1: window object present' in bytes(rig.logs)
assert b'Window break message 2: window object MISSING' in bytes(rig.logs)
rigw = Rig(settings={'Multiplayer.LogWindowBreaks': 0}); rigw.factory(); assert not rigw.messages
assert read(rigw.u, SHELLBASE + 0xfe184) == rigw.dllbase + syms['_kpc_window_break']   # still hides
rigw = Rig(settings={'Multiplayer.LogWindowBreaks': 0, 'Multiplayer.HideBrokenWindow': 0}); rigw.factory(); assert not rigw.messages
assert read(rigw.u, SHELLBASE + 0xfe184) == SHELLBASE + 0x5bd30
# Single-player chat (SinglePlayerChat): the Message action's multiplayer test
# (0xca8e8 -> 0xcab80) also passes in a single-player world; the done callback
# (stored at 0xca9c0, 0xce110) runs "!" lines as local console cheats; cheat
# keys add a chat line in single player.
rigs = Rig(); rigs.factory(); assert not rigs.messages, rigs.messages
us, ds = rigs.u, rigs.dllbase
code = bytes(us.mem_read(SHELLBASE + 0xca8e8, 5))
assert code[0] == 0xe8 and (SHELLBASE + 0xca8e8 + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == ds + syms['_kpc_chat_gate']
assert read(us, SHELLBASE + 0xca9c6) == ds + syms['_kpc_chat_done']
mp = [0]
def mp_test(uu, at, size, user):
    sp = uu.reg_read(x.UC_X86_REG_ESP); uu.reg_write(x.UC_X86_REG_EAX, mp[0]); uu.reg_write(x.UC_X86_REG_ESP, sp + 4); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
us.hook_add(uc.UC_HOOK_CODE, mp_test, begin=SHELLBASE + 0xcab80, end=SHELLBASE + 0xcab80)
SH = 0x5c8000
us.mem_write(SH + 0x1398, pack(1))
assert rigs.invoke_address(ds + syms['_kpc_chat_gate'], (), this=SH) & 0xff == 1          # single player, in a world
us.mem_write(SH + 0x1398, pack(0))
assert rigs.invoke_address(ds + syms['_kpc_chat_gate'], (), this=SH) & 0xff == 0          # menus / loading: no
mp[0] = 1; assert rigs.invoke_address(ds + syms['_kpc_chat_gate'], (), this=SH) & 0xff == 1; mp[0] = 0
events = []
def done_hook(uu, at, size, user):
    sp = uu.reg_read(x.UC_X86_REG_ESP); events.append(('done', rigs.cstr(SH + 0x204)))
    uu.reg_write(x.UC_X86_REG_ESP, sp + 4); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
us.hook_add(uc.UC_HOOK_CODE, done_hook, begin=SHELLBASE + 0xce110, end=SHELLBASE + 0xce110)
def add_hook2(uu, at, size, user):
    sp = uu.reg_read(x.UC_X86_REG_ESP); events.append(('say', rigs.cstr(read(uu, sp + 4)), read(uu, sp + 12)))
    uu.reg_write(x.UC_X86_REG_ESP, sp + 20); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
us.hook_add(uc.UC_HOOK_CODE, add_hook2, begin=SHELLBASE + 0xd56d0, end=SHELLBASE + 0xd56d0)
CONS, CONS_VT, RUNSTR = 0x5c9000, 0x5c9100, 0x62000700
us.mem_write(RUNSTR, b'\xc2\x04\x00'); us.mem_write(CONS, pack(CONS_VT)); us.mem_write(CONS_VT + 0x28, pack(RUNSTR))
def run_hook(uu, at, size, user):
    events.append(('console', rigs.cstr(read(uu, uu.reg_read(x.UC_X86_REG_ESP) + 4))))
us.hook_add(uc.UC_HOOK_CODE, run_hook, begin=RUNSTR, end=RUNSTR)
us.mem_write(SHELLBASE + 0x1c8de0, pack(CONS)); us.mem_write(SHELLBASE + 0x1c8de8, pack(SH))
def typed(text):
    events.clear(); us.mem_write(SH + 0x204, text + b'\0'); rigs.invoke_address(ds + syms['_kpc_chat_done'], ()); return list(events)
assert typed(b'!gimmie') == [('done', ''), ('console', 'Gimme'), ('say', '[Cheat] Gimmie: ammo and the next weapon', 4)]
assert typed(b'!NextArmor') == [('done', ''), ('console', 'NextArmor'), ('say', '[Cheat] Next armour piece', 4)]
ev = typed(b'!campaign earth'); assert ev[0] == ('done', '') and len(ev) == 2 and 'only on a co-op server' in ev[1][1], ev
ev = typed(b'!what'); assert 'Unknown command' in ev[1][1], ev
assert typed(b'hello') == [('done', 'hello')]                                               # ordinary line: sent as before
mp[0] = 1; assert typed(b'!gimmie') == [('done', '!gimmie')]; mp[0] = 0                    # multiplayer: to the server
cmds = []
def command_on2(uu, at, size, user):
    sp = uu.reg_read(x.UC_X86_REG_ESP); cmds.append(read(uu, sp + 4))
    uu.reg_write(x.UC_X86_REG_ESP, sp + 8); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
us.hook_add(uc.UC_HOOK_CODE, command_on2, begin=SHELLBASE + 0xc9d80, end=SHELLBASE + 0xc9d80)
us.mem_write(SH + 0x8c + 0x12ed, b'\x00')
events.clear(); rigs.invoke_address(ds + syms['_kpc_command_on'], (98,), this=SH)
assert cmds == [98] and events == [('say', '[Cheat] Gimmie', 4)], (cmds, events)
mp[0] = 1; events.clear(); rigs.invoke_address(ds + syms['_kpc_command_on'], (98,), this=SH); assert events == []; mp[0] = 0
events.clear(); rigs.invoke_address(ds + syms['_kpc_command_on'], (41,), this=SH); assert events == []
# Main-menu Quit while connected to a remote server disconnects (MenuDisconnect):
# the call 0x21dfb -> quit dialog 0x2b800 goes through kpc_main_quit.
code = bytes(us.mem_read(SHELLBASE + 0x21dfb, 5))
assert code[0] == 0xe8 and (SHELLBASE + 0x21dfb + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == ds + syms['_kpc_main_quit']
quits = []
def quit_dialog(uu, at, size, user):
    sp = uu.reg_read(x.UC_X86_REG_ESP); quits.append(('dialog', uu.reg_read(x.UC_X86_REG_ECX), read(uu, sp + 4)))
    uu.reg_write(x.UC_X86_REG_ESP, sp + 8); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
us.hook_add(uc.UC_HOOK_CODE, quit_dialog, begin=SHELLBASE + 0x2b800, end=SHELLBASE + 0x2b800)
CAPI, DISC = 0x5ca000, 0x62000740
us.mem_write(DISC, b'\xc3'); us.mem_write(CAPI + 0x14, pack(DISC)); us.mem_write(SHELLBASE + 0x1c8dfc, pack(CAPI))
us.hook_add(uc.UC_HOOK_CODE, lambda uu, at, size, user: quits.append(('disconnect',)), begin=DISC, end=DISC)
CMQ, SESSQ = 0x5cb000, 0x5cb800
us.mem_write(CLIENT + 0xaacd0, pack(0))
rigs.invoke_address(ds + syms['_kpc_main_quit'], (0,), this=0x777); assert quits == [('dialog', 0x777, 0)], quits   # not connected
quits.clear(); us.mem_write(CLIENT + 0xaacd0, pack(CMQ)); us.mem_write(CMQ + 0x1004, pack(SESSQ)); us.mem_write(SESSQ + 0xa8, pack(2)); us.mem_write(SESSQ + 0x50, pack(0))
rigs.invoke_address(ds + syms['_kpc_main_quit'], (0,), this=0x777); assert quits == [('disconnect',)], quits        # remote server
assert b'Main menu: Quit while connected - disconnecting' in bytes(rigs.logs)
quits.clear(); us.mem_write(SESSQ + 0x50, pack(1))
rigs.invoke_address(ds + syms['_kpc_main_quit'], (0,), this=0x777); assert quits == [('dialog', 0x777, 0)], quits   # local game
us.mem_write(CLIENT + 0xaacd0, pack(0))
rigs0 = Rig(settings={'Multiplayer.SinglePlayerChat': 0}); rigs0.factory(); assert not rigs0.messages
assert bytes(rigs0.u.mem_read(SHELLBASE + 0xca8e8, 5)) == bytes(rigs0.initial_shell[0xca8e8:0xca8ed])
assert read(rigs0.u, SHELLBASE + 0xca9c6) == SHELLBASE + 0xce110
rigq = Rig(settings={'Multiplayer.MenuDisconnect': 0}); rigq.factory(); assert not rigq.messages
assert bytes(rigq.u.mem_read(SHELLBASE + 0x21dfb, 5)) == bytes(rigq.initial_shell[0x21dfb:0x21e00])
print(args.fixtures.name, 'passed console key: toggle character dropped within 250 ms, later characters kept, chat typing drops actions and clears the command state, scoreboard shown while held and closed on release, chat Ctrl+A/C/X/V, chat scrollback, long chat lines, highlight, bar placement, spawn lightning off, chat history, hidden while loading, label hint, chat in cinematics, window-break log, broken window hidden at once, single-player chat and cheat lines, menu disconnect, disabled')
