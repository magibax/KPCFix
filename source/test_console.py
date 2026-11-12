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


rig = Rig(); rig.factory(); assert not rig.messages, rig.messages
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
rigc = Rig(settings={'Multiplayer.ChatBlocksActions': 0}); rigc.factory(); assert not rigc.messages
assert read(rigc.u, SHELLBASE + 0xff724) == SHELLBASE + 0xc9d80
# Disabled: both sites keep their original bytes.
rig0 = Rig(settings={'ConsoleKeyFix': 0}); rig0.factory(); assert not rig0.messages
assert bytes(rig0.u.mem_read(CLIENT + 0x6dc9, 5)) == bytes(rig0.initial_client[0x6dc9:0x6dce])
assert bytes(rig0.u.mem_read(CLIENT + 0x700a, 5)) == bytes.fromhex('83f8607445')
print(args.fixtures.name, 'passed console key: toggle character dropped within 250 ms, later characters kept, chat typing drops actions, disabled')
