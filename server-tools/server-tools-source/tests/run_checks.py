import sys,struct
# Requires the optional Python unicorn package and GNU binutils.
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parent
build=tempfile.TemporaryDirectory()
t=Path(build.name)
subprocess.run(['as','--32',str(root.parent/'logging.S'),'-o',str(t/'code.o')],check=True)
h=(root/'harness.S').read_text()
if '--usa' in sys.argv:
 h=h.replace('0x39b907ab','0x3a942fb8').replace('.org 0x150\n.long 0x18000','.org 0x150\n.long 0x1b000').replace('.org 0x1c5a','.org 0x1de3').replace('0x12f70','0x16028').replace('.org 0x18000','.org 0x1b000')
(t/'harness.S').write_text(h)
subprocess.run(['as','--32',str(t/'harness.S'),'-o',str(t/'harness.o')],check=True)
subprocess.run(['ld','-m','elf_i386','-e','_start','-T',str(root/'harness.ld'),str(t/'code.o'),str(t/'harness.o'),'-o',str(t/'harness')],check=True)
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_INTR
from unicorn.x86_const import *
b=open(t/'harness','rb').read();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0xe0000,0x520000);u.mem_map(0x7000000,0x100000)
entry,phoff=struct.unpack_from('<II',b,24);phentsize,phnum=struct.unpack_from('<HH',b,42)
for i in range(phnum):
 typ,off,va,pa,fs,ms,flags,align=struct.unpack_from('<8I',b,phoff+i*phentsize)
 if typ==1:u.mem_write(va,b[off:off+fs])
u.reg_write(UC_X86_REG_ESP,0x70ff000)
out=bytearray();exitcode=None
def intr(uc,n,data):
 global exitcode
 assert n==128,n
 a=uc.reg_read(UC_X86_REG_EAX)
 if a==4:
  assert uc.reg_read(UC_X86_REG_EBX)==1
  size=uc.reg_read(UC_X86_REG_EDX);out.extend(uc.mem_read(uc.reg_read(UC_X86_REG_ECX),size));uc.reg_write(UC_X86_REG_EAX,size)
 elif a==1:
  exitcode=uc.reg_read(UC_X86_REG_EBX);uc.emu_stop()
 else:raise AssertionError(a)
u.hook_add(UC_HOOK_INTR,intr)
u.emu_start(entry,0,count=100000)
assert exitcode==0,exitcode
p=b'[KPCFix Server Tools] '
actions=[b'darnell toggled invulnerability ON',b'darnell toggled invulnerability OFF',b'darnell toggled noclip ON',b'darnell toggled noclip OFF']
expected=b''.join(b'\r       \r'+p+a+b'\r\n>status'+b'\x08'*4 for a in actions)
expected+=b'\r       \r'+p+b'Chat: darnell: hello 100%?there\r\n>status'+b'\x08'*4
expected+=p+actions[0]+b'\r\n'
assert out==expected,(bytes(out),expected)
print('PASS: direct packet layout, broadcast recipient, message lifetime, single console entry, ON/OFF selection, duplicate report suppression, command/cursor restoration and redirected output')
