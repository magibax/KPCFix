from fixture_layout import rva, italian, crt_address
"""Execute the ORIGINAL address builder, with/without the checked call-site patch.
Winsock conversion APIs are mocked. No native network test is claimed.
"""
import sys,json,struct,socket
from pathlib import Path
import test_runtime
p32=lambda v:struct.pack('<I',v)
argv=sys.argv[:]
sys.argv=argv+['--platform','windows','--expose-rig']
Rig,dll,syms,uc,x,args=test_runtime.main()
class BindRig(Rig):
 def __init__(self,**kw):
  self.parses=[];super().__init__(**kw)
  for i,(name,nargs,slot) in enumerate([('InetAddr',1,0x97288),('Htons',1,0x972c0)]):
   at=0x60003000+i*32;self.stub_names[at]=name;self.stub_args[name]=nargs
   self.u.mem_write(at,b'\xc2\x04\x00');self.u.mem_protect(0x1d37000,4096,7)
   self.u.mem_write(0x1ca0000+slot,p32(at))
 def api(self,u,address,size,user):
  name=self.stub_names.get(address)
  if name in ('InetAddr','Htons'):
   sp=u.reg_read(x.UC_X86_REG_ESP);a=struct.unpack('<I',u.mem_read(sp+4,4))[0]
   if name=='Htons':v=((a&255)<<8)|((a>>8)&255)
   else:
    text=self.cstr(a);self.parses.append(text)
    try:v=int.from_bytes(socket.inet_aton(text),'little') if text else 0xffffffff
    except OSError:v=0xffffffff
   u.reg_write(x.UC_X86_REG_EAX,v);return
  return super().api(u,address,size,user)
 def builder(self,ip,port):
  out,text,sp=0x588000,0x589000,0x418000
  self.u.mem_write(text,ip.encode()+b'\0' if ip is not None else b'\0')
  self.u.mem_write(0x1d4b628,p32(text if ip is not None else 0))
  self.u.mem_write(out,b'\xcc'*16)
  self.u.mem_write(sp,p32(0x62000100)+p32(out)+p32(port))
  self.u.reg_write(x.UC_X86_REG_ESP,sp)
  self.u.emu_start(0x1d172b0,0x62000100,count=10000)
  assert self.u.reg_read(x.UC_X86_REG_EIP)==0x62000100
  assert self.u.reg_read(x.UC_X86_REG_ESP)==sp+4
  assert self.u.reg_read(x.UC_X86_REG_EAX)==1
  return bytes(self.u.mem_read(out,16))
rows=[]
for base,clock in [(0x68000000,1),(0x03000000,0)]:
 for enabled in (0,1):
  r=BindRig(dllbase=base,settings={'Multiplayer.EmptyBindIPFix':enabled,'Multiplayer.NetTrace':0,'InputClock':clock});r.factory()
  assert not r.messages,r.messages
  for ip,expected in [(None,bytes(4)),('',bytes(4) if enabled else b'\xff'*4),('0.0.0.0',bytes(4)),('192.168.1.3',bytes([192,168,1,3])),('127.0.0.1',bytes([127,0,0,1])),('bad-ip',b'\xff'*4)]:
   for port in (0,42972):
    got=r.builder(ip,port)
    assert got==b'\x02\x00'+port.to_bytes(2,'big')+expected+bytes(8),(enabled,ip,got.hex())
    rows.append({'dll_base':hex(base),'enabled':enabled,'IP':ip,'port':port,'sockaddr':got.hex(),'status':'pass'})
  assert ('' in r.parses)==(not enabled)
Path(args.output).write_text(json.dumps({'tests':rows,'count':len(rows),'native_validation':'pending'},indent=2))
print('PASS',len(rows),'original address-builder cases, including reproducing the original empty-string bug')
