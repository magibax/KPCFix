"""Run the compiled proxy with mapped original images and mocked Win32 APIs.
Usage: python check_proxy.py FIXTURE_DIRECTORY [--cheats] [--off] [--bad-object] [--coop-session [--trace-timing]
       [--intro [--test-start=R1M3A] [--intro-overrun] [--script-clock|--no-script-clock]]
       [--sound-clock|--no-sound-clock] [--no-health-bars]]
SHA compression is accelerated with hashlib in this emulator; the same C SHA
implementation is separately tested natively by the build validation workflow.
"""
from pathlib import Path
import hashlib,json,re,struct,sys
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
 if key=='TraceTiming':return int('--trace-timing' in sys.argv)
 if key=='CoopIntermission':return int('--intermission' in sys.argv)
 if key=='TraceMusic':return int('--trace-music' in sys.argv)
 if key=='CoopMusic':return 0 if '--no-music' in sys.argv else a[2]
 if key=='CoopIntro':return int('--intro' in sys.argv)
 if key=='CoopScriptClock':
  # Default argument is CoopIntro; the flags override it either way.
  if '--no-script-clock' in sys.argv:return 0
  if '--script-clock' in sys.argv:return 1
  return a[2]
 if key=='CoopStartScoreboard':return 0 if '--no-start-scoreboard' in sys.argv else a[2]
 if key=='CoopSpawners':return 0 if '--no-spawners' in sys.argv else a[2]
 if key=='TraceSpawners':return int('--trace-spawners' in sys.argv)
 if key=='CoopSkill':
  for x in sys.argv:
   if x.startswith('--skill='):return int(x.split('=',1)[1])&0xffffffff
  return a[2]
 if key=='CoopHealthBars':return 0 if '--no-health-bars' in sys.argv else a[2]
 if key=='CoopSoundClock':
  # Default argument is the effective CoopScriptClock.
  if '--no-sound-clock' in sys.argv:return 0
  if '--sound-clock' in sys.argv:return 1
  return a[2]
 return int('--cheats' in sys.argv)
functions['GetPrivateProfileIntW']=stub('GetPrivateProfileIntW',4,profile_int)
def profile_str(a):
 # Only CoopTestStartWorld is read as a string: --test-start=<value>.
 value=''
 if wstr(a[1])=='CoopTestStartWorld':
  for x in sys.argv:
   if x.startswith('--test-start='):value=x.split('=',1)[1]
 data=(value[:a[4]-1]+'\0').encode('utf-16le');u.mem_write(a[3],data);return len(value[:a[4]-1])
