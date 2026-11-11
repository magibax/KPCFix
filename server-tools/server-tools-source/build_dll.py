"""Build the x86 server.dll proxy, without redistributing original game binaries.
Usage: python server-tools-source/build_dll.py
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
supported=json.loads((package/'SERVER_TOOLS_SUPPORTED_BUILDS.json').read_text())
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
# Co-op lobby: C ABI adapter into the Windows-tested chat broadcast routine
# (the same path that announces cheat permission changes).
.global runtime_broadcast
runtime_broadcast:
 push ebp
 mov ebp,[esp+8]
 mov edx,[esp+12]
 lea eax,[ebp+%d]
 call eax
 pop ebp
 ret
'''%(symbols['console_line'],symbols['broadcast_only']))
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
apis=['GetModuleHandleW','GetModuleFileNameW','LoadLibraryW','GetProcAddress','DisableThreadLibraryCalls','GetCurrentProcess','VirtualProtect','VirtualQuery','FlushInstructionCache','CreateFileW','GetFileSizeEx','ReadFile','WriteFile','CloseHandle','GetPrivateProfileIntW','GetStdHandle','ExitProcess','GetTickCount']
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
# Fixed, reviewed upgrade allowlist. Never inherit arbitrary hashes from a
# previous JSON manifest: future candidates require explicit review and
# supported Windows upgrade evidence before being added here.
new_hash=hashlib.sha256(out.read_bytes()).hexdigest()
previous_proxies=[
    '6d6b76ab4b04842464fc11a4e18e6a6c81f3318271d31b9602cb4604e889d380',
    '67a6be68223efb0321d41f270ae10923f0d6073731de57a61b600794c3f0eeea',
    '9b6bd69e894925e6b31f1ed32b6ff82331af104e2c20fbb6aeb43dc4bef5be9a',
    '66653630a0b717a70c3dd51a3b97efd75bd9b20495bd0b4d089b914b60942a37',
    'c2486afa934b7216486891dc640929018ec02cdd4341734c9adc7889bff94c3b',
    '354a75a1d56968a69ab47539a715f805c210fdcddab02ec0aa0c2fa93ceacd2b',
    '23dd5de7f099c04706d2a59603bb88763d21bbde2cd9053cf6ebade935598b08',
    '7112b5ed97f94f8b7cd84bc80f7fdc945f0fb02e1a1cc6ad8f574b9b70d6d373',
    'a63d186bb4995e471651f7d9c42089eea784e07c6d5ef76355936fa71e3a7e6c',
    'b0404468077d0aec012fa9268618d375b99361d16dc669178dc8df0c45f0fa38',
    '8de76e10a81be3ceb1ca13b93fe2c9606dc5bf0f7a27784592b15bb7698f0807',
    'bac7696bc79f10b637134b7bbffea1ead01e67c8207452f5c3f339a8b5a353d4',
    # spawn-list-control-v8 (f94affd): previously tracked experimental DLL;
    # reviewed so testers can upgrade it in place to the co-op session build.
    '7ea0dfa44dc7870579de6b4e6ffea9018903b7525c3aa7d1b8c9d91013d3fc0d',
    # coop-session-v1 first CI artifact (d8e3420, PR #10); same protocol.
    '3048d6f3dcab6959628edc77fd5a5f5b164a455e346965b11ee0c61ba7eb4257',
]
if new_hash in previous_proxies:
 raise SystemExit('Current proxy SHA-256 unexpectedly appears in the previous allowlist')
report=dict(previous_proxy_hashes=previous_proxies,proxy_sha256=new_hash,proxy_bytes=out.stat().st_size,proxy_symbols=proxy_symbols,hook_symbols=symbols,patches=patches,hook_bytes=len(code),exports=['CreateServer','DeleteServer'])
(package/'SERVER_TOOLS_BUILD.json').write_text(json.dumps(report,indent=2)+'\n')
print(out,report['proxy_bytes'],report['proxy_sha256'])
