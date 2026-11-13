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
rigc = Rig(settings={'Multiplayer.ChatBlocksActions': 0}); rigc.factory(); assert not rigc.messages
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
print(args.fixtures.name, 'passed console key: toggle character dropped within 250 ms, later characters kept, chat typing drops actions and clears the command state, window-break log, broken window hidden at once, disabled')
