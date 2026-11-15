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
# A new connection drops a held reply: it belonged to the server that is gone.
setup_shell(u); rig.tick = 720000; rig.invoke_address(preload, (world(rig, 'worlds\\realm3\\R3M1B'),), this=SHELL)
log.clear(); rig.tick = 720100; answer(); assert log == [], log
u.mem_write(dll + syms['_g_loading_entered'], pack(0))            # as kpc_new_connection_print does
rig.tick = 726000; rig.invoke_address(draw, (2,), this=SCREENS); assert ('reply', P) not in log and ('initvars', P) not in log, log
assert b'spawn reply dropped (new connection)' in bytes(rig.logs)
rig.tick = 760000; rig.invoke_address(draw, (2,), this=SCREENS); assert ('reply', P) not in log, log   # not at the safety cap either
u.mem_write(dll + syms['_g_loading_entered'], pack(1))
# Synchronised load (CampaignLoadingSync): the shell's OnMessage slot
# (0xff734 -> 0xcad30) takes 0x70 (hold for every player) and 0x71 (go). The
# held reply waits past the minimum until 0x71; world sounds are paused while
# it is held (PauseAll/ResumeAll on [0xaacd0]+0x284); 0x69 then closes the
# picture at once. Other messages reach the original.
assert read(u, base + 0xff734) == dll + syms['_kpc_shell_message']
assert b'Synchronised load: with a co-op server' in bytes(rig.logs)
msgs_seen = []
def on_message(uu, at, size, user):
    sp = uu.reg_read(x.UC_X86_REG_ESP); msgs_seen.append(read(uu, sp + 4))
    uu.reg_write(x.UC_X86_REG_ESP, sp + 12); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
u.hook_add(uc.UC_HOOK_CODE, on_message, begin=base + 0xcad30, end=base + 0xcad30)
SOUND_CM = 0x5aa000
u.mem_write(CLIENT + 0xaacd0, pack(SOUND_CM))
def sound_stub(address, tag):
    def run(uu, at, size, user):
        log.append((tag, uu.reg_read(x.UC_X86_REG_ECX))); sp = uu.reg_read(x.UC_X86_REG_ESP)
        uu.reg_write(x.UC_X86_REG_EAX, 1); uu.reg_write(x.UC_X86_REG_ESP, sp + 4); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
    u.hook_add(uc.UC_HOOK_CODE, run, begin=address, end=address)
