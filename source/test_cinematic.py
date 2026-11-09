"""Execute the original client call site against a simulated DirectDraw surface."""
import struct
import sys
import test_runtime

sys.argv += ['--expose-rig']
Rig, _, _, uc, x, args = test_runtime.main()
pack = lambda *v: struct.pack('<'+'I'*len(v),*v)
surface, table, stub, rect, stack = 0x578000,0x578100,0x60006000,0x57a000,0x418400
start, tail = 0x1d1b08b,0x1d1b16d
for enabled in (True,False):
    rig=Rig(settings={} if enabled else {'AspectRatio.Cinematics4x3':0})
    rig.factory()
    assert not rig.messages,rig.messages
    u=rig.u
    assert u.mem_read(start,1)==(b'\xe9' if enabled else b'\x55')
    u.mem_write(surface,pack(table));u.mem_write(table+20,pack(stub));u.mem_write(stub,b'\xc2\x18\x00')
    calls=[]
    def capture(u,at,size,user):
        sp=u.reg_read(x.UC_X86_REG_ESP)
        dst,r,src,sr,flags,fx=struct.unpack('<6I',u.mem_read(sp+4,24))
        assert dst==surface
        if not src:
            assert flags==0x01000400 and u.mem_read(fx,4)==pack(100) and u.mem_read(fx+80,4)==pack(0)
        else:
            assert src==0x1234 and (sr,flags,fx)==(0,0x1000000,0)
        calls.append((src,struct.unpack('<4i',u.mem_read(r,16))))
        u.reg_write(x.UC_X86_REG_EAX,1 if fail and not src else 0)
    u.hook_add(uc.UC_HOOK_CODE,capture,begin=stub,end=stub)
    for full,fit in [((0,0,1920,1080),(240,0,1680,1080)),((0,0,2560,1440),(320,0,2240,1440)),((0,0,640,480),(0,0,640,480)),((10,10,1290,1034),(10,42,1290,1002))]:
        for fail in (False,True):
            calls.clear()
            u.mem_write(rect,pack(*full));u.mem_write(stack,pack(rect,0x1234,0,0x1000000,0))
            u.reg_write(x.UC_X86_REG_EAX,table);u.reg_write(x.UC_X86_REG_EBP,surface);u.reg_write(x.UC_X86_REG_ESP,stack)
            u.emu_start(start,tail,count=1000)
            assert u.reg_read(x.UC_X86_REG_EIP)==tail and u.reg_read(x.UC_X86_REG_ESP)==stack+20
            assert calls==([(0,full),(0x1234,full if fail else fit)] if enabled and fit!=full else [(0x1234,full)])
print(args.fixtures.name,'passed fullscreen geometry, original mode, fill failure and COM stack checks')
