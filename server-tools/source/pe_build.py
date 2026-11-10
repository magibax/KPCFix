from pathlib import Path
import struct,subprocess

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


