"""Validate the two-byte edit and run the shell's row-placement instructions."""
from pathlib import Path
import sys
import test_runtime
sys.argv+=['--expose-rig']
Rig,_,_,_,x,args=test_runtime.main()
site={'Europe':0xb92ff,'USA':0xb92ff,'Germany':0xb92af,'Italy-Covermount':0xb92d0}[args.fixtures.name]
for percent in (100,150,200):
 r=Rig(settings={'HUD.ScalePercent':percent})
 r.factory()
 assert not r.messages,r.messages
 address=r.shellbase+site
 assert r.u.mem_read(address,4)==(b'\x2b\xf8' if percent==100 else b'\x90\x90')+b'\x03\xf9'
 for top,bar,font in [(100,16,8),(116,16,8),(133,17,10)]:
  for drift in (1,2,3):
   r.u.reg_write(x.UC_X86_REG_EDI,(bar-font)//2)
   r.u.reg_write(x.UC_X86_REG_ECX,top)
   r.u.reg_write(x.UC_X86_REG_EAX,(-drift)&0xffffffff)
   r.u.emu_start(address,address+4,count=3)
   assert r.u.reg_read(x.UC_X86_REG_EDI)==top+(bar-font)//2+(drift if percent==100 else 0)
print(args.fixtures.name,'passed')
