"""Focus loss: the WM_ACTIVATEAPP sound pause/resume calls (client.exe 0x6d5c,
0x6cdb) are skipped as a balanced pair for a remote multiplayer client and
forwarded unchanged for single player, a hosting client and menus. The
original pause/resume routines are recording stubs."""
import json
import struct
import sys
import test_runtime

sys.argv += ['--expose-rig']
Rig, _, _, uc, x, args = test_runtime.main()
syms = json.loads(args.symbols.read_text())
pack = lambda *v: struct.pack('<' + 'I' * len(v), *[i & 0xffffffff for i in v])
read = lambda u, a: struct.unpack('<I', u.mem_read(a, 4))[0]
CLIENT, CM, SESSION, SOUND = 0x1ca0000, 0x5c0000, 0x5c1000, 0x5c2000


def stubs(rig):
    calls = []
    def stub(name):
        def run(u, at, size, user):
            calls.append((name, u.reg_read(x.UC_X86_REG_ECX)))
            sp = u.reg_read(x.UC_X86_REG_ESP)
            u.reg_write(x.UC_X86_REG_EAX, 1); u.reg_write(x.UC_X86_REG_ESP, sp + 4); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
        return run
    rig.u.hook_add(uc.UC_HOOK_CODE, stub('pause'), begin=CLIENT + 0x74b90, end=CLIENT + 0x74b90)
    rig.u.hook_add(uc.UC_HOOK_CODE, stub('resume'), begin=CLIENT + 0x74c10, end=CLIENT + 0x74c10)
    return calls


def session(rig, mode, local_server=0, present=True):
    rig.u.mem_write(CLIENT + 0xaacd0, pack(CM if present else 0))
    rig.u.mem_write(CM + 0x1004, pack(SESSION)); rig.u.mem_write(SESSION + 0xa8, pack(mode))
    rig.u.mem_write(SESSION + 0x50, pack(local_server))


def focus(rig, calls, lose):
    calls.clear()
    fn = rig.dllbase + syms['_kpc_focus_release' if lose else '_kpc_focus_restore']
    rig.invoke_address(fn, (), this=SOUND)
    return list(calls)


rig = Rig(); rig.factory(); assert not rig.messages, rig.messages
for site, name in ((0x6d5c, '_kpc_focus_release'), (0x6cdb, '_kpc_focus_restore')):
    code = bytes(rig.u.mem_read(CLIENT + site, 5))
    assert code[0] == 0xe8 and (CLIENT + site + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == rig.dllbase + syms[name]
assert b'Focus loss: remote multiplayer keeps server-owned sounds running' in bytes(rig.logs)
calls = stubs(rig)
# Remote multiplayer (mode 2, no local server): neither pause nor resume.
session(rig, 2)
assert focus(rig, calls, True) == [] and focus(rig, calls, False) == []
# Single player / hosting (local server present): stock pause and resume.
session(rig, 2, local_server=0x1234)
assert focus(rig, calls, True) == [('pause', SOUND)] and focus(rig, calls, False) == [('resume', SOUND)]
# Mode 4 (not multiplayer) and no game (menu, mode 5 via no session): stock.
session(rig, 4)
assert focus(rig, calls, True) == [('pause', SOUND)] and focus(rig, calls, False) == [('resume', SOUND)]
session(rig, 2, present=False)
assert focus(rig, calls, True) == [('pause', SOUND)] and focus(rig, calls, False) == [('resume', SOUND)]
# Balanced pair: lost while in multiplayer, restored after leaving it -> no stray resume;
# lost in single player, restored in multiplayer -> the matching resume still runs.
session(rig, 3); assert focus(rig, calls, True) == []
session(rig, 4); assert focus(rig, calls, False) == []
session(rig, 4); assert focus(rig, calls, True) == [('pause', SOUND)]
session(rig, 2); assert focus(rig, calls, False) == [('resume', SOUND)]
# Disabled: original calls stay in place.
rig0 = Rig(settings={'Multiplayer.KeepAudioOnFocusLoss': 0}); rig0.factory(); assert not rig0.messages
for site in (0x6d5c, 0x6cdb):
    assert bytes(rig0.u.mem_read(CLIENT + site, 5)) == bytes(rig0.initial_client[site:site + 5])
print(args.fixtures.name, 'passed focus loss: remote multiplayer skip, single player/hosting/menus stock, balanced pair, disabled')
