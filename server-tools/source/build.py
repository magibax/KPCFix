"""Build a test patch from an exact, unmodified 1.13 object.lto.
Requires Python 3 and GNU binutils (as, ld, objcopy, nm).
Usage: python build.py /path/to/stock/object.lto /tmp/patched-object.lto
The output object is for local testing; do not redistribute game binaries.
"""
from pathlib import Path
import hashlib, json, struct, subprocess, sys, tempfile
root = Path(__file__).resolve().parent.parent
stock = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(stock).hexdigest() == '3a03f8796c482b1a7ad72755df33ee8f1f8056bb10957f6f700e64bd006b3e5b'
m = json.loads((root / 'patch.json').read_text())
b = bytearray(stock)
# Retain the working Test 2 noclip gate/trampoline and relocation edit.
for c in m['changes']:
    if not (0x116580 <= c['offset'] < 0x117000) and c['offset'] not in range(488,492):
        b[c['offset']] = c['patched']
with tempfile.TemporaryDirectory() as tmp:
    t = Path(tmp)
    subprocess.run(['as','--32',str(root/'source/logging.S'),'-o',str(t/'code.o')],check=True)
    subprocess.run(['ld','-m','elf_i386','-e','inv_hook','-Ttext=0x116580',str(t/'code.o'),'-o',str(t/'code.elf')],check=True)
    subprocess.run(['objcopy','-O','binary','-j','.text',str(t/'code.elf'),str(t/'code.bin')],check=True)
    code=(t/'code.bin').read_bytes()
    symbols={v[2]:int(v[0],16) for line in subprocess.check_output(['nm',str(t/'code.elf')],text=True).splitlines() if len(v:=line.split())==3}
assert len(code) <= 0xa80 and not any(stock[0x116580:0x116580+len(code)])
b[0x116580:0x116580+len(code)] = code
for site,name in [(0xef39a,'inv_hook'),(0xef3b5,'inv_hook'),(0xfeed5,'chat_hook')]:
    assert stock[site] == 0xe8
    b[site:site+5] = b'\xe8'+struct.pack('<i',symbols[name]-site-5)
# Native host command + dynamic invulnerability/noclip gates.
def jump(site,name,size,opcode=0xe8):
    b[site:site+size]=bytes([opcode])+struct.pack('<i',symbols[name]-site-5)+b'\x90'*(size-5)
assert stock[0xf2df1:0xf2df7]==bytes.fromhex('ff90fc010000')
jump(0xf2df1,'flags_hook',6)
# Keep the original JNE; the runtime gate supplies its decision flags.
b[0xef380:0xef382]=stock[0xef380:0xef382]
assert stock[0xef378:0xef380]==bytes.fromhex('8b81d001000085c0')
jump(0xef378,'inv_gate',8)
assert stock[0xfa540:0xfa546]==bytes.fromhex('8b4424046a2c')
jump(0xfa540,'noclip_request',6,0xe9)
# Restore bytes in the displaced old request handler (unreachable now).
b[0xfa546:0xfa54c]=stock[0xfa546:0xfa54c]
assert stock[0xfced1:0xfced6]==bytes.fromhex('a160de1610')
jump(0xfced1,'init_hook',5)
assert stock[0xfd4f0:0xfd4f6]==bytes.fromhex('8b4c240485c9')
jump(0xfd4f0,'leave_hook',6,0xe9)
# A1's absolute operand is displaced by CALL: remove its HIGHLOW relocation.
reloc_rva,reloc_size=struct.unpack_from('<II',stock,struct.unpack_from('<I',stock,60)[0]+24+96+5*8)
at=0x150000;end=at+reloc_size;removed=0
while at<end:
    page,size=struct.unpack_from('<II',stock,at)
    if not size:break
    for x in range(at+8,at+size,2):
        r=struct.unpack_from('<H',stock,x)[0]
        if (r>>12)==3 and page+(r&4095)==0xfced2:
            struct.pack_into('<H',b,x,0);removed+=1
    at+=size
assert removed==1
p=struct.unpack_from('<I',b,0x3c)[0]
s=p+24+struct.unpack_from('<H',b,p+20)[0]
struct.pack_into('<I',b,s+8,0x116580+len(code)-0x1000)
# Include legacy changed positions even where the new byte equals stock.
positions=set(c['offset'] for c in m['changes']) | {i for i,(a,z) in enumerate(zip(stock,b)) if a!=z}
m['changes']=[dict(offset=i,stock=stock[i],patched=b[i]) for i in sorted(positions)]
m['patched']=hashlib.sha256(b).hexdigest()
# Extend only unused .data virtual padding for the runtime policy/table.
assert struct.unpack_from('<I',stock,s+2*40+8)[0]==0x4bea8
struct.pack_into('<I',b,s+2*40+8,0x4c000)
on=bytearray(b);off=bytearray(b)
assert on[symbols['initial_mode']]==1
off[symbols['initial_mode']]=0
positions=set(c['offset'] for c in m['changes']) | {i for i,(a,z) in enumerate(zip(stock,on)) if a!=z} | {symbols['initial_mode']}
m['changes']=[dict(offset=i,stock=stock[i],patched=on[i],disabled=off[i]) for i in sorted(positions)]
m['patched']=hashlib.sha256(on).hexdigest()
m['disabled']=hashlib.sha256(off).hexdigest()
m['symbols']=symbols
b=on
(root/'patch.json').write_text(json.dumps(m,indent=2)+'\n')
Path(sys.argv[2]).write_bytes(b)
Path(sys.argv[2]+'.disabled').write_bytes(off)
print('Enabled:',m['patched'],'Disabled:',m['disabled'],len(code),'hook bytes')
