"""Smooth scripted cameras (SmoothCutsceneCamera): the camera sample store call
(cshell 0xab104 -> 0xa8f30) and the per-frame client update tick interpolate
between server samples; first samples, cuts and samples after a pause are
shown as they are; nothing is written once samples stop. The original store
is a recording stub; the camera object is a scratch buffer."""
import json
import struct
import sys
import test_runtime

sys.argv += ['--expose-rig']
Rig, _, _, uc, x, args = test_runtime.main()
syms = json.loads(args.symbols.read_text())
pack = lambda *v: struct.pack('<' + 'I' * len(v), *[i & 0xffffffff for i in v])
read = lambda u, a: struct.unpack('<I', u.mem_read(a, 4))[0]
fl = lambda *v: struct.pack('<' + 'f' * len(v), *v)
SHELLBASE, CAM, POS, ROT = 0x20000000, 0x5a0000, 0x5a1000, 0x5a1100

rig = Rig(); rig.factory(); assert not rig.messages, rig.messages
u, dll = rig.u, rig.dllbase
code = bytes(u.mem_read(SHELLBASE + 0xab104, 5))
assert code[0] == 0xe8 and (SHELLBASE + 0xab104 + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == dll + syms['_kpc_camera_set']
code = bytes(u.mem_read(0x1ca0000 + 0x13876, 5))
assert code[0] == 0xe8 and (0x1ca0000 + 0x13876 + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == dll + syms['_kpc_client_frame']
assert b'Cutscene camera smoothing: server camera samples interpolated every frame.' in bytes(rig.logs)
stored = []
def store(uu, at, size, user):
    sp = uu.reg_read(x.UC_X86_REG_ESP); cam, p, r = uu.reg_read(x.UC_X86_REG_ECX), read(uu, sp + 4), read(uu, sp + 8)
    stored.append(struct.unpack('<3f', uu.mem_read(p, 12))); uu.mem_write(cam + 0x10, bytes(uu.mem_read(p, 12))); uu.mem_write(cam + 0x1c, bytes(uu.mem_read(r, 16)))
    uu.reg_write(x.UC_X86_REG_ESP, sp + 12); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
u.hook_add(uc.UC_HOOK_CODE, store, begin=SHELLBASE + 0xa8f30, end=SHELLBASE + 0xa8f30)
u.mem_write(SHELLBASE + 0x1c49dc, pack(CAM))
setcam, tick = dll + syms['_kpc_camera_set'], dll + syms['_kpc_camera_tick']
def sample(t, px, rot=(0, 0, 0, 1)):
    rig.tick = t - 5; u.mem_write(POS, fl(px, 0, 0)); u.mem_write(ROT, fl(*rot))
    rig.invoke_address(setcam, (POS, ROT), this=CAM)
def frame(t):
    rig.tick = t - 5; rig.invoke_address(tick, ())
    return struct.unpack('<3f', u.mem_read(CAM + 0x10, 12))[0], struct.unpack('<4f', u.mem_read(CAM + 0x1c, 16))
sample(1000, 0.0); assert stored == [(0.0, 0.0, 0.0)], stored              # first: shown as it is
sample(1016, 16.0); assert len(stored) == 1                                 # next: becomes the target
px, rot = frame(1024); assert abs(px - 8.0) < 1e-3, px                      # half the 16 ms interval
px, rot = frame(1040); assert abs(px - 16.0) < 1e-3, px                     # reached, held
sample(1056, 32.0, (0, 0.7071068, 0, 0.7071068))
px, rot = frame(1064); assert abs(px - 19.2) < 1e-3, px                      # 8 ms of the measured 40 ms gap
assert abs(sum(c * c for c in rot) - 1.0) < 1e-4 and 0 < rot[1] < 0.7071068, rot    # normalised, part way
sample(1072, 2000.0); assert stored[-1] == (2000.0, 0.0, 0.0), stored         # a cut: snapped at once
u.mem_write(CAM + 0x10, fl(-1, -1, -1))
frame(1300); assert struct.unpack('<3f', u.mem_read(CAM + 0x10, 12)) == (-1.0, -1.0, -1.0)   # stale: stock owns it
n = len(stored); sample(1400, 5.0); assert len(stored) == n + 1             # after a pause: shown as it is
# Disabled: the store call keeps its original bytes.
rig0 = Rig(settings={'Multiplayer.SmoothCutsceneCamera': 0}); rig0.factory(); assert not rig0.messages
assert bytes(rig0.u.mem_read(SHELLBASE + 0xab104, 5)) == bytes(rig0.initial_shell[0xab104:0xab109])
print(args.fixtures.name, 'passed cutscene camera smoothing: interpolation, normalised rotation, cuts, stale samples, disabled')
