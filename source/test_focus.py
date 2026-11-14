"""Focus loss (KeepAudioOnFocusLoss): with the console variable on, nothing
pauses when the window loses focus, in every mode. The WM_ACTIVATEAPP sound
pause/resume calls (client.exe 0x6d5c, 0x6cdb) are skipped as a balanced pair,
the music pause/resume of cshell OnEvent 4/3 (0xc80e2, 0xc80ba) is skipped
when the pause comes from the focus-loss render shutdown (0x6d1d), and the
local server's active-window test (0x157c5) reports active. With
KeepRendererOnFocusLoss=1 (default) the renderer shutdown (0x6d1d) and its
restart (0x6c97) are skipped as a pair and the client world's window tests
(0x158e3, 0x15922) report active while it is kept. With the variable off
every call is forwarded unchanged. The original routines are recording
stubs."""
import json
import struct
import sys
import test_runtime

sys.argv += ['--expose-rig']
Rig, _, _, uc, x, args = test_runtime.main()
syms = json.loads(args.symbols.read_text())
pack = lambda *v: struct.pack('<' + 'I' * len(v), *[i & 0xffffffff for i in v])
read = lambda u, a: struct.unpack('<I', u.mem_read(a, 4))[0]
CLIENT, SHELLBASE, CM, SESSION, SOUND, MUSIC = 0x1ca0000, 0x20000000, 0x5c0000, 0x5c1000, 0x5c2000, 0x5c4000
CVAR = 0x5c3000
SHELLOBJ = 0x5c6000                  # client shell object ([cshell+0x1c8de8]); state at +0x1398


def call_target(u, at):
    code = bytes(u.mem_read(at, 5))
    assert code[0] == 0xe8, (hex(at), code.hex())
    return (at + 5 + struct.unpack('<i', code[1:])[0]) & 0xffffffff


def stub(rig, calls, address, name, pop=4, result=1):
    def run(u, at, size, user):
        calls.append((name, u.reg_read(x.UC_X86_REG_ECX)))
        sp = u.reg_read(x.UC_X86_REG_ESP)
        u.reg_write(x.UC_X86_REG_EAX, result if not callable(result) else result())
        u.reg_write(x.UC_X86_REG_ESP, sp + pop); u.reg_write(x.UC_X86_REG_EIP, read(u, sp))
    rig.u.hook_add(uc.UC_HOOK_CODE, run, begin=address, end=address)


