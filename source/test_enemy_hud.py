"""Exercise the installed enemy bridges with synthetic screen and atlas surfaces."""
import struct
import sys
from pathlib import Path
import test_hud as hud

region = Path(sys.argv[sys.argv.index('--fixtures') + 1]).name
layout = {'Europe': (0xb8dc0, 0xcdb48), 'USA': (0xb8dc0, 0xcdb48),
          'Germany': (0xb8d70, 0xcdaf8), 'Italy-Covermount': (0xb8d90, 0xcdba8)}
draw, caller = layout[region]
H, _, _, _, uc, x = hud.main()
for percent in (100, 150, 200):
    rig = H(percent=percent)
    base = rig.shellbase
    if percent == 100:
        assert bytes(rig.u.mem_read(base+caller,5)) == rig.initial_shell[caller:caller+5]
        continue
    def target(at):
        instruction = bytes(rig.u.mem_read(base + at, 5))
        assert instruction[0] == 0xe8
        return base + at + 5 + struct.unpack('<i', instruction[1:])[0]
    assert target(caller) == rig.symbol('kpc_enemy_draw')
    code = bytearray()
    def invoke(address, args):
        for arg in reversed(args):
            code.extend(b'\x68' + hud.p32(arg))
        code.extend(b'\xe8' + hud.p32(address - (base + draw + len(code) + 5)))
        code.extend(b'\x83\xc4' + bytes([len(args)*4]))
    for offset in [0x178,0x1f5,0x27b,0x2ff,0x3fb,0x496]:
        invoke(target(draw+offset), [hud.SCREEN,hud.SOURCE,hud.RECT,hud.SRCR])
    # Call the same scoped glyph handler used by the original font path.
    invoke(rig.symbol('kpc_hud_solid'), [hud.API,hud.SCREEN,hud.SOURCE,hud.SRCR,1850,108,0])
    rig.u.mem_write(base+draw, bytes(code)+b'\xc3')
    dim, opaque = hud.SCALE_STUB+128, hud.SCALE_STUB+144
    rig.w32(hud.API+0x174,dim)
    rig.w32(hud.API+0x18c,opaque)
    rig.u.mem_write(dim,b'\xc3')
    rig.u.mem_write(opaque,b'\xc3')
    rig.u.mem_write(hud.RECT,hud.rect_bytes((1820,100,1870,116)))
    rig.u.mem_write(hud.SRCR,hud.rect_bytes((0,0,8,12)))
    draws=[]
    def api(u,addr,size,user):
        sp=u.reg_read(x.UC_X86_REG_ESP)
        if addr==dim:
            dst,w,h=struct.unpack('<3I',u.mem_read(sp+4,12))
            assert dst==hud.SCREEN
            rig.w32(w,1920);rig.w32(h,1080)
        elif addr==opaque:
            dst,src,rect,crop=struct.unpack('<4I',u.mem_read(sp+4,16))
            assert (dst,src,crop)==(hud.SCREEN,hud.SOURCE,hud.SRCR)
            draws.append(struct.unpack('<4i',u.mem_read(rect,16)))
    rig.u.hook_add(uc.UC_HOOK_CODE,api,begin=dim,end=opaque)
    rig.cdecl_call(target(caller))
    def scale(rect):
        return tuple(a+int((v-a)*percent/100) for v,a in zip(rect,(1920,0,1920,0)))
    assert draws == [scale((1820,100,1870,116))]*6
    assert rig.draws[-1]['dr']==scale((1850,108,1858,120))
    assert rig.r32(rig.symbol('active_hud'))==0
print(region, 'passed stock, 150%, 200%, crop, glyph, scope and calling convention checks')
