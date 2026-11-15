"""Exercise movie-only stretch/fallback against mocked surface and GDI methods."""
import struct,sys
import test_runtime
sys.argv+=['--expose-rig']
Rig,_,syms,uc,x,args=test_runtime.main()
r=Rig();r.factory();assert not r.messages,r.messages
u=r.u
dst,src=0x578000,0x578100
table=0x578200
stub=0x60006000
def put(p,*a):u.mem_write(p,struct.pack('<'+'I'*len(a),*a))
for p in (dst,src):put(p,table)
for slot,index in ((5,0),(17,1),(26,2)):put(table+slot*4,stub+index*16)
for index,name in enumerate(('set_mode','set_origin','stretch'),3):
    put(r.dllbase+syms['_'+name],stub+index*16)
put(r.dllbase+syms['_initialized'],1)
events=[]
for i,n in enumerate((6,2,2,2,4,11)):
    u.mem_write(stub+i*16,b'\xc2'+struct.pack('<H',4*n))
def cb(u,at,size,_):
    i=(at-stub)//16;n=(6,2,2,2,4,11)[i]
    a=struct.unpack('<'+'I'*n,u.mem_read(u.reg_read(x.UC_X86_REG_ESP)+4,4*n))
    events.append((i,a));ret=0
    if i==1:
        if a[0]==dst and fail:ret=1
        else:put(a[1],a[0]+4)
    if i==3:ret=1
    if i==4:
        if a[3]:put(a[3],0,0)
        ret=1
    if i==5:
        assert a==(dst+4,320,0,1920,1440,src+4,0,0,640,480,0xcc0020)
        ret=1
    u.reg_write(x.UC_X86_REG_EAX,ret)
for i in range(6):u.hook_add(uc.UC_HOOK_CODE,cb,begin=stub+i*16,end=stub+i*16)
put(0x57a000,0,0,2560,1440);put(0x57a100,0,0,640,480)
for fail in (False,True):
    events.clear()
    r.invoke_address(r.dllbase+syms['_kpc_movie_blt'],(dst,0x57a000,src,0x57a100,0x1000000,0))
    assert [i for i,a in events]==([0,1,1,3,4,5,4,3,2,2] if not fail else [0,1,1,2,0]),events
print('Movie stretch rectangle, release order and DirectDraw fallback passed.')
