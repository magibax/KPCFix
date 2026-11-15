#!/usr/bin/env python3
"""Execute the complete original HUD loop to reproduce or reject the v1 bug."""
from fixture_layout import rva, italian, crt_address
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys

p32=lambda n:struct.pack('<I',n&0xffffffff)
f32=lambda n:struct.unpack('<I',struct.pack('<f',n))[0]


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--package',type=Path,required=True)
    ap.add_argument('--symbols',type=Path,required=True)
    ap.add_argument('--fixtures',type=Path,required=True)
    ap.add_argument('--dependency-dir',type=Path,required=True)
    ap.add_argument('--expect',choices=('broken','fixed'),required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();root=args.package.resolve()
    sys.path.insert(0,str(root/'source'))
    spec=importlib.util.spec_from_file_location('loop_test_runtime',root/'source/test_runtime.py')
    mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    saved=sys.argv;sys.argv=[saved[0],'--fixtures',str(args.fixtures),'--symbols',str(args.symbols),
                           '--dependency-dir',str(args.dependency_dir),'--platform','windows','--expose-rig']
    try:Rig,dll,syms,uc,x,_=mod.main()
    finally:sys.argv=saved
    rows=[]
    patterns={
        'all_empty':['empty']*12,
        'all_visible':['visible']*12,
        'mixed':['empty','visible','hidden','not_drawable','visible','empty']*2,
        'hidden':['hidden']*12,
        'not_drawable':['not_drawable']*12,
        'fading_visible':['fading']*12,
    }
    for scale in (100,200):
      for db,sb in ((0x68000000,0x20000000),(0x03000000,0x10000000)):
       for name,pattern in patterns.items():
        r=Rig(dllbase=db,shellbase=sb,settings={'HUD.ScalePercent':scale,'Cursor.ScalePercent':100,'Tooltip.ScalePercent':100,'LockInputRate':0})
        r.factory();assert not r.messages,r.messages
        u=r.u;manager=0x580000;context=0x581000;vtable=0x582000
        screen=0x599000;draw=0x62001000;draws=[];visits=[]
        u.mem_write(manager,bytes(0x100));u.mem_write(manager+0x34,p32(context))
        u.mem_write(manager+0x39,b'\x01')
        for at,value in ((context+0x1f8,screen),(context+0x210,2560),(context+0x20c,1440),(vtable+8,draw)):
            u.mem_write(at,p32(value))
        u.mem_write(draw,b'\xc2\x04\x00')
        expected=[]
        for i,kind in enumerate(pattern):
            panel=0x584000+i*0x200
            if kind=='empty':continue
            u.mem_write(manager+4+i*4,p32(panel));u.mem_write(panel,p32(vtable))
            u.mem_write(panel+0x28,p32(context))
            u.mem_write(panel+0x44,b'\x00' if kind=='not_drawable' else b'\x01')
            u.mem_write(panel+0x45,b'\x01' if kind in ('visible','not_drawable') else b'\x00')
            u.mem_write(panel+0x50,p32(1 if kind=='fading' else 0))
            if kind in ('visible','fading'):expected.append(panel)
        def called(cpu,address,size,user):
            panel=cpu.reg_read(x.UC_X86_REG_ECX);sp=cpu.reg_read(x.UC_X86_REG_ESP)
            assert struct.unpack('<I',cpu.mem_read(sp+4,4))[0]==f32(.006)
            assert struct.unpack('<3I',cpu.mem_read(panel+0x30,12))==(screen,2560,1440)
            draws.append(panel)
        def entered(cpu,address,size,user):visits.append(cpu.reg_read(x.UC_X86_REG_ESI))
        u.hook_add(uc.UC_HOOK_CODE,called,begin=draw,end=draw)
        u.hook_add(uc.UC_HOOK_CODE,entered,begin=sb+rva(0x9f9e0),end=sb+rva(0x9f9e0))
        exception=None
        try:r.invoke_address(sb+rva(0x9f9b0),[f32(.006)],this=manager,limit=100000)
        except (uc.UcError,AssertionError) as error:exception=str(error)
        passed=exception is None and draws==expected and len(visits)==12
        should_fail=args.expect=='broken' and scale==200 and name in ('all_empty','mixed','hidden','not_drawable')
        if should_fail:assert not passed,(name,'v1 defect was not reproduced')
        else:assert passed,(scale,hex(db),name,exception,len(visits),draws,expected)
        rows.append({'scale':scale,'dll_base':hex(db),'shell_base':hex(sb),'pattern':name,
                     'result':'reproduced_v1_failure' if not passed else 'pass',
                     'loop_visits':len(visits),'draw_calls':len(draws),'error':exception,
                     'eip':hex(u.reg_read(x.UC_X86_REG_EIP))})
    report={'status':'pass','expected':args.expect,'dll_sha256':hashlib.sha256((root/'dinput.dll').read_bytes()).hexdigest(),
            'groups':len(rows),'reproduced_failures':sum(v['result']!='pass' for v in rows),'results':rows,
            'scope':'Full original cshell+rva(0x9f9b0) loop, original branches and linked DLL callbacks under CPU emulation. Synthetic child panels, mocked Win32 and final draw methods; not native gameplay.'}
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS:',len(rows),'full HUD-loop cases;',report['reproduced_failures'],'expected v1 failures',flush=True)


if __name__=='__main__':main()
