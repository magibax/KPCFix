#!/usr/bin/env python3
"""Compare original 3D projection + count/font paths at independent HUD scales."""
from fixture_layout import rva, italian, crt_address
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

import test_hud

p32=lambda v:struct.pack('<I',v&0xffffffff)
f32=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
PANEL,API,SCREEN,SOURCE=0x580000,0x585000,0x599000,0x59a000
CAMERA,RECT,XY,TEMP,RECORD,NODE=0x5b1000,0x5b1100,0x5b1200,0x5b1300,0x5b1400,0x5b1500
SCOPE,STACK=0x5b0100,0x418000


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--package',type=Path,required=True)
    ap.add_argument('--symbols',type=Path,required=True)
    ap.add_argument('--fixtures',type=Path,required=True)
    ap.add_argument('--dependency-dir',type=Path,required=True)
    ap.add_argument('--expect',choices=('broken','fixed'),required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args()
    # Use the same synthetic services with the actual selected v2/v3 DLL.
    test_hud.test_runtime.ROOT=args.package.resolve()
    saved=sys.argv;sys.argv=[saved[0],'--fixtures',str(args.fixtures),'--symbols',str(args.symbols),
      '--dependency-dir',str(args.dependency_dir),'--output',str(args.output),'--expose-hud-rig']
    try:H,Rig,dll,syms,uc,x=test_hud.main()
    finally:sys.argv=saved
    results=[]
    for db,sb in ((0x68000000,0x20000000),(0x03000000,0x10000000)):
      baseline=[]
      for percent in (100,150,200,300):
        r=H(percent=percent,dllbase=db,shellbase=sb);r.setup_panel(flags=0x40)
        r.scope(ax=1280,ay=0)
        font=sb+rva(0x1c13a0)+0x53c
        r.u.mem_write(font,bytes(0x53c))
        for at,value in ((font+0x520,API),(font+0x528,SOURCE),(font+0x52c,0xffffffff),
                         (font+0x534,12),(font+0x538,0),(font+0x514,4)):
            r.w32(at,value)
        for c in range(ord('0'),ord('9')+1):
            r.u.mem_write(font+0x104+c*2,struct.pack('<h',8))
            r.u.mem_write(font+0x304+c*2,struct.pack('<h',(c-ord('0'))*8))
        def snprintf(cpu,address,size,user):
            sp=cpu.reg_read(x.UC_X86_REG_ESP)
            ret,dest,capacity,fmt,va=struct.unpack('<5I',cpu.mem_read(sp,20))
            assert r.cstr(fmt) in ('%d','%i','%u'),r.cstr(fmt)
            value=struct.unpack('<i',cpu.mem_read(va,4))[0]
            text=str(value).encode();assert len(text)<capacity
            cpu.mem_write(dest,text+b'\0');cpu.reg_write(x.UC_X86_REG_EAX,len(text))
            cpu.reg_write(x.UC_X86_REG_ECX,0x12341234);cpu.reg_write(x.UC_X86_REG_EDX,0x56785678)
            cpu.reg_write(x.UC_X86_REG_ESP,sp+4);cpu.reg_write(x.UC_X86_REG_EIP,ret)
        r.u.hook_add(uc.UC_HOOK_CODE,snprintf,begin=crt_address(r,0xe5d7c),end=crt_address(r,0xe5d7c))
        native=(1155,0,1405,73)
        r.u.mem_write(RECT,struct.pack('<4i',*native))
        # Actual original no-op/replaced rectangle helper, not a model transform.
        r.invoke_address(sb+rva(0xa0540),[RECT],this=PANEL)
        left,top,right,bottom=struct.unpack('<4i',r.u.mem_read(RECT,16))
        for at,value in ((CAMERA+0x60,left),(CAMERA+0x64,top),
                         (CAMERA+0x70,right-left),(CAMERA+0x74,bottom-top),
                         (CAMERA+0x78,f32(.7853981634)),(CAMERA+0x7c,f32(.3))):r.w32(at,value)
        r.u.mem_write(CAMERA+0x80,struct.pack('<12f',1,0,0,0,0,1,0,0,0,0,1,0))
        r.invoke_address(sb+rva(0xa78d0),this=CAMERA)
        if percent==100:baseline=[]
        for idx,(point,quantity) in enumerate((((-80,15,1000),22),((0,20,1000),20),((90,5,900),180))):
            r.invoke_address(sb+rva(0xa79d0),[*(f32(v) for v in point),XY,XY+4,TEMP],this=CAMERA)
            projected=struct.unpack('<2f',r.u.mem_read(XY,8))
            r.u.mem_write(RECORD+4,struct.pack('<2f',*projected));r.w32(NODE+8,quantity)
            r.draws=[]
            # Execute actual original float conversion, +20 gap, five arguments,
            # call site, number helper, formatting wrapper and bitmap glyph code.
            for reg,value in ((x.UC_X86_REG_ESP,STACK),(x.UC_X86_REG_ECX,NODE),
                              (x.UC_X86_REG_ESI,PANEL),(x.UC_X86_REG_EDI,RECORD),
                              (x.UC_X86_REG_EBX,0x100),(x.UC_X86_REG_EBP,0x778899aa)):
                r.u.reg_write(reg,value)
            r.u.emu_start(sb+rva(0xa1958),sb+rva(0xa197c),count=100000)
            assert r.u.reg_read(x.UC_X86_REG_EIP)==sb+rva(0xa197c)
            assert r.u.reg_read(x.UC_X86_REG_ESP)==STACK
            for reg,value in ((x.UC_X86_REG_ESI,PANEL),(x.UC_X86_REG_EDI,RECORD),
                              (x.UC_X86_REG_EBX,0x100),(x.UC_X86_REG_EBP,0x778899aa)):
                assert r.u.reg_read(reg)==value
            assert r.r32(r.symbol('active_hud'))==SCOPE
            assert len(r.draws)==len(str(quantity)),r.draws
            rects=[list(d['dr']) for d in r.draws]
            if percent==100:
                baseline.append(rects);error=0
            else:
                expected=[[a+int((v-a)*percent/100) for v,a in zip(row,(1280,0,1280,0))] for row in baseline[idx]]
                error=max(abs(v-w) for row,ref in zip(rects,expected) for v,w in zip(row,ref))
                if args.expect=='fixed':assert error<=2,(percent,quantity,projected,rects,expected,error)
                else:assert error>10,(percent,quantity,'v2 misalignment not reproduced',error)
            results.append(dict(scale=percent,dll_base=hex(db),quantity=quantity,projected=list(projected),
                                glyph_rects=rects,maximum_baseline_error_pixels=error))
    report={'status':'pass','expected':args.expect,'dll_sha256':hashlib.sha256((args.package/'dinput.dll').read_bytes()).hexdigest(),
            'cases':len(results),'results':results,
            'scope':'Original camera projection, count conversion/+20 call block, number helper and bitmap-font paths with the selected linked DLL. Mock snprintf and surface APIs. Integer projection rounding permits two pixels of baseline error. No native Windows visual test.'}
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS:',len(results),'carousel projection/count/font cases;',args.expect,flush=True)


if __name__=='__main__':main()
