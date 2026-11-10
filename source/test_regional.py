"""Exercise a real regional 1.13 pair, the stock mouse switch and the version gate."""
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path
import test_runtime
from fixture_layout import rva

def run():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fixtures', 'originals', 'symbols', 'dependency-dir', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    saved = sys.argv
    sys.argv = [saved[0], '--fixtures', str(args.fixtures), '--symbols', str(args.symbols),
                '--dependency-dir', str(args.dependency_dir), '--expose-rig']
    try:
        Rig, dll, symbols, uc, x, _ = test_runtime.main()
    finally:
        sys.argv = saved
    rows = []
    def word(cpu, address):
        return struct.unpack('<I',cpu.mem_read(address,4))[0]
    def hook_target(cpu,address):
        return (address+5+struct.unpack('<i',cpu.mem_read(address+1,4))[0]) & 0xffffffff
    for dll_base,shell_base in ((0x68000000,0x20000000),(0x03000000,0x10000000)):
        rig=Rig(dllbase=dll_base,shellbase=shell_base,pending=True,settings={
            'HUD.ScalePercent':150, 'Multiplayer.EmptyBindIPFix':1})
        rig.factory()
        assert not rig.messages,rig.messages
        assert rig.load_shell()==shell_base
        assert not rig.messages and rig.invoke('KPCFixStatus')&0x87==7,rig.messages
        assert b'13 checked HUD call sites' in rig.logs
        assert rig.cstr(rig.invoke('KPCFixVersion')).startswith('KPCFix 1.13 for Windows')
        # Decode the target of each installed relative CALL or JMP.
        from test_hud import SITES
        for english,before,bridge in SITES:
            address=shell_base+rva(english)
            assert rig.u.mem_read(address,1)==(b'\xe9' if bridge=='hud_rect_entry' else b'\xe8')
            assert hook_target(rig.u,address)==dll_base+symbols['_'+bridge]
        assert hook_target(rig.u,0x1d172e2)==dll_base+symbols['_kpc_bind_ip']
        for i in range(8):
            assert [rig.timing_call(i,.25) for _ in range(4)]==[0,0,0,1]
        for event,expected in ((9998,1),(10000,1),(10006,6)):
            assert rig.interval(10000,event,0,6,0)==expected
        assert rig.physics()==[(17.0,-82.03125,200.0)]
        for params in ({'ground':0},{'jump':4},{'mode':1},{'vy':2}):
            assert rig.physics(**params)[0][1]>-82
        for position in (0,1,10,50,100):
            value,back=rig.sensitivity(position)
            assert abs(value-max(.01,position*.02))<1e-5,(position,value)
            assert abs(back-max(.5,position))<1e-4,(position,back)
        writes=rig.vprotect_calls
        rig.load_shell()
        assert rig.vprotect_calls==writes and rig.pages==rig.initial_protection
        rows.append({'mode':'full','shell_base':hex(shell_base),'status':'pass'})
        stock=Rig(dllbase=dll_base,shellbase=shell_base,settings={
            'HUD.ScalePercent':150,'StockMouse':1,'Multiplayer.EmptyBindIPFix':1})
        stock.factory()
        assert not stock.messages and stock.invoke('KPCFixStatus')&0x87==7
        for offset,size in ((0x33c91,6),(0x342c3,6),(0x344a5,10),(0x1aa0e,5),(0x1ac90,5)):
            assert stock.u.mem_read(0x1ca0000+offset,size)==stock.initial_client[offset:offset+size]
        for english,size in ((0x24660,6),(0x2466a,6),(0x24677,8),(0x24bd2,6),(0xdb800,6),
                           (0x2508b,5),(0xfd824,4),(0x24e44,7),(0x24ef3,7),(0x24f76,7),(0x2501a,7)):
            at=rva(english);assert stock.u.mem_read(shell_base+at,size)==stock.initial_shell[at:at+size]
        assert 'FindFirstFileW' not in stock.calls
        assert hook_target(stock.u,0x1d172e2)==dll_base+symbols['_kpc_bind_ip']
        assert hook_target(stock.u,shell_base+rva(0x9f95b))==dll_base+symbols['_hud_panel_entry']
        value,back=stock.sensitivity(50)
        assert abs(value-10)<1e-4 and abs(back-50)<1e-4
        rows.append({'mode':'stock mouse','shell_base':hex(shell_base),'status':'pass'})
    for source in ('client.exe','cshell.dll','both'):
        rejected=Rig()
        for filename in ('client.exe','cshell.dll'):
            if source in (filename,'both'):
                rejected.file_data['C:\\Games\\Psycho\\'+filename]=(args.originals/filename).read_bytes()
        rejected.factory()
        assert rejected.invoke('KPCFixStatus') & 128 and rejected.messages
        assert rejected.client_bytes()==rejected.initial_client and rejected.shell_bytes()==rejected.initial_shell
        assert 'FindFirstFileW' not in rejected.calls
        rows.append({'disc_file_rejected':source,'status':'pass'})
    # A modified instruction at a required shell site must abort that module's edits.
    rejected=Rig(pending=True,settings={'HUD.ScalePercent':150})
    rejected.factory()
    rejected.u.mem_write(rejected.shellbase+rva(0x9f95b),b'\x90')
    before=rejected.shell_bytes()
    rejected.load_shell()
    assert rejected.messages and rejected.shell_bytes()==before
    rows.append({'memory_mismatch':'no shell edits','status':'pass'})
    args.output.write_text(json.dumps({'status':'pass','region':args.fixtures.name,
        'dll_sha256':hashlib.sha256(dll.data).hexdigest(),'results':rows},indent=2)+'\n')
    print('PASS:',args.fixtures.name, len(rows),'regional cases')

if __name__=='__main__':run()