sound_stub(CLIENT + 0x74c90, 'pauseall'); sound_stub(CLIENT + 0x74ce0, 'resumeall')
message = dll + syms['_kpc_shell_message']
rig.invoke_address(message, (0x48, 0x1234), this=SHELL); assert msgs_seen == [0x48], msgs_seen   # others: original
rig.invoke_address(message, (0x70, 0x1234), this=SHELL); assert msgs_seen == [0x48]              # taken
setup_shell(u); rig.tick = 800000; rig.invoke_address(preload, (world(rig, 'worlds\\realm2\\R2M0'),), this=SHELL)
assert b'Loading world: worlds\\realm2\\R2M0 (picture shown; waiting for every player)' in bytes(rig.logs)
log.clear(); rig.tick = 800100; answer()
assert log == [('pauseall', SOUND_CM + 0x284)], log                                             # held, sounds paused
assert b'held until every player has loaded (sounds paused)' in bytes(rig.logs)
log.clear(); rig.tick = 806000; rig.invoke_address(draw, (3,), this=SCREENS)
assert log == [('draw', SCREENS, 3)], log                                                        # past the minimum: still held
log.clear(); rig.tick = 806500; rig.invoke_address(message, (0x71, 0), this=SHELL)
assert log == [('reply', P), ('initvars', P), ('resumeall', SOUND_CM + 0x284)], log             # go: sent, sounds back
assert b'spawn reply sent (server: every player has loaded)' in bytes(rig.logs)
log.clear(); rig.invoke_address(close_hook, (), this=SCREENS); assert log == [('close', SCREENS)], log   # 0x69: closed at once
# A go that arrives before the reply: nothing is held.
rig.invoke_address(message, (0x70, 0), this=SHELL)
setup_shell(u); rig.tick = 810000; rig.invoke_address(preload, (world(rig, 'worlds\\realm2\\R2M1A'),), this=SHELL)
rig.invoke_address(message, (0x71, 0), this=SHELL)
log.clear(); rig.tick = 810100; answer(); assert log == [('reply', P), ('initvars', P)], log
# No go from the server: released at the safety cap.
rig.invoke_address(message, (0x70, 0), this=SHELL)
setup_shell(u); rig.tick = 820000; rig.invoke_address(preload, (world(rig, 'worlds\\realm2\\R2M1B'),), this=SHELL)
log.clear(); rig.tick = 820100; answer(); assert log[:1] == [('pauseall', SOUND_CM + 0x284)], log
log.clear(); rig.tick = 894000; rig.invoke_address(draw, (3,), this=SCREENS); assert ('reply', P) not in log, log
log.clear(); rig.tick = 896000; rig.invoke_address(draw, (3,), this=SCREENS)
assert log[:3] == [('reply', P), ('initvars', P), ('resumeall', SOUND_CM + 0x284)], log
# Without 0x70 the minimum applies as before.
setup_shell(u); rig.tick = 900000; rig.invoke_address(preload, (world(rig, 'worlds\\realm2\\R2M1C'),), this=SHELL)
log.clear(); rig.tick = 900100; answer(); assert log == [], log
log.clear(); rig.tick = 905000; rig.invoke_address(draw, (3,), this=SCREENS); assert log[:2] == [('reply', P), ('initvars', P)], log
# Clean music (CleanMusic): at 0x46 (call 0xcb47d -> 0xcdc80) the song stops at
# a bar line (d9980) and READY/INTENSITY are held (d9d00); after the multiplayer
# entry wipe (0xc8622 -> d96f0) the held state is dropped (d9d00 again) and it is
# applied (d9d20) when the loading picture closes, or at once without a picture.
MGR = 0x5ab000
u.mem_write(base + 0x1d2788, pack(MGR))
for site, name in ((0xcb47d, '_kpc_change_levels'), (0xc8622, '_kpc_enter_music')):
    code = bytes(u.mem_read(base + site, 5))
    assert code[0] == 0xe8 and (base + site + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == dll + syms[name], name
assert b'Clean music: on multiplayer level changes' in bytes(rig.logs)
def music_stub(address, tag, pop):
    def run(uu, at, size, user):
        sp = uu.reg_read(x.UC_X86_REG_ESP)
        log.append((tag, uu.reg_read(x.UC_X86_REG_ECX)) + ((read(uu, sp + 4),) if pop else ()))
        uu.reg_write(x.UC_X86_REG_ESP, sp + 4 + pop); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
    u.hook_add(uc.UC_HOOK_CODE, run, begin=address, end=address)
music_stub(base + 0xcdc80, 'changing', 4); music_stub(base + 0xd96f0, 'unload', 4); music_stub(base + 0xcd910, 'clear', 0)
CLEARED = [('clear', SHELL), ('frame', SHELL)] * 3                       # level change without a world name: black frames
music_stub(base + 0xd9980, 'stop', 0); music_stub(base + 0xd9d00, 'hold', 0); music_stub(base + 0xd9d20, 'apply', 0)
changing, entering = dll + syms['_kpc_change_levels'], dll + syms['_kpc_enter_music']
log.clear(); rig.invoke_address(changing, (0x55,), this=SHELL)
assert log == [('changing', SHELL, 0x55)] + CLEARED + [('unload', MGR, 1), ('hold', MGR)], log   # single player's cd088/cd093
setup_shell(u); rig.tick = 950000; rig.invoke_address(preload, (world(rig, 'worlds\\realm1\\R1M3A'),), this=SHELL)
log.clear(); rig.invoke_address(entering, (1,), this=MGR)
assert log == [], log                                                     # no second wipe; picture up: nothing starts
log.clear(); rig.tick = 956000; rig.invoke_address(close_hook, (), this=SCREENS)
assert log == [('close', SCREENS), ('apply', MGR)], log                   # picture closes: music starts
# A world without the picture (deathmatch lobby): applied at entry (the
# single-player moment, c8636).
setup_shell(u); log.clear(); rig.invoke_address(changing, (0x55,), this=SHELL)
setup_shell(u); rig.invoke_address(preload, (world(rig, 'worlds\\deathmatch\\PM03'),), this=SHELL)
log.clear(); rig.invoke_address(entering, (1,), this=MGR)
assert log == [('apply', MGR)], log
# An entry without 0x46 (first join): the stock wipe, then hold and apply.
log.clear(); rig.invoke_address(entering, (1,), this=MGR)
assert log == [('unload', MGR, 1), ('hold', MGR), ('apply', MGR)], log
# Level change picture (LevelChangePicture): 0x70 carries the next world's
# path (read through ILTClient+0x50); at 0x46 its loading picture is shown at
# once instead of the stale back buffers.
API_VT, READSTR, NAME = 0x5ac000, 0x5ac100, 0x5ac200
u.mem_write(API, pack(API_VT)); u.mem_write(API_VT + 0x50, pack(READSTR)); u.mem_write(NAME, b'worlds\\realm2\\R2M0\0')
def readstr(uu, at, size, user):
    sp = uu.reg_read(x.UC_X86_REG_ESP); uu.reg_write(x.UC_X86_REG_EAX, NAME)
    uu.reg_write(x.UC_X86_REG_ESP, sp + 8); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
u.hook_add(uc.UC_HOOK_CODE, readstr, begin=READSTR, end=READSTR)
rig.invoke_address(message, (0x70, 0x4444), this=SHELL)
setup_shell(u, state=5); log.clear(); rig.invoke_address(changing, (0x55,), this=SHELL)
assert ('load', 'worlds\\realm2\\R2M0') in log and ('show', SCREENS, 0) in log and ('clear', SHELL) not in log, log
assert b'Level change: loading picture shown at once for worlds\\realm2\\R2M0' in bytes(rig.logs)
log.clear(); rig.invoke_address(changing, (0x55,), this=SHELL)
assert log[:7] == [('changing', SHELL, 0x55)] + CLEARED, log               # name used once; next change: black
# R?M0 intro worlds (IntroPictureHold): after the go and entry the picture
# stays until Server Tools reports the intro start (0x74), capped at 20 s.
u.mem_write(NAME, b'worlds\\realm3\\R3M0\0')
for with_start in (True, False):
    rig.invoke_address(message, (0x70, 0x4444), this=SHELL)
    setup_shell(u); rig.tick = 1000000 if with_start else 1100000; base_t = rig.tick
    rig.invoke_address(preload, (world(rig, 'worlds\\realm3\\R3M0'),), this=SHELL)
    rig.tick = base_t + 100; answer(); assert ('pauseall', SOUND_CM + 0x284) in log, log
    log.clear(); rig.tick = base_t + 200; rig.invoke_address(message, (0x71, 0), this=SHELL)
    assert ('resumeall', SOUND_CM + 0x284) not in log, log                              # go: sounds stay paused (R?M0)
    log.clear(); rig.tick = base_t + 300; rig.invoke_address(close_hook, (), this=SCREENS)
    assert ('close', SCREENS) not in log, log                                          # entry: kept
    assert b'Loading picture: kept until the intro starts' in bytes(rig.logs)
    log.clear(); rig.tick = base_t + 9000; rig.invoke_address(draw, (3,), this=SCREENS)
    assert log == [('draw', SCREENS, 3)], log                                           # past the minimum: still up
    if with_start:
        rig.invoke_address(message, (0x74, 0), this=SHELL); assert 0x74 not in msgs_seen
        assert b'Loading picture: the intro started; closing' in bytes(rig.logs)
        log.clear(); rig.tick = base_t + 9100; rig.invoke_address(draw, (3,), this=SCREENS)
    else:
        log.clear(); rig.tick = base_t + 300 + 20001; rig.invoke_address(draw, (3,), this=SCREENS)
        assert b'closed after the intro wait cap' in bytes(rig.logs)
    assert ('close', SCREENS) in log and ('resumeall', SOUND_CM + 0x284) in log, log  # sounds resume with the close
    assert log.index(('close', SCREENS)) < log.index(('resumeall', SOUND_CM + 0x284)), log
# Music stop from Server Tools (0x75; the stock FADEOUT 0x53 is a no-op): the held
# state is dropped and the song stopped; the stock handler never sees it.
u.mem_write(base + 0x1d2788, pack(MGR)); log.clear(); msgs_seen.clear()
rig.invoke_address(message, (0x75, 0), this=SHELL)
assert ('stop', MGR) in log and msgs_seen == [], (log, msgs_seen)
assert b'Music: stopped by the server' in bytes(rig.logs)
# Other worlds are not held for an intro.
u.mem_write(NAME, b'worlds\\realm3\\R3M1A\0')
rig.invoke_address(message, (0x70, 0x4444), this=SHELL)
setup_shell(u); rig.tick = 1200000; rig.invoke_address(preload, (world(rig, 'worlds\\realm3\\R3M1A'),), this=SHELL)
rig.tick = 1200100; answer(); rig.invoke_address(message, (0x71, 0), this=SHELL)
log.clear(); rig.invoke_address(close_hook, (), this=SCREENS); assert ('close', SCREENS) in log, log
u.mem_write(API, pack(0))
rig9 = Rig(settings={'Multiplayer.CleanMusic': 0, 'Multiplayer.LevelChangePicture': 0, 'Multiplayer.StopSoundsOnLevelChange': 0}); rig9.factory(); assert not rig9.messages
for site in (0xcb47d, 0xc8622):
    assert bytes(rig9.u.mem_read(base + site, 5)) == bytes(rig9.initial_shell[site:site + 5]), hex(site)
# CampaignLoadingSync=0: the slot stays original and 0x70/0x71 reach the stock handler.
rig8 = Rig(settings={'Multiplayer.CampaignLoadingSync': 0, 'SinglePlayerLoadingMs': 0}); rig8.factory(); assert not rig8.messages
assert read(rig8.u, base + 0xff734) == base + 0xcad30
# Minimum update rate (MinUpdateRate, default 60): in remote multiplayer the
# client's UpdateRate byte (client.exe 0x1d41adc) is raised to 60 every frame.
CM2, SESS2 = 0x5ad000, 0x5ad800
u.mem_write(CLIENT + 0xaacd0, pack(CM2)); u.mem_write(CM2 + 0x1004, pack(SESS2)); u.mem_write(SESS2 + 0xa8, pack(2)); u.mem_write(SESS2 + 0x50, pack(0))
u.mem_write(0x1d41adc, b'\x0a'); rig.invoke_address(frame, (0x35,), this=CM)
assert u.mem_read(0x1d41adc, 1)[0] == 60
u.mem_write(SESS2 + 0x50, pack(1)); u.mem_write(0x1d41adc, b'\x0a'); rig.invoke_address(frame, (0x35,), this=CM)
assert u.mem_read(0x1d41adc, 1)[0] == 10                                      # local game: untouched
u.mem_write(CLIENT + 0xaacd0, pack(0))
# 4:3 loading pictures (Cinematics4x3): the picture blits (0xad978 type 5,
# 0xac6a8 type 0) black the screen, then scale the picture into the centred 4:3 rect.
for site in (0xad978, 0xac6a8):
    code = bytes(u.mem_read(base + site, 6))
    assert code[0] == 0xe8 and code[5] == 0x90 and (base + site + 5 + struct.unpack('<i', code[1:5])[0]) & 0xffffffff == dll + syms['_kpc_loading_blit'], hex(site)
assert b'Loading pictures: drawn 4:3' in bytes(rig.logs)
scales, made = [], []
STUBS2 = 0x62000900
def cstub(index, nargs, fn):
    at = STUBS2 + index * 0x10; u.mem_write(at, b'\xc3')
    def run(uu, a, size, user):
        sp = uu.reg_read(x.UC_X86_REG_ESP); args = [read(uu, sp + 4 + 4 * k) for k in range(nargs)]
        uu.reg_write(x.UC_X86_REG_EAX, fn(args) & 0xffffffff); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp)); uu.reg_write(x.UC_X86_REG_ESP, sp + 4)
    u.hook_add(uc.UC_HOOK_CODE, run, begin=at, end=at)
    return at
