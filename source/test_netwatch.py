"""Server-connection timeout: run the original CNetMgr::Update comparison at
client.exe 0x4b4fc through the KPCFix stub and record whether control reaches
the native disconnect path (0x4b50c) or skips it (0x4b53f)."""
import struct
import sys
import test_runtime

sys.argv += ['--expose-rig']
Rig, _, _, uc, x, args = test_runtime.main()
pack = lambda *v: struct.pack('<' + 'I' * len(v), *[i & 0xffffffff for i in v])
CLIENT, SITE, DISCONNECT, SKIP = 0x1ca0000, 0x1ca0000 + 0x4b4fc, 0x1ca0000 + 0x4b50c, 0x1ca0000 + 0x4b53f
CONN, FRAME, DRIVER, SHELL, NAME = 0x5b0000, 0x5b1000, 0x5b2000, 0x5b3000, 0x5b4000
STACK = 0x5b8000
CLIENT_NAME = 0x1d4053c   # "Client: "


def decide(rig, idle, state=1, name=CLIENT_NAME, timeout=120.0, flags=0, shell=True):
    u = rig.u
    u.mem_write(CONN + 0x30, pack(DRIVER)); u.mem_write(CONN + 0x74, struct.pack('<f', idle))
    u.mem_write(CONN + 0x60, bytes([flags])); u.mem_write(DRIVER + 0x4c, struct.pack('<f', timeout))
    u.mem_write(FRAME + 8, pack(name))
    u.mem_write(rig.shellbase + 0x1c8de8, pack(SHELL if shell else 0)); u.mem_write(SHELL + 0x1398, pack(state))
    landed = []
    def stop(u, at, size, user):
        landed.append(at); u.emu_stop()
    h1 = u.hook_add(uc.UC_HOOK_CODE, stop, begin=DISCONNECT, end=DISCONNECT)
    h2 = u.hook_add(uc.UC_HOOK_CODE, stop, begin=SKIP, end=SKIP)
    for reg, v in ((x.UC_X86_REG_ESI, CONN), (x.UC_X86_REG_EBP, FRAME), (x.UC_X86_REG_ESP, STACK),
                   (x.UC_X86_REG_EBX, 0x12345678), (x.UC_X86_REG_EDI, 0x3456789a)):
        u.reg_write(reg, v)
    u.reg_write(x.UC_X86_REG_FPCW, 0x037f); u.reg_write(x.UC_X86_REG_FPTAG, 0xffff)
    u.emu_start(SITE, 0, count=500)
    u.hook_del(h1); u.hook_del(h2)
    assert len(landed) == 1, landed
    assert u.reg_read(x.UC_X86_REG_ESP) == STACK and u.reg_read(x.UC_X86_REG_ESI) == CONN
    assert u.reg_read(x.UC_X86_REG_EBX) == 0x12345678 and u.reg_read(x.UC_X86_REG_EDI) == 0x3456789a
    assert u.reg_read(x.UC_X86_REG_ECX) == DRIVER          # as the original mov ecx,[esi+0x30]
    assert u.reg_read(x.UC_X86_REG_FPSW) >> 11 & 7 == 0     # x87 stack balanced (fld + fcomp)
    return landed[0] == DISCONNECT


# Default 15 s, after the shell is loaded.
rig = Rig(); rig.factory(); assert not rig.messages, rig.messages
code = bytes(rig.u.mem_read(SITE, 16))
assert code[0] == 0xe8 and code[5:14] == b'\x90' * 9 and code[14:16] == b'\x75\x33', code.hex()
assert b'Server timeout: in-world multiplayer connection seconds=0x0000000f' in bytes(rig.logs)
assert not decide(rig, 10.0)                       # quiet for 10 s: still connected
assert decide(rig, 15.2)                           # 15 s in a multiplayer world: native disconnect
assert not decide(rig, 15.2, state=5)              # changing levels: stock 120 s
assert not decide(rig, 15.2, state=7)              # scoreboard: stock
assert not decide(rig, 15.2, shell=False)          # no shell yet (connecting)
assert not decide(rig, 15.2, name=CLIENT_NAME + 1) # another manager (e.g. a local server)
assert not decide(rig, 15.2, timeout=1e6)          # local single-player driver
assert not decide(rig, 15.2, flags=1)              # local connection flag
assert decide(rig, 120.5, state=5) and not decide(rig, 119.0, state=5)   # stock threshold kept
# Disabled: the original bytes stay in place and the stock comparison runs.
rig0 = Rig(settings={'Multiplayer.ServerTimeoutSeconds': 0}); rig0.factory(); assert not rig0.messages
assert bytes(rig0.u.mem_read(SITE, 16)) == bytes(rig0.initial_client[0x4b4fc:0x4b50c])
assert not decide(rig0, 30.0) and decide(rig0, 121.0)
# Configured 30 s; invalid values are refused before any edit.
rig30 = Rig(settings={'Multiplayer.ServerTimeoutSeconds': 30}); rig30.factory()
assert not decide(rig30, 20.0) and decide(rig30, 31.0)
for bad in (3, 121):
    rigb = Rig(settings={'Multiplayer.ServerTimeoutSeconds': bad}); rigb.factory()
    assert any('ServerTimeoutSeconds' in m for m in rigb.messages), rigb.messages
    assert bytes(rigb.u.mem_read(SITE, 14)) == bytes(rigb.initial_client[0x4b4fc:0x4b50a])
print(args.fixtures.name, 'passed server timeout: 15 s in world, stock 120 s while loading/scoreboard/connecting, local and other managers untouched, disabled, configured, limits')
