"""Execute the live-control hooks against mapped game images and mocked APIs.
Usage: python check_live.py STOCK_OBJECT DEDICATED_EXE [--disabled]
Requires unicorn. No native Windows process is started.
"""
from pathlib import Path
import hashlib,json,struct,sys,os
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
root=Path(__file__).resolve().parents[2];m=json.loads(Path(os.environ.get('KPC_TEST_PATCH_MANIFEST',str(root/'legacy-object-patches.json'))).read_text())
b=bytearray(Path(sys.argv[1]).read_bytes());assert hashlib.sha256(b).hexdigest()==m['original']
mode='disabled' if '--disabled' in sys.argv else 'patched'
for c in m['changes']:b[c['offset']]=c[mode]
assert hashlib.sha256(b).hexdigest()==m[mode]
# Exercise the current DLL hook payload over the legacy restoration fixture.
current=json.loads((root/'SERVER_TOOLS_BUILD.json').read_text())
hook=(root/'server-tools-source/build/hooks.bin').read_bytes()
b[0x116580:0x116580+len(hook)]=hook
b[current['hook_symbols']['initial_mode']]=0 if mode=='disabled' else 1

exe=Path(sys.argv[2]).read_bytes();base=int(os.environ.get('KPC_TEST_BASE','0x10000000'),0);ebase=0x400000
u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(base,0x200000);u.mem_map(ebase,0x20000);u.mem_map(0x30000000,0x100000)
def w(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
def r(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def string(a):
 out=bytearray()
 while a and len(out)<2048:
  c=bytes(u.mem_read(a,1));a+=1
  if c==b'\0':break
  out+=c
 return bytes(out)
def pe(data,at):
 p=struct.unpack_from('<I',data,60)[0];n=struct.unpack_from('<H',data,p+6)[0];opt=struct.unpack_from('<H',data,p+20)[0];s=p+24+opt
 u.mem_write(at,bytes(data[:0x1000]))
 for i in range(n):
  vs,va,size,raw=struct.unpack_from('<4I',data,s+i*40+8)
  if size:u.mem_write(at+va,bytes(data[raw:raw+size]))
 return struct.unpack_from('<I',data,p+8)[0]
pe(b,base)
# Apply the remaining Windows HIGHLOW relocations at a nonpreferred load base.
if base!=0x10000000:
 p=struct.unpack_from('<I',b,60)[0]
 rv,size=struct.unpack_from('<II',b,p+24+96+5*8);at=base+rv;end=at+size
 while at<end:
  page=r(at);length=r(at+4)
  if not length:break
  for x in range(at+8,at+length,2):
   rel=struct.unpack('<H',u.mem_read(x,2))[0]
   if rel>>12==3:
    target=base+page+(rel&4095);w(target,r(target)+base-0x10000000)
  at+=length
stamp=pe(exe,ebase)
usa=stamp==0x3a942fb8
console=ebase+(0x16028 if usa else 0x12f70)
listaddr=ebase+(0x13f00 if usa else 0x10e50)
findaddr=ebase+(0x16e0 if usa else 0x1640)
addaddr=ebase+(0x1690 if usa else 0x15f0)
w(console+0x58,2);w(console+0x5c,6);u.mem_write(console+4,b'status\0');u.mem_write(console+0x64,b'>\0')
shell=0x30000000;engine=0x30010000;evtable=engine+0x1000
players=[0x30020000,0x30021000];handles=[0x1111,0x2222]
objects=[0x30040000,0x30041000];decoy=0x30042000
for h in objects+[decoy]:u.mem_write(h,b'\xa5'*0x200);w(h,0)
for i,p in enumerate(players):
 w(p+8,objects[i]);w(p+0x120,handles[i]);u.mem_write(p+0x1e8,[b'Alice\0',b'Bob\0'][i])
w(base+0x16de60,shell);w(shell+0x1d0,1);w(base+0x16de80,engine);w(engine,evtable);w(base+0x157724,0x30012000)
msg=0x30050000;mv=msg+0x100;w(msg,mv)
outputs=[];broadcasts=[];targeted=[];packets=[];registrations=[];node=0x30060000
stubs={};next_stub=0x300e0000

def stub(argc,fn,address=None):
 global next_stub
 a=address or next_stub;next_stub+=16
 def invoke():
  sp=u.reg_read(UC_X86_REG_ESP);args=[r(sp+4+i*4) for i in range(argc)];value=fn(args)
  u.reg_write(UC_X86_REG_EAX,0 if value is None else value)
  u.reg_write(UC_X86_REG_ESP,sp+4+argc*4);u.reg_write(UC_X86_REG_EIP,r(sp))
 stubs[a]=invoke
 return a
# cdecl functions leave their arguments to the caller.
def cdecl(argc,fn,address):
 def invoke():
  sp=u.reg_read(UC_X86_REG_ESP);value=fn([r(sp+4+i*4) for i in range(argc)])
  u.reg_write(UC_X86_REG_EAX,0 if value is None else value)
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,r(sp))
 stubs[address]=invoke

def getmode(a):w(a[1],1);return 1
getmode_addr=stub(2,getmode)
w(base+0x117058,stub(1,lambda a:ebase if a[0]==0 else 0x70000000))
w(base+0x117054,stub(2,lambda a:getmode_addr))
w(base+0x1170cc,stub(1,lambda a:99))
def writefile(a):
 outputs.append(bytes(u.mem_read(a[1],a[2])));w(a[3],a[2]);return 1
w(base+0x117098,stub(5,writefile))
def find(a):
 assert u.reg_read(UC_X86_REG_ECX)==listaddr
 assert string(a[0])==b'kpc_cheats'
 return node if registrations else 0
stub(1,find,findaddr)
def add(a):
 assert u.reg_read(UC_X86_REG_ECX)==listaddr
 assert string(a[0])==b'kpc_cheats'
 assert a[1]==base+m['symbols']['host_command']
 registrations.append(a[1]);w(node,a[1]);return 1
stub(2,add,addaddr)
def getflags(a):
 assert a[0] in objects,('invalid engine object handle',hex(a[0]))
 return r(a[0])
cdecl(1,getflags,base+0x116010) # spare address, not a game entry
w(engine+0x1f8,base+0x116010)
def setflags(a):w(a[0],a[1]);return 0
cdecl(2,setflags,base+0x116020);w(engine+0x1fc,base+0x116020)
cdecl(2,lambda a:targeted.append(tuple(a)),base+0xf96f0)
# Exercise the actual game iterators and checked HOBJECT -> player converter.
# The list also contains a non-player object, which the converter must skip.
listed=[objects[0],decoy,objects[1]]
for i,h in enumerate(listed):
 n=0x30030000+i*0x20;w(n,h);w(n+4,n+0x20 if i+1<len(listed) else 0)
w(0x30012000+4,0x30030000)
cdecl(1,lambda a:0xabcd,base+0x116030);w(engine+8,base+0x116030)
cdecl(1,lambda a:0xabcd if a[0] in objects else 0xdcba,base+0x116040);w(engine+0x10,base+0x116040)
cdecl(2,lambda a:int(a[0]==a[1]),base+0x116050);w(engine+0x14,base+0x116050)
cdecl(1,lambda a:players[objects.index(a[0])],base+0x116060);w(engine+0x130,base+0x116060)
cdecl(0,lambda a:msg,base+0xf88c0)
def word(a):packets.append(('word',a[0]))
def text(a):packets.append(('text',string(a[0])))
w(mv+0x3c,stub(1,word));w(mv+0x4c,stub(1,text));w(mv,stub(0,lambda a:0))
def send(a):
 assert a==[msg,0x45,0,1],a
 assert [x[1] for x in packets[:2]]==[4,0],packets
 broadcasts.append(packets[2][1]);packets.clear()
w(evtable+0xc4,stub(4,send))
cdecl(5,lambda a:0,base+0xf8e90)
def code(uc,a,size,data):
 if a in stubs:stubs[a]()
u.hook_add(UC_HOOK_CODE,code)
stop=0x300f0000

def call(name,args=(),esi=None):
 sp=0x300df000;w(sp,stop)
 for i,v in enumerate(args):w(sp+4+i*4,v)
 u.reg_write(UC_X86_REG_ESP,sp)
 if esi is not None:u.reg_write(UC_X86_REG_ESI,esi)
 u.emu_start(base+m['symbols'][name],stop,count=20000)
 assert u.reg_read(UC_X86_REG_EIP)==stop,(name,hex(u.reg_read(UC_X86_REG_EIP)))
 assert u.reg_read(UC_X86_REG_ESP)==sp+4,(name,'stack mismatch')

def command(value=None):
 arg=0x30070000;txt=arg+0x100
 w(arg,1 if value is None else 2);w(arg+8,txt)
 if value is not None:u.mem_write(txt,value.encode()+b'\0')
 call('host_command',[arg])

def gate(enabled):
 u.reg_write(UC_X86_REG_ECX,shell);call('inv_gate')
 assert bool(u.reg_read(UC_X86_REG_EFLAGS)&64)==enabled
 before=len(targeted);call('noclip_request',[handles[0]])
 assert len(targeted)==before+int(enabled)

def flags(p,new):call('flags_hook',[r(p+8),new],esi=p)

call('init_hook');assert len(registrations)==1
assert r(base+0x174f04)==1
start_enabled=mode=='patched';assert r(base+0x174f00)==int(start_enabled)
gate(start_enabled)
if not start_enabled:command('1')
assert r(base+0x174f00)==1
# Two active players; disabling must clear both and target each OFF correction.
for p in players:u.mem_write(p+0x13f,b'\x01');w(r(p+8),0x200000)
targeted.clear();broadcasts.clear();outputs.clear();command('0')
assert broadcasts==[b'Cheats disabled'],broadcasts
assert sorted(targeted)==[(handles[0],44),(handles[1],44)]
for p in players:
 assert bytes(u.mem_read(p+0x13f,1))==b'\x00'
 # The normal OFF acknowledgement performs the collision transition.
 assert r(r(p+8))&0x200000!=0
assert b''.join(outputs).count(b'[KPCFix Server Tools] Cheats disabled')==1
for h in objects+[decoy]:assert bytes(u.mem_read(h+4,0x1fc))==b'\xa5'*0x1fc,'engine object memory corrupted'
for p in players:flags(p,0x22000)
assert all(r(h)&0x200000==0 for h in objects)
gate(False)
# Repeated/late ON reports get at most one correction until OFF acknowledgement.
before=len(targeted);flags(players[0],0x200000);flags(players[0],0x200000)
assert len(targeted)==before+1
assert broadcasts==[b'Cheats disabled']
flags(players[0],0x22000)
flags(players[0],0x200000)
assert len(targeted)==before+2
flags(players[0],0x22000)
# OFF -> ON is live, and only the host's change is broadcast.
command('1');assert broadcasts[-1]==b'Cheats enabled';gate(True)
before=len(broadcasts);command('1');command();command('bad')
assert len(broadcasts)==before
# Re-enabling then disabling again still corrects the right active client.
w(r(players[0]+8),0x200000);command('0');assert targeted[-1]==(handles[0],44)
# Disconnect frees outstanding correction entries.
call('leave_hook',[0,handles[0]])
assert handles[0] not in [r(base+0x174f10+i*8) for i in range(16)]
# Ordinary player chat containing the command cannot modify host permission.
u.mem_write(0x30072000,b'kpc_cheats 1\0');u.mem_write(0x30072100,b'%s %s\0')
old_state=r(base+0x174f00);n=len(broadcasts)
call('chat_hook',[0,0,0x30072100,players[0]+0x1e8,0x30072000])
assert r(base+0x174f00)==old_state and len(broadcasts)==n
# Re-initialization updates an existing callback, without adding a cyclic node.
call('init_hook');assert len(registrations)==1
print('PASS live host command:', 'USA' if usa else 'Europe',mode,'real object-list iteration and player conversion, non-player skip, engine-memory guards, client OFF acknowledgement, live broadcasts, silent gates and late-report deduplication')