functions['GetPrivateProfileStringW']=stub('GetPrivateProfileStringW',6,profile_str)
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
        0xfeec4:(6,'81c6e8010000'),0xf167a:(5,'e841faffff'),0x100744:(5,'e877feffff'),0xfde97:(5,'e824270000'),
        0x10494b:(5,'e80064fbff'),0xffbbf:(5,'e85c0b0000'),0xffba5:(5,'e8865afcff'),
        0xea6d2:(5,'e809e90000'),0xea7c3:(5,'e818e80000'),0xeaa10:(5,'e8cbe50000'),0xeab11:(5,'e8cae40000'),
        0x93c5d:(2,'0c10'),0x93c65:(5,'e896530600')}
 music_flag='--no-music' not in sys.argv
 intro_flag='--intro' in sys.argv
 trace_timing='--trace-timing' in sys.argv
 script_clock='--script-clock' in sys.argv or (intro_flag and '--no-script-clock' not in sys.argv)
 hooked_optional={0x10494b:music_flag or '--trace-music' in sys.argv,0xffbbf:music_flag or intro_flag,0x1045b1:intro_flag,
                  0xffba5:script_clock or trace_timing}
 bars='--no-health-bars' not in sys.argv
 for rva in (0xea6d2,0xea7c3,0xeaa10,0xeab11):hooked_optional[rva]=bars
 spawners='--no-spawners' not in sys.argv;trace_spawners='--trace-spawners' in sys.argv
 hooked_optional[0x93c65]=trace_spawners
 # CMonsterSpawner forced MP-off (or al,0x10 at 93c5d): NOPs with CoopSpawners.
 assert bytes(u.mem_read(obj+0x93c5d,2))==(b'\x90\x90' if spawners else bytes.fromhex('0c10'))
 del sites[0x93c5d]
 sb='--intermission' not in sys.argv and '--no-start-scoreboard' not in sys.argv
 hooked_optional[0xffbbf]=hooked_optional[0xffbbf] or sb   # per-frame hook also drives the start scoreboard
 skill=[int(x.split('=',1)[1]) for x in sys.argv if x.startswith('--skill=')]
 target={}
 for rva,(size,orig) in sites.items():
  now=bytes(u.mem_read(obj+rva,size))
  if rva==0xf13c4 or (rva in hooked_optional and not hooked_optional[rva]):
   assert now==bytes.fromhex(orig),hex(rva)
   continue
  assert now[0]==0xe8 and all(x==0x90 for x in now[5:]),hex(rva)
  target[rva]=(obj+rva+5+struct.unpack('<i',now[1:5])[0])&0xffffffff
  assert proxy<=target[rva]<proxy+0x100000
 assert target[0x100744]==target[0xfde97]
 text=b''.join(log).decode('latin1')
 assert ('effective CoopSpawners=1' if spawners else 'effective CoopSpawners=0') in text
 assert ('effective CoopStartScoreboard=1' if sb else 'effective CoopStartScoreboard=0') in text
 if skill and 0<=skill[0]<=4:
  assert read(obj+0x15b678)==skill[0] and 'effective CoopSkill=%d'%skill[0] in text
 else:
  assert read(obj+0x15b678)==0 and 'effective CoopSkill=unchanged' in text
 if trace_spawners:
  # Original filter f9000 runs for real against the multiplayer shell below.
  write(obj+0x16de60,scratch+0x100000);write(scratch+0x100000+0x1d0,1)
  res=[]
  for args in ((0,1),(0,6),(2,6),(0,0x11),(4,4)):
   call_regs(target[0x93c65],list(args),{},0);res.append(u.reg_read(UC_X86_REG_EAX)&0xff)
  assert res==[1,0,1,0,1],res
  text=b''.join(log).decode('latin1')
  assert 'spawner-filter skill=0 miscflags=1 create=1' in text and 'spawner-filter skill=0 miscflags=17 create=0' in text
  print('PASS co-op spawner trace: authored difficulty bits and designer 0x10 still honoured')
 if bars:
  # Enemy health bars (CoopHealthBars): the four callers of the single-player
  # test f8fe0 get 1 (proceed as in single player); Send is limited to 10 Hz.
  for rva in (0xea6d2,0xea7c3,0xeaa10):
   call_regs(target[rva],[],{},0);assert u.reg_read(UC_X86_REG_EAX)&0xff==1,hex(rva)
  sends=[]
  for ms in (0,50,49,1,100,250):
   clock[0]+=ms;call_regs(target[0xeab11],[],{},0);sends.append(u.reg_read(UC_X86_REG_EAX)&0xff)
  assert sends==[1,0,0,1,1,1],sends
  text=b''.join(log).decode('latin1')
  assert 'effective CoopHealthBars=1' in text and text.count(' health-bar first monster damage')==1
  call_regs(target[0xea7c3],[],{},0);assert b''.join(log).decode('latin1').count(' health-bar ')==1,'logged once per epoch'
  print('PASS co-op health bars: update/damage/remove gates open, send limited to 10 Hz')
 else:
  assert 'effective CoopHealthBars=0' in b''.join(log).decode('latin1')
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
 # World-change requests: native end-level calls with --intermission, otherwise
 # the shell's pending-NextLevel flag +0x1ae9, consumed here as 100720 would.
 intermission='--intermission' in sys.argv;pending=[0]
 def changes():
  if intermission:return len(ends)
  assert u.mem_read(shell+0x1ae8,1)[0]==0,'no native intermission flag in co-op default mode'
  if u.mem_read(shell+0x1ae9,1)[0]:pending[0]+=1;u.mem_write(shell+0x1ae9,b'\0')
  return pending[0]
 # Per-player music (CoopMusic, default on). A fake g_pServerDE (16de80) whose
 # thiscall vtable slots record StartMessage(+0xac), WriteByte(+0x8),
 # WriteString(+0x14) and EndMessage(+0xb4) as [client, id, payload...].
 music_on='--no-music' not in sys.argv;msgs=[];frames=[]
 de=base+0x9000;vt=base+0x9100;code=base+0x9200;write(obj+0x16de80,de);write(de,vt)
 def tstub(pop,fn):
  def run():
   sp=u.reg_read(UC_X86_REG_ESP);r=fn([read(sp+4+4*k) for k in range(pop//4)])
   if r is not None:u.reg_write(UC_X86_REG_EAX,r)
   u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,read(sp))
  return run
 def m_start(a):msgs.append([a[0],a[1]]);return 0x5000+len(msgs)
 def m_byte(a):assert a[0]==0x5000+len(msgs);msgs[-1].append(a[1]&0xff)
 def m_str(a):assert a[0]==0x5000+len(msgs);msgs[-1].append(cstr(a[1]))
 for off,(pop,fn) in {0xac:(8,m_start),0x8:(8,m_byte),0x14:(8,m_str),0xb4:(4,lambda a:None)}.items():
  write(vt+off,code+off);stubs[code+off]=tstub(pop,fn)
 stubs[obj+0x100720]=thiscall_stub(frames,0)
 event=base+0xa000
 def music_world(song,intensity=1,transition=2,play=0):
  # Fake music event as built by bad50 (fields verified in baa90), then the
  # hooked WorldProperties call site 10494b.
  u.mem_write(event+0x108,song.encode()+b'\0');u.mem_write(event+0x188,bytes([play,intensity,0xff,transition,0]))   # real worlds: no 'play' token, so +0x188=0
  stubs[obj+0xbad50]=tstub(0,lambda a:event)
  call_regs(target[0x10494b],[obj+0x14c06c,3,strings+0x900],{},0)
 def frame(ms=1600):
  clock[0]+=ms;call_regs(target[0xffbbf],[],{UC_X86_REG_ECX:shell},0)
 def sbm():return [(m[0],m[1]) for m in msgs if m[1] in (0x48,0x69)]
 def start_seq(client,song):return [[client,0x4a,song],[client,0x51,2],[client,0x4c,song],[client,0x50,1]]
 if intro_flag:
  # Synchronised original intro (CoopIntro). The stock WorldProperties lookup
  # (call site 1045b1 -> c50c0) runs inside the native NextLevel, so a small
  # trampoline at 1005c0 performs it there, then continues into the NextLevel
  # stub. The original script player (c5340/c55b0) and cinematic release
  # helpers (c5a10/c59a0) are recording stubs: camera presentation itself is
  # client-side and NOT emulated.
  name='mapdata\\realm1\\r1m3\\r1m3a\\cathedral1start.scr';path=strings+0xb00
  u.mem_write(path,name.encode()+b'\0')
  def cstub(n,fn):
   def run():
    sp=u.reg_read(UC_X86_REG_ESP);r=fn([read(sp+4+4*k) for k in range(n)])
    u.reg_write(UC_X86_REG_EAX,0 if r is None else r);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
   return run
  scripts=[];playing=[0];cine=[]
  # Fake original script object (fields verified in c6260/c6380/c5340):
  # +0x104 event count, +0x108 event array, +0x110 time, +0x114 next index;
  # event +4 = authored time. Playing list: count 15a0a4, array 15a0a8.
  # Authored times from CATHEDRAL1START.SCR: start, streamsound, subtitle,
  # window_break, last event.
  authored=[0.0,1.0,8.5,47.8,53.0]
  sobj=code+0x600;sarr=code+0x780;evs=code+0x800;plist=code+0x900   # object spans +0x118
  write(sobj+0x104,len(authored));write(sobj+0x108,sarr)
  for i,t in enumerate(authored):
   write(sarr+4*i,evs+16*i);u.mem_write(evs+16*i+4,struct.pack('<f',t))
  write(obj+0x15a0a8,plist);write(obj+0x15a0a4,0)
  play_at=[]
  def play(a):
   scripts.append(cstr(a[0]));playing[0]=1;play_at.append(len(msgs))
   u.mem_write(sobj+0x110,struct.pack('<f',0.0));write(sobj+0x114,0)
   write(plist,sobj);write(obj+0x15a0a4,1)
  given=[]
  def script_update(a):
   # c5630 stand-in: advance the playing script exactly like c6260.
   given.append(a[0])
   if not read(obj+0x15a0a4):return
   t=struct.unpack('<f',bytes(u.mem_read(sobj+0x110,4)))[0]+struct.unpack('<f',struct.pack('<I',a[0]))[0]
   u.mem_write(sobj+0x110,struct.pack('<f',t));i=read(sobj+0x114)
   while i<len(authored) and authored[i]<=struct.unpack('<f',struct.pack('<f',t))[0]:i+=1
   write(sobj+0x114,i)
   if i==len(authored):write(obj+0x15a0a4,0);playing[0]=0
  stubs[obj+0xc5630]=cstub(1,script_update)
  stubs[obj+0xc50c0]=cstub(1,lambda a:0x4321)
  stubs[obj+0xc5340]=cstub(1,play)
  stubs[obj+0xc55b0]=cstub(1,lambda a:playing[0])
  stubs[obj+0xc5a10]=cstub(1,lambda a:cine.append(('input',a[0]&0xff)))
  stubs[obj+0xc59a0]=cstub(1,lambda a:cine.append(('letterbox',a[0]&0xff)))
  nl=code+0x400;stubs[nl]=thiscall_stub(nextlevels,0);del stubs[obj+0x1005c0]
  tr=obj+0x1005c0;lk=target[0x1045b1]
  u.mem_write(tr,b'\x68'+struct.pack('<I',path)+b'\xe8'+struct.pack('<i',lk-(tr+10))+b'\x83\xc4\x04\xe9'+struct.pack('<i',nl-(tr+18)))
  test_start=[x.split('=',1)[1] for x in sys.argv if x.startswith('--test-start=')]
  world='R1M3A' if test_start and test_start[0].lower()=='r1m3a' else 'R1M2B'
  roster(0xa1);spawn(alice);roster(0xa1,0xb2);spawn(bob)
  chat(alice,'!ready');chat(bob,'!ready');out=chat(alice,'!start')
  assert any('Starting -> '+world in m for m in out),out
  if sb:assert sbm()==[(0x111,0x48),(0x222,0x48)] and changes()==0;frame(1100)
  assert changes()==1
  next_level()
  text=b''.join(log).decode('latin1')
  if world!='R1M3A':
   if test_start:assert 'CoopTestStartWorld ignored: not a route world' in text
   assert 'intro-armed' not in text
   spawn(alice);assert any('Intro skipped' in m for m in said),said
   print('PASS co-op intro: invalid/absent test start ignored; no intro outside R1M3A');return
  assert 'effective CoopTestStartWorld=R1M3A (test start)' in text and 'effective CoopIntro=1' in text
  assert ('effective CoopScriptClock=1' if script_clock else 'effective CoopScriptClock=0') in text
  assert 'intro-armed '+name in text and read(obj+0x16da88)==2
  n=len(said);spawn(alice);frame(5000)
  assert not scripts and any('Intro starts when all players are in (1/2)' in m for m in said[n:]),said[n:]
  spawn(bob);frame(1000);assert not scripts,'settle delay after the last player'
  frame(2100);assert scripts==[name] and any('R1M3A intro starting (epoch 1, 2/2 players)' in m for m in said),said[-3:]
  if sb:
   # Shown at !start, closed by the stock re-entry, shown again after each spawn,
   # closed for both immediately before the script player is called.
   assert sbm()==[(0x111,0x48),(0x222,0x48),(0x111,0x48),(0x222,0x48),(0x111,0x69),(0x222,0x69)],sbm()
   closes=[i for i,m in enumerate(msgs) if m[1]==0x69];assert closes and max(closes)<play_at[0],(closes,play_at)
   print('PASS co-op start scoreboard: shown at !start and after re-entry, closed before the intro script')
  else:assert sbm()==[]
  frame();frame();assert scripts==[name],'the intro starts exactly once'
  assert any('Not in play' in m for m in chat(alice,'!next')),'no world change during the intro'
  overrun='--intro-overrun' in sys.argv
  if overrun:
   frame(120000);assert cine==[('input',1),('letterbox',0)] and any('Intro overran' in m for m in said),(cine,said[-3:])
  elif 0xffba5 in target:
   # Measured host cadence, scaled for speed: frames every 40 ms of wall time,
   # the engine crediting 50 ms each (the 1.25x of 8 ms frames credited 10 ms).
   dt=struct.unpack('<I',struct.pack('<f',0.05))[0];t0=clock[0];n=0
   while playing[0] and n<3000:
    clock[0]+=40;call_regs(target[0xffba5],[dt],{},0);n+=1
   wall=clock[0]-t0;assert not playing[0],'script finished'
   if script_clock:
    assert 52960<=wall<=53120,wall          # authored 53 s on the wall clock
    assert max(struct.unpack('<f',struct.pack('<I',g))[0] for g in given)<=0.05+1e-7,'never a larger step than the engine'
   else:
    assert all(g==dt for g in given),'observer forwards the engine dt bit-identically'
    assert 42360<=wall<=42480,wall          # stock: 53 s of script in 42.4 s
   frame();assert any('Intro finished' in m for m in said) and not cine,(cine,said[-3:])
   text=b''.join(log).decode('latin1')
   if trace_timing:
    ev=[l for l in text.splitlines() if ' intro-event ' in l]
    assert len(ev)==len(authored),ev
    # Narration (streamsound, i=1 at 1.0 s) to window smash (i=3 at 47.8 s):
    # authored 46.8 s; the stock clock compresses it to 46.8/1.25 = 37.44 s.
    voice=re.search(r'intro-event i=1 at_ms=1000 script_ms=(\d+) wall_ms=(\d+)',text);assert voice,ev
    smash=re.search(r'intro-event i=3 at_ms=47800 script_ms=(\d+) wall_ms=(\d+)',text);assert smash,ev
    w=int(smash.group(2))-int(voice.group(2));assert (46720<=w<=46880) if script_clock else (37360<=w<=37520),w
    clk=re.search(r'intro-clock frames=(\d+) engine_ms=(\d+) script_ms=(\d+) wall_ms=(\d+) governor=(on|off)',text);assert clk,text[-400:]
    assert clk.group(5)==('on' if script_clock else 'off')
    cal=re.search(r'clock-calibration frames=(\d+) engine_ms=(\d+) script_ms=(\d+) wall_ms=(\d+)',text);assert cal
    assert int(cal.group(2))*4==int(cal.group(4))*5 or abs(int(cal.group(2))*4-int(cal.group(4))*5)<=250,cal.group(0)
   else:
    assert 'intro-event' not in text and 'clock-calibration' not in text
   print('PASS co-op script clock:',('governed' if script_clock else 'observed'),'53 s script ran',wall,'ms of wall clock')
  else:
   playing[0]=0;frame();assert any('Intro finished' in m for m in said) and not cine,(cine,said[-3:])
  frame();assert scripts==[name]
  text=b''.join(log).decode('latin1')
  for marker in ('intro-start '+name,'intro-playing','intro-abort' if overrun else 'intro-finished '+name):
   assert marker in text,marker
  print('PASS co-op intro: test start R1M3A, barrier 2/2 + settle, start once, no !next during intro,',
        'overrun release' if overrun else 'completion')
  return
 if music_on:music_world('song5')                          # preloaded world, before anyone joins
 roster(0xa1);assert spawn(alice)==0xc5380000
 roster(0xa1,0xb2);assert spawn(bob)==0xc5320000          # slot 1: +96 Z
 if music_on:
  frame(100);assert msgs==[],'music waits for the delivery delay'
  frame();assert msgs==start_seq(0x111,'song5')+start_seq(0x222,'song5'),msgs
  n=len(msgs);spawn(alice,path=0xf167a);frame();frame();assert len(msgs)==n,'no resend on respawn in the same epoch'
  assert frames and all(f==shell for f in frames),'per-frame hook forwards to 100720 with the shell'
 assert not telefrag,'native telefrag loop must be skipped in a session'
 assert any('Bob joined the lobby' in m for m in said),said
 assert chat(alice,'hello')==[]
 assert any('Alice(R)' in m for m in chat(alice,'!ready'))
 assert any('Only the host' in m for m in chat(bob,'!start'))
 chat(bob,'!READY')
 out=chat(alice,'!start');assert any('Starting -> R1M2B' in m for m in out),out
 if sb:
  assert sbm()==[(0x111,0x48),(0x222,0x48)],sbm()
  assert changes()==0,'world change held while the scoreboard shows';frame(500);assert changes()==0;frame(600)
 else:assert sbm()==[]
 assert changes()==1 and (not intermission or ends[0]==shell) and read(obj+0x16da88)==2   # count-1: native pre-increment wraps to R1M2B
 next_level();assert len(nextlevels)==1 and read(obj+0x16da88)==0
 if music_on:n=len(msgs);music_world('song5')               # reloaded world creates its music (epoch 1)
 spawn(alice);assert any('R1M2B started (epoch 1)' in m for m in said),said
 spawn(bob)
 if music_on:
  frame();assert msgs[n:]==start_seq(0x111,'song5')+start_seq(0x222,'song5'),msgs[n:]
  frame();assert len(msgs)==n+8,'exactly one delivery per player per epoch'
 exit_trigger('worlds\\realm1\\R1M2C',flag=0,activator=0xa1);assert changes()==1   # not the native MP value: refused
 exit_trigger('',activator=0xa1);assert changes()==1                               # missing NextWorld: refused
 exit_trigger('worlds\\realm1\\R1M2C',activator=0xa1);assert changes()==2 and read(obj+0x16da88)==0
 for i in range(5):exit_trigger('worlds\\realm1\\R1M2C',activator=0xb2)
 assert changes()==2,'exit must be debounced'
 next_level();assert read(obj+0x16da88)==1
 if music_on:n=len(msgs)                                    # this world creates no music event
 spawn(alice,path=0xf167a);spawn(bob);assert not telefrag
 if music_on:frame();frame();assert len(msgs)==n,'no stale previous-world track'
 roster(0xa1)                                              # Bob disconnected
 clock[0]+=1000;out=chat(alice,'!coop')
 assert any('Bob left' in m for m in said) and any('PLAYING R1M2C e2' in m for m in out),(said,out)
 # Golden-mirror exit object on R1M2C, NextWorld as logged on Windows.
 n=len(said);exit_trigger('worlds/realm1/r1m3a',activator=0xa1)
 assert changes()==3 and read(obj+0x16da88)==1 and any('Exit reached -> R1M3A' in m for m in said[n:]),said[n:]
 exit_trigger('worlds/realm1/r1m3a',activator=0xa1);assert changes()==3
 next_level();assert read(obj+0x16da88)==2
 if music_on:n=len(msgs);music_world('song1',intensity=0xff,play=1)   # play flag, no intensity: PLAY_STRING
 spawn(alice);assert any('R1M3A started (epoch 3)' in m for m in said),said
 if music_on:frame();assert msgs[n:]==[[0x111,0x4a,'song1'],[0x111,0x51,2],[0x111,0x4b,'song1']],'R1M3A song1 only to the remaining player'
 n=len(said)
 for i in range(4):exit_trigger('worlds/realm1/r1m3b',activator=0xa1)
 assert changes()==3 and sum('outside the MVP route' in m for m in said[n:])==1,said[n:]
 assert any('End of the MVP route' in m for m in chat(alice,'!next'))
 text=b''.join(log).decode('latin1')
 for marker in ('build=coop-session-v2 proto=kpc-coop/1 sha256=','effective CoopSession=1','session armed kpc-coop/1',
                'change [Co-op] All ready','world-loaded worlds\\realm1\\R1M2C','telefrag=skipped',
                'exit-refused flag152 differs','exit-seen next= flag152=1',
                'exit-seen next=worlds\\realm1\\R1M2C flag152=1 exit=0x','activator=member:Alice start=-',
                'exit next=worlds/realm1/r1m3a flag152=1','world-loaded worlds\\realm1\\R1M3A',
                'native intermission requested' if intermission else 'native pending NextLevel set (no intermission scoreboard)',
                'effective CoopIntermission=1' if intermission else 'effective CoopIntermission=0'):
  assert marker in text,marker
 assert text.count('exit-seen')<=3*8,'exit logging must stay bounded per epoch'
 assert intermission or not ends,'default co-op mode must not call the native end-level (MID_MULTI_INTERMISSION)'
 assert 'intro-' not in text and 'CoopIntro' not in text and 'CoopTestStartWorld' not in text,'intro is opt-in'
 if music_on:
  assert 'effective CoopMusic=1' in text
  assert 'music-send client=273 name=Alice song=song5 intensity=1 transition=2 sent' in text
  assert 'music-send client=273 name=Alice song=song1 intensity=255 transition=2 sent' in text
 else:
  assert 'effective CoopMusic=0' in text and 'music-send' not in text and not [m for m in msgs if m[1] not in (0x48,0x69)]
 if '--trace-music' in sys.argv:
  # Drive the hooked WorldProperties call: bad50 is stubbed to record its arguments.
  music=[]
  def music_stub():
   sp=u.reg_read(UC_X86_REG_ESP);music.append((read(sp+4),read(sp+8),read(sp+12)))
   u.reg_write(UC_X86_REG_EAX,0x1234);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
  stubs[obj+0xbad50]=music_stub
  argv=strings+0x900;toks=[b'music1',b'ambient',b'3']
  for i,tk in enumerate(toks):u.mem_write(strings+0xa00+i*0x20,tk+b'\0');write(argv+4*i,strings+0xa00+i*0x20)
  call_regs(target[0x10494b],[obj+0x14c06c,3,argv],{},0)
  assert u.reg_read(UC_X86_REG_EAX)==0x1234 and music==[(obj+0x14c06c,3,argv)],music
  text=b''.join(log).decode('latin1')
  assert 'effective TraceMusic=1' in text and 'music-world argc=3 music1 ambient 3 event=created' in text
 session_lines=[l for l in text.splitlines() if ' seq=' in l]
 if '--trace-timing' in sys.argv:
  assert 'effective TraceTiming=1' in text
  assert session_lines and all(re.match(r'\[KPCFix Co-op\] t=\d+ g=\d+ e=\d+ seq=',l) for l in session_lines),session_lines[:3]
 else:
  assert 'TraceTiming' not in text and all(l.startswith('[KPCFix Co-op] g=') for l in session_lines),session_lines[:3]
 assert sbm()==([(0x111,0x48),(0x222,0x48)] if sb else []),'no scoreboard on natural exits or worlds without an intro'
 if sb:assert 'scoreboard show client=273 msg=0x48 sent' in text and 'native pending NextLevel deferred' in text
 print('PASS co-op session hooks:',len(said),'broadcasts,',len(ends),'native end-level requests,',len(nextlevels),'NextLevel calls, telefrag skipped')

call(proxy+ps['_DllMain'],[proxy,1,0],True)
u.mem_write(scratch+0x90000,b'KPC\0')
assert call(proxy+ps['_CreateServer'],[3,scratch+0x90000,scratch+0x90010])==17
# CoopSoundClock: server.dll call site 2a1fc (e8 0a fd ff ff -> sound-track
# updater 29f0b, thiscall, one float dt, ret 4). Installed only in a session
# with the sound clock on and the Europe-family server.dll hash.
sound_site=bytes(u.mem_read(engine+0x2a1fc,5))
intro_on='--intro' in sys.argv
script_on=coop_session and '--off' not in sys.argv and ('--script-clock' in sys.argv or (intro_on and '--no-script-clock' not in sys.argv))
sound_on=coop_session and '--off' not in sys.argv and ('--sound-clock' in sys.argv or (script_on and '--no-sound-clock' not in sys.argv))
if not sound_on:
 assert sound_site==bytes.fromhex('e80afdffff'),sound_site.hex()
else:
 assert sound_site[0]==0xe8,sound_site.hex()
 sound_target=(engine+0x2a1fc+5+struct.unpack('<i',sound_site[1:5])[0])&0xffffffff
 assert proxy<=sound_target<proxy+0x100000
 got=[]
 def sound_tracks():
  sp=u.reg_read(UC_X86_REG_ESP);got.append((u.reg_read(UC_X86_REG_ECX),read(sp+4)))
  u.reg_write(UC_X86_REG_ESP,sp+8);u.reg_write(UC_X86_REG_EIP,read(sp))
 stubs[engine+0x29f0b]=sound_tracks
 # 44.19 s narration: time left 44.69 s (2c6e6). Frames every 40 ms of wall
 # time, each credited 50 ms (the measured 1.25x), as the server sends them.
 dt=struct.unpack('<I',struct.pack('<f',0.05))[0];t0=clock[0];left=44.69;wall=None;mgr=0x30081234
 for i in range(3000):
  clock[0]+=40;call_regs(sound_target,[dt],{UC_X86_REG_ECX:mgr},4)
  left-=struct.unpack('<f',struct.pack('<I',got[-1][1]))[0]
  if left<=0:wall=clock[0]-t0;break
 assert all(m==mgr for m,_ in got),'the server manager is forwarded'
 assert max(struct.unpack('<f',struct.pack('<I',g))[0] for _,g in got)<=0.05+1e-7,'never a larger step than the engine'
 assert wall is not None and 44600<=wall<=44800,wall   # stock: 44.69/1.25 = 35.75 s
 del stubs[engine+0x29f0b]
 text=b''.join(log).decode('latin1')
 assert 'effective CoopSoundClock=1' in text and 'sound clock hook installed (server.dll 2a1fc -> 29f0b)' in text
 print('PASS co-op sound clock: 44.69 s narration timer expires after',wall,'ms of wall clock (stock 35750)')
if coop_session and '--off' not in sys.argv and not sound_on and script_on:
 text=b''.join(log).decode('latin1');assert 'effective CoopSoundClock=0' in text and 'sound clock hook installed' not in text
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
 if not coop_session:
  for rva,orig in ((0x93c5d,'0c10'),(0x93c65,'e896530600'),(0xea6d2,'e809e90000'),(0xea7c3,'e818e80000'),(0xeaa10,'e8cbe50000'),(0xeab11,'e8cae40000')):
   assert bytes(u.mem_read(obj+rva,len(orig)//2))==bytes.fromhex(orig),hex(rva)
 if coop_session:coop_scenario()
 call(proxy+ps['_DeleteServer'],[0x30080000]);assert setup_calls[-1]==('delete',0x30080000)
 print('PASS compiled DLL:',fixtures.name,'proxy relocation, export forwarding, IAT interception, object relocation, guarded memory patching, initial policy, idempotence, unchanged headers/relocations and unchanged original file')
