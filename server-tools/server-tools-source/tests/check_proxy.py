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
class StubTable(dict):
 """Stub address -> handler. Each address gets its own one-address code hook
 the first time it is set (Unicorn checks ranged hooks when it translates a
 block, so code elsewhere runs without a Python call per instruction). The
 hook looks the handler up on every hit, so replacing or deleting an entry
 keeps working; a deleted entry's hook does nothing."""
 def __init__(self):
  super().__init__();self.hooked=set()
 def __setitem__(self,at,handler):
  super().__setitem__(at,handler)
  if at not in self.hooked:
   self.hooked.add(at)
   u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:self[a]() if a in self else None,begin=at,end=at)
   u.ctl_remove_cache(at,at+1)   # in case the block at this address was already translated
console=[];log=[];file_handles={};hash_states={};stubs=StubTable();stub_index=0
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
# --winmm: the default install. The DLL is winmm.dll (dedicated.exe imports
# WINMM.dll) and the original engine keeps its name server.dll. Without it the
# earlier install: the DLL is server.dll and the engine server.original.dll.
winmm_mode='--winmm' in sys.argv
engine_name='server.dll' if winmm_mode else 'server.original.dll'
paths={None:prefix+'dedicated.exe',exe:prefix+'dedicated.exe',proxy:prefix+('winmm.dll' if winmm_mode else 'server.dll'),engine:prefix+engine_name,obj:prefix+'object.lto'}
files={prefix+'dedicated.exe':dedicated,prefix+engine_name:backend,prefix+'object.lto':object_file}
SYSWINMM=0x6f000000;sysdir='C:\\Windows\\system32'
winmm_loads=[]
if '--bad-object' in sys.argv:files[prefix+'object.lto']=object_file[:-1]+bytes([object_file[-1]^1])
def getfilename(a):
 s=paths.get(a[0],paths[None]);b=s.encode('utf-16-le')+b'\0\0';u.mem_write(a[1],b);return len(s)
functions['GetModuleFileNameW']=stub('GetModuleFileNameW',3,getfilename)
functions['GetModuleHandleW']=stub('GetModuleHandleW',1,lambda a:exe if not a[0] else api)
functions['GetModuleHandleA']=stub('GetModuleHandleA',1,lambda a:exe if not a[0] else api)
def loadlibraryw(a):
 name=wstr(a[0]);winmm_loads.append(name)
 if name==prefix+engine_name:return engine
 if name==sysdir+'\\winmm.dll':return SYSWINMM
 if name==paths[proxy]:return proxy
 return 0
functions['LoadLibraryW']=stub('LoadLibraryW',1,loadlibraryw)
def sysdirectory(a):
 u.mem_write(a[0],sysdir.encode('utf-16-le')+b'\0\0');return len(sysdir)
