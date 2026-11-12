"""Campaign loading art: PreLoadWorld slot, deferred 0x69 close and per-frame
screen draw against the original client/shell images. The shell routines the
hooks call (Show, Close, Draw, loader, renderer, multiplayer test, the
original PreLoadWorld and ILTClient DeleteSurface) are replaced by recording
stubs; their ABIs were taken from the original code (see src/loading.c)."""
import json
import struct
import sys
import test_runtime

sys.argv += ['--expose-rig']
Rig, _, _, uc, x, args = test_runtime.main()
syms = json.loads(args.symbols.read_text())
pack = lambda *v: struct.pack('<' + 'I' * len(v), *[i & 0xffffffff for i in v])
read = lambda u, a: struct.unpack('<I', u.mem_read(a, 4))[0]
SHELL, SCREENS, SCREEN, RENDER, API, FREE, TEXT = 0x5a0000, 0x5a2000, 0x5a3000, 0x5a4000, 0x5a5000, 0x5a6000, 0x5a7000


def install_stubs(rig, multiplayer):
    u, base, log = rig.u, rig.shellbase, []
    def stub(address, pop, action, nargs=None):
        def run(u, at, size, user):
            sp = u.reg_read(x.UC_X86_REG_ESP)
            n = pop // 4 if nargs is None else nargs
            a = struct.unpack('<' + 'I' * n, u.mem_read(sp + 4, 4 * n)) if n else ()
            result = action(u.reg_read(x.UC_X86_REG_ECX), a)
            if result is not None:
                u.reg_write(x.UC_X86_REG_EAX, result)
            u.reg_write(x.UC_X86_REG_ESP, sp + 4 + pop)
            u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
        u.hook_add(uc.UC_HOOK_CODE, run, begin=address, end=address)
    def show(this, a):
        log.append(('show', this, a[0])); u.mem_write(SCREENS, pack(SCREEN, a[0]))
    def close(this, a):
        log.append(('close', this)); u.mem_write(SCREENS, pack(0, 0xffffffff))
    def load(this, a):
        name = rig.cstr(a[0]); log.append(('load', name)); return 0x7777
    stub(base + 0xad1b0, 4, show)
    stub(base + 0xad3c0, 0, close)
    stub(base + 0xad380, 4, lambda this, a: log.append(('draw', this, a[0])))
    stub(base + 0xad7b0, 0, load, nargs=1)              # cdecl: caller pops
    stub(base + 0xcd990, 0, lambda this, a: log.append(('frame', this)))
    stub(base + 0xcab80, 0, lambda this, a: 1 if multiplayer else 0)
    stub(base + 0xcd360, 4, lambda this, a: log.append(('preload', this, rig.cstr(a[0]))))
    stub(FREE, 0, lambda this, a: log.append(('free', read(u, u.reg_read(x.UC_X86_REG_ESP) + 4))))
    u.mem_write(base + 0x1c8dfc, pack(API)); u.mem_write(API + 0x158, pack(FREE)); u.mem_write(FREE, b'\xc3')
    return log


def world(rig, text):
    rig.u.mem_write(TEXT, text.encode() + b'\0')
    return TEXT


def setup_shell(u, screen_type=1, state=5, frames=3, surface=0x1111):
    u.mem_write(SHELL + 0x138c, pack(SCREENS)); u.mem_write(SHELL + 0x1398, pack(state))
    u.mem_write(SHELL + 0x1388, pack(RENDER)); u.mem_write(RENDER + 0x21c, pack(frames))
    u.mem_write(SCREENS, pack(SCREEN, screen_type)); u.mem_write(SCREEN + 0xc, pack(surface))


base = 0x20000000
# Disabled: every site keeps its original bytes.
rig = Rig(settings={'Multiplayer.CampaignLoadingArt': 0}); rig.factory(); assert not rig.messages, rig.messages
assert read(rig.u, base + 0xff73c) == base + 0xcd360
assert bytes(rig.u.mem_read(base + 0xcbba6, 5)) == bytes(rig.initial_shell[0xcbba6:0xcbbab])
assert bytes(rig.u.mem_read(base + 0xcdb92, 5)) == bytes(rig.initial_shell[0xcdb92:0xcdb97])
assert b'Campaign loading art' not in bytes(rig.logs)