def setup(rig, cvar):
    """Recording stubs for the original routines; console lookup/setter."""
    calls = []
    for address, name in ((CLIENT + 0x74b90, 'pause'), (CLIENT + 0x74c10, 'resume'),
                          (CLIENT + 0x74c90, 'pauseall'), (CLIENT + 0x74ce0, 'resumeall'),
                          (CLIENT + 0x73bb0, 'update'),
                          (SHELLBASE + 0xd9ad0, 'music_pause'), (SHELLBASE + 0xd9b00, 'music_resume')):
        stub(rig, calls, address, name)
    # cdecl: r_TermRender(cm, 1) runs the shell's OnEvent 4, whose music call is hooked.
    def term(u, at, size, user):
        calls.append(('term', read(u, u.reg_read(x.UC_X86_REG_ESP) + 4)))
        sp = u.reg_read(x.UC_X86_REG_ESP)
        u.reg_write(x.UC_X86_REG_ESP, sp - 4); u.mem_write(sp - 4, pack(0x62000200))  # nested music call
        u.reg_write(x.UC_X86_REG_ECX, MUSIC); u.reg_write(x.UC_X86_REG_EIP, rig.dllbase + syms['_kpc_music_pause'])
    rig.u.mem_write(0x62000200, b'\xc3')                          # returns to term's caller
    rig.u.hook_add(uc.UC_HOOK_CODE, term, begin=CLIENT + 0x53980, end=CLIENT + 0x53980)
    stub(rig, calls, CLIENT + 0x287b0, 'active', result=lambda: rig.active)
    # cdecl r_InitRender(cm, mode) runs the shell's OnEvent 3, whose music resume
    # is hooked; it then returns 0 (success) through a "xor eax,eax; ret" tail.
    def init(u, at, size, user):
        sp = u.reg_read(x.UC_X86_REG_ESP); calls.append(('init', read(u, sp + 4)))
        u.reg_write(x.UC_X86_REG_ESP, sp - 4); u.mem_write(sp - 4, pack(0x62000210))  # nested music call
        u.reg_write(x.UC_X86_REG_ECX, MUSIC); u.reg_write(x.UC_X86_REG_EIP, rig.dllbase + syms['_kpc_music_resume'])
    rig.u.mem_write(0x62000210, b'\x31\xc0\xc3')
    rig.u.hook_add(uc.UC_HOOK_CODE, init, begin=CLIENT + 0x534f0, end=CLIENT + 0x534f0)
    rig.active = 0

    def lookup(u, at, size, user):
        sp = u.reg_read(x.UC_X86_REG_ESP); assert rig.cstr(read(u, sp + 8)) == 'KeepAudioOnFocusLoss'
        u.reg_write(x.UC_X86_REG_EAX, CVAR if cvar['exists'] else 0); u.reg_write(x.UC_X86_REG_EIP, read(u, sp)); u.reg_write(x.UC_X86_REG_ESP, sp + 4)
    def setter(u, at, size, user):
        sp = u.reg_read(x.UC_X86_REG_ESP); value = rig.cstr(read(u, sp + 12))
        cvar['sets'].append((rig.cstr(read(u, sp + 8)), value))
        cvar['exists'] = True; u.mem_write(CVAR + 0x18, struct.pack('<f', float(value)))
        u.reg_write(x.UC_X86_REG_EAX, 1); u.reg_write(x.UC_X86_REG_EIP, read(u, sp)); u.reg_write(x.UC_X86_REG_ESP, sp + 4)
    rig.u.hook_add(uc.UC_HOOK_CODE, lookup, begin=CLIENT + 0x1b010, end=CLIENT + 0x1b010)
    rig.u.hook_add(uc.UC_HOOK_CODE, setter, begin=CLIENT + 0x1abd0, end=CLIENT + 0x1abd0)
    return calls


def session(rig, mode, local_server=0, present=True, state=1):
    """state: client shell state (1 = in a world; 5 = changing level; None = no shell yet)."""
    rig.u.mem_write(SHELLBASE + 0x1c8de8, pack(0 if state is None else SHELLOBJ))
    rig.u.mem_write(SHELLOBJ + 0x1398, pack(state or 0))
    rig.u.mem_write(CLIENT + 0xaacd0, pack(CM if present else 0))
    rig.u.mem_write(CM + 0x1004, pack(SESSION)); rig.u.mem_write(SESSION + 0xa8, pack(mode))
    rig.u.mem_write(SESSION + 0x50, pack(local_server))


def lose(rig, calls):
    """Deactivate: 0x6d1d render shutdown (music pause inside), 0x6d5c sound pause."""
    calls.clear()
    # cdecl with two arguments: call through a tiny caller that cleans up.
    rig.u.mem_write(0x62000300, b'\x6a\x01\x68' + pack(CM) + b'\xe8' + struct.pack('<i', rig.dllbase + syms['_kpc_focus_term_render'] - (0x62000300 + 12)) + b'\x83\xc4\x08\xc3')
    rig.invoke_address(0x62000300, ())
    rig.invoke_address(rig.dllbase + syms['_kpc_focus_release'], (), this=SOUND)
    return list(calls)


def regain(rig, calls):
    """Activate: 0x6c97 render restart (OnEvent 3 music resume 0xc80ba inside), 0x6cdb sound resume."""
    calls.clear()
    rig.u.mem_write(0x62000340, b'\x68' + pack(0x5c5000) + b'\x68' + pack(CM) + b'\xe8' + struct.pack('<i', rig.dllbase + syms['_kpc_focus_init_render'] - (0x62000340 + 15)) + b'\x83\xc4\x08\xc3')
    assert rig.invoke_address(0x62000340, ()) == 0
    rig.invoke_address(rig.dllbase + syms['_kpc_focus_restore'], (), this=SOUND)
    return list(calls)


