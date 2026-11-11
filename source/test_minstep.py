"""Optional minimum local-server step (LocalServerMinStepMs): run the patched
client.exe 0x664b2 slot with a synthetic server manager and record whether
the frame is simulated (0x664d5, elapsed in st(0)) or skipped (0x665b2, with
its elapsed time carried into [esi+0x218]). Default 0 keeps clock_v2."""
import struct
import sys
import test_runtime

sys.argv += ['--expose-rig']
Rig, _, _, uc, x, args = test_runtime.main()
pack = lambda *v: struct.pack('<' + 'I' * len(v), *[i & 0xffffffff for i in v])
CLIENT = 0x1ca0000
SLOT, SIMULATE, SKIP = CLIENT + 0x664b2, CLIENT + 0x664d5, CLIENT + 0x665b2
MGR, STACK = 0x5d0000, 0x5d8000
f32 = lambda v: struct.pack('<f', v)
g32 = lambda u, a: struct.unpack('<f', u.mem_read(a, 4))[0]


def st0(u):
    top = u.reg_read(x.UC_X86_REG_FPSW) >> 11 & 7          # ST(0) is the physical register FP[TOP]
    mantissa, exponent = u.reg_read(getattr(x, 'UC_X86_REG_FP%d' % top))
    sign = -1 if exponent & 0x8000 else 1
    return sign * mantissa * 2.0 ** ((exponent & 0x7fff) - 16383 - 63) if mantissa else 0.0


def frame(rig, elapsed, last=100.0):
    u = rig.u
    u.mem_write(MGR + 0x214, f32(elapsed)); u.mem_write(MGR + 0x218, f32(last)); u.mem_write(MGR + 0x1f0, f32(0.5))
    u.mem_write(STACK, pack(0xb0b0b0b0, 0xe0e0e0e0))     # pushed ebp, ebx (0x6646f/0x66470)
    landed = []
    def stop(u, at, size, user):
        landed.append(at); u.emu_stop()
    hooks = [u.hook_add(uc.UC_HOOK_CODE, stop, begin=a, end=a) for a in (SIMULATE, SKIP)]
    u.reg_write(x.UC_X86_REG_ESI, MGR); u.reg_write(x.UC_X86_REG_ESP, STACK)
    u.reg_write(x.UC_X86_REG_EBX, 0x11); u.reg_write(x.UC_X86_REG_EBP, 0x22)
    u.reg_write(x.UC_X86_REG_FPCW, 0x037f); u.reg_write(x.UC_X86_REG_FPTAG, 0xffff); u.reg_write(x.UC_X86_REG_FPSW, 0)
    u.emu_start(SLOT, 0, count=200)
    for h in hooks:
        u.hook_del(h)
    assert len(landed) == 1, landed
    top = u.reg_read(x.UC_X86_REG_FPSW) >> 11 & 7
    if landed[0] == SIMULATE:
        assert top == 7 and abs(st0(u) - elapsed) < 1e-6          # elapsed left in st(0) for the stock clamp
        assert u.reg_read(x.UC_X86_REG_ESP) == STACK
        assert abs(g32(u, MGR + 0x218) - last) < 1e-6
        return 'simulate', None
    assert top == 0                                               # x87 stack balanced
    assert u.reg_read(x.UC_X86_REG_ESP) == STACK + 8                # ebp, ebx popped like clock_v2
    assert u.reg_read(x.UC_X86_REG_EBP) == 0xb0b0b0b0 and u.reg_read(x.UC_X86_REG_EBX) == 0xe0e0e0e0
    assert g32(u, MGR + 0x1f0) == 0.0
    return 'skip', g32(u, MGR + 0x218)


rig = Rig(settings={'LocalServerMinStepMs': 10}); rig.factory(); assert not rig.messages, rig.messages
assert bytes(rig.u.mem_read(SLOT, 1)) == b'\xe9' and bytes(rig.u.mem_read(SLOT + 5, 30)) == b'\x90' * 30
assert b'LocalServerMinStepMs=0x0000000a: EXPERIMENTAL minimum local-server step' in bytes(rig.logs)
assert frame(rig, 0.012) == ('simulate', None)
assert frame(rig, 0.010)[0] == 'simulate'                         # exactly the minimum runs
kind, last = frame(rig, 0.006, last=100.0)
assert kind == 'skip' and abs(last - (100.0 - 0.006)) < 1e-5        # carried: next elapsed = now - last
# Two 6 ms frames: the second sees 12 ms (now advanced 6 ms from the carried last).
now = 100.006
kind, _ = frame(rig, now - last, last=last); assert kind == 'simulate', kind
assert frame(rig, 0.0) == ('skip', 100.0)                         # nothing to carry
assert frame(rig, -0.001) == ('skip', 100.0)
assert frame(rig, 0.25)[0] == 'simulate'                          # the stock 0.2 s maximum still applies after
# Default: Timing V3 clock_v2 unchanged.
rig0 = Rig(); rig0.factory(); assert not rig0.messages
assert bytes(rig0.u.mem_read(SLOT, 8)) == bytes([0xd9, 0x86, 0x14, 0x02, 0x00, 0x00, 0xd9, 0xe4])
assert b'LocalServerMinStepMs' not in bytes(rig0.logs)
rigb = Rig(settings={'LocalServerMinStepMs': 51}); rigb.factory()
assert any('LocalServerMinStepMs' in m for m in rigb.messages), rigb.messages
print(args.fixtures.name, 'passed minimum local-server step: simulate at/above minimum, skip and carry below, no carry for <=0, default clock_v2, limits')
