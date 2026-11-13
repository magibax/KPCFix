"""Run the compiled proxy with mapped original images and mocked Win32 APIs.
Usage: python check_proxy.py FIXTURE_DIRECTORY [--cheats] [--off] [--bad-object] [--no-server-frame]
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
def profile_int(a):
 key=wstr(a[1])
 if key=='Enabled':return 0 if '--off' in sys.argv else 1
 if key=='ServerFrameMs':return 0 if '--no-server-frame' in sys.argv else a[2]
 return int('--cheats' in sys.argv)
functions['GetPrivateProfileIntW']=stub('GetPrivateProfileIntW',4,profile_int)
protect=[]
def virtualprotect(a):
 protect.append(tuple(a[:3]));write(a[3],0x20 if obj<=a[0]<obj+0x117000 else 2);return 1
functions['VirtualProtect']=stub('VirtualProtect',4,virtualprotect)
functions['FlushInstructionCache']=stub('FlushInstructionCache',3,lambda a:1)
def query(a):
 vals=[obj+0x129000,obj,4,0x4c000,0x1000,4,0x1000000]
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

call(proxy+ps['_DllMain'],[proxy,1,0],True)
u.mem_write(scratch+0x90000,b'KPC\0')
assert call(proxy+ps['_CreateServer'],[3,scratch+0x90000,scratch+0x90010])==17
assert read(scratch+0x90010)==0x30080000 and setup_calls[0][0]==3
# ServerFrameMs (default 16), Europe-family server.dll only: 235b0 (e8 aa 6a 00 00
# -> frame 2a05f, thiscall(flags, float time), ret 8). A call sooner than 16 ms
# after the last simulated frame returns 1 without running; the next one runs.
frame_site=bytes(u.mem_read(engine+0x235b0,5))
frame_text=b''.join(log).decode('latin1')
if usa or '--off' in sys.argv or '--no-server-frame' in sys.argv:
 if not usa:assert frame_site==bytes.fromhex('e8aa6a0000'),frame_site.hex()
 assert 'Minimum server frame installed' not in frame_text
 if usa and '--off' not in sys.argv and '--no-server-frame' not in sys.argv:assert 'ServerFrameMs is not supported for this server.dll (USA)' in frame_text
 print('PASS server frames unchanged (USA, disabled or ServerFrameMs=0)')
else:
 assert frame_site[0]==0xe8 and frame_site!=bytes.fromhex('e8aa6a0000'),frame_site.hex()
 frame_target=(engine+0x235b0+5+struct.unpack('<i',frame_site[1:5])[0])&0xffffffff
 assert proxy<=frame_target<proxy+0x100000
 assert 'Minimum server frame installed, ServerFrameMs=16' in frame_text
 runs=[]
 def server_frame():
  sp=u.reg_read(UC_X86_REG_ESP);srv_=u.reg_read(UC_X86_REG_ECX)
  t=struct.unpack('<f',struct.pack('<I',read(sp+8)))[0]+struct.unpack('<f',u.mem_read(srv_+0x1f8,4))[0]
  runs.append((read(sp+4),round(t,4)));u.mem_write(srv_+0x218,struct.pack('<f',t))
  u.reg_write(UC_X86_REG_EAX,1);u.reg_write(UC_X86_REG_ESP,sp+12);u.reg_write(UC_X86_REG_EIP,read(sp))
 stubs[engine+0x2a05f]=server_frame
 srv=scratch+0x95000;u.mem_write(srv+0x1f8,struct.pack('<f',0.0));u.mem_write(srv+0x218,struct.pack('<f',0.0))
 f=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
 def thiscall(at,this,args):
  sp=scratch+0x1df000;write(sp,stop)
  for i,v in enumerate(args):write(sp+4+i*4,v)
  u.reg_write(UC_X86_REG_ECX,this);u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(at,stop,count=100000)
  assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==sp+4+4*len(args)
  return u.reg_read(UC_X86_REG_EAX)
 res=[thiscall(frame_target,srv,[7,f(t)]) for t in (1.000,1.008,1.016,1.024,1.032,1.040,1.048,2.000,1.990)]
 assert all(r==1 for r in res),res
 # First call runs (no previous frame), then every second 8 ms call; a long gap runs; time going back runs.
 assert [t for _,t in runs]==[1.0,1.016,1.032,1.048,2.0,1.99],runs
 assert all(fl==7 for fl,_ in runs),'flags forwarded'
 del stubs[engine+0x2a05f]
 print('PASS server frame: 8 ms calls simulated as 16 ms frames, long gaps and time resets run at once')
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
 call(proxy+ps['_DeleteServer'],[0x30080000]);assert setup_calls[-1]==('delete',0x30080000)
 print('PASS compiled DLL:',fixtures.name,'proxy relocation, export forwarding, IAT interception, object relocation, guarded memory patching, initial policy, idempotence, unchanged headers/relocations and unchanged original file')