def world_active(rig, calls):
    calls.clear()
    return rig.invoke_address(rig.dllbase + syms['_kpc_focus_world_active'], ())


def server_active(rig, calls):
    calls.clear()
    return rig.invoke_address(rig.dllbase + syms['_kpc_focus_active'], ())


STOCK_LOSE = [('term', CM), ('music_pause', MUSIC), ('pause', SOUND)]
STOCK_REGAIN = [('init', CM), ('music_resume', MUSIC), ('resume', SOUND)]
KEPT_LOSE = []                       # renderer kept: no shutdown, no music or sound pause

rig = Rig(); rig.factory(); assert not rig.messages, rig.messages
for site, name in ((0x6d5c, '_kpc_focus_release'), (0x6cdb, '_kpc_focus_restore'), (0x6d1d, '_kpc_focus_term_render'),
                   (0x157c5, '_kpc_focus_active'), (0x13bcd, '_kpc_sound_update'), (0x6c97, '_kpc_focus_init_render'),
                   (0x158e3, '_kpc_focus_world_active'), (0x15922, '_kpc_focus_world_active')):
    assert call_target(rig.u, CLIENT + site) == rig.dllbase + syms[name], hex(site)
for site, name in ((0xc80e2, '_kpc_music_pause'), (0xc80ba, '_kpc_music_resume')):
    assert call_target(rig.u, SHELLBASE + site) == rig.dllbase + syms[name], hex(site)
assert b'Focus loss: KeepAudioOnFocusLoss default=0x00000001' in bytes(rig.logs)
assert b'Focus loss: KeepRendererOnFocusLoss=1' in bytes(rig.logs)
cvar = {'exists': False, 'sets': []}
calls = setup(rig, cvar)
# Every mode keeps running: remote multiplayer, single player / hosting (local
# server), not multiplayer (mode 4) and menus (no session).
for mode, local in ((2, 0), (2, 0x1234), (4, 0x1234), (2, None)):
    session(rig, mode, local or 0, present=local is not None)
    rig.active = 0
    assert lose(rig, calls) == KEPT_LOSE and world_active(rig, calls) == 1   # world keeps animating
    assert regain(rig, calls) == []                                          # no restart: never shut down
    assert world_active(rig, calls) == 0                                     # stock test again once regained
    assert server_active(rig, calls) == 1                                    # local server not paused
    rig.active = 1; assert server_active(rig, calls) == 1
assert cvar['sets'] == [('KeepAudioOnFocusLoss', '1')], cvar        # created once with the INI default
# Outside a world (startup and movies before the shell exists, menus, loading,
# level change) the renderer is not kept: the stock shutdown and restart run as a
# pair, while the music and sound pauses stay skipped (playtest 2026-11-13: a
# focus change during the startup movies left a black screen).
for state in (None, 0, 5, 7):
    session(rig, 2, 0x1234, state=state)
    rig.active = 0
    assert lose(rig, calls) == [('term', CM)], state                   # shutdown runs, no music/sound pause
    assert world_active(rig, calls) == 0                                # world not forced active
    assert regain(rig, calls) == [('init', CM)], state                 # matching restart, no music/sound resume
