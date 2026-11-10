"""Build the x86 server.dll proxy, without redistributing original game binaries.
Usage: python source/build_dll.py
Requires gcc with -m32 support and GNU binutils.
"""
from pathlib import Path
import hashlib,json,struct,subprocess
from pe_build import elf_to_coff,run
root=Path(__file__).resolve().parent;package=root.parent;work=root/'build';work.mkdir(exist_ok=True)
# Assemble the previously tested object-relative runtime hooks.
run(['as','--32',root/'logging.S','-o',work/'hooks.o'])
run(['ld','-m','elf_i386','-e','inv_hook','-Ttext=0x116580',work/'hooks.o','-o',work/'hooks.elf'])
run(['objcopy','-O','binary','-j','.text',work/'hooks.elf',work/'hooks.bin'])
symbols={v[2]:int(v[0],16) for line in subprocess.check_output(['nm',str(work/'hooks.elf')],text=True).splitlines() if len(v:=line.split())==3}
code=(work/'hooks.bin').read_bytes();assert len(code)<=0xa80
legacy=json.loads((package/'legacy-object-patches.json').read_text())
trampoline=bytearray(27)
for c in legacy['changes']:
 if 0x116550<=c['offset']<0x11656b:trampoline[c['offset']-0x116550]=c['patched']
assert any(trampoline)
patches=[]
def patch(rva,old,name,kind=0xe8,reloc=-1):
 before=bytes.fromhex(old);after=bytes([kind])+struct.pack('<i',symbols[name]-rva-5)+b'\x90'*(len(before)-5)
 patches.append(dict(rva=rva,before=before.hex(),after=after.hex(),size=len(before),reloc=reloc))
patch(0xef39a,'e8f1940000','inv_hook');patch(0xef3b5,'e8d6940000','inv_hook');patch(0xfeed5,'e8b69fffff','chat_hook')
patch(0xf2df1,'ff90fc010000','flags_hook');patch(0xef378,'8b81d001000085c0','inv_gate')
patch(0xfa540,'8b4424046a2c','noclip_request',0xe9);patch(0xfced1,'a160de1610','init_hook',reloc=1)
patch(0xfd4f0,'8b4c240485c9','leave_hook',0xe9)
patches.append(dict(rva=0xfbcfa,before='890d50d91610',after=(b'\xe9'+struct.pack('<i',0x116550-0xfbcfa-5)+b'\x90').hex(),size=6,reloc=2))
def array(data):return '{'+','.join('0x%02x'%v for v in data)+'}'
supported=json.loads((package/'SUPPORTED_BUILDS.json').read_text())
h='#define HOOK_RVA 0x116580u\n#define INITIAL_OFFSET %du\n'%(symbols['initial_mode']-0x116580)
h+='static const U8 hook_code[]='+array(code)+';\nstatic const U8 noclip_init[]='+array(trampoline)+';\n'
h+='typedef struct {U32 rva,size;I32 reloc;U8 before[8],after[8];} RuntimePatch;\nstatic const RuntimePatch patches[]={\n'
for p in patches:h+='{0x%x,%d,%d,%s,%s},\n'%(p['rva'],p['size'],p['reloc'],array(bytes.fromhex(p['before'])),array(bytes.fromhex(p['after'])))
h+='};\n#define PATCH_COUNT (sizeof(patches)/sizeof(patches[0]))\n'
h+='static const U8 object_hash[32]='+array(bytes.fromhex(legacy['original']))+';\n'
for name,key in [('exe_hashes','dedicated.exe'),('backend_hashes','server.dll')]:
 h+='static const U8 '+name+'[2][32]={'+','.join(array(bytes.fromhex(p[key]['sha256'])) for p in supported['servers'])+'};\n'
