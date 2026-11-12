#!/usr/bin/env python3
"""Build the freestanding x86 Windows proxy with GCC and GNU binutils."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parent
KERNEL_APIS=('GetModuleHandleW','GetModuleFileNameW','GetModuleHandleExW','GetSystemDirectoryW',
    'LoadLibraryW','FreeLibrary','GetProcAddress','DisableThreadLibraryCalls','GetCurrentProcess',
    'GetCurrentThreadId','GetTickCount','Sleep','GetLastError','SetLastError','VirtualProtect',
    'FlushInstructionCache','CreateFileW','GetFileSizeEx','ReadFile','WriteFile','CloseHandle',
    'GetPrivateProfileIntW','WideCharToMultiByte','CopyFileW','MoveFileExW','DeleteFileW',
    'FlushFileBuffers','FindFirstFileW','FindNextFileW','FindClose')
EXPORTS=('DirectInputCreateA','DirectInputCreateW','DirectInputCreateEx','DllGetClassObject',
    'DllCanUnloadNow','DllRegisterServer','DllUnregisterServer','KPCFixVersion','KPCFixStatus')


def run(argv):
    subprocess.run([str(a) for a in argv],check=True)


def elf_to_coff(source,destination):
    # objcopy preserves ELF numeric relocation codes when changing containers.
    # Map the two permitted ELF i386 relocations to actual COFF relocation types.
    run(['objcopy','-O','pe-i386','--prefix-symbols=_','-R','.comment','-R','.note.GNU-stack',source,destination])
    b=bytearray(destination.read_bytes());elf=source.read_bytes()
    machine,count,_,symoff,nsym,optional,_=struct.unpack_from('<HHIIIHH',b)
    assert machine==0x14c and optional==0
    string_at=symoff+nsym*18
    def cstr(blob,at):return blob[at:blob.index(0,at)].decode()
    def coff_name(field):
        if field[:4]==b'\0'*4:return cstr(b,string_at+struct.unpack_from('<I',field,4)[0])
        return field.split(b'\0')[0].decode()
    coff_symbols={};i=0
    while i<nsym:
        at=symoff+i*18;coff_symbols[coff_name(bytes(b[at:at+8]))]=i;i+=1+b[at+17]
    coff_sections={}
    for i in range(count):
        at=20+i*40;field=bytes(b[at:at+8]);name=field.split(b'\0')[0].decode()
        if name.startswith('/'):name=cstr(b,string_at+int(name[1:]))
        raw,reloc=struct.unpack_from('<II',b,at+20);n=struct.unpack_from('<H',b,at+32)[0]
        coff_sections[name]=(i+1,raw,{struct.unpack_from('<I',b,reloc+j*10)[0]:reloc+j*10 for j in range(n)})
    assert elf[:6]==b'\x7fELF\x01\x01'
    shoff=struct.unpack_from('<I',elf,32)[0];shent,shnum,shnames=struct.unpack_from('<HHH',elf,46)
    es=[struct.unpack_from('<10I',elf,shoff+j*shent) for j in range(shnum)]
    names_at=es[shnames][4]
    section_names=[cstr(elf,names_at+s[0]) for s in es]
    symtab=next(s for s in es if s[1]==2);symbols=[struct.unpack_from('<IIIBBH',elf,symtab[4]+j*symtab[9]) for j in range(symtab[5]//symtab[9])]
    strtab=es[symtab[6]][4]
    # ELF section symbols disappear in objcopy's COFF symbol table. Add explicit
    # zero-offset section labels and rewrite every relocation's symbol index.
    section_symbols={};extra=bytearray()
    for name,(number,raw,relocations) in coff_sections.items():
        section_symbols[name]=nsym+len(extra)//18
        extra.extend(struct.pack('<8sIhHBB',('_es%03d'%number).encode(),0,number,0,3,0))
    b[string_at:string_at]=extra
    struct.pack_into('<I',b,12,nsym+len(extra)//18)
    converted=[]
    for rel_section in es:
        if rel_section[1]!=9:continue
        target=es[rel_section[7]];name=section_names[rel_section[7]]
        if name not in coff_sections:raise ValueError('Relocated section was removed: '+name)
        _,raw,relocations=coff_sections[name]
        assert len(relocations)==rel_section[5]//rel_section[9]
        for j in range(rel_section[5]//rel_section[9]):
            offset,info=struct.unpack_from('<II',elf,rel_section[4]+j*rel_section[9]);kind=info&255
            symbol=symbols[info>>8];stype=symbol[3]&15
            if stype==3:index=section_symbols[section_names[symbol[5]]]
            else:
                symbol_name='_'+cstr(elf,strtab+symbol[0])
                if symbol_name not in coff_symbols:raise ValueError('Missing COFF symbol: '+symbol_name)
                index=coff_symbols[symbol_name]
            at=relocations[offset];struct.pack_into('<I',b,at+4,index)
            addend=struct.unpack_from('<I',elf,target[4]+offset)[0]
            if kind==1:                         # R_386_32 -> IMAGE_REL_I386_DIR32
                struct.pack_into('<H',b,at+8,6)
                struct.pack_into('<I',b,raw+offset,addend)
            elif kind==2:                       # R_386_PC32 -> IMAGE_REL_I386_REL32
                struct.pack_into('<H',b,at+8,20)
                struct.pack_into('<I',b,raw+offset,(addend+4)&0xffffffff)
            else:
                raise ValueError(f'Unsupported ELF relocation {kind}; refusing output')
            converted.append(kind)
    destination.write_bytes(b)
    return {'object':destination.name,'dir32':converted.count(1),'rel32':converted.count(2)}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build-dir',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--platform',choices=('windows',),default='windows')
    args=ap.parse_args()
    work=args.build_dir.resolve();work.mkdir(parents=True,exist_ok=True)
    output=args.output.resolve()
    if output.exists():raise SystemExit('Output must be a new file')
    cflags=['-m32','-O2','-std=c11','-ffreestanding','-fno-builtin','-fno-pic','-fno-pie',
        '-fno-stack-protector','-fstack-clash-protection','-fno-asynchronous-unwind-tables',
        '-fno-unwind-tables','-fno-ident','-fshort-wchar','-mno-sse','-mno-mmx',
        '-mpreferred-stack-boundary=2','-mincoming-stack-boundary=2','-Wall','-Wextra',
        '-Wno-misleading-indentation','-I',str(ROOT/'src')]
    conversions=[];objects=[]
    for name in ('kpcfix.c','sha256.c','hud.c','ui.c','configs.c','cinematic.c','loading.c','console.c','camera.c','netwatch.c','netwatch_hook.S','focus.c','minstep_hook.S','cinematic_hook.S','timing.S','ground.S','input_interval.S','hud_hooks.S','ui_hooks.S'):
        source=ROOT/'src'/name;obj=work/(source.stem+'.obj');elf=work/(source.stem+'.elf.o')
        if source.suffix=='.c':run(['gcc',*cflags,'-c',source,'-o',elf])
        else:run(['as','--32','-o',elf,source])
        conversions.append(elf_to_coff(elf,obj));objects.append(obj)
    # Generate import libraries without copying or shipping any system DLL.
    # These build-only export stubs are never run and must never be deployed.
    (work/'api_stub.S').write_text('.text\n.global api_stub\napi_stub:\n ret\n')
    run(['as','--32','-o',work/'api_stub.elf.o',work/'api_stub.S'])
    elf_to_coff(work/'api_stub.elf.o',work/'api_stub.obj')
    libraries=[]
    for library,names in [('KERNEL32',KERNEL_APIS),('USER32',('MessageBoxA',))]:
        definition=work/(library+'.def')
        definition.write_text('LIBRARY '+library+'.dll\nEXPORTS\n'+''.join(' '+n+'=api_stub\n' for n in names))
        archive=work/(library+'.a');libraries.append(archive)
        run(['ld','-m','i386pe','--dll','--entry','_api_stub','--no-insert-timestamp',
            '--out-implib',archive,'-o',work/(library+'.build-only.dll'),work/'api_stub.obj',definition])
    definition=work/'dinput.def'
    definition.write_text('LIBRARY dinput.dll\nEXPORTS\n'+''.join(' '+n+'='+n+'\n' for n in EXPORTS))
    common=['ld','-m','i386pe','--dll','--entry','_DllMain','--image-base','0x68000000',
        '--dynamicbase','--nxcompat','--enable-reloc-section','--exclude-all-symbols',
        '--subsystem','windows','--major-os-version','5','--minor-os-version','1',
        '--major-subsystem-version','5','--minor-subsystem-version','1',
        '--no-insert-timestamp']
    run([*common,'--strip-all','-Map',work/'dinput.map','-o',output,*objects,definition,*libraries])
    run([*common,'-o',work/'dinput.unstripped.dll',*objects,definition,*libraries])
    symbols={}
    for line in subprocess.check_output(['nm','-an',str(work/'dinput.unstripped.dll')],text=True).splitlines():
        parts=line.split()
        if len(parts)==3:
            try:address=int(parts[0],16)
            except ValueError:continue
            if 0x68000000<=address<0x68100000:symbols[parts[2]]=address-0x68000000
    (work/'SYMBOLS.json').write_text(json.dumps(symbols,indent=2)+'\n')
    b=output.read_bytes();report={'output':output.name,'platform':args.platform,'sha256':hashlib.sha256(b).hexdigest(),
        'bytes':len(b),'exports':list(EXPORTS),'elf_to_coff':conversions,
        'build_only_system_stubs':'Not deployed. Used only to generate import libraries.'}
    (work/'BUILD_RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