# Lost in a world, regained after the state changed: still one balanced pair (no restart).
session(rig, 2, 0x1234, state=1); assert lose(rig, calls) == KEPT_LOSE
session(rig, 2, 0x1234, state=5); assert regain(rig, calls) == []
assert bytes(rig.logs).count(b'Focus regained') == 0
# "KeepAudioOnFocusLoss 0" (console or the Audio checkbox): everything stock.
rig.u.mem_write(CVAR + 0x18, struct.pack('<f', 0.0))
session(rig, 2, 0x1234)
rig.active = 0
assert lose(rig, calls) == STOCK_LOSE and world_active(rig, calls) == 0
assert regain(rig, calls) == STOCK_REGAIN
assert server_active(rig, calls) == 0               # single player pauses as stock
# Balanced pairs when the setting changes while away.
rig.u.mem_write(CVAR + 0x18, struct.pack('<f', 1.0)); assert lose(rig, calls) == KEPT_LOSE
rig.u.mem_write(CVAR + 0x18, struct.pack('<f', 0.0)); assert regain(rig, calls) == []
assert lose(rig, calls) == STOCK_LOSE
rig.u.mem_write(CVAR + 0x18, struct.pack('<f', 1.0)); assert regain(rig, calls) == STOCK_REGAIN
# A music pause outside a focus loss (menus at 0x2c04d, renderer restarts) stays stock.
calls.clear(); rig.invoke_address(rig.dllbase + syms['_kpc_music_pause'], (), this=MUSIC)
assert calls == [('music_pause', MUSIC)]
# Resync on regain (ResyncAudioOnFocus=1): 300 ms after a skipped restore the
# per-frame update runs PauseAll + ResumeAll once, then the original update.
def update(rig, calls):
    calls.clear(); rig.invoke_address(rig.dllbase + syms['_kpc_sound_update'], (), this=SOUND); return list(calls)
assert b'Focus regain: kept sounds re-synchronised' in bytes(rig.logs)
session(rig, 4, 0x1234)
assert update(rig, calls) == [('update', SOUND)]                       # nothing pending
rig.tick = 50000; lose(rig, calls); regain(rig, calls)
rig.tick = 50100; assert update(rig, calls) == [('update', SOUND)]     # too early
rig.tick = 50400
assert update(rig, calls) == [('pauseall', SOUND), ('resumeall', SOUND), ('update', SOUND)]
assert update(rig, calls) == [('update', SOUND)]                       # once only
assert b'Focus regained: sounds re-synchronised to real time.' in bytes(rig.logs)
# A stock (unskipped) restore schedules nothing.
rig.u.mem_write(CVAR + 0x18, struct.pack('<f', 0.0))
lose(rig, calls); regain(rig, calls); rig.tick = 70000
assert update(rig, calls) == [('update', SOUND)]
# ResyncAudioOnFocus=0: the update call stays original.
rigr = Rig(settings={'Multiplayer.ResyncAudioOnFocus': 0}); rigr.factory(); assert not rigr.messages
assert bytes(rigr.u.mem_read(CLIENT + 0x13bcd, 5)) == bytes(rigr.initial_client[0x13bcd:0x13bd2])
assert b'Focus regain' not in bytes(rigr.logs)
# KeepRendererOnFocusLoss=0: restart and world tests stay original; the shutdown
# runs (music marked as a focus loss) while sounds and the local server keep running.
rigk = Rig(settings={'Multiplayer.KeepRendererOnFocusLoss': 0}); rigk.factory(); assert not rigk.messages
for site in (0x6c97, 0x158e3, 0x15922):
    assert bytes(rigk.u.mem_read(CLIENT + site, 5)) == bytes(rigk.initial_client[site:site + 5]), hex(site)
assert b'Focus loss: KeepRendererOnFocusLoss=0' in bytes(rigk.logs)
callsk = setup(rigk, {'exists': False, 'sets': []}); session(rigk, 4, 0x1234)
assert lose(rigk, callsk) == [('term', CM)]
# INI KeepAudioOnFocusLoss=0: hooks stay (the menu can turn it on), the console
# variable is created with "0" and everything is stock until it is set to 1.
rig0 = Rig(settings={'Multiplayer.KeepAudioOnFocusLoss': 0}); rig0.factory(); assert not rig0.messages
assert b'Focus loss: KeepAudioOnFocusLoss default=0x00000000' in bytes(rig0.logs)
cvar0 = {'exists': False, 'sets': []}
calls0 = setup(rig0, cvar0)
session(rig0, 4, 0x1234)
assert lose(rig0, calls0) == STOCK_LOSE and regain(rig0, calls0) == STOCK_REGAIN
assert cvar0['sets'] == [('KeepAudioOnFocusLoss', '0')], cvar0
rig0.u.mem_write(CVAR + 0x18, struct.pack('<f', 1.0))
assert lose(rig0, calls0) == KEPT_LOSE and regain(rig0, calls0) == []
# Setup > Audio checkbox: the Audio screen's vtable slots (cshell 0xfd264 +8..+0x18)
# wrap init/term/draw/mouse-move/event; an extra instance of the game's checkbox
# class (ctor 0xd060, images 0xd1b0; both stubbed with a recording vtable) is
# created below Sound Quality and receives the same calls.
for offset, name, original in ((0x08, '_kpc_audio_init', 0x7360), (0x0c, '_kpc_audio_term', 0x7cc0), (0x10, '_kpc_audio_draw', 0x7d00),
                               (0x14, '_kpc_audio_move', 0x7d40), (0x18, '_kpc_audio_event', 0x7e70)):
    assert read(rig.u, SHELLBASE + 0xfd264 + offset) == rig.dllbase + syms[name], name
    assert read(rig0.u, SHELLBASE + 0xfd264 + offset) == rig0.dllbase + syms[name], name