(work/'runtime_data.h').write_text(h)
# Tiny ABI adapter into the existing object-relative console printer.
(work/'runtime_print.S').write_text('''.intel_syntax noprefix
.code32
.text
.global runtime_print
runtime_print:
 push ebp
 mov ebp,[esp+8]
 mov edx,[esp+12]
 mov ecx,[esp+16]
 lea eax,[ebp+%d]
 call eax
 pop ebp
 ret
'''%symbols['console_line'])
flags=['-m32','-O2','-std=c11','-ffreestanding','-fno-builtin','-fno-pic','-fno-pie','-fno-stack-protector','-fstack-clash-protection','-fno-asynchronous-unwind-tables','-fno-unwind-tables','-fno-ident','-fshort-wchar','-mno-sse','-mno-mmx','-mpreferred-stack-boundary=2','-mincoming-stack-boundary=2','-Wall','-Wextra','-I',str(root/'src'),'-I',str(work)]
objects=[]
for name in ['proxy.c','sha256.c','runtime_print.S']:
 src=work/name if name.endswith('.S') else root/'src'/name;elf=work/(src.stem+'.elf.o');obj=work/(src.stem+'.obj')
 if name.endswith('.c'):run(['gcc',*flags,'-c',src,'-o',elf])
 else:run(['as','--32',src,'-o',elf])
 elf_to_coff(elf,obj);objects.append(obj)
# Import library generated from build-only stubs; no system DLL is shipped.
(work/'stub.S').write_text('.text\n.global api_stub\napi_stub:\n ret\n')
run(['as','--32',work/'stub.S','-o',work/'stub.o']);elf_to_coff(work/'stub.o',work/'stub.obj')
apis=['GetModuleHandleW','GetModuleFileNameW','LoadLibraryW','GetProcAddress','DisableThreadLibraryCalls','GetCurrentProcess','VirtualProtect','VirtualQuery','FlushInstructionCache','CreateFileW','GetFileSizeEx','ReadFile','WriteFile','CloseHandle','GetPrivateProfileIntW','GetStdHandle','ExitProcess']
(work/'KERNEL32.def').write_text('LIBRARY KERNEL32.dll\nEXPORTS\n'+''.join(' '+v+'=api_stub\n' for v in apis))
run(['ld','-m','i386pe','--dll','--entry','_api_stub','--no-insert-timestamp','--out-implib',work/'KERNEL32.a','-o',work/'KERNEL32.build-only.dll',work/'stub.obj',work/'KERNEL32.def'])
(work/'server.def').write_text('LIBRARY server.dll\nEXPORTS\n CreateServer=CreateServer @1\n DeleteServer=DeleteServer @2\n')
common=['ld','-m','i386pe','--dll','--entry','_DllMain','--image-base','0x69000000','--dynamicbase','--nxcompat','--enable-reloc-section','--exclude-all-symbols','--subsystem','windows','--major-os-version','5','--minor-os-version','1','--major-subsystem-version','5','--minor-subsystem-version','1','--no-insert-timestamp']
out=package/'KPCServerTools.dll'
run([*common,'--strip-all','-o',out,*objects,work/'server.def',work/'KERNEL32.a'])
run([*common,'-o',work/'server.unstripped.dll',*objects,work/'server.def',work/'KERNEL32.a'])
proxy_symbols={}
for line in subprocess.check_output(['nm','-an',str(work/'server.unstripped.dll')],text=True).splitlines():
 v=line.split()
 if len(v)==3:
  try:a=int(v[0],16)
  except ValueError:continue
  if 0x69000000<=a<0x69100000:proxy_symbols[v[2]]=a-0x69000000
report=dict(previous_proxy_hashes=['67a6be68223efb0321d41f270ae10923f0d6073731de57a61b600794c3f0eeea'],proxy_sha256=hashlib.sha256(out.read_bytes()).hexdigest(),proxy_bytes=out.stat().st_size,proxy_symbols=proxy_symbols,hook_symbols=symbols,patches=patches,hook_bytes=len(code),exports=['CreateServer','DeleteServer'])
(package/'SERVER_TOOLS_BUILD.json').write_text(json.dumps(report,indent=2)+'\n')
print(out,report['proxy_bytes'],report['proxy_sha256'])