rig = Rig(); rig.factory(); assert not rig.messages, rig.messages
u = rig.u; dll = rig.dllbase
preload, close_hook, draw_hook = dll + syms['_kpc_preload_world'], dll + syms['_kpc_close_on_entry'], dll + syms['_kpc_draw_screen']
assert read(u, base + 0xff73c) == preload
for site, target in ((0xcbba6, close_hook), (0xcdb92, draw_hook)):
    code = bytes(u.mem_read(base + site, 5))
    assert code[0] == 0xe8 and (base + site + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == target
assert b'Campaign loading art: original R?M? pictures in multiplayer campaign loads, minimum ms=0x00001388' in bytes(rig.logs)
log = install_stubs(rig, multiplayer=True)

# 1. Campaign world in multiplayer: loading screen type 0, the world's own
#    picture, the old surface freed, frames rendered, original called last.
setup_shell(u)
rig.tick = 100000
rig.invoke_address(preload, (world(rig, 'Worlds\\Realm1\\R1M1A.dat'),), this=SHELL)
assert log == [('show', SCREENS, 0), ('load', 'Worlds\\Realm1\\R1M1A.dat'), ('free', 0x1111),
               ('frame', SHELL), ('frame', SHELL), ('frame', SHELL),
               ('preload', SHELL, 'Worlds\\Realm1\\R1M1A.dat')], log
assert read(u, SCREEN + 0xc) == 0x7777
# 2. 0x69 on entry 2 s later: the close is deferred; drawing continues.
log.clear(); rig.tick = 102000
rig.invoke_address(close_hook, (), this=SCREENS); assert log == [], log
rig.invoke_address(draw_hook, (0x55,), this=SCREENS); assert log == [('draw', SCREENS, 0x55)], log
# 3. At 5 s the next frame closes it instead of drawing.
log.clear(); rig.tick = 100000 + 4998
rig.invoke_address(draw_hook, (0x55,), this=SCREENS); assert log == [('draw', SCREENS, 0x55)], log
log.clear(); rig.tick = 100000 + 5000
rig.invoke_address(draw_hook, (0x55,), this=SCREENS); assert log == [('close', SCREENS)], log
log.clear(); rig.invoke_address(draw_hook, (0x56,), this=SCREENS); assert log == [('draw', SCREENS, 0x56)], log

# 4. A slow load (8 s): entry closes at once; nothing is added.
setup_shell(u, screen_type=0); log.clear(); rig.tick = 200000
rig.invoke_address(preload, (world(rig, 'worlds/realm2/r2m2a'),), this=SHELL)
assert log[0] == ('load', 'worlds/realm2/r2m2a') and log[-1][0] == 'preload' and ('show', SCREENS, 0) not in log
log.clear(); rig.tick = 208000
rig.invoke_address(close_hook, (), this=SCREENS); assert log == [('close', SCREENS)], log

# 5. Another screen replaced the picture (e.g. a scoreboard): no stale close.
setup_shell(u); log.clear(); rig.tick = 300000
rig.invoke_address(preload, (world(rig, 'worlds\\realm4\\R4M0'),), this=SHELL)
rig.tick = 301000; rig.invoke_address(close_hook, (), this=SCREENS)
u.mem_write(SCREENS + 4, pack(2)); log.clear(); rig.tick = 306000
rig.invoke_address(draw_hook, (1,), this=SCREENS); assert log == [('draw', SCREENS, 1)], log

# 6. Deathmatch world and an unrelated name: stock behaviour only.
for name in ('worlds\\deathmatch\\PM01', 'worlds\\realm0\\tutorial', 'r1x0'):
    setup_shell(u); log.clear()
    rig.invoke_address(preload, (world(rig, name),), this=SHELL)
    assert log == [('preload', SHELL, name)], (name, log)
    log.clear(); rig.invoke_address(close_hook, (), this=SCREENS); assert log == [('close', SCREENS)], log

# 7. Not in a world change (state 1): picture shown, no forced frames.
setup_shell(u, state=1); log.clear()
rig.invoke_address(preload, (world(rig, 'worlds/realm5/r5m1b'),), this=SHELL)
assert not any(e[0] == 'frame' for e in log) and log[-1][0] == 'preload', log

# 8. Single player (multiplayer test 0): stock behaviour only.
rig2 = Rig(); rig2.factory(); log2 = install_stubs(rig2, multiplayer=False); u2 = rig2.u
u2.mem_write(SHELL + 0x138c, pack(SCREENS)); u2.mem_write(SCREENS, pack(SCREEN, 1))
rig2.u.mem_write(TEXT, b'worlds\\realm1\\r1m0\0')
rig2.invoke_address(rig2.dllbase + syms['_kpc_preload_world'], (TEXT,), this=SHELL)
assert log2 == [('preload', SHELL, 'worlds\\realm1\\r1m0')], log2

# 9. Configured minimum 0: never deferred.
rig3 = Rig(settings={'Multiplayer.CampaignLoadingMinimumMs': 0}); rig3.factory(); log3 = install_stubs(rig3, True)
rig = rig3; u = rig3.u; setup_shell(u)
rig3.invoke_address(rig3.dllbase + syms['_kpc_preload_world'], (world(rig3, 'worlds\\realm1\\R1M0'),), this=SHELL)
log3.clear(); rig3.invoke_address(rig3.dllbase + syms['_kpc_close_on_entry'], (), this=SCREENS)
assert log3 == [('close', SCREENS)], log3

# 11. Spawn hold: the 0x17 handler's reply calls go through the hooks; while
#     the picture is up and under its minimum both replies wait, then are sent
#     in order from the draw hook (minimum reached / replaced / safety cap).
rig = Rig(); rig.factory(); assert not rig.messages, rig.messages
u = rig.u; dll = rig.dllbase; log = install_stubs(rig, multiplayer=True)
for site, name in ((0xbeadc, '_kpc_spawn_reply'), (0xbeae3, '_kpc_spawn_initvars')):
    code = bytes(u.mem_read(base + site, 5))
    assert code[0] == 0xe8 and (base + site + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == dll + syms[name], name
assert b'Loading picture spawn hold: the player enters the world' in bytes(rig.logs)
def reply_stub(address, tag):
    def run(uu, at, size, user):
        log.append((tag, uu.reg_read(x.UC_X86_REG_ECX))); sp = uu.reg_read(x.UC_X86_REG_ESP)
        uu.reg_write(x.UC_X86_REG_ESP, sp + 4); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
    u.hook_add(uc.UC_HOOK_CODE, run, begin=address, end=address)
reply_stub(base + 0xbf300, 'reply'); reply_stub(base + 0xbf400, 'initvars')
P = 0x5a8000
preload, reply, initvars, draw = (dll + syms[n] for n in ('_kpc_preload_world', '_kpc_spawn_reply', '_kpc_spawn_initvars', '_kpc_draw_screen'))
def answer():
    rig.invoke_address(reply, (), this=P); rig.invoke_address(initvars, (), this=P)
# The first load of a connection is never held (menu connect dialog): no 0x69 yet.
setup_shell(u); rig.tick = 390000
rig.invoke_address(preload, (world(rig, 'worlds\\realm1\\R1M2B'),), this=SHELL)
log.clear(); rig.tick = 390100; answer(); assert log == [('reply', P), ('initvars', P)], log
rig.invoke_address(close_hook, (), this=SCREENS)                 # 0x69: this connection is now in a world
setup_shell(u); rig.tick = 400000
rig.invoke_address(preload, (world(rig, 'worlds\\realm1\\R1M1A'),), this=SHELL)
assert b'Loading world: worlds\\realm1\\R1M1A (picture shown)' in bytes(rig.logs)
log.clear(); rig.tick = 401500; answer(); assert log == [], log           # fast load: held
assert b'spawn reply held' in bytes(rig.logs)
rig.tick = 403000; rig.invoke_address(draw, (7,), this=SCREENS); assert log == [('draw', SCREENS, 7)], log
log.clear(); rig.tick = 405000; rig.invoke_address(draw, (7,), this=SCREENS)
assert log == [('reply', P), ('initvars', P), ('draw', SCREENS, 7)], log  # sent at the minimum, in order
assert b'spawn reply sent (minimum reached)' in bytes(rig.logs)
# Slow load (reply after the minimum): sent at once.
setup_shell(u); rig.tick = 500000; rig.invoke_address(preload, (world(rig, 'worlds\\realm1\\R1M1B'),), this=SHELL)
log.clear(); rig.tick = 506000; answer(); assert log == [('reply', P), ('initvars', P)], log
# Another screen replaces the picture: released on the next frame.
setup_shell(u); rig.tick = 600000; rig.invoke_address(preload, (world(rig, 'worlds\\realm2\\R2M0'),), this=SHELL)
log.clear(); rig.tick = 600500; answer(); assert log == []
u.mem_write(SCREENS + 4, pack(2)); rig.invoke_address(draw, (1,), this=SCREENS)
assert log[:2] == [('reply', P), ('initvars', P)], log
# A new connection ("Got ID packet" print, client.exe 0x6986e) clears the
# entered flag again; the call is forwarded to the original print.
code = bytes(u.mem_read(0x1ca0000 + 0x6986e, 5))
assert code[0] == 0xe8 and (0x1ca0000 + 0x6986e + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == dll + syms['_kpc_new_connection_print']
assert read(u, dll + syms['_g_loading_entered']) == 1 and read(u, dll + syms['_g_id_print_target']) == 0x1ca0000 + 0x1c8d0
# First connect from the menu: the connect dialog never reaches the shell's
# screen draw, so the client update's per-frame call (client.exe 0x13876 ->
# 0x13bb0) releases the hold on its own (the 2026-11-12 hang).
CLIENT = 0x1ca0000
code = bytes(u.mem_read(CLIENT + 0x13876, 5))
assert code[0] == 0xe8 and (CLIENT + 0x13876 + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == dll + syms['_kpc_client_frame']
def update_stub(uu, at, size, user):
    log.append(('update', uu.reg_read(x.UC_X86_REG_ECX), read(uu, uu.reg_read(x.UC_X86_REG_ESP) + 4))); sp = uu.reg_read(x.UC_X86_REG_ESP)
    uu.reg_write(x.UC_X86_REG_ESP, sp + 8); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
u.hook_add(uc.UC_HOOK_CODE, update_stub, begin=CLIENT + 0x13bb0, end=CLIENT + 0x13bb0)
frame = dll + syms['_kpc_client_frame']; CM = 0x5a9000
setup_shell(u, state=5); rig.tick = 650000; rig.invoke_address(preload, (world(rig, 'WORLDS\\REALM1\\R1M2B.DAT'),), this=SHELL)
log.clear(); rig.tick = 650200; answer(); assert log == [], log
rig.tick = 652000; rig.invoke_address(frame, (0x33,), this=CM); assert log == [('update', CM, 0x33)], log
log.clear(); rig.tick = 655000; rig.invoke_address(frame, (0x33,), this=CM)
assert log == [('reply', P), ('initvars', P), ('update', CM, 0x33)], log
log.clear(); rig.invoke_address(frame, (0x34,), this=CM); assert log == [('update', CM, 0x34)], log
# Deathmatch world: never held.
setup_shell(u); rig.invoke_address(preload, (world(rig, 'worlds\\deathmatch\\PM01'),), this=SHELL)
assert b'Loading world: worlds\\deathmatch\\PM01 (stock: not a campaign world)' in bytes(rig.logs)
log.clear(); answer(); assert log == [('reply', P), ('initvars', P)], log
# A new load drops a stale hold (nothing sent for the old world).
setup_shell(u); rig.tick = 700000; rig.invoke_address(preload, (world(rig, 'worlds\\realm3\\R3M0'),), this=SHELL)
log.clear(); rig.tick = 700100; answer(); assert log == []
rig.invoke_address(preload, (world(rig, 'worlds\\realm3\\R3M1A'),), this=SHELL)
log.clear(); rig.tick = 709000; rig.invoke_address(draw, (2,), this=SCREENS); assert ('reply', P) not in log, log
# Hold disabled: the two calls stay original.
rig5 = Rig(settings={'Multiplayer.CampaignLoadingHoldSpawn': 0}); rig5.factory(); assert not rig5.messages
for site in (0xbeadc, 0xbeae3):
    assert bytes(rig5.u.mem_read(base + site, 5)) == bytes(rig5.initial_shell[site:site + 5])
# The per-frame client update hook is shared with camera smoothing: only with
# both off does it keep its original bytes.
rig6b = Rig(settings={'Multiplayer.CampaignLoadingHoldSpawn': 0, 'Multiplayer.SmoothCutsceneCamera': 0}); rig6b.factory(); assert not rig6b.messages
assert bytes(rig6b.u.mem_read(0x1ca0000 + 0x13876, 5)) == bytes(rig6b.initial_client[0x13876:0x1387b])
assert b'spawn hold' not in bytes(rig5.logs)

# 12. Chat message time (ChatMessageSeconds, default 8): only the immediate of
#     MOV [ESP+0x10], 3.0f at cshell 0xcb38d changes; 3 keeps the original.
assert bytes(rig5.u.mem_read(base + 0xcb38d, 8)) == bytes.fromhex('c744241000000041')
assert b'Chat message time: seconds=0x00000008' in bytes(rig5.logs)
rig6 = Rig(settings={'Multiplayer.ChatMessageSeconds': 3}); rig6.factory(); assert not rig6.messages
assert bytes(rig6.u.mem_read(base + 0xcb38d, 8)) == bytes(rig6.initial_shell[0xcb38d:0xcb395]) == bytes.fromhex('c744241000004040')
rig7 = Rig(settings={'Multiplayer.ChatMessageSeconds': 61}); rig7.factory()
assert any('ChatMessageSeconds' in m for m in rig7.messages), rig7.messages

# 10. An out-of-range minimum is refused before any edit.
rig4 = Rig(settings={'Multiplayer.CampaignLoadingMinimumMs': 30001}); rig4.factory()
assert any('CampaignLoadingMinimumMs' in m for m in rig4.messages), rig4.messages
assert read(rig4.u, base + 0xff73c) == base + 0xcd360
print(args.fixtures.name, 'passed campaign loading art: slot/calls, picture swap, 5 s minimum, slow load, replaced screen, deathmatch, single player, disabled, spawn hold, limits')