SCREEN, WVT, WCODE, MOUSE = 0x5c8000, 0x5c6000, 0x5c7000, 0x5c9000
box = {'at': None, 'hit': 0, 'label': 0}
def ret_stub(address, name, argbytes, result=lambda: 1, effect=None):
    def run(u, at, size, user):
        sp = u.reg_read(x.UC_X86_REG_ESP)
        calls.append((name, u.reg_read(x.UC_X86_REG_ECX)) + tuple(read(u, sp + 4 + 4 * i) for i in range(argbytes // 4)))
        if effect: effect(u)
        u.reg_write(x.UC_X86_REG_EAX, result()); u.reg_write(x.UC_X86_REG_EIP, read(u, sp)); u.reg_write(x.UC_X86_REG_ESP, sp + 4 + argbytes)
    rig.u.hook_add(uc.UC_HOOK_CODE, run, begin=address, end=address)
for slot, name, argbytes in ((0x00, 'w_dtor', 4), (0x08, 'w_init', 12), (0x0c, 'w_draw', 4), (0x24, 'w_hover', 8), (0x2c, 'w_event', 8)):
    rig.u.mem_write(WVT + slot, pack(WCODE + slot))
    ret_stub(WCODE + slot, name, argbytes)   # 0xd590 returns 1 for every click, hit or not
ret_stub(SHELLBASE + 0xd370, 'hit_box', 0, result=lambda: box['hit'])
ret_stub(SHELLBASE + 0xd400, 'hit_label', 0, result=lambda: box['label'])
def construct(u):
    box['at'] = u.reg_read(x.UC_X86_REG_ECX); u.mem_write(box['at'], pack(WVT)); u.mem_write(box['at'] + 0xf4, pack(0))
ret_stub(SHELLBASE + 0xd060, 'ctor', 0, result=lambda: box['at'], effect=construct)
ret_stub(SHELLBASE + 0xd1b0, 'images', 8)
for address, name, argbytes in ((0x7360, 's_init', 0), (0x7cc0, 's_term', 0), (0x7d00, 's_draw', 4), (0x7d40, 's_move', 0), (0x7e70, 's_event', 8)):
    ret_stub(SHELLBASE + address, name, argbytes)
rig.u.mem_write(SCREEN + 0x48, pack(0x111, 0x222, 0x333)); rig.u.mem_write(SCREEN + 0x58, pack(MOUSE)); rig.u.mem_write(SCREEN + 0x5c, b'\x01')
rig.u.mem_write(MOUSE + 4, struct.pack('<ff', 250.7, 400.2))
def screen(name, *a):
    calls.clear(); return rig.invoke_address(rig.dllbase + syms[name], a, this=SCREEN)
rig.u.mem_write(CVAR + 0x18, struct.pack('<f', 1.0))
assert screen('_kpc_audio_init') == 1
b = box['at']
assert calls == [('s_init', SCREEN), ('ctor', b), ('w_init', b, 0x111, 0x333, 0x222),   # [+0x48], [+0x50], [+0x4c] as 0x7512
                 ('images', b, SHELLBASE + 0x109de8, SHELLBASE + 0x109e04)], calls
assert read(rig.u, b + 0x28) == 0xbc and read(rig.u, b + 0x2c) == 0x100 - 40       # gap above Enable Sound FX
assert rig.cstr(b + 0x108) == 'Keep Running When Minimised' and rig.cstr(b + 0x40).startswith('Keep sound, music') and 'out of sync' in rig.cstr(b + 0x40)
assert bytes(rig.u.mem_read(b + 0x14c, 1)) == b'\x01'                               # on = 1 = lit image +0x100 (checked.pcx)
screen('_kpc_audio_draw', 0x7777); assert calls == [('s_draw', SCREEN, 0x7777), ('w_draw', b, 0x7777)], calls
screen('_kpc_audio_move'); assert calls == [('s_move', SCREEN), ('w_hover', b, 250, 400)], calls
# A click elsewhere reaches the stock widgets (RT10: the box's own handler says 1 for any click).
box['hit'] = 0; assert screen('_kpc_audio_event', 1, 5) == 1 and calls == [('hit_box', b), ('hit_label', b), ('s_event', SCREEN, 1, 5)], calls
box['hit'] = 1; assert screen('_kpc_audio_event', 1, 5) == 1 and calls == [('hit_box', b), ('w_event', b, 1, 5)], calls    # on the box
box['hit'], box['label'] = 0, 1; assert screen('_kpc_audio_event', 1, 5) == 1 and calls == [('hit_box', b), ('hit_label', b), ('w_event', b, 1, 5)], calls  # on its label
box['label'] = 0
screen('_kpc_audio_event', 0x0d, 0); assert calls == [('s_event', SCREEN, 0x0d, 0)], calls                 # keys: stock widgets only
# The widget's change callback (+0x150, cdecl(widget)) writes the console variable.
callback = read(rig.u, b + 0x150)
rig.u.mem_write(0x62000400, b'\x68' + pack(b) + b'\xe8' + struct.pack('<i', callback - (0x62000400 + 10)) + b'\x83\xc4\x04\xc3')
rig.u.mem_write(b + 0x14c, b'\x00'); cvar['sets'].clear(); rig.invoke_address(0x62000400, ())
assert cvar['sets'] == [('KeepAudioOnFocusLoss', '0')], cvar
rig.u.mem_write(b + 0x14c, b'\x01'); cvar['sets'].clear(); rig.invoke_address(0x62000400, ())
assert cvar['sets'] == [('KeepAudioOnFocusLoss', '1')], cvar
# Console change while the screen is open: the box follows on the next draw.
rig.u.mem_write(CVAR + 0x18, struct.pack('<f', 0.0)); screen('_kpc_audio_draw', 1)
assert bytes(rig.u.mem_read(b + 0x14c, 1)) == b'\x00'                               # off = unlit
# Screen inactive (+0x5c = 0): nothing is forwarded.
rig.u.mem_write(SCREEN + 0x5c, b'\x00')
screen('_kpc_audio_draw', 1); screen('_kpc_audio_move'); assert calls == [('s_move', SCREEN)], calls
rig.u.mem_write(SCREEN + 0x5c, b'\x01')
# Term releases the box (scalar destructor with flag 0: static storage); reopening recreates it.
screen('_kpc_audio_term'); assert calls == [('s_term', SCREEN), ('w_dtor', b, 0)], calls
screen('_kpc_audio_draw', 1); assert calls == [('s_draw', SCREEN, 1)], calls
screen('_kpc_audio_init'); assert [c[0] for c in calls] == ['s_init', 'ctor', 'w_init', 'images']
screen('_kpc_audio_init'); assert [c[0] for c in calls] == ['s_init', 'w_dtor', 'ctor', 'w_init', 'images']
print(args.fixtures.name, 'passed focus loss: every mode keeps sounds, music and the local server running and, in a world, the renderer and world (stock renderer restart outside a world), input released; KeepRendererOnFocusLoss=0; console variable off = stock; balanced pairs; resync on regain; INI default 0; Audio-menu checkbox')
