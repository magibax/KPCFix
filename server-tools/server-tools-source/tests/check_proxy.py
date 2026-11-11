"""Run the compiled proxy with mapped original images and mocked Win32 APIs.
Usage: python check_proxy.py FIXTURE_DIRECTORY [--cheats] [--off] [--bad-object] [--coop-session]
SHA compression is accelerated with hashlib in this emulator; the same C SHA
implementation is separately tested natively by the build validation workflow.
"""
from pathlib import Path
import hashlib,json,struct,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
root=Path(__file__).resolve().parents[2];fixtures=Path(sys.argv[1]);report=json.loads((root/'SERVER_TOOLS_BUILD.json').read_text())
u=Uc(UC_ARCH_X86,UC_MODE_32)
proxy=0x4e000000;obj=0x02f00000;engine=0x10000000;exe=0x400000;api=0x70000000;scratch=0x30000000
u.mem_map(scratch,0x200000);u.mem_map(api,0x10000)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def write(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
def cstr(a):
 b=bytearray()
 while a:
  x=bytes(u.mem_read(a,1));a+=1
  if x==b'\0':break
  b+=x
 return bytes(b).decode('latin1')
def wstr(a):
 b=bytearray()
 while a:
  x=bytes(u.mem_read(a,2));a+=2
  if x==b'\0\0':break
  b+=x
 return bytes(b).decode('utf-16-le')
console=[];log=[];file_handles={};hash_states={};stubs={};stub_index=0
class Stopped(Exception):pass
def stub(name,argc,fn,address=None,cdecl=False):
 global stub_index
 at=address or api+stub_index*16;stub_index+=1
 def invoke():
  sp=u.reg_read(UC_X86_REG_ESP);args=[read(sp+4+i*4) for i in range(argc)]
  ret=fn(args)
  u.reg_write(UC_X86_REG_EAX,0 if ret is None else ret)
  u.reg_write(UC_X86_REG_ESP,sp+4+(0 if cdecl else argc*4));u.reg_write(UC_X86_REG_EIP,read(sp))
 stubs[at]=invoke
 return at
functions={}
def load(data,base):
 p=struct.unpack_from('<I',data,60)[0];op=p+24;size=struct.unpack_from('<I',data,op+56)[0];preferred=struct.unpack_from('<I',data,op+28)[0]
 u.mem_map(base,(size+4095)&~4095);u.mem_write(base,data[:0x1000])
 n=struct.unpack_from('<H',data,p+6)[0];s=op+struct.unpack_from('<H',data,p+20)[0]
 for i in range(n):
  vs,va,z,raw=struct.unpack_from('<4I',data,s+i*40+8)
  if z:u.mem_write(base+va,data[raw:raw+z])
 reloc,length=struct.unpack_from('<II',data,op+96+5*8)
 if base!=preferred and reloc:
  at=base+reloc;end=at+length
  while at<end:
   page,sz=read(at),read(at+4)
   if not sz:break
   for x in range(at+8,at+sz,2):
    v=struct.unpack('<H',u.mem_read(x,2))[0]
    if v>>12==3:write(base+page+(v&4095),read(base+page+(v&4095))+base-preferred)
   at+=sz
 return op

def imports(data,base):
 p=struct.unpack_from('<I',data,60)[0];di=read(base+p+24+96+8);at=base+di
 while read(at+12):
  names=read(at) or read(at+16);iat=read(at+16);i=0
  while read(base+names+i*4):
   v=read(base+names+i*4)
   if not v&0x80000000:
    name=cstr(base+v+2)
    if name in functions:write(base+iat+i*4,functions[name])
   i+=1
  at+=20

dedicated=(fixtures/'dedicated.exe').read_bytes();backend=(fixtures/'server.dll').read_bytes();object_file=(fixtures/'object.lto').read_bytes();dll=(root/'KPCServerTools.dll').read_bytes()
load(dedicated,exe);load(backend,engine);load(object_file,obj);load(dll,proxy)
pe=struct.unpack_from('<I',dedicated,60)[0];usa=struct.unpack_from('<I',dedicated,pe+8)[0]==0x3a942fb8
prefix='C:\\Games\\Psycho\\'
paths={None:prefix+'dedicated.exe',exe:prefix+'dedicated.exe',proxy:prefix+'server.dll',engine:prefix+'server.original.dll',obj:prefix+'object.lto'}
files={prefix+'dedicated.exe':dedicated,prefix+'server.original.dll':backend,prefix+'object.lto':object_file}
if '--bad-object' in sys.argv:files[prefix+'object.lto']=object_file[:-1]+bytes([object_file[-1]^1])
def getfilename(a):
 s=paths.get(a[0],paths[None]);b=s.encode('utf-16-le')+b'\0\0';u.mem_write(a[1],b);return len(s)
functions['GetModuleFileNameW']=stub('GetModuleFileNameW',3,getfilename)
functions['GetModuleHandleW']=stub('GetModuleHandleW',1,lambda a:exe if not a[0] else api)
functions['GetModuleHandleA']=stub('GetModuleHandleA',1,lambda a:exe if not a[0] else api)
functions['LoadLibraryW']=stub('LoadLibraryW',1,lambda a:engine if wstr(a[0])==prefix+'server.original.dll' else 0)
functions['DisableThreadLibraryCalls']=stub('DisableThreadLibraryCalls',1,lambda a:1)
functions['GetCurrentProcess']=stub('GetCurrentProcess',0,lambda a:0xffffffff)
functions['GetStdHandle']=stub('GetStdHandle',1,lambda a:99)
coop_session='--coop-session' in sys.argv
def profile_int(a):
 key=wstr(a[1])
 if key=='Enabled':return 0 if '--off' in sys.argv else 1
 if key=='CoopSession':return int(coop_session)
 if key in ('CoopMinPlayers','CoopMaxPlayers'):return a[2]
 return int('--cheats' in sys.argv)
functions['GetPrivateProfileIntW']=stub('GetPrivateProfileIntW',4,profile_int)
clock=[1000]
functions['GetTickCount']=stub('GetTickCount',0,lambda a:clock[0])
protect=[]
def virtualprotect(a):
 protect.append(tuple(a[:3]));write(a[3],0x20 if obj<=a[0]<obj+0x117000 else 2);return 1
functions['VirtualProtect']=stub('VirtualProtect',4,virtualprotect)
functions['FlushInstructionCache']=stub('FlushInstructionCache',3,lambda a:1)
def mapped(address):
 return any(b<=address<=e for b,e,_ in u.mem_regions())
def query(a):
 # One committed read/write page per query; unmapped addresses fail.
 if not mapped(a[0]):return 0
 vals=[a[0]&~0xfff,obj,4,0x1000,0x1000,4,0x1000000]
 for i,v in enumerate(vals):write(a[1]+i*4,v)
 return 28
functions['VirtualQuery']=stub('VirtualQuery',3,query)
def openfile(a):
 p=wstr(a[0])
 if p not in files and not p.endswith('KPCServerTools.log'):return 0xffffffff
 h=200+len(file_handles);file_handles[h]=[p,0];return h
functions['CreateFileW']=stub('CreateFileW',7,openfile)
def filesize(a):
 write(a[1],len(files[file_handles[a[0]][0]]));write(a[1]+4,0);return 1
functions['GetFileSizeEx']=stub('GetFileSizeEx',2,filesize)
def readfile(a):
 p,pos=file_handles[a[0]];data=files[p][pos:pos+a[2]];file_handles[a[0]][1]+=len(data)
 if data:u.mem_write(a[1],data)
 write(a[3],len(data));return 1
functions['ReadFile']=stub('ReadFile',5,readfile)
def writefile(a):
 data=bytes(u.mem_read(a[1],a[2]));(console if a[0]==99 else log).append(data);write(a[3],a[2]);return 1
functions['WriteFile']=stub('WriteFile',5,writefile)
functions['CloseHandle']=stub('CloseHandle',1,lambda a:1)
def exited(a):raise Stopped(a[0])
functions['ExitProcess']=stub('ExitProcess',1,exited)
functions['GetConsoleMode']=stub('GetConsoleMode',2,lambda a:(write(a[1],1),1)[1])
setup_calls=[]
create_at=engine+(0x2372e if usa else 0x23684);delete_at=engine+(0x23980 if usa else 0x238d6)
def create(a):setup_calls.append(tuple(a));write(a[2],0x30080000);return 17
stub('engine_CreateServer',3,create,create_at,cdecl=True)
stub('engine_DeleteServer',1,lambda a:setup_calls.append(('delete',a[0])),delete_at,cdecl=True)
def getproc(a):
 module,name=a
 if name<=65535:return 0
 s=cstr(name)
 if module==engine:return {'CreateServer':create_at,'DeleteServer':delete_at}.get(s,0)
 if module==obj:return obj+0xfdab0 if s=='ObjectDLLSetup' else 0
 return functions.get(s,0)
functions['GetProcAddress']=stub('GetProcAddress',2,getproc)
# Bind imported system pointers in all modules; backend's GetProcAddress is then
# replaced by the compiled proxy itself during CreateServer.
imports(dll,proxy);imports(backend,engine);imports(object_file,obj)
# Accelerate SHA operations while retaining actual ReadFile bytes and hash gates.
ps=report['proxy_symbols']
stub('sha_init',1,lambda a:hash_states.__setitem__(a[0],bytearray()),proxy+ps['_kpc_sha_init'],cdecl=True)
stub('sha_update',3,lambda a:hash_states[a[0]].extend(u.mem_read(a[1],a[2])),proxy+ps['_kpc_sha_update'],cdecl=True)
stub('sha_final',2,lambda a:u.mem_write(a[1],hashlib.sha256(hash_states[a[0]]).digest()),proxy+ps['_kpc_sha_final'],cdecl=True)
# Native editor state used by the installed assembly console logger.
con=exe+(0x16028 if usa else 0x12f70);write(con+0x58,2);write(con+0x5c,6);u.mem_write(con+4,b'status\0');u.mem_write(con+0x64,b'>\0')
def code(uc,a,size,data):
 if a in stubs:stubs[a]()
u.hook_add(UC_HOOK_CODE,code)
stop=scratch+0x1f0000

def call(at,args,stdcall=False):
 sp=scratch+0x1df000;write(sp,stop)
 for i,v in enumerate(args):write(sp+4+i*4,v)
 u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(at,stop,count=400000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop,hex(u.reg_read(UC_X86_REG_EIP))
 assert u.reg_read(UC_X86_REG_ESP)==sp+4+(4*len(args) if stdcall else 0)
 return u.reg_read(UC_X86_REG_EAX)

def call_regs(at,args,regs,pop):
 sp=scratch+0x1df000;write(sp,stop)
 for i,v in enumerate(args):write(sp+4+i*4,v)
 for r,v in regs.items():u.reg_write(r,v)
 u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(at,stop,count=400000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop,hex(u.reg_read(UC_X86_REG_EIP))
 assert u.reg_read(UC_X86_REG_ESP)==sp+4+pop,(hex(u.reg_read(UC_X86_REG_ESP)),hex(sp))

def coop_scenario():
 """Drive the installed co-op hooks with a synthetic shell/player world.
 Native engine routines are replaced by recording stubs; the patch sites,
 compiled proxy and assembled broadcast adapter are the shipped bytes."""
 sites={0xf13c4:(6,'ff91e0010000'),0xf13a4:(5,'e817fdffff'),0x2d455:(6,'8b90d0010000'),0x1045b1:(5,'e80a0bfcff'),
        0xfeec4:(6,'81c6e8010000'),0xf167a:(5,'e841faffff'),0x100744:(5,'e877feffff'),0xfde97:(5,'e824270000')}
 target={}
 for rva,(size,orig) in sites.items():
  now=bytes(u.mem_read(obj+rva,size))
  if rva in (0xf13c4,0x1045b1):
   assert now==bytes.fromhex(orig),hex(rva)
   continue
  assert now[0]==0xe8 and all(x==0x90 for x in now[5:]),hex(rva)
  target[rva]=(obj+rva+5+struct.unpack('<i',now[1:5])[0])&0xffffffff
  assert proxy<=target[rva]<proxy+0x100000
 assert target[0x100744]==target[0xfde97]
 said=[];ends=[];telefrag=[];nextlevels=[]
 base=scratch+0x100000;shell=base;manager=base+0x4000;players=base+0x5000;strings=base+0x8000
 write(obj+0x16de60,shell);write(shell+0x1d0,1);write(shell+0x1ae0,3)
 u.mem_write(shell+0x210,b'worlds\\realm1\\R1M2B\0');u.mem_write(shell+0x210+0x7f,b'worlds\\realm1\\R1M2C\0')
 u.mem_write(shell+0x210+2*0x7f,b'worlds\\realm1\\R1M3A\0')
 write(obj+0x16da88,0);write(obj+0x157724,manager)
 def broadcast():
  said.append(cstr(u.reg_read(UC_X86_REG_EDX)));sp=u.reg_read(UC_X86_REG_ESP)
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
 def thiscall_stub(record,pop):
  def run():
   record.append(u.reg_read(UC_X86_REG_ECX));sp=u.reg_read(UC_X86_REG_ESP)
   if record is nextlevels:
    c=read(obj+0x16da88)+1;write(obj+0x16da88,0 if c>read(shell+0x1ae0)-1 else c)
   u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,read(sp))
  return run
 stubs[obj+report['hook_symbols']['broadcast_only']]=broadcast
 stubs[obj+0x100760]=thiscall_stub(ends,0)
 stubs[obj+0x1005c0]=thiscall_stub(nextlevels,0)
 stubs[obj+0xf10c0]=thiscall_stub(telefrag,12)
 def player(i,client,hobj,name):
  at=players+i*0x400;write(at+8,hobj);write(at+0x120,client);u.mem_write(at+0x1e8,name.encode()+b'\0');return at
 alice=player(0,0x111,0xa1,'Alice');bob=player(1,0x222,0xb2,'Bob')
 def roster(*objs):
  node=manager+0x100;write(manager+4,node if objs else 0)
  for i,h in enumerate(objs):
   write(node,h);write(node+4,node+8 if i+1<len(objs) else 0);node+=8
 def spawn(who,path=0xf13a4,z=0xc5380000):
  pos=strings+0x800;write(pos,0x44c80000);write(pos+4,0xc3800000);write(pos+8,z)
  call_regs(target[path],[pos,0x10168660,0x10168650],{UC_X86_REG_ECX:who},12)
  return read(pos+8)
 def chat(who,text):
  u.mem_write(strings,text.encode()+b'\0');n=len(said)
  call_regs(target[0xfeec4],[],{UC_X86_REG_ESI:who,UC_X86_REG_EAX:strings},0)
  assert u.reg_read(UC_X86_REG_ESI)==who+0x1e8 and u.reg_read(UC_X86_REG_EAX)==strings
  return said[n:]
 def exit_trigger(world,flag=1,activator=0):
  # Native multiplayer state: the exit's property reader sets +0x152 to 1 for
  # every exit when the game type is non-zero (object.lto 2d33f-2d352).
  # Stack as inside handler 2d450: [esp+4]=handler return, [esp+8]=&activator.
  trig=strings+0x400;u.mem_write(trig+0x54,world.encode()+b'\0');u.mem_write(trig+0x152,bytes([flag]))
  u.mem_write(trig+0xd3,b'\0');act=strings+0x700;write(act,activator)
  call_regs(target[0x2d455],[0x1002d442,act],{UC_X86_REG_EAX:shell,UC_X86_REG_ECX:trig},0)
  assert u.reg_read(UC_X86_REG_EDX)==1  # native multiplayer gate preserved
 def next_level():
  clock[0]+=4000;call_regs(target[0x100744],[],{UC_X86_REG_ECX:shell},0)
 roster(0xa1);assert spawn(alice)==0xc5380000
 roster(0xa1,0xb2);assert spawn(bob)==0xc5320000          # slot 1: +96 Z
 assert not telefrag,'native telefrag loop must be skipped in a session'
 assert any('Bob joined the lobby' in m for m in said),said
 assert chat(alice,'hello')==[]
 assert any('Alice(R)' in m for m in chat(alice,'!ready'))
 assert any('Only the host' in m for m in chat(bob,'!start'))
 chat(bob,'!READY')
 out=chat(alice,'!start');assert any('Starting -> R1M2B' in m for m in out),out
 assert len(ends)==1 and ends[0]==shell and read(obj+0x16da88)==2   # count-1: native pre-increment wraps to R1M2B
 next_level();assert len(nextlevels)==1 and read(obj+0x16da88)==0
 spawn(alice);assert any('R1M2B started (epoch 1)' in m for m in said),said
 spawn(bob)
 exit_trigger('worlds\\realm1\\R1M2C',flag=0,activator=0xa1);assert len(ends)==1   # not the native MP value: refused
 exit_trigger('',activator=0xa1);assert len(ends)==1                               # missing NextWorld: refused
 exit_trigger('worlds\\realm1\\R1M2C',activator=0xa1);assert len(ends)==2 and read(obj+0x16da88)==0
 for i in range(5):exit_trigger('worlds\\realm1\\R1M2C',activator=0xb2)
 assert len(ends)==2,'exit must be debounced'
 next_level();assert read(obj+0x16da88)==1
 spawn(alice,path=0xf167a);spawn(bob);assert not telefrag
 roster(0xa1)                                              # Bob disconnected
 clock[0]+=1000;out=chat(alice,'!coop')
 assert any('Bob left' in m for m in said) and any('PLAYING R1M2C e2' in m for m in out),(said,out)
 # Golden-mirror exit object on R1M2C, NextWorld as logged on Windows.
 n=len(said);exit_trigger('worlds/realm1/r1m3a',activator=0xa1)
 assert len(ends)==3 and read(obj+0x16da88)==1 and any('Exit reached -> R1M3A' in m for m in said[n:]),said[n:]
 exit_trigger('worlds/realm1/r1m3a',activator=0xa1);assert len(ends)==3
 next_level();assert read(obj+0x16da88)==2
 spawn(alice);assert any('R1M3A started (epoch 3)' in m for m in said),said
 n=len(said)
 for i in range(4):exit_trigger('worlds/realm1/r1m3b',activator=0xa1)
 assert len(ends)==3 and sum('outside the MVP route' in m for m in said[n:])==1,said[n:]
 assert any('End of the MVP route' in m for m in chat(alice,'!next'))
 text=b''.join(log).decode('latin1')
 for marker in ('build=coop-session-v2 proto=kpc-coop/1 sha256=','effective CoopSession=1','session armed kpc-coop/1',
                'change [Co-op] All ready','world-loaded worlds\\realm1\\R1M2C','telefrag=skipped',
                'exit-refused flag152 differs','exit-seen next= flag152=1',
                'exit-seen next=worlds\\realm1\\R1M2C flag152=1 exit=0x','activator=member:Alice start=-',
                'exit next=worlds/realm1/r1m3a flag152=1','world-loaded worlds\\realm1\\R1M3A'):
  assert marker in text,marker
 assert text.count('exit-seen')<=3*8,'exit logging must stay bounded per epoch'
 print('PASS co-op session hooks:',len(said),'broadcasts,',len(ends),'native end-level requests,',len(nextlevels),'NextLevel calls, telefrag skipped')

call(proxy+ps['_DllMain'],[proxy,1,0],True)
u.mem_write(scratch+0x90000,b'KPC\0')
assert call(proxy+ps['_CreateServer'],[3,scratch+0x90000,scratch+0x90010])==17
assert read(scratch+0x90010)==0x30080000 and setup_calls[0][0]==3
# Locate the engine's original import slot, after initialization.
pe=struct.unpack_from('<I',backend,60)[0];at=engine+read(engine+pe+24+96+8);slot=None
while read(at+12):
 names=read(at);iat=read(at+16);i=0
 while names and read(engine+names+i*4):
  n=read(engine+names+i*4)
  if not n&0x80000000 and cstr(engine+n+2)=='GetProcAddress':slot=engine+iat+i*4
  i+=1
 at+=20
assert slot
if '--off' in sys.argv:
 assert read(slot)==functions['GetProcAddress'];assert bytes(u.mem_read(obj+0x116580,report['hook_bytes']))==bytes(report['hook_bytes'])
 print('PASS compiled DLL disabled/pass-through; original object memory untouched')
else:
 assert read(slot)!=functions['GetProcAddress']
 u.mem_write(scratch+0x90100,b'ObjectDLLSetup\0')
 before_headers=bytes(u.mem_read(obj,0x1000));before_reloc=bytes(u.mem_read(obj+0x175000,0x208a4))
 try:
  assert call(read(slot),[obj,scratch+0x90100],True)==obj+0xfdab0
 except Stopped:
  assert '--bad-object' in sys.argv
  assert bytes(u.mem_read(obj+0x116580,report['hook_bytes']))==bytes(report['hook_bytes'])
  print('PASS altered object rejected before patch installation');sys.exit(0)
 assert '--bad-object' not in sys.argv
 for p in report['patches']:assert bytes(u.mem_read(obj+p['rva'],p['size']))==bytes.fromhex(p['after'])
 initial=report['hook_symbols']['initial_mode'];assert bytes(u.mem_read(obj+initial,1))==bytes([int('--cheats' in sys.argv)])
 assert bytes(u.mem_read(obj,0x1000))==before_headers
 assert bytes(u.mem_read(obj+0x175000,0x208a4))==before_reloc
 assert read(obj+0x16d950)&3==0
 count=len(protect)
 assert call(read(slot),[obj,scratch+0x90100],True)==obj+0xfdab0
 assert len(protect)==count,'second lookup reinstalled hooks'
 assert object_file==files[prefix+'object.lto']
 if coop_session:coop_scenario()
 call(proxy+ps['_DeleteServer'],[0x30080000]);assert setup_calls[-1]==('delete',0x30080000)
 print('PASS compiled DLL:',fixtures.name,'proxy relocation, export forwarding, IAT interception, object relocation, guarded memory patching, initial policy, idempotence, unchanged headers/relocations and unchanged original file')