functions['GetSystemDirectoryW']=stub('GetSystemDirectoryW',2,sysdirectory)
other_loads=[]
def loadlibrarya(a):other_loads.append(cstr(a[0]));return 0x5555
functions['LoadLibraryA']=stub('LoadLibraryA',1,loadlibrarya)
# System winmm exports: timeGetTime answers 0x1234 (stdcall, no arguments).
sys_time=stub('sys_timeGetTime',0,lambda a:0x1234)
sys_other=stub('sys_winmm_other',0,lambda a:0)
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
 if key=='CoopCampaign':return int('--campaign' in sys.argv)
 if key=='CoopIntroSettleMs':return a[2]
 if key=='CoopIntroAll':return 0 if '--no-intro-all' in sys.argv else a[2]
 if key=='CoopStartScoreboard':return 0 if '--no-start-scoreboard' in sys.argv else a[2]
 if key=='CoopSpawners':return 0 if '--no-spawners' in sys.argv else a[2]
 if key=='TraceSpawners':return int('--trace-spawners' in sys.argv)
 if key=='CoopSinglePlayerStart':return 0 if '--no-sp-start' in sys.argv else a[2]
 if key=='TraceScripts':return int('--trace-scripts' in sys.argv)
 if key=='CoopScriptEvents':return 0 if '--no-script-events' in sys.argv else a[2]
 if key=='CoopFriendlyFire':return int('--friendly-fire' in sys.argv)
 if key=='CoopSharedItems':return 0 if '--no-shared-items' in sys.argv else a[2]
 if key=='CoopSpawnSpread':return 0 if '--no-spawn-spread' in sys.argv else a[2]
 if key=='CoopServerFrameMs':return 0 if '--no-server-frame' in sys.argv else a[2]
 if key=='CoopSkill':
  for x in sys.argv:
   if x.startswith('--skill='):return int(x.split('=',1)[1])&0xffffffff
  return a[2]
 if key=='CoopHealthBars':return 0 if '--no-health-bars' in sys.argv else a[2]
 if key=='CoopSpawnerNearest':return 0 if '--no-spawner-nearest' in sys.argv else a[2]
 if key=='CoopCheatKeys':return 0 if '--no-cheat-keys' in sys.argv else a[2]
 if key=='CoopKeepInventory':return 0 if '--no-keep-inventory' in sys.argv else a[2]
 if key=='CoopKeepOnDeath':return 0 if '--no-keep-on-death' in sys.argv else a[2]
 if key=='CoopPowerupResend':return 0 if '--no-powerup-resend' in sys.argv else a[2]
 if key=='CoopItemRespawnSec':return 0 if '--no-item-respawn' in sys.argv else a[2]
 if key=='CoopWindowShards':return 0 if '--no-window-shards' in sys.argv else a[2]
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
 if wstr(a[1])=='CoopLobbyWorld':
  for x in sys.argv:
   if x.startswith('--lobby='):value=x.split('=',1)[1]
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
 if module==SYSWINMM:return sys_time if s=='timeGetTime' else sys_other
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
        0x93c5d:(2,'0c10'),0x93c65:(5,'e896530600'),
        0xf12be:(5,'e8adfdffff'),0xf15f2:(5,'e8a9f2ffff'),
        0xc4238:(5,'e803110000'),0xb8677:(5,'e8c4cc0000'),0xc5358:(5,'e853020000'),
        0xc53a3:(5,'e8c83b0300'),0xc53c0:(5,'e8ab3b0300'),0x44dd6:(5,'e855f40700'),
        0xc1a89:(6,'8b88d0010000'),0xc1abe:(5,'e8dd320300'),0xc28a5:(6,'8b90d0010000'),
        0xc269b:(6,'8b88d0010000'),0xc2a75:(6,'8b90d0010000'),0xb8ad8:(6,'8b88d0010000'),
        0x2e205:(5,'e8f6060c00'),0x2ffea:(5,'e811e90b00'),0x3005a:(5,'e8a1e80b00'),
        0xf3c08:(6,'39b8f0010000'),0xcc145:(5,'e836000000'),0x30c9e:(5,'e8ed800c00'),
        0xfdfdb:(6,'ff90f0000000'),0xc8348:(6,'8b88d0010000'),0x409bb:(5,'e870380800'),0xb676f:(5,'e8cccc0300'),
        0xc2c9c:(5,'e87fcc0200'),0xc2c56:(5,'e8f5cc0200'),
        0xd51fb:(6,'ff90d0010000'),0xfead7:(5,'e834ceffff'),0xcc29e:(5,'e8cd020000'),0xcbfff:(5,'e87c000000'),
        0x48413:(6,'ff91fc010000'),0xc837b:(5,'e8a03d0000')}
 music_flag='--no-music' not in sys.argv
 intro_flag='--intro' in sys.argv
 trace_timing='--trace-timing' in sys.argv
 script_clock='--script-clock' in sys.argv or (intro_flag and '--no-script-clock' not in sys.argv)
 # The per-frame hook (ffbbf) runs in every session: music, intro, scoreboard,
 # inventory restore and the powerup re-send.
 hooked_optional={0x10494b:music_flag or '--trace-music' in sys.argv,0xffbbf:True,0x1045b1:intro_flag,
                  0xffba5:script_clock or trace_timing}
 spawner_nearest='--no-spawner-nearest' not in sys.argv;cheat_keys='--no-cheat-keys' not in sys.argv
 item_respawn='--no-item-respawn' not in sys.argv;keep_inv='--no-keep-inventory' not in sys.argv
 hooked_optional[0xd51fb]=spawner_nearest;hooked_optional[0xfead7]=cheat_keys
 hooked_optional[0xcc29e]=hooked_optional[0xcbfff]=hooked_optional[0xc837b]=item_respawn
 window_shards='--no-window-shards' not in sys.argv;hooked_optional[0x48413]=window_shards
 bars='--no-health-bars' not in sys.argv
 for rva in (0xea6d2,0xea7c3,0xeaa10,0xeab11):hooked_optional[rva]=bars
 spawners='--no-spawners' not in sys.argv;trace_spawners='--trace-spawners' in sys.argv
 hooked_optional[0x93c65]=trace_spawners
 sp_start='--no-sp-start' not in sys.argv;trace_scripts='--trace-scripts' in sys.argv
 hooked_optional[0xf12be]=hooked_optional[0xf15f2]=hooked_optional[0xfdfdb]=sp_start
 for rva in (0xc4238,0xb8677,0xc5358,0xc53a3,0xc53c0,0x44dd6):hooked_optional[rva]=trace_scripts
 script_events='--no-script-events' not in sys.argv
 for rva in (0xc1a89,0xc1abe,0xc28a5,0xc269b,0xc2a75,0xb8ad8,0x2e205,0x2ffea,0x3005a,0xc8348):hooked_optional[rva]=script_events
 friendly_fire='--friendly-fire' in sys.argv
 hooked_optional[0xf3c08]=not friendly_fire
 shared_items='--no-shared-items' not in sys.argv
 hooked_optional[0xcc145]=hooked_optional[0x30c9e]=hooked_optional[0xb676f]=shared_items
 # CMonsterSpawner forced MP-off (or al,0x10 at 93c5d): NOPs with CoopSpawners.
 assert bytes(u.mem_read(obj+0x93c5d,2))==(b'\x90\x90' if spawners else bytes.fromhex('0c10'))
 del sites[0x93c5d]
 # Campaign mode defaults CoopStartScoreboard to 0 (KPCFix clients show the loading picture).
 sb='--intermission' not in sys.argv and '--no-start-scoreboard' not in sys.argv and '--campaign' not in sys.argv
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
 # CoopSinglePlayerStart (default 1): spawn path 1 skips the multiplayer start
 # strategy f1070 (the engine's own single-player fallback runs); spawn path 2
 # runs the start lookup f08a0 with game type 0, restored afterwards.
 text=b''.join(log).decode('latin1')
 assert ('effective CoopSinglePlayerStart=1' if sp_start else 'effective CoopSinglePlayerStart=0') in text
 if sp_start:
  seen=[]
  def f08a0():
   sp=u.reg_read(UC_X86_REG_ESP);seen.append((u.reg_read(UC_X86_REG_ECX),read(shell+0x1d0),cstr(read(sp+4))))
   u.reg_write(UC_X86_REG_EAX,0x7171);u.reg_write(UC_X86_REG_ESP,sp+8);u.reg_write(UC_X86_REG_EIP,read(sp))
  def f1070():
   seen.append('f1070');sp=u.reg_read(UC_X86_REG_ESP)
   u.reg_write(UC_X86_REG_EAX,0x99);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
  stubs[obj+0xf08a0]=f08a0;stubs[obj+0xf1070]=f1070
  u.mem_write(strings+0xf00,b'PlayerStart0\0')
  call_regs(target[0xf15f2],[strings+0xf00],{UC_X86_REG_ECX:0x5555},4)
  assert u.reg_read(UC_X86_REG_EAX)==0x7171 and seen==[(0x5555,0,'PlayerStart0')],seen
  assert read(shell+0x1d0)==1,'game type restored'
  call_regs(target[0xf12be],[],{UC_X86_REG_ECX:0x5555},0)
  assert u.reg_read(UC_X86_REG_EAX)==0 and 'f1070' not in seen,seen
  text=b''.join(log).decode('latin1')
  assert 'spawn-start single-player start path=f15f2 name=PlayerStart0 found' in text and 'spawn-start single-player start path=f12be' in text
  # Client enter: the original attach call runs, then the waiting player is
  # moved to the single-player start (GetObjectPos/SetObjectPos on g_pServerDE).
  de_calls=[];start=strings+0xf80;write(start+8,0x5717);pl=strings+0xfc0;write(pl+8,0xa1)
  seen.clear()
  def f08a0b():
   sp=u.reg_read(UC_X86_REG_ESP);seen.append(read(shell+0x1d0));u.reg_write(UC_X86_REG_EAX,start)
   u.reg_write(UC_X86_REG_ESP,sp+8);u.reg_write(UC_X86_REG_EIP,read(sp))
  stubs[obj+0xf08a0]=f08a0b
  def de_fn(name,setpos=False):
   def run():
    sp=u.reg_read(UC_X86_REG_ESP);a0,a1=read(sp+4),read(sp+8)
    if name=='getpos':u.mem_write(a1,struct.pack('<3f',1.0,2.0,3.0))
    de_calls.append((name,a0,struct.unpack('<3f',u.mem_read(a1,12)) if name=='setpos' else a1))
    u.reg_write(UC_X86_REG_EIP,read(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
   return run
  dev=base+0xb000;write(obj+0x16de80,dev)
  for off,name in ((0xf0,'attach'),(0x1d0,'getpos'),(0x1e0,'setpos')):
   code_at=strings+0x1200+off;stubs[code_at]=de_fn(name);write(dev+off,code_at)
  call_regs(target[0xfdfdb],[0x111,pl],{},0)
  assert de_calls[0]==('attach',0x111,pl),de_calls
  assert de_calls[1][:2]==('getpos',0x5717) and de_calls[2]==('setpos',0xa1,(1.0,2.0,3.0)),de_calls
  assert seen==[0] and read(shell+0x1d0)==1,'game type 0 during the lookup, restored'
  assert 'enter player placed at the single-player start until it spawns' in b''.join(log).decode('latin1')
  del stubs[obj+0xf08a0];del stubs[obj+0xf1070]
  print('PASS co-op single-player start: f12be strategy skipped, f15f2 lookup with game type 0 and restored, waiting player placed on enter')
 # CoopScriptEvents (default 1): the five game-type gates read 0, and the
 # player event applies its command to every player in the engine list.
 text=b''.join(log).decode('latin1')
 assert ('effective CoopScriptEvents=1' if script_events else 'effective CoopScriptEvents=0') in text
 if script_events:
  for rva in (0xc1a89,0xc269b,0xb8ad8,0xc8348):   # c8348: armour touch, single-player rule
   call_regs(target[rva],[],{UC_X86_REG_ECX:0x1234},0);assert u.reg_read(UC_X86_REG_ECX)==0,hex(rva)
  for rva in (0xc28a5,0xc2a75):
   call_regs(target[rva],[],{UC_X86_REG_EDX:0x1234},0);assert u.reg_read(UC_X86_REG_EDX)==0,hex(rva)
  applied=[]
  def to_player():
   sp=u.reg_read(UC_X86_REG_ESP);h=read(read(sp+4));u.reg_write(UC_X86_REG_EAX,{0xa1:0x7a1,0xb2:0x7b2}.get(h,0))
   u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
  def f4da0():
   sp=u.reg_read(UC_X86_REG_ESP);applied.append((u.reg_read(UC_X86_REG_ECX),cstr(read(sp+4))));u.reg_write(UC_X86_REG_EAX,1)
   u.reg_write(UC_X86_REG_ESP,sp+8);u.reg_write(UC_X86_REG_EIP,read(sp))
  stubs[obj+0xee7d0]=to_player;stubs[obj+0xf4da0]=f4da0
  node=manager+0x100;write(manager+4,node)
  for i,h in enumerate((0xa1,0xc3,0xb2)):                       # 0xc3: a non-player object
   write(node,h);write(node+4,node+8 if i<2 else 0);node+=8
  u.mem_write(strings+0xf00,b'invulnerable true\0')
  call_regs(target[0xc1abe],[strings+0xf00],{UC_X86_REG_ECX:0x7a1},4)
  assert applied==[(0x7a1,'invulnerable true'),(0x7b2,'invulnerable true')],applied
  # Personal commands (teleport, animate, lookat...): only the activator.
  u.mem_write(strings+0xf40,b'teleport gauntletspot\0');applied.clear()
  call_regs(target[0xc1abe],[strings+0xf40],{UC_X86_REG_ECX:0x7a1},4)
  assert applied==[(0x7a1,'teleport gauntletspot')],applied        # no activator yet: first player
  def is_player():   # ee900 takes a POINTER to the HOBJECT (object.lto ee925: mov ecx,[esp+4]; mov edx,[ecx])
   sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,1 if read(read(sp+4)) in (0xa1,0xb2) else 0)
   u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
  stubs[obj+0xee900]=is_player
  ref_c3=strings+0x1180;ref_b2=strings+0x1184;trig=strings+0x1188;write(ref_c3,0xc3);write(ref_b2,0xb2);write(trig+0x54,0)
  for site in (0x2e205,0x2ffea,0x3005a):
   call_regs(target[site],[ref_c3],{UC_X86_REG_ECX:trig},0);assert u.reg_read(UC_X86_REG_EAX)&0xff==0     # not a player: not recorded
  call_regs(target[0x2e205],[ref_b2],{UC_X86_REG_ECX:trig},0);assert u.reg_read(UC_X86_REG_EAX)&0xff==1  # no command list: passes
  call_regs(target[0x2ffea],[ref_b2],{},0);assert u.reg_read(UC_X86_REG_EAX)&0xff==1   # Bob touched the trigger
  applied.clear();call_regs(target[0xc1abe],[strings+0xf40],{UC_X86_REG_ECX:0x7a1},4)
  assert applied==[(0x7b2,'teleport gauntletspot')],applied
  u.mem_write(strings+0xf80,b'giveitem gauntlets\0');applied.clear()
  call_regs(target[0xc1abe],[strings+0xf80],{UC_X86_REG_ECX:0x7a1},4)
  assert applied==[(0x7a1,'giveitem gauntlets'),(0x7b2,'giveitem gauntlets')],applied
  del stubs[obj+0xee900]
  assert 'script-event player teleport gauntletspot -> activator, players=1' in b''.join(log).decode('latin1')
  write(manager+4,0);applied.clear()                              # nobody in the list: the given player
  call_regs(target[0xc1abe],[strings+0xf00],{UC_X86_REG_ECX:0x7a1},4);assert applied==[(0x7a1,'invulnerable true')],applied
  del stubs[obj+0xee7d0];del stubs[obj+0xf4da0]
  assert 'script-event player invulnerable true -> players=2' in b''.join(log).decode('latin1')
  print('PASS co-op script events: five game-type gates open, shared commands to every player, personal commands to the activator')
 # CoopFriendlyFire=0 (default): damage from another player is skipped
 # (jump to f3f1e); self, monster and anonymous damage reach the stock
 # teamplay test (cmp [shell+0x1f0],edi) unchanged.
 assert ('effective CoopFriendlyFire=1' if friendly_fire else 'effective CoopFriendlyFire=0') in b''.join(log).decode('latin1')
 if not friendly_fire:
  skipped=[]
  def ff_skip():
   skipped.append(1);u.reg_write(UC_X86_REG_EIP,stop)
  def to_player2():
   sp=u.reg_read(UC_X86_REG_ESP);h=read(read(sp+4));u.reg_write(UC_X86_REG_EAX,0x7000 if h in (0xa1,0xb2) else 0)
   u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
  stubs[obj+0xf3f1e]=ff_skip;stubs[obj+0xee7d0]=to_player2
  me=strings+0xe00;rec=strings+0xe80;write(me+8,0xa1);write(shell+0x1f0,0)
  def hit(damager):
   skipped.clear();write(rec+0x20,damager)
   call_regs(target[0xf3c08],[],{UC_X86_REG_ESI:me,UC_X86_REG_EBP:rec,UC_X86_REG_EAX:shell,UC_X86_REG_EDI:1},0)
   assert u.reg_read(UC_X86_REG_ESI)==me and u.reg_read(UC_X86_REG_EBP)==rec and u.reg_read(UC_X86_REG_EDI)==1
   return bool(skipped),(u.reg_read(UC_X86_REG_EFLAGS)>>6)&1
  assert hit(0xb2)==(True,0)          # another player: no damage
  assert hit(0xa1)==(False,0)         # self: stock path, teamplay 0 -> ZF clear
  assert hit(0xc3)==(False,0)         # a monster
  assert hit(0)==(False,0)            # no damager
  write(shell+0x1f0,1);assert hit(0xa1)==(False,1);write(shell+0x1f0,0)   # stock compare replayed
  del stubs[obj+0xf3f1e];del stubs[obj+0xee7d0]
  print('PASS co-op friendly fire off: damage from another player skipped, self/monster damage unchanged')
 # CoopSharedItems (default 1): after the toucher's own pickup (cc180), a
 # progression item is given to every other player with f3440(id,1,1), once
 # per item object; health/ammo/charms are not shared.
 assert ('effective CoopSharedItems=1' if shared_items else 'effective CoopSharedItems=0') in b''.join(log).decode('latin1')
 if shared_items:
  picked=[];gave=[]
  def cc180():
   sp=u.reg_read(UC_X86_REG_ESP);picked.append((u.reg_read(UC_X86_REG_ECX),read(sp+4)))
   u.reg_write(UC_X86_REG_ESP,sp+8);u.reg_write(UC_X86_REG_EIP,read(sp))
  def f3440():
   sp=u.reg_read(UC_X86_REG_ESP);gave.append((u.reg_read(UC_X86_REG_ECX),read(sp+4),read(sp+8),read(sp+12)));u.reg_write(UC_X86_REG_EAX,1)
   u.reg_write(UC_X86_REG_ESP,sp+16);u.reg_write(UC_X86_REG_EIP,read(sp))
  def to_player3():
   sp=u.reg_read(UC_X86_REG_ESP);h=read(read(sp+4));u.reg_write(UC_X86_REG_EAX,{0xa1:pa,0xb2:pb}.get(h,0))
   u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
  pa=strings+0xd00;pb=strings+0xd80;write(pa+8,0xa1);write(pb+8,0xb2)
  stubs[obj+0xcc180]=cc180;stubs[obj+0xf3440]=f3440;stubs[obj+0xee7d0]=to_player3
  node=manager+0x100;write(manager+4,node)
  for i,h in enumerate((0xa1,0xb2)):
   write(node,h);write(node+4,node+8 if i<1 else 0);node+=8
  item=strings+0xc00;item2=strings+0xc80;dfn=strings+0xbc0;dfn2=strings+0xbe0
  write(item+0x54,dfn);write(dfn,0x27);write(item2+0x54,dfn2);write(dfn2,0x17)
  if script_events:u.mem_write(obj+0x13e1b8,b'\x01')            # piece given at the touch (ReallyGiveArmor)
  call_regs(target[0xcc145],[pa],{UC_X86_REG_ECX:item},4)
  assert picked==[(item,pa)] and gave==[(pb,0x27,1,1)],(picked,gave)
  call_regs(target[0xcc145],[pb],{UC_X86_REG_ECX:item},4)        # same item again: not shared twice
  assert gave==[(pb,0x27,1,1)] and len(picked)==2,gave
  call_regs(target[0xcc145],[pa],{UC_X86_REG_ECX:item2},4)       # health: not shared
  assert gave==[(pb,0x27,1,1)] and len(picked)==3,gave
  write(dfn,0x09);item3=strings+0xb00;write(item3+0x54,dfn)       # ZeroCannon
  call_regs(target[0xcc145],[pb],{UC_X86_REG_ECX:item3},4)
  assert gave[-1]==(pa,0x09,1,1),gave
  text=b''.join(log).decode('latin1')
  assert 'shared item=Gauntlets id=39 given to 1 other player(s)' in text and 'shared item=ZeroCannon id=9' in text,text[-400:]
  if script_events:
   # Single-player armour rule: a piece not given at the touch is left to its cutscene's giveitem.
   u.mem_write(obj+0x13e1b8,b'\x00');item4=strings+0xa80;dfn4=strings+0xa40;write(item4+0x54,dfn4);write(dfn4,0x28)
   n_gave=len(gave);call_regs(target[0xcc145],[pa],{UC_X86_REG_ECX:item4},4)
   assert len(gave)==n_gave and 'armour piece touched; its cutscene gives it' in b''.join(log).decode('latin1')
  # "inventory add key" (b676f, first player only in stock): every player gets it, the finder is named.
  gave.clear();call_regs(target[0xb676f],[0x24,1,1],{UC_X86_REG_ECX:pa},12)
  assert gave==[(pa,0x24,1,1),(pb,0x24,1,1)],gave
  text=b''.join(log).decode('latin1')
  assert 'inventory add item=Key id=36 players=2' in text and 'found the key (everyone has it)' in text,text[-400:]
  gave.clear();call_regs(target[0xb676f],[0x19,1,1],{UC_X86_REG_ECX:pa},12)
  assert gave==[(pa,0x19,1,1),(pb,0x19,1,1)] and b''.join(log).decode('latin1').count('everyone has it')==1,gave
  # Ammo: the amount the picker received (dd190 before/after) goes to every other player.
  counts={}
  def dd190():
   sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,counts.get((u.reg_read(UC_X86_REG_ECX),read(sp+4)),0))
   u.reg_write(UC_X86_REG_ESP,sp+8);u.reg_write(UC_X86_REG_EIP,read(sp))
  inva=strings+0x1300;invb=strings+0x1340;write(pa+0x54,inva);write(pb+0x54,invb)
  def cc180_ammo():
   sp=u.reg_read(UC_X86_REG_ESP);counts[(inva,0x11)]=counts.get((inva,0x11),0)+30
   u.reg_write(UC_X86_REG_ESP,sp+8);u.reg_write(UC_X86_REG_EIP,read(sp))
  stubs[obj+0xdd190]=dd190;stubs[obj+0xcc180]=cc180_ammo
  ammo=strings+0x1380;dfna=strings+0x13c0;write(ammo+0x54,dfna);write(dfna,0x11)
  gave.clear();call_regs(target[0xcc145],[pa],{UC_X86_REG_ECX:ammo},4)
  assert gave==[(pb,0x11,30,1)],gave
  gave.clear();call_regs(target[0xcc145],[pa],{UC_X86_REG_ECX:ammo},4)   # every pickup is shared
  assert gave==[(pb,0x11,30,1)],gave
  assert 'shared ammo=MagmaAmmo amount=30 to 1 other player(s)' in b''.join(log).decode('latin1')
  del stubs[obj+0xdd190];stubs[obj+0xcc180]=cc180
  # Cutscene camera: every player's server view position follows the camera.
  views=[]
  def ef920():
   sp=u.reg_read(UC_X86_REG_ESP);views.append(('set',u.reg_read(UC_X86_REG_ECX),read(sp+4)))
   u.reg_write(UC_X86_REG_ESP,sp+8);u.reg_write(UC_X86_REG_EIP,read(sp))
  def ef950():
   sp=u.reg_read(UC_X86_REG_ESP);views.append(('reset',u.reg_read(UC_X86_REG_ECX)))
   u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
  stubs[obj+0xef920]=ef920;stubs[obj+0xef950]=ef950
  call_regs(target[0xc2c9c],[0x7777],{UC_X86_REG_ECX:pa},4)
  assert views==[('set',pa,0x7777),('set',pb,0x7777)],views
  views.clear();call_regs(target[0xc2c56],[],{UC_X86_REG_ECX:pa},0)
  assert views==[('reset',pa),('reset',pb)],views
  del stubs[obj+0xef920];del stubs[obj+0xef950]
  print('PASS co-op ammo shared by received amount; cutscene view position for every player')
  write(manager+4,0)
  del stubs[obj+0xcc180];del stubs[obj+0xf3440];del stubs[obj+0xee7d0]
  # Secret found: the message goes to every session member (cdecl, caller pops).
  msgs2=[]
  def f8d90():
   sp=u.reg_read(UC_X86_REG_ESP);msgs2.append(tuple(read(sp+4+4*k) for k in range(4)))
   u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
  stubs[obj+0xf8d90]=f8d90
  # No session member yet at this point of the scenario: the finder only.
  call_regs(target[0x30c9e],[0x999,1,0x40400000,0x4444],{},0)
  assert msgs2==[(0x999,1,0x40400000,0x4444)],msgs2
  assert 'secret found; message sent to every player' in b''.join(log).decode('latin1')
  del stubs[obj+0xf8d90]
  print('PASS co-op shared items: weapons/armour/pamphlets/keys to every player once per item, health not shared')
 played_lists=[]
 def c4230():
  sp=u.reg_read(UC_X86_REG_ESP);played_lists.append(u.reg_read(UC_X86_REG_ECX))
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
 stubs[obj+0xc4230]=c4230
 call_regs(target[0x409bb],[],{UC_X86_REG_ECX:0x5150,UC_X86_REG_ESI:strings+0x1100},0)
 assert played_lists==[0x5150],played_lists
 del stubs[obj+0xc4230]
 print('PASS co-op deco use: command list played unchanged, use object remembered for exit re-arming')
 if trace_scripts:
  played=[]
  def c5340():
   sp=u.reg_read(UC_X86_REG_ESP);played.append(cstr(read(sp+4)));u.reg_write(UC_X86_REG_EAX,0)
   u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
  def c55b0():
   sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,1)
   u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
  stubs[obj+0xc5340]=c5340;stubs[obj+0xc55b0]=c55b0
  u.mem_write(strings+0xf00,b'mapdata\\realm1\\r1m1\\r1m1a\\r1m1a_bigdoor.scr\0')
  call_regs(target[0xb8677],[strings+0xf00],{},0)
  call_regs(target[0xc4238],[strings+0xf00],{},0)
  call_regs(target[0xc5358],[strings+0xf00],{},0);assert u.reg_read(UC_X86_REG_EAX)&0xff==1
  call_regs(target[0xc53a3],[0x1013df14,strings+0xf00],{},0)
  assert played==['mapdata\\realm1\\r1m1\\r1m1a\\r1m1a_bigdoor.scr']*2,played
  text=b''.join(log).decode('latin1')
  for line in ('script scriptplay mapdata','script command-play mapdata','r1m1a_bigdoor.scr (already running)','r1m1a_bigdoor.scr (not loaded)'):
   assert line in text,line
  assert 'effective TraceScripts=1' in text
  del stubs[obj+0xc5340];del stubs[obj+0xc55b0]
  print('PASS co-op TraceScripts: scriptplay/command starts and refusals logged, calls forwarded')
 def player(i,client,hobj,name):
  at=players+i*0x400;write(at+8,hobj);write(at+0x120,client);u.mem_write(at+0x1e8,name.encode()+b'\0');return at
 alice=player(0,0x111,0xa1,'Alice');bob=player(1,0x222,0xb2,'Bob')
 # Default HOBJECT -> CPlayer* (ee7d0) for the per-frame inventory code. The
 # fake players have no vtable, so they read as dead: no snapshots are taken
 # until coop_new_features() gives Alice one.
 def to_player_default():
  sp=u.reg_read(UC_X86_REG_ESP);h=read(read(sp+4));u.reg_write(UC_X86_REG_EAX,{0xa1:alice,0xb2:bob}.get(h,0))
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
 stubs[obj+0xee7d0]=to_player_default
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
 if '--campaign' in sys.argv:
  # CoopCampaign=1: WORLDORDER graph and host selection. The emulated maplist
  # holds only R1M2B, R1M2C, R1M3A, so campaign worlds are written into the
  # slot NextLevel takes next (memory only).
  text=b''.join(log).decode('latin1')
  assert 'effective CoopCampaign=1 (WORLDORDER graph, 48 worlds' in text
  assert 'effective CoopStartScoreboard=0' in text,'campaign default: no start scoreboard'
  roster(0xa1);spawn(alice);roster(0xa1,0xb2);spawn(bob)
  assert any('full campaign (R1M0..R5M1B, 48 worlds)' in m for m in chat(bob,'!campaign'))
  assert any('Only the host' in m for m in chat(bob,'!campaign fire'))
  assert any('Use: !campaign full' in m for m in chat(alice,'!campaign earth realm fire')),'malformed: help, not silence'
  assert changes()==0
  # !campaign <realm> starts at once from that realm (no !ready needed).
  out=chat(alice,'!campaign earth')
  assert any('Host started from Earth (R2M0..R5M1B, 36 worlds) -> R2M0' in m for m in out),out
  if sb:frame(1100)
  assert changes()==1
  cur=read(obj+0x16da88);slot=cur+1 if cur+1<3 else 0
  assert cstr(shell+0x210+slot*0x7f)=='worlds\\realm2\\r2m0',cstr(shell+0x210+slot*0x7f)
  text=b''.join(log).decode('latin1');assert 'maplist slot=%d world=worlds\\realm2\\r2m0'%slot in text
  next_level()
  # The NextLevel stub advances the cursor exactly like fc640: it now names R2M0.
  assert read(obj+0x16da88)==slot
  u.mem_write(shell+0x210+slot*0x7f,b'worlds\\realm2\\R2M0\0')   # as the engine would show it
  spawn(alice);assert any('R2M0 started (epoch 1)' in m for m in said),said[-3:]
  # Natural exit to the WORLDORDER successor; anything else is refused.
  n=len(said);exit_trigger('worlds/realm2/r2m1b',activator=0xa1);assert changes()==1
  assert any('not the next world of the selected route' in m for m in said[n:]),said[n:]
  exit_trigger('worlds/realm2/r2m1a',activator=0xa1);assert changes()==2
  next_level();spawn(alice)
  # Host abort returns everybody to the lobby: CoopLobbyWorld when it is a
  # deathmatch map, else a random deathmatch map PM01-PM10 (never a campaign world).
  out=chat(alice,'!abort');lobby='--lobby=' in ' '.join(sys.argv)
  assert any(('-> lobby PM01' in m) if lobby else re.search(r'Host aborted -> lobby PM(0[1-9]|10)',m) for m in out),out
  assert changes()==3
  next_level()
  assert any('LOBBY' in m for m in chat(alice,'!coop')),'back in the lobby phase'
  assert not any('Starting' in m for m in chat(bob,'!start')),'only the host starts'
  # !changelevel (host): straight from the lobby into a world missing from the maplist.
  assert any('Only the host' in m for m in chat(bob,'!changelevel R1M2A')) and changes()==3
  assert any('Unknown world' in m for m in chat(alice,'!changelevel R7M1A')) and changes()==3
  out=chat(alice,'!changelevel worlds\\realm4\\R4M1A')
  assert any('Host changed level (campaign continues from R4M1A) -> R4M1A' in m for m in out),out
  assert changes()==4
  cur=read(obj+0x16da88);slot=cur+1 if cur+1<3 else 0
  assert cstr(shell+0x210+slot*0x7f)=='worlds\\realm4\\r4m1a',cstr(shell+0x210+slot*0x7f)
  print('PASS co-op campaign: selection, start into a world missing from the maplist, successor-only exits, abort to lobby, !changelevel')
  return
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
 if '--no-keep-inventory' not in sys.argv:
  assert 'inventory-cleared fresh start from the lobby: 0 saved inventories cleared' in b''.join(log).decode('latin1'),'a start from the lobby forgets the saved inventories'
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
 # 2026-11-12 playtest follow-ups: spawner reference, pickup respawn, cheat
 # keybinds, !kill/!class/!all, inventory restore, powerup re-send.
 def coop_new_features():
  nb=base+0x20000;dev=read(obj+0x16de80);pos={}
  def getpos():
   sp=u.reg_read(UC_X86_REG_ESP);h,out=read(sp+4),read(sp+8)
   u.mem_write(out,struct.pack('<3f',*pos.get(h,(0.0,0.0,0.0))))
   u.reg_write(UC_X86_REG_EIP,read(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
  def tcall(n,fn):          # thiscall: ECX + n stack args, callee pops
   def run():
    sp=u.reg_read(UC_X86_REG_ESP);r=fn(u.reg_read(UC_X86_REG_ECX),[read(sp+4+4*k) for k in range(n)])
    if r is not None:u.reg_write(UC_X86_REG_EAX,r)
    u.reg_write(UC_X86_REG_ESP,sp+4+4*n);u.reg_write(UC_X86_REG_EIP,read(sp))
   return run
  def ccall(n,fn):          # cdecl: caller pops
   def run():
    sp=u.reg_read(UC_X86_REG_ESP);r=fn([read(sp+4+4*k) for k in range(n)])
    if r is not None:u.reg_write(UC_X86_REG_EAX,r)
    u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
   return run
  gp=nb+0x1f00;stubs[gp]=getpos;old1d0=read(dev+0x1d0);write(dev+0x1d0,gp)
  fl=lambda a:struct.unpack('<f',bytes(u.mem_read(a,4)))[0]
  logtext=lambda:b''.join(log).decode('latin1')
  # Spawners: a player reference is replaced by the nearest live player.
  roster(0xa1,0xb2)
  pos.update({0xa1:(0.0,0.0,0.0),0xb2:(1000.0,0.0,0.0),0x5f0:(990.0,0.0,0.0),0x5f1:(7.0,8.0,9.0)})
  spw=nb;write(spw+8,0x5f0);write(spw+0x170,0xa1);out=nb+0x200
  if spawner_nearest:
   call_regs(target[0xd51fb],[0xa1,out],{UC_X86_REG_ESI:spw,UC_X86_REG_EAX:dev},0)
   assert struct.unpack('<3f',u.mem_read(out,12))==(1000.0,0.0,0.0) and read(spw+0x170)==0xb2
   assert u.reg_read(UC_X86_REG_ESI)==spw
   call_regs(target[0xd51fb],[0x5f1,out],{UC_X86_REG_ESI:spw,UC_X86_REG_EAX:dev},0)   # a trigger: measured as before
   assert struct.unpack('<3f',u.mem_read(out,12))==(7.0,8.0,9.0) and read(spw+0x170)==0xb2
   roster(0xa1);write(spw+0x170,0xa1)                                                  # alone: unchanged
   call_regs(target[0xd51fb],[0xa1,out],{UC_X86_REG_ESI:spw,UC_X86_REG_EAX:dev},0)
   assert struct.unpack('<3f',u.mem_read(out,12))==(0.0,0.0,0.0) and read(spw+0x170)==0xa1
   assert 'spawner reference moved to the nearest player' in logtext()
   print('PASS co-op spawners: activation radius from the nearest player, trigger references unchanged')
  else:assert 'effective CoopSpawnerNearest=0' in logtext()
  roster(0xa1,0xb2)
  # Pickups: respawnable items get the stock respawn countdown; the respawn
  # waits while a player stands on the spot.
  if item_respawn:
   hides=[];spawned=[]
   stubs[obj+0xcc570]=tcall(1,lambda this,a:hides.append((this,a[0],u.mem_read(this+0xb4,1)[0],fl(this+0xb8),u.mem_read(this+0x81,1)[0])))
   stubs[obj+0xcc080]=tcall(0,lambda this,a:spawned.append(this))
   def item_of(i,iid,e4=0):
    it=nb+0x400+i*0x100;d=nb+0x1000+i*0x10;u.mem_write(it,bytes(0x100));write(it+0x54,d);write(d,iid);write(it+8,0x1e0+i)
    u.mem_write(it+0xe4,bytes([e4]));return it
   it=item_of(0,9);call_regs(target[0xcc29e],[0],{UC_X86_REG_ECX:it},4);assert hides[-1]==(it,0,1,15.0,1),hides
   for i,(iid,force,e4) in enumerate(((0x19,0,0),(0x26,0,0),(9,1,0),(0x11,0,1))):   # pamphlet, ARMOR_START marker, forced, expiring
    it2=item_of(i+1,iid,e4);call_regs(target[0xcc29e],[force],{UC_X86_REG_ECX:it2},4);assert hides[-1][2]==0,(iid,hides[-1])
   it3=item_of(5,0x1e);call_regs(target[0xcc29e],[0],{UC_X86_REG_ECX:it3},4);assert hides[-1][2]==1   # BlackDiamond
   # Armour pieces (the game's "armor shards", 0x27-0x2c) now respawn too.
   arm=item_of(6,0x29);call_regs(target[0xcc29e],[0],{UC_X86_REG_ECX:arm},4);assert hides[-1]==(arm,0,1,15.0,1),hides
   # Their touch c8340 -> cc120 (site c837b): the first pickup is stock; a
   # returned piece is given quietly to a player who lacks it, then hidden again.
   saved={k:stubs.get(obj+k) for k in (0xcc120,0xee7d0,0xf25f0,0xf3440)}
   picks=[];gives=[];has={}
   pl={0xa1:nb+0x2000,0xb2:nb+0x2400};vt=nb+0x2800;alive=nb+0x2900
   stubs[alive]=tcall(0,lambda this,a:0);write(vt+0x60,alive)
   for h,p in pl.items():
    u.mem_write(p,bytes(0x300));write(p,vt);u.mem_write(p+0x1e8,(b'Alice' if h==0xa1 else b'Bob')+b'\0')
   stubs[obj+0xcc120]=tcall(2,lambda this,a:picks.append((this,a[0])) or 1)
   stubs[obj+0xee7d0]=ccall(1,lambda a:pl.get(read(a[0]),0))
   stubs[obj+0xf25f0]=tcall(1,lambda this,a:has.get((this,a[0]),0))
   stubs[obj+0xf3440]=tcall(3,lambda this,a:gives.append((this,a[0],a[1])) or 1)
   first=item_of(7,0x29);n=len(hides)
   call_regs(target[0xc837b],[0xa1,0],{UC_X86_REG_ECX:first},8)
   assert picks==[(first,0xa1)] and gives==[] and len(hides)==n,'first pickup: stock path (cutscene, script)'
   write(first+0xb4,1)                                                    # brought back by the respawn
   has[(pl[0xa1],0x29)]=1
   call_regs(target[0xc837b],[0xa1,0],{UC_X86_REG_ECX:first},8)
   assert len(picks)==1 and gives==[] and len(hides)==n and u.reg_read(UC_X86_REG_EAX)&0xff==0,'Alice has it: walks over it'
   call_regs(target[0xc837b],[0xb2,0],{UC_X86_REG_ECX:first},8)
   assert len(picks)==1 and gives==[(pl[0xb2],0x29,1)] and hides[-1]==(first,0,1,15.0,1),(gives,hides[-1:])
   assert 'quiet pickup item=Belt by Bob' in logtext() or 'quiet pickup item=' in logtext(),logtext()[-300:]
   other=item_of(8,9);call_regs(target[0xc837b],[0xb2,0],{UC_X86_REG_ECX:other},8);assert picks[-1]==(other,0xb2),'not armour: stock'
   for k,v in saved.items():
    if v is None:stubs.pop(obj+k,None)
    else:stubs[obj+k]=v
   pos[0x1e0]=(0.0,0.0,0.0);u.mem_write(it+0x7c,struct.pack('<f',-0.01))
   call_regs(target[0xcbfff],[],{UC_X86_REG_ECX:it},0)
   assert spawned==[] and fl(it+0x7c)==0.5,'Alice stands on it: wait and retry'
   pos[0xa1]=(300.0,0.0,0.0);call_regs(target[0xcbfff],[],{UC_X86_REG_ECX:it},0);assert spawned==[it]
   pos[0xa1]=(0.0,0.0,0.0)
   assert 'item-respawn scheduled item=ZeroCannon' in logtext() and 'item-respawn respawned item=ZeroCannon' in logtext()
   del stubs[obj+0xcc570];del stubs[obj+0xcc080]
   print('PASS co-op pickup respawn: weapons, BlackDiamonds and armour pieces scheduled, returned armour picked up quietly, progression/forced/expiring items not, spot must be clear')
  else:assert 'effective CoopItemRespawnSec=0' in logtext()
  # Window break (483cc): the hide at 48413 keeps the visible bit until the
  # shard message 0x39 has gone out; the frame hook clears it 200 ms later.
  if window_shards:
   setf=[];old1f8,old1fc=read(dev+0x1f8),read(dev+0x1fc)
   stubs[nb+0x1f10]=ccall(2,lambda a:setf.append((a[0],a[1])));write(dev+0x1fc,nb+0x1f10)
   stubs[nb+0x1f20]=ccall(1,lambda a:0x2011 if a[0]==0x600 else 0);write(dev+0x1f8,nb+0x1f20)
   call_regs(target[0x48413],[0x600,0x10],{},0)
   assert setf==[(0x600,0x11)],setf
   frame(100);assert setf==[(0x600,0x11)],'still visible while the message is read'
   frame(150);assert setf==[(0x600,0x11),(0x600,0x2010)],setf
   frame(500);assert len(setf)==2,'hidden once'
   call_regs(target[0x48413],[0x600,0x2011],{},0);assert setf[-1]==(0x600,0x2011),'a flag change that keeps it visible passes through'
   assert 'window break: kept visible 200 ms for the shard message' in logtext()
   write(dev+0x1f8,old1f8);write(dev+0x1fc,old1fc)
   print('PASS co-op window shards: hide deferred 200 ms after the break message')
  else:assert 'effective CoopWindowShards=0' in logtext()
  # Per-player cheats: inventory stubs shared by the cheat, class and restore checks.
  state0,state4=read(obj+0x174f00),read(obj+0x174f04);write(obj+0x174f00,1);write(obj+0x174f04,1)
  has={};counts={};gives=[];classes=[];dmg=[];dead=[0]
  inv=nb+0xc00;write(alice+0x54,inv);write(alice+0x264,0);vt_a=nb+0xa00;write(alice,vt_a)
  stubs[obj+0xdd190]=tcall(1,lambda this,a:counts.get(a[0],0) if this==inv else 0)
  # Weapon ownership is a node in the inventory list (dd170); f25f0 answers
  # only for armour pieces (object.lto f25f0-f2666).
  stubs[obj+0xf25f0]=tcall(1,lambda this,a:has.get(a[0],0) if this==alice and 0x27<=a[0]<=0x2c else 0)
  stubs[obj+0xdd170]=tcall(1,lambda this,a:(0x7000+a[0] if has.get(a[0]) else 0) if this==inv and 2<=a[0]<=0x0e else 0)
  def give(this,a):
   gives.append((this,a[0],a[1],a[2]))
   if a[0]<=0x0e or 0x27<=a[0]<=0x2c:has[a[0]]=1
   else:counts[a[0]]=counts.get(a[0],0)+a[1]
   return 1
  stubs[obj+0xf3440]=tcall(3,give)
  stubs[obj+0xf2090]=tcall(1,lambda this,a:classes.append((this,a[0],read(this+0x264))))
  write(vt_a+0x60,nb+0xb60);stubs[nb+0xb60]=tcall(0,lambda this,a:dead[0])
  write(vt_a+0x50,nb+0xb50);stubs[nb+0xb50]=tcall(1,lambda this,a:dmg.append(bytes(u.mem_read(a[0],0x34))))
  if cheat_keys:
   flags=obj+0x16d8a0;fns=obj+0x16d98c;calls=[]
   for cid in (0x2f,0x31,0x39):write(flags+4*cid,3)
   for cid,off in ((0x2f,0xb10),(0x31,0xb20),(0x39,0xb30)):
    write(fns+4*cid,nb+off);stubs[nb+off]=ccall(4,lambda a,cid=cid:(calls.append((cid,a[0])),has.__setitem__(0x27,1) if cid==0x39 else None)[0])
   stubs[obj+0xf4370]=ccall(1,lambda a:alice if a[0]==0x111 else 0)
   seen=[]
   def fb910():
    sp=u.reg_read(UC_X86_REG_ESP);cid=read(sp+8)        # the emulated message carries the cheat id
    seen.append((read(flags+4*cid)&3,read(fns+4*cid)))
    u.reg_write(UC_X86_REG_EIP,read(fns+4*cid))         # dispatch exactly as fb910's table call
   stubs[obj+0xfb910]=fb910
   n=len(said);call_regs(target[0xfead7],[0x111,0x39],{},0)
   assert seen[-1][0]==0 and proxy<=seen[-1][1]<proxy+0x100000 and calls==[(0x39,0x111)],(seen,calls)
   assert read(flags+4*0x39)==3 and read(fns+4*0x39)==nb+0xb30,'table restored after the message'
   assert any('Alice used next armor: Gauntlets (key)' in m for m in said[n:]),said[n:]
   call_regs(target[0xfead7],[0x111,0x2f],{},0);assert calls[-1]==(0x2f,0x111)
   assert any('Alice used gimmie gimmie gimmie: all weapons and ammo (key)' in m for m in said)
   classes.clear();call_regs(target[0xfead7],[0x111,0x31],{},0)
   assert classes==[(alice,1,1)] and read(alice+0x264)==1 and (0x31,0x111) not in calls
   assert any('Alice used cycle player class: now Beast King (key)' in m for m in said)
   write(obj+0x174f00,0);n=len(said);calls.clear();call_regs(target[0xfead7],[0x111,0x39],{},0)
   assert not calls and any('Cheats are disabled on this server (host console: kpc_cheats 1): next armor refused for Alice' in m for m in said[n:]),said[n:]
   n=len(said);call_regs(target[0xfead7],[0x111,0x39],{},0);assert len(said)==n,'repeated key: refused once per 3 s'
   n=len(said);call_regs(target[0xfead7],[0x111,0x2f],{},0)
   assert not calls and any('gimmie gimmie gimmie refused for Alice' in m for m in said[n:]),'another refused cheat still says so'
   classes.clear();call_regs(target[0xfead7],[0x111,0x31],{},0)
   assert classes and read(alice+0x264)==2,'class cycle key needs no cheat permission'
   write(alice+0x264,1);write(obj+0x174f00,1)
   del stubs[obj+0xfb910];del stubs[obj+0xf4370]
   print('PASS co-op cheat keys: gimmie/class/next armor per player under kpc_cheats, stock table restored')
  else:assert 'effective CoopCheatKeys=0' in logtext()
  # Chat: !kill (no permission), !class, !all (kpc_cheats).
  out=chat(alice,'!kill');assert any('Alice used !kill' in m for m in out),out
  rec=dmg[-1];assert struct.unpack('<f',rec[0:4])[0]==100000.0 and struct.unpack('<2I',rec[0x1c:0x24])==(0xa1,0xa1)
  assert struct.unpack('<I',rec[0x24:0x28])[0]==0x400 and rec[0x2c]==1
  dead[0]=1;n=len(dmg);chat(alice,'!kill');assert len(dmg)==n,'already dead';dead[0]=0
  write(alice+0x264,0);classes.clear();out=chat(alice,'!class demon')
  assert read(alice+0x264)==3 and classes==[(alice,1,3)] and any('Alice is now Demon' in m for m in out),out
  out=chat(alice,'!class');assert read(alice+0x264)==0 and any('Alice is now Starbearer' in m for m in out),out
  out=chat(alice,'!class wizard');assert any('Use: !class' in m for m in out) and read(alice+0x264)==0
  has.clear();has[0x27]=1;counts.clear();gives.clear();out=chat(alice,'!all')
  assert any('Alice used !all' in m for m in out),out
  assert [g[1] for g in gives]==[0x28,0x29,0x2a,0x2b,0x2c]+list(range(2,15))+[0x11,0x12,0x13,0x14],gives
  assert all(g[2]==1000 for g in gives[-4:])
  write(obj+0x174f00,0);n=len(gives);out=chat(alice,'!all');assert len(gives)==n and any('Cheats are disabled' in m for m in out)
  out=chat(alice,'!class demon');assert read(alice+0x264)==3 and any('Alice is now Demon' in m for m in out),'!class needs no cheat permission'
  assert any('Cheats are disabled on this server (host console: kpc_cheats 1): !all refused for Alice' in m for m in said),said[-4:]
  write(alice+0x264,0);write(obj+0x174f00,1)
  # Class change keeps the slots filled: a class-specific weapon the player owns
  # is matched by the new class's weapon of its group (def+8 class, def+0xc any).
  wcls={2:0xff,3:1,4:3,5:0,6:2,7:0xff,8:0xff,9:0xff,10:0xff,11:3,12:2,13:1,14:0}
  for wid,c in wcls.items():
   wo=nb+0x1800+wid*0x10;wd=nb+0x1a00+wid*0x10;write(alice+0x54+wid*4,wo);write(wo+8,wd)
   write(wd+8,c if c<4 else 0);u.mem_write(wd+0xc,bytes([1 if c==0xff else 0]))
  has.clear();has.update({0x27:1,5:1,8:1,14:1});gives.clear();write(alice+0x264,0)
  out=chat(alice,'!class demon');assert read(alice+0x264)==3,out
  assert sorted(g[1] for g in gives)==[4,11],gives
  assert 'class-change class=Demon weapons-before=3 class-weapons-given=2' in logtext(),logtext()[-400:]
  gives.clear();chat(alice,'!class demon');assert gives==[],'already has the Demon weapons'
  for wid in wcls:write(alice+0x54+wid*4,0)
  has.clear();write(alice+0x264,0)
  print('PASS co-op chat: !kill self damage, !class by name or cycle, !all under kpc_cheats')
  # Inventory: snapshot every second while alive, restored after a respawn.
  if keep_inv:
   has.clear();counts.clear();has.update({3:1,8:1,0x27:1});counts[0x11]=120;write(alice+0x264,2)
   frame(1100)
   has.clear();counts.clear();write(alice+0x264,0);gives.clear();classes.clear()
   spawn(alice,path=0xf167a);frame(100);assert gives==[],'restore waits for the spawn to finish'
   frame(400)
   if '--no-keep-on-death' in sys.argv:
    assert gives==[] and classes==[] and 'inventory-restored' not in logtext(),'a death in the same world keeps the stock fresh inventory'
    assert 'CoopKeepOnDeath=0' in logtext()
    print('PASS co-op inventory: CoopKeepOnDeath=0 leaves a respawn in the same world fresh')
   else:
    assert classes==[(alice,1,2)],classes
    assert [(g[1],g[2]) for g in gives]==[(0x27,1),(3,1),(8,1),(0x11,120)],gives
    assert 'inventory-restored Alice class=Celestial weapons=2 armour=1 shards=0 ammo=120/0/0/0 gives=5' in logtext(),logtext()[-600:]
    frame(100);n=len(gives);frame(1000);assert len(gives)==n,'restored once'
    print('PASS co-op inventory: snapshot while alive, class and missing items restored after a respawn')
  else:assert 'effective CoopKeepInventory=0' in logtext()
  # Powerup: re-sent to every other member a moment after the activator's.
  if script_events and '--no-powerup-resend' not in sys.argv:
   n=len(said);spawn(bob);assert any('Bob joined in progress' in m for m in said[n:]),said[n:]
   msg=[]
   stubs[obj+0x52140]=tcall(1,lambda this,a:(write(this,a[0]),write(this+4,0x7700),msg.append(['new']))[0])
   stubs[obj+0x522b0]=tcall(1,lambda this,a:msg[-1].append(('obj',a[0])))
   stubs[obj+0x52190]=tcall(1,lambda this,a:msg[-1].append(('byte',a[0])))
   stubs[obj+0x52350]=tcall(2,lambda this,a:(msg[-1].append(('send',a[0],a[1])),write(this+4,0))[0])
   stubs[obj+0x52160]=tcall(0,lambda this,a:None)
   stubs[obj+0xf4da0]=tcall(1,lambda this,a:1)
   def is_player2():
    sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,1 if read(read(sp+4)) in (0xa1,0xb2) else 0)
    u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(sp))
   stubs[obj+0xee900]=is_player2;ref=nb+0xd00;write(ref,0xa1)
   call_regs(target[0x2ffea],[ref],{},0)                     # Alice touched the armour trigger
   u.mem_write(strings+0xf40,b'powerup\0');call_regs(target[0xc1abe],[strings+0xf40],{UC_X86_REG_ECX:alice},4)
   frame(100);assert msg==[]
   frame(400);assert msg==[['new',('obj',0xa1),('byte',0x35),('send',0x222,0x39)]],msg
   frame(500);assert len(msg)==1,'sent once'
   assert 'powerup effect 0x35 re-sent to 1 observer(s)' in logtext()
   for k in (0x52140,0x522b0,0x52190,0x52350,0x52160,0xf4da0,0xee900):del stubs[obj+k]
   print('PASS co-op powerup: the activator effect re-sent to the observers')
  write(obj+0x174f00,state0);write(obj+0x174f04,state4)
  write(alice,0);write(alice+0x54,0);write(dev+0x1d0,old1d0);roster(0xa1)
  for k in (0xf25f0,0xdd170,0xdd190,0xf3440,0xf2090):del stubs[obj+k]
 if not intermission:coop_new_features()
 # CoopSpawnSpread: a second player at a taken (non-R1M2B) start queries the
 # engine (IntersectSegment g_pServerDE+0x90: floor and line rays) and the
 # BSP; with no world loaded the body check is UNKNOWN, so it fails closed
 # and the start is shared. Without the engine query it also shares.
 text=b''.join(log).decode('latin1')
 spread='--no-spawn-spread' not in sys.argv
 assert ('effective CoopSpawnSpread=1' if spread else 'effective CoopSpawnSpread=0') in text
 if spread and '--intermission' not in sys.argv:
  rays=[]
  def intersect():
   sp=u.reg_read(UC_X86_REG_ESP);q,info=read(sp+4),read(sp+8)
   fy=struct.unpack('<f',u.mem_read(q+4,4))[0];ty=struct.unpack('<f',u.mem_read(q+16,4))[0]
   down=ty<fy-100;rays.append('floor' if down else 'line')
   if down:u.mem_write(info,struct.pack('<3f',0,-302.0,0));u.mem_write(info+12,struct.pack('<3f',0,1.0,0))
   u.reg_write(UC_X86_REG_EAX,1 if down else 0);u.reg_write(UC_X86_REG_EIP,read(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
  dev2=read(obj+0x16de80);old90=read(dev2+0x90);code90=strings+0x1400;stubs[code90]=intersect;write(dev2+0x90,code90)
  zz=0xc4000000                                                 # z=-512: not the R1M2B anchor
  n0=len(said);spawn(alice,z=zz);spawn(bob,z=zz)
  text=b''.join(log).decode('latin1')
  assert 'spawn-spread no candidate passed; start shared' in text,text[-500:]
  assert rays and rays[0]=='floor' and 'line' in rays,rays
  write(dev2+0x90,0);n1=text.count('spawn-spread')
  spawn(bob,z=zz)                                               # respawn: previous reservation retried
  write(dev2+0x90,old90);del stubs[code90]
  print('PASS co-op spawn spread: taken start probed (floor, line, body), unknown body fails closed')
 # Log budget, per map epoch: a flood stops at the budget with one notice and
 # the next world's lines are written again (a whole campaign keeps its log).
 n0=len(log)
 for i in range(1100):chat(alice,'!coop')
 flood=b''.join(log[n0:]).decode('latin1')
 assert flood.count(' log-budget ')==1 and flood.count('[KPCFix Co-op] g=')<=1001,flood.count('[KPCFix Co-op] g=')
 n0=len(log);chat(alice,'!coop');assert len(log)==n0,'lines after the budget are dropped'
 next_level();n0=len(log);chat(alice,'!coop')
 assert b' say ' in b''.join(log[n0:]),'the next world renews the budget'
 print('PASS co-op log budget: per map epoch, one notice, renewed by the next world')
 print('PASS co-op session hooks:',len(said),'broadcasts,',len(ends),'native end-level requests,',len(nextlevels),'NextLevel calls, telefrag skipped')

def import_slot(data,base,function):
 p=struct.unpack_from('<I',data,60)[0];at=base+read(base+p+24+96+8)
 while read(at+12):
  names=read(at) or read(at+16);iat=read(at+16);i=0
  while read(base+names+i*4):
   v=read(base+names+i*4)
   if not v&0x80000000 and cstr(base+v+2)==function:return base+iat+i*4
   i+=1
  at+=20
def export(name):
 p=struct.unpack_from('<I',dll,60)[0];d=read(proxy+p+24+96);n=read(proxy+d+24)
 names,ords,funcs=read(proxy+d+32),read(proxy+d+36),read(proxy+d+28)
 for i in range(n):
  if cstr(proxy+read(proxy+names+i*4))==name:
   return proxy+read(proxy+funcs+struct.unpack('<H',u.mem_read(proxy+ords+i*2,2))[0]*4)
loadlib_slot=import_slot(dedicated,exe,'LoadLibraryA')
write(loadlib_slot,functions['LoadLibraryA'])   # only this dedicated.exe import is bound
assert read(loadlib_slot)==functions['LoadLibraryA']
assert call(proxy+ps['_DllMain'],[proxy,1,0],True)==1
assert report['winmm_exports']>=190 and export('timeGetTime') and export('mciSendCommandA') and export('CreateServer')
if winmm_mode:
 assert winmm_loads[0]==sysdir+'\\winmm.dll',winmm_loads
 assert call(export('timeGetTime'),[],True)==0x1234,'winmm export forwarded to the system DLL'
 assert read(loadlib_slot)==proxy+ps['_server_load_a'],'dedicated.exe LoadLibraryA redirected'
 u.mem_write(scratch+0x90100,b'other.dll\0');assert call(read(loadlib_slot),[scratch+0x90100],True)==0x5555 and other_loads==['other.dll']
 u.mem_write(scratch+0x90100,b'.\\SERVER.DLL\0');assert call(read(loadlib_slot),[scratch+0x90100],True)==proxy,'server.dll answered by Server Tools'
 assert winmm_loads[-1]==paths[proxy] and other_loads==['other.dll']
 print('PASS winmm.dll mode: system winmm forwarding, dedicated.exe server.dll redirect, original server.dll loaded unrenamed')
else:
 assert read(loadlib_slot)==functions['LoadLibraryA'] and not winmm_loads,'server.dll mode: no winmm load, no redirect'
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
# CoopServerFrameMs (session, default 16): server.dll 235b0 (e8 aa 6a 00 00 ->
# frame 2a05f, thiscall(flags, float time), ret 8). A call sooner than 16 ms
# after the last simulated frame returns 1 without running; the next one runs.
frame_site=bytes(u.mem_read(engine+0x235b0,5))
frame_on=coop_session and '--off' not in sys.argv and '--no-server-frame' not in sys.argv
if not frame_on:
 assert frame_site==bytes.fromhex('e8aa6a0000'),frame_site.hex()
else:
 assert frame_site[0]==0xe8,frame_site.hex()
 frame_target=(engine+0x235b0+5+struct.unpack('<i',frame_site[1:5])[0])&0xffffffff
 assert proxy<=frame_target<proxy+0x100000
 runs=[]
 def server_frame():
  sp=u.reg_read(UC_X86_REG_ESP);srv_=u.reg_read(UC_X86_REG_ECX)
  t=struct.unpack('<f',struct.pack('<I',read(sp+8)))[0]+struct.unpack('<f',u.mem_read(srv_+0x1f8,4))[0]
  runs.append((read(sp+4),round(t,4)));u.mem_write(srv_+0x218,struct.pack('<f',t))
  u.reg_write(UC_X86_REG_EAX,1);u.reg_write(UC_X86_REG_ESP,sp+12);u.reg_write(UC_X86_REG_EIP,read(sp))
 stubs[engine+0x2a05f]=server_frame
 srv=scratch+0x95000;u.mem_write(srv+0x1f8,struct.pack('<f',0.0));u.mem_write(srv+0x218,struct.pack('<f',0.0))
 f=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
 res=[]
 for t in (1.000,1.008,1.016,1.024,1.032,1.040,1.048,2.000,1.990):   # dedicated.exe calls about every 8 ms
  call_regs(frame_target,[7,f(t)],{UC_X86_REG_ECX:srv},8);res.append(u.reg_read(UC_X86_REG_EAX))
 assert all(r==1 for r in res),res
 # First call runs (no previous frame), then every second 8 ms call; a long gap runs; time going back runs.
 assert [t for _,t in runs]==[1.0,1.016,1.032,1.048,2.0,1.99],runs
 assert all(fl==7 for fl,_ in runs),'flags forwarded'
 del stubs[engine+0x2a05f]
 text=b''.join(log).decode('latin1')
 assert 'effective CoopServerFrameMs=16' in text and 'server frame hook installed (server.dll 235b0 -> 2a05f), minimum step ms=16' in text
 print('PASS co-op server frame: 8 ms calls simulated as 16 ms frames, long gaps and time resets run at once')
if coop_session and '--off' not in sys.argv and '--no-server-frame' in sys.argv:
 text=b''.join(log).decode('latin1');assert 'effective CoopServerFrameMs=0' in text and 'server frame hook installed' not in text
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