def rect(ptr): return struct.unpack('<4i', bytes(u.mem_read(ptr, 16))) if ptr else None
u.mem_write(API + 0x18c, pack(cstub(0, 4, lambda a: scales.append((a[0], a[1], rect(a[2]), a[3])) or 1)))
u.mem_write(API + 0x154, pack(cstub(1, 2, lambda a: made.append((a[0], a[1])) or 0x9990)))
u.mem_write(API + 0x1ac, pack(cstub(2, 3, lambda a: 1))); u.mem_write(API + 0x158, pack(cstub(3, 1, lambda a: 1)))
def call_cdecl(r, address, args, stub=0x62000a00):
    """cdecl call through a tiny caller that pushes the arguments and cleans up."""
    code = b''.join(bytes([0x68]) + pack(a) for a in reversed(args))
    at = stub + len(code)
    code += bytes([0xe8]) + struct.pack('<i', address - (at + 5)) + bytes([0x83, 0xc4, 4 * len(args), 0xc3])
    r.u.mem_write(stub, code); return r.invoke_address(stub, ())
R = 0x5ae000; u.mem_write(R, struct.pack('<4i', 0, 0, 2560, 1440))
PIC = 0x5af800                                                                   # a picture without a renderer surface
call_cdecl(rig, dll + syms['_kpc_loading_blit'], (0x7100, PIC, R, 0))
assert made == [(8, 8)] and scales == [(0x7100, 0x9990, (0, 0, 2560, 1440), 0), (0x7100, PIC, (320, 0, 2240, 1440), 0)], scales
scales.clear(); u.mem_write(R, struct.pack('<4i', 0, 0, 1024, 768))
call_cdecl(rig, dll + syms['_kpc_loading_blit'], (0x7100, PIC, R, 0))
assert scales == [(0x7100, PIC, (0, 0, 1024, 768), 0)], scales                    # already 4:3, nothing to smooth: unchanged
# Smooth loading pictures (LoadingPictureFilter): a picture with a renderer
# surface is resampled into a target-sized surface; when GDI on the surfaces
# fails the copy is deleted and the stock stretch is used.
PIC2, REN2, DDS2, DDVT2, GETDC2 = 0x5af900, 0x5afa00, 0x5afb00, 0x5afc00, 0x62000b00
u.mem_write(PIC2 + 0xc, pack(REN2)); u.mem_write(REN2, pack(DDS2)); u.mem_write(DDS2, pack(DDVT2))
u.mem_write(GETDC2, b'\xb8\x01\x00\x00\x00\xc2\x08\x00')                        # GetDC: failure
for k in range(32): u.mem_write(DDVT2 + 4 * k, pack(GETDC2))
deleted = []
SURF3, REN3 = 0x5afd00, 0x5afe00
u.mem_write(SURF3 + 0xc, pack(REN3)); u.mem_write(REN3, pack(DDS2))
u.mem_write(API + 0x154, pack(cstub(7, 2, lambda a: made.append((a[0], a[1])) or SURF3)))
u.mem_write(API + 0x158, pack(cstub(5, 1, lambda a: deleted.append(a[0]) or 1)))
u.mem_write(API + 0x174, pack(cstub(6, 3, lambda a: (u.mem_write(a[1], pack(640)), u.mem_write(a[2], pack(480)), 1)[-1])))
u.mem_write(R, struct.pack('<4i', 0, 0, 2560, 1440)); scales.clear(); made.clear()
R2 = 0x5ae100; u.mem_write(R2, struct.pack('<4i', 0, 0, 640, 480))
call_cdecl(rig, dll + syms['_kpc_loading_blit'], (0x7100, PIC2, R, R2), stub=0x62000c00)   # own stub: unicorn keeps translated code
assert made == [(8, 8), (1920, 1440)], made                                        # border fill, then the smooth copy
assert deleted == [SURF3, SURF3] and scales[-1] == (0x7100, PIC2, (320, 0, 2240, 1440), R2), (deleted, scales)
assert b'Loading pictures: smooth scaling unavailable' in bytes(rig.logs)
# Single-player loading picture minimum (SinglePlayerLoadingMs, default 5000).
rigsp = Rig(); rigsp.factory(); assert not rigsp.messages
usp = rigsp.u; logsp = install_stubs(rigsp, multiplayer=False)
for site, name in ((0xcb8d7, '_kpc_sp_show'), (0xc7d8a, '_kpc_sp_enter_close'), (0xc6f89, '_kpc_sp_unpause')):
    code = bytes(usp.mem_read(base + site, 5))
    assert code[0] == 0xe8 and (base + site + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == rigsp.dllbase + syms[name], name
assert b'Single-player loading picture: shown for at least' in bytes(rigsp.logs)
pauses = []
def pause_stub(uu, at, size, user):
    sp = uu.reg_read(x.UC_X86_REG_ESP); pauses.append((read(uu, sp + 4), read(uu, sp + 8)))
    uu.reg_write(x.UC_X86_REG_ESP, sp + 12); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
usp.hook_add(uc.UC_HOOK_CODE, pause_stub, begin=base + 0xcd7b0, end=base + 0xcd7b0)
seen41 = []
def msg_stub(uu, at, size, user):
    sp = uu.reg_read(x.UC_X86_REG_ESP); seen41.append(read(uu, sp + 4))
    uu.reg_write(x.UC_X86_REG_ESP, sp + 12); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
usp.hook_add(uc.UC_HOOK_CODE, msg_stub, begin=base + 0xcad30, end=base + 0xcad30)
usp.mem_write(base + 0x1c8de8, pack(SHELL)); setup_shell(usp, state=4)
dsp = rigsp.dllbase
rigsp.tick = 100000; logsp.clear()
rigsp.invoke_address(dsp + syms['_kpc_sp_show'], (5,), this=SCREENS)
assert logsp[0] == ('show', SCREENS, 5) and logsp.count(('frame', SHELL)) == 3, logsp   # presented before the blocking load
rigsp.tick = 101000; logsp.clear(); rigsp.invoke_address(dsp + syms['_kpc_sp_enter_close'], (), this=SCREENS)
assert logsp == [] and b'kept up until its minimum time' in bytes(rigsp.logs)          # not closed yet
rigsp.invoke_address(dsp + syms['_kpc_sp_unpause'], (0, 0), this=SHELL)
rigsp.invoke_address(dsp + syms['_kpc_sp_unpause'], (0, 0), this=SHELL)
assert pauses == [(1, 0)], pauses                                                      # pause instead of unpause, once
rigsp.invoke_address(dsp + syms['_kpc_shell_message'], (0x41, 0), this=SHELL); assert seen41 == []   # our pause's echo
rigsp.invoke_address(dsp + syms['_kpc_shell_message'], (0x41, 0), this=SHELL); assert seen41 == [0x41]
usp.mem_write(RENDER + 0x218, b'\x00'); rigsp.tick = 103000; logsp.clear()
rigsp.invoke_address(dsp + syms['_kpc_draw_screen'], (2,), this=SCREENS)
assert usp.mem_read(RENDER + 0x218, 1)[0] == 1 and logsp == [('draw', SCREENS, 2)]    # redrawn over the camera
rigsp.tick = 105100; logsp.clear(); rigsp.invoke_address(dsp + syms['_kpc_draw_screen'], (2,), this=SCREENS)
assert ('close', SCREENS) in logsp and pauses[-1] == (0, 0), (logsp, pauses)           # minimum: closed, unpaused
assert b'Single-player loading picture closed (minimum reached)' in bytes(rigsp.logs)
# A slow load (entry after the minimum): closed at once, no pause.
setup_shell(usp, state=4); rigsp.tick = 200000; rigsp.invoke_address(dsp + syms['_kpc_sp_show'], (5,), this=SCREENS)
rigsp.tick = 206000; logsp.clear(); rigsp.invoke_address(dsp + syms['_kpc_sp_enter_close'], (), this=SCREENS)
assert logsp == [('close', SCREENS)]
rigsp.invoke_address(dsp + syms['_kpc_sp_unpause'], (0, 0), this=SHELL); assert pauses[-1] == (0, 0)   # stock unpause
# Single player: the stock intensity drop on death runs.
deaths = []
def sp_intensity(uu, at, size, user):
    sp = uu.reg_read(x.UC_X86_REG_ESP); deaths.append((uu.reg_read(x.UC_X86_REG_ECX), read(uu, sp + 4)))
    uu.reg_write(x.UC_X86_REG_ESP, sp + 8); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
usp.hook_add(uc.UC_HOOK_CODE, sp_intensity, begin=base + 0xd99a0, end=base + 0xd99a0)
rigsp.invoke_address(dsp + syms['_kpc_death_music'], (1,), this=0x5ab000)
assert deaths == [(0x5ab000, 1)], deaths
rigsp0 = Rig(settings={'SinglePlayerLoadingMs': 0}); rigsp0.factory(); assert not rigsp0.messages
assert bytes(rigsp0.u.mem_read(base + 0xc7d8a, 5)) == bytes(rigsp0.initial_shell[0xc7d8a:0xc7d8f])
# Music kept on death (KeepMusicOnDeath, default 1): the player-state 3
# intensity drop (0xcacb1 -> d99a0, thiscall(mgr, 1)) is skipped in multiplayer.
code = bytes(u.mem_read(base + 0xcacb1, 5))
assert code[0] == 0xe8 and (base + 0xcacb1 + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff == dll + syms['_kpc_death_music']
music_stub(base + 0xd99a0, 'intensity', 4)
u.mem_write(base + 0x1c8de8, pack(SHELL))
log.clear(); rig.invoke_address(dll + syms['_kpc_death_music'], (1,), this=MGR)
assert log == [], log
assert b'Music: kept on death (multiplayer' in bytes(rig.logs)
# Voice lines stop at a level change (StopSoundsOnLevelChange): at 0x46 every
# server-created sound instance is killed (0x73a20, thiscall(sm, inst)); the
# shell's own sounds (instance flags & 0x100) stay.
CM3 = 0x5ae800; SM3 = CM3 + 0x284; INSTS = [0x5af000, 0x5af100, 0x5af200]
u.mem_write(CLIENT + 0xaacd0, pack(CM3)); u.mem_write(SM3 + 0x120, pack(1)); u.mem_write(SM3 + 0x530, pack(3))
for k, inst in enumerate(INSTS):
    u.mem_write(SM3 + 0x330 + 4 * k, pack(inst)); u.mem_write(inst + 0x4c, pack(k)); u.mem_write(inst + 0x14, pack(0x100 if k == 1 else 0))
killed = []
def kill_inst(uu, at, size, user):
    sp = uu.reg_read(x.UC_X86_REG_ESP); sm, inst = uu.reg_read(x.UC_X86_REG_ECX), read(uu, sp + 4)
    n = read(uu, sm + 0x530); slot = read(uu, inst + 0x4c); last = read(uu, sm + 0x330 + 4 * (n - 1))
    uu.mem_write(sm + 0x330 + 4 * slot, pack(last)); uu.mem_write(last + 0x4c, pack(slot)); uu.mem_write(sm + 0x330 + 4 * (n - 1), pack(0))
    uu.mem_write(sm + 0x530, pack(n - 1)); killed.append((sm, inst))
    uu.reg_write(x.UC_X86_REG_ESP, sp + 8); uu.reg_write(x.UC_X86_REG_EIP, read(uu, sp))
u.hook_add(uc.UC_HOOK_CODE, kill_inst, begin=CLIENT + 0x73a20, end=CLIENT + 0x73a20)
setup_shell(u); rig.invoke_address(changing, (0x55,), this=SHELL)
assert killed == [(SM3, 0x5af000), (SM3, 0x5af200)], killed
assert read(u, SM3 + 0x530) == 1 and read(u, SM3 + 0x330) == 0x5af100
assert b"Level change: the old world's sounds stopped" in bytes(rig.logs)
u.mem_write(CLIENT + 0xaacd0, pack(0))
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
print(args.fixtures.name, 'passed campaign loading art: slot/calls, picture swap, 5 s minimum, slow load, replaced screen, deathmatch, single player, disabled, spawn hold, synchronised load, clean music, level change picture, update rate, 4:3 pictures, single-player picture minimum, music kept on death, sounds stopped at a level change, R?M0 picture until the intro, smooth loading pictures (fallback), limits')
