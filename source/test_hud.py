#!/usr/bin/env python3
"""Run exact original HUD code and linked HUD bridges with mocked surface APIs.

This is a CPU/ABI regression suite, not a Windows graphics or gameplay test.
"""
from fixture_layout import rva, italian, crt_address
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

import test_runtime

SITES = (
    (0x9f95b, 'ff50048b16', 'hud_panel_entry'),
    (0x9fa12, '8b1150ff5208', 'hud_post_entry'),
    (0xa0540, 'c204009090', 'hud_rect_entry'),
    (0xb27cb, 'ff9188010000', 'hud_solid_ecx'),
    (0xb28e7, 'ff9188010000', 'hud_solid_ecx'),
    (0xb2ac5, 'ff9090010000', 'hud_scale_eax'),
    (0xb2ca0, 'ff9090010000', 'hud_scale_eax'),
    (0xb2ef4, 'ff9290010000', 'hud_scale_edx'),
    (0xb3110, 'ff9290010000', 'hud_scale_edx'),
    (0xb349f, 'ff9290010000', 'hud_scale_edx'),
    (0xe2e6b, 'ff9188010000', 'hud_cross_ecx'),
    (0xe2f42, 'ff9088010000', 'hud_cross_eax'),
    (0xa1977, 'e864edffff', 'hud_carousel_entry'),
)
PANEL, CLIENT, VTABLE, API = 0x580000, 0x582000, 0x583000, 0x585000
ART, FONT, SCREEN, SOURCE = 0x586000, 0x590000, 0x599000, 0x59a000
RECT, SRCR, SCOPE, STRING = 0x5b0000, 0x5b0040, 0x5b0100, 0x5b0200
DRAW_STUB, DIM_STUB, SOLID_STUB, SCALE_STUB = 0x62001000, 0x62002000, 0x62002020, 0x62002040
STOP, STACK = 0x62000100, 0x418000
p32 = lambda n: struct.pack('<I', n & 0xffffffff)
f32 = lambda n: struct.unpack('<I', struct.pack('<f', n))[0]
rect_bytes = lambda v: struct.pack('<4i', *v)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--fixtures', type=Path, required=True)
    ap.add_argument('--symbols', type=Path, required=True)
    ap.add_argument('--dependency-dir', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--expose-hud-rig', action='store_true',help=argparse.SUPPRESS)
    args = ap.parse_args()
    saved = sys.argv
    sys.argv = [saved[0], '--fixtures', str(args.fixtures), '--symbols', str(args.symbols),
                '--dependency-dir', str(args.dependency_dir), '--platform', 'windows', '--expose-rig']
    try:
        Rig, dll, syms, uc, x, _ = test_runtime.main()
    finally:
        sys.argv = saved
    results = []
    sites = [(rva(at), before, bridge) for at,before,bridge in SITES]

    class H(Rig):
        def api(self,u,address,size,user):
            super().api(u,address,size,user)
            if self.stub_names.get(address)=='GetCurrentThreadId':
                u.reg_write(x.UC_X86_REG_EAX,getattr(self,'current_thread',1))

        def __init__(self, percent=200, **kw):
            settings = kw.pop('settings', {})
            settings['LockInputRate']=0
            settings['Cursor.ScalePercent']=100
            settings['Tooltip.ScalePercent']=100
            settings['HUD.ScalePercent'] = percent
            super().__init__(settings=settings, **kw)
            self.draws = []
            self.dims = (16, 16)
            self.factory()
            assert not self.messages, self.messages
            self.u.mem_write(DIM_STUB, b'\xc3')
            self.u.mem_write(SOLID_STUB, b'\xc3')
            self.u.mem_write(SCALE_STUB, b'\xc3')
            self.u.hook_add(uc.UC_HOOK_CODE, self.graphics, begin=DIM_STUB, end=SCALE_STUB)
            for offset, value in ((0x174, DIM_STUB), (0x188, SOLID_STUB), (0x190, SCALE_STUB)):
                self.u.mem_write(API + offset, p32(value))
            self.u.mem_write(self.shellbase + rva(0x1c8dfc), p32(API))

        def graphics(self, u, address, size, user):
            sp = u.reg_read(x.UC_X86_REG_ESP)
            if address == DIM_STUB:
                src, w, h = struct.unpack('<3I', u.mem_read(sp+4, 12))
                assert src == SOURCE
                u.mem_write(w, p32(self.dims[0])); u.mem_write(h, p32(self.dims[1]))
            elif address in (SOLID_STUB, SCALE_STUB):
                n = 6 if address == SOLID_STUB else 5
                a = struct.unpack('<'+'I'*n, u.mem_read(sp+4, n*4))
                if address == SOLID_STUB:
                    dst, src, sr, xx, yy, color = a
                    xx, yy = struct.unpack('<ii', p32(xx)+p32(yy))
                    src_rect = tuple(struct.unpack('<4i', u.mem_read(sr, 16))) if sr else None
                    wh = (src_rect[2]-src_rect[0], src_rect[3]-src_rect[1]) if sr else self.dims
                    dest_rect = (xx, yy, xx+wh[0], yy+wh[1])
                    kind = 'solid'
                else:
                    dst, src, dr, sr, color = a
                    dest_rect = tuple(struct.unpack('<4i', u.mem_read(dr, 16))) if dr else None
                    src_rect = tuple(struct.unpack('<4i', u.mem_read(sr, 16))) if sr else None
                    kind = 'scale'
                self.draws.append(dict(kind=kind, dst=dst, src=src, dr=dest_rect, sr=src_rect, color=color))
                u.reg_write(x.UC_X86_REG_EAX, 0x13572468)

        def w32(self, at, value): self.u.mem_write(at, p32(value))
        def r32(self, at): return struct.unpack('<I', self.u.mem_read(at, 4))[0]
        def symbol(self, name): return self.dllbase + syms['_'+name]
        def scope(self, ax=1280, ay=1440, active=True):
            self.u.mem_write(SCOPE, struct.pack('<6I', PANEL, SCREEN, 2560, 1440, ax, ay))
            self.w32(self.symbol('active_hud'), SCOPE if active else 0)
            self.w32(self.symbol('hud_thread'), 1)

        def setup_panel(self, width=2560, height=1440, flags=0x42, px=90, py=430):
            self.u.mem_write(PANEL, bytes(0x200))
            for at, value in ((PANEL, VTABLE), (PANEL+0x28, CLIENT), (PANEL+0x30, SCREEN),
                              (PANEL+0x10, px), (PANEL+0x14, py), (PANEL+0x18, flags),
                              (PANEL+0x3c, ART), (PANEL+0x2c, 0xaabbccdd),
                              (CLIENT+0x210, width), (CLIENT+0x20c, height),
                              (VTABLE+4, DRAW_STUB), (VTABLE+8, DRAW_STUB)):
                self.w32(at, value)
            self.u.mem_write(PANEL+0x1c, struct.pack('<ff', .25, .75))

        def code(self, actions):
            b = bytearray(b'\x56\x8b\xf1')  # preserve ESI, panel in ESI
            for target, argv, this, cleanup in actions:
                for a in reversed(argv): b.extend(b'\x68'+p32(a))
                b.extend(b'\xb9'+p32(this) if this is not None else b'\x8b\xce')
                b.extend(b'\xe8'+p32(target-(DRAW_STUB+len(b)+5)))
                if cleanup: b.extend(b'\x83\xc4'+bytes([cleanup]))
            b.extend(b'\x5e\xc2\x04\x00')
            self.u.mem_write(DRAW_STUB, bytes(b))

        def panel_call(self, post=False):
            return self.invoke_address(self.symbol('kpc_hud_panel_post' if post else 'kpc_hud_panel_draw'),
                                       [f32(.006)], this=PANEL)

        def cdecl_call(self, target, argv=(), registers=None):
            self.u.mem_write(STACK, p32(STOP)+b''.join(p32(a) for a in argv))
            self.u.reg_write(x.UC_X86_REG_ESP, STACK)
            preserve = ((x.UC_X86_REG_EBX, 0x11112222), (x.UC_X86_REG_ESI, 0x33334444),
                        (x.UC_X86_REG_EDI, 0x55556666), (x.UC_X86_REG_EBP, 0x77778888))
            for reg, value in preserve: self.u.reg_write(reg, value)
            for reg, value in (registers or {}).items(): self.u.reg_write(reg, value)
            self.u.emu_start(target, STOP, count=100000)
            assert self.u.reg_read(x.UC_X86_REG_EIP) == STOP
            assert self.u.reg_read(x.UC_X86_REG_ESP) == STACK+4
            for reg, value in preserve:
                assert self.u.reg_read(reg) == (registers or {}).get(reg, value)
            return self.u.reg_read(x.UC_X86_REG_EAX)

    if args.expose_hud_rig:return H,Rig,dll,syms,uc,x
    for value in (0, 100):
        r = H(percent=value)
        for site, before, _ in sites:
            size=len(bytes.fromhex(before))
            assert bytes(r.u.mem_read(r.shellbase+site,size)) == r.initial_shell[site:site+size]
        results.append(dict(test='off_no_HUD_edits', value=value))
    for value in (1,49,401,0xffffffff):
        r = Rig(settings={'HUD.ScalePercent': value}); r.factory()
        assert r.messages and r.client_bytes()==r.initial_client and r.shell_bytes()==r.initial_shell
        results.append(dict(test='invalid_scale_no_game_edits', value=value))

    # Expected-byte mismatch and failures within the HUD part of the transaction.
    for failure in ('bytes', 'protect', 'cache'):
        r = H(pending=True)
        site = r.shellbase+rva(0xb28e7)
        if failure == 'bytes':
            page=site&~4095; r.u.mem_protect(page,4096,7);r.u.mem_write(site,b'\x90');r.u.mem_protect(page,4096,5)
            initial = r.shell_bytes()
        else:
            initial = r.shell_bytes()
            stub = r.stub_address['VirtualProtect' if failure=='protect' else 'FlushInstructionCache']
            hit = [False]
            def fail(u,address,size,user):
                sp=u.reg_read(x.UC_X86_REG_ESP)
                a=struct.unpack('<4I' if failure=='protect' else '<3I',u.mem_read(sp+4,16 if failure=='protect' else 12))
                target=a[0] if failure=='protect' else a[1]
                if target==site and not hit[0]:
                    hit[0]=True;u.reg_write(x.UC_X86_REG_EAX,0)
            r.u.hook_add(uc.UC_HOOK_CODE,fail,begin=stub,end=stub)
        r.load_shell(); assert r.messages and r.shell_bytes()==initial
        assert r.r32(r.symbol('g_hud_shell'))==0 and r.pages==r.initial_protection
        results.append(dict(test='cshell_transaction_rollback', failure=failure))

    # Destination rectangles, including the original no-op helper's strict
    # all-register/flags/x87 preservation and ret-4 stack convention.
    for percent in (50,150,200,400):
        r=H(percent=percent); r.scope()
        source=(1100,1380,1160,1420);r.u.mem_write(RECT,rect_bytes(source))
        values={x.UC_X86_REG_EAX:0x12345678,x.UC_X86_REG_EBX:0x23456789,
                x.UC_X86_REG_ECX:PANEL,x.UC_X86_REG_EDX:0x3456789a,
                x.UC_X86_REG_ESI:0x456789ab,x.UC_X86_REG_EDI:0x56789abc,x.UC_X86_REG_EBP:0x6789abcd}
        for reg,value in values.items():r.u.reg_write(reg,value)
        r.u.mem_write(0x62000200,b'\xd9\xe8\xd9\xeb') # nonempty x87: fld1; fldpi
        r.u.emu_start(0x62000200,0x62000204,count=2)
        r.u.reg_write(x.UC_X86_REG_EFLAGS,0x202|0x45)
        r.u.reg_write(x.UC_X86_REG_FPCW,0x027f)
        r.u.mem_write(STACK,p32(STOP)+p32(RECT));r.u.reg_write(x.UC_X86_REG_ESP,STACK)
        before_flags=r.u.reg_read(x.UC_X86_REG_EFLAGS)
        fp_regs=(x.UC_X86_REG_FPCW,x.UC_X86_REG_FPSW,x.UC_X86_REG_FPTAG)+tuple(getattr(x,'UC_X86_REG_FP'+str(i)) for i in range(8))
        before_fp=[r.u.reg_read(reg) for reg in fp_regs]
        r.u.emu_start(r.shellbase+rva(0xa0540),STOP,count=100000)
        assert r.u.reg_read(x.UC_X86_REG_ESP)==STACK+8
        assert all(r.u.reg_read(reg)==value for reg,value in values.items())
        assert r.u.reg_read(x.UC_X86_REG_EFLAGS)==before_flags
        assert [r.u.reg_read(reg) for reg in fp_regs]==before_fp
        expected=tuple(anchor+int((v-anchor)*percent/100) for v,anchor in zip(source,(1280,1440,1280,1440)))
        assert tuple(struct.unpack('<4i',r.u.mem_read(RECT,16)))==expected
        results.append(dict(test='rectangle_bridge_ABI_and_scale', percent=percent))

    # Run the original panel bitmap helper and original glyph renderer together
    # through the new scope wrapper. Compare 1440p/200% with 720p/100% exactly.
    for flags in (0,8,0x42,0x30):
        captures=[]
        for percent,width,height in ((100,1280,720),(200,2560,1440)):
            r=H(percent=percent);r.setup_panel(width,height,flags)
            for at,value in ((ART+4,SOURCE),(ART+8,40),(ART+12,32),
                              (FONT+0x520,API),(FONT+0x528,SOURCE),(FONT+0x52c,0xffee00ff),
                              (FONT+0x534,12),(FONT+0x538,0),(FONT+0x514,4)):
                r.w32(at,value)
            for char in b'230':
                r.u.mem_write(FONT+0x104+char*2,struct.pack('<h',8))
                r.u.mem_write(FONT+0x304+char*2,struct.pack('<h',char*8))
            r.u.mem_write(STRING,b'230\0')
            # Original GetPosition decides per-panel anchoring at each resolution.
            r.invoke_address(r.shellbase+rva(0xa0110),[RECT,RECT+4],this=PANEL)
            xx,yy=struct.unpack('<ii',r.u.mem_read(RECT,8))
            r.code([(r.shellbase+rva(0xa0560),[xx,yy,0,0],None,0),
                    (r.shellbase+rva(0xb2810),[SCREEN,xx+7,yy+5,STRING,1000],FONT,0)])
            r.panel_call();assert len(r.draws)==4,r.draws
            assert r.r32(r.symbol('active_hud'))==0
            assert r.r32(CLIENT+0x210)==width and r.r32(CLIENT+0x20c)==height
            captures.append(r.draws)
        old,new=captures
        for a,b in zip(old,new):
            assert b['dr']==tuple(v*2 for v in a['dr']),(flags,a,b)
            assert (a['src'],a['sr'],a['color'])==(b['src'],b['sr'],b['color'])
        results.append(dict(test='original_bitmap_and_font_vs_720_baseline', flags=hex(flags), draws=new))

    # All seven reviewed font call sites retain their original cdecl arguments;
    # off-screen/menu draws are passthrough, with no changes to source UVs.
    for site,before,name in sites[3:10]:
        for active,destination in ((True,SCREEN),(False,SCREEN),(True,SCREEN+0x100)):
            r=H();r.scope(active=active)
            r.u.mem_write(RECT,rect_bytes((1100,1380,1110,1392)))
            r.u.mem_write(SRCR,rect_bytes((8,0,18,12)))
            solid='solid' in name
            argv=[destination,SOURCE,SRCR,1100,1380,0xabcdef01] if solid else [destination,SOURCE,RECT,SRCR,0xabcdef01]
            reg=x.UC_X86_REG_ECX if 'ecx' in name else x.UC_X86_REG_EAX if 'eax' in name else x.UC_X86_REG_EDX
            # Finish immediately after the six-byte patched call; caller cleans args.
            r.u.mem_write(STACK,b''.join(p32(v) for v in argv));r.u.reg_write(x.UC_X86_REG_ESP,STACK)
            r.u.reg_write(reg,API);r.u.emu_start(r.shellbase+site,r.shellbase+site+6,count=100000)
            assert r.u.reg_read(x.UC_X86_REG_ESP)==STACK
            assert r.u.reg_read(x.UC_X86_REG_EAX)==0x13572468
            a=r.draws[-1];scaled=active and destination==SCREEN
            assert a['dr']==((920,1320,940,1344) if scaled else (1100,1380,1110,1392)),(name,a)
            assert a['sr']==(8,0,18,12) and a['color']==0xabcdef01
        results.append(dict(test='font_call_ABI_scope_and_passthrough', site=hex(site)))

    for site,before,name in sites[10:12]:
        for custom in (False,True):
            r=H();obj=CLIENT
            for at,value in ((obj+0x1f8,SCREEN),(obj+0x204,2560),(obj+0x208,1440)):
                r.w32(at,value)
            cx,cy=(1500,800) if custom else (1280,720)
            r.w32(r.shellbase+rva(0x1d89f4),f32(1 if custom else 0))
            r.w32(r.shellbase+rva(0x1d89f0),f32(cx));r.w32(r.shellbase+rva(0x1d89ec),f32(cy))
            argv=[SCREEN,SOURCE,0,cx-8,cy-8,0x12345678]
            r.u.mem_write(STACK,b''.join(p32(v) for v in argv));r.u.reg_write(x.UC_X86_REG_ESP,STACK)
            r.u.reg_write(x.UC_X86_REG_EBP,obj)
            r.u.reg_write(x.UC_X86_REG_ECX if 'ecx' in name else x.UC_X86_REG_EAX,API)
            r.u.emu_start(r.shellbase+site,r.shellbase+site+6,count=100000)
            assert r.u.reg_read(x.UC_X86_REG_ESP)==STACK and r.u.reg_read(x.UC_X86_REG_EBP)==obj
            assert r.draws[-1]['dr']==(cx-16,cy-16,cx+16,cy+16),r.draws
        results.append(dict(test='crosshair_call_ABI_center_and_custom', site=hex(site)))

    # An active scope owned by another thread must not scale unrelated draws.
    r=H();r.scope();r.current_thread=2;r.u.mem_write(SRCR,rect_bytes((0,0,10,12)))
    r.cdecl_call(r.symbol('kpc_hud_solid'),[API,SCREEN,SOURCE,SRCR,1100,1380,7])
    assert r.draws[-1]['kind']=='solid' and r.draws[-1]['dr']==(1100,1380,1110,1392)
    results.append(dict(test='other_thread_passthrough'))

    # Nested panel draws restore their enclosing HUD scope, including invalid
    # inner panels, and restore null after the outermost panel returns.
    for invalid in (False,True):
        r=H();r.setup_panel();inner=PANEL+0x400;inner_vt=VTABLE+0x100;inner_code=DRAW_STUB+0x300
        r.u.mem_write(inner,bytes(r.u.mem_read(PANEL,0x200)))
        r.w32(inner,inner_vt);r.w32(inner+0x18,8);r.w32(inner_vt+4,inner_code)
        if invalid:r.w32(inner+0x28,0)
        r.u.mem_write(inner_code,b'\xc2\x04\x00')
        r.u.mem_write(RECT,rect_bytes((1100,1380,1110,1392)))
        r.code([(r.symbol('kpc_hud_panel_draw'),[f32(.006)],inner,0),
                (r.symbol('kpc_hud_adjust_rect'),[PANEL,RECT],None,8)])
        r.panel_call()
        assert tuple(struct.unpack('<4i',r.u.mem_read(RECT,16)))==(920,1320,940,1344)
        assert r.r32(r.symbol('active_hud'))==0
        results.append(dict(test='nested_panel_scope_restored',invalid_inner=invalid))

    # Original panel-manager call sites must preserve the caller's EDX/EAX and
    # stack layout after replacing call+following instruction with a near call.
    for site,post in ((rva(0x9f95b),False),(rva(0x9fa12),True)):
        r=H();r.setup_panel();r.code([])
        r.u.mem_write(STACK,p32(f32(.006))+bytes(64));r.w32(STACK+(0x10 if post else 0x14),0x12345678)
        r.u.reg_write(x.UC_X86_REG_ESP,STACK);r.u.reg_write(x.UC_X86_REG_ECX,PANEL)
        r.u.reg_write(x.UC_X86_REG_ESI,PANEL);r.u.reg_write(x.UC_X86_REG_EBP,0x778899aa)
        if post:r.u.reg_write(x.UC_X86_REG_EAX,f32(.006))
        # Post hook leaves the original rva(0x9fa18) MOV intact and executable.
        r.u.emu_start(r.shellbase+site,r.shellbase+rva(0x9fa1c) if post else r.shellbase+site+5,count=100000)
        assert r.u.reg_read(x.UC_X86_REG_ESP)==STACK+(0 if post else 4)
        if post:assert bytes(r.u.mem_read(r.shellbase+rva(0x9fa18),4))==bytes.fromhex('8b442410')
        assert r.u.reg_read(x.UC_X86_REG_EAX if post else x.UC_X86_REG_EDX)==(0x12345678 if post else VTABLE)
        assert r.u.reg_read(x.UC_X86_REG_EBP)==0x778899aa
        assert r.r32(r.symbol('active_hud'))==0
        results.append(dict(test='original_panel_manager_bridge_ABI', post=post))

    report=dict(status='pass', dll_sha256=hashlib.sha256((Path(__file__).resolve().parents[1]/'dinput.dll').read_bytes()).hexdigest(),
                groups=len(results), unicorn_version=uc.__version__,results=results,
                scope='Exact original HUD bitmap/font code and actual linked PE32 hooks under CPU emulation. Synthetic HUD objects and mocked Win32/surface APIs. No native Windows gameplay or visual validation.')
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS:',len(results),'HUD integration groups',flush=True)


if __name__=='__main__': main()
