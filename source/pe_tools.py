"""Small read-only PE32 parser/CPU-test mapper. No Windows execution support."""
import struct


class PE:
    def __init__(self,data):
        self.data=bytes(data)
        assert self.data[:2]==b'MZ'
        self.pe=struct.unpack_from('<I',data,0x3c)[0];assert data[self.pe:self.pe+4]==b'PE\0\0'
        self.machine,self.count=struct.unpack_from('<HH',data,self.pe+4)
        self.opt=self.pe+24;assert struct.unpack_from('<H',data,self.opt)[0]==0x10b
        self.base=struct.unpack_from('<I',data,self.opt+28)[0]
        self.size=struct.unpack_from('<I',data,self.opt+56)[0]
        self.header_size=struct.unpack_from('<I',data,self.opt+60)[0]
        self.entry=struct.unpack_from('<I',data,self.opt+16)[0]
        table=self.opt+struct.unpack_from('<H',data,self.pe+20)[0]
        self.sections=[]
        for i in range(self.count):
            at=table+i*40
            vs,rva,size,raw=struct.unpack_from('<IIII',data,at+8)
            self.sections.append({'name':data[at:at+8].split(b'\0')[0].decode(),'rva':rva,'vsize':vs,'size':size,'raw':raw,'flags':struct.unpack_from('<I',data,at+36)[0]})

    def offset(self,rva):
        if rva<self.header_size:return rva
        for s in self.sections:
            if s['rva']<=rva<s['rva']+s['size']:return s['raw']+rva-s['rva']
        raise ValueError(f'No file bytes at RVA {rva:x}')

    def cstring(self,rva):
        at=self.offset(rva);end=self.data.index(0,at)
        return self.data[at:end].decode('ascii')

    def directory(self,index):return struct.unpack_from('<II',self.data,self.opt+96+index*8)

    def imports(self):
        rva,size=self.directory(1);result=[]
        if not rva:return result
        at=self.offset(rva)
        while True:
            lookup,_,_,name,iat=struct.unpack_from('<5I',self.data,at)
            if not any((lookup,name,iat)):break
            j=0
            while True:
                value=struct.unpack_from('<I',self.data,self.offset((lookup or iat)+j*4))[0]
                if not value:break
                symbol=(value&0xffff) if value&0x80000000 else self.cstring(value+2)
                result.append((self.cstring(name),symbol,iat+j*4));j+=1
            at+=20
        return result

    def exports(self):
        rva,size=self.directory(0)
        if not rva:return {}
        at=self.offset(rva)
        ordinal,nfunc,nname,funcs,names,indices=struct.unpack_from('<6I',self.data,at+16)
        result={}
        for i in range(nname):
            name=struct.unpack_from('<I',self.data,self.offset(names+i*4))[0]
            index=struct.unpack_from('<H',self.data,self.offset(indices+i*2))[0]
            address=struct.unpack_from('<I',self.data,self.offset(funcs+index*4))[0]
            result[self.cstring(name)]=address
        return result

    def map(self,cpu,base):
        cpu.mem_map(base,(self.size+4095)&~4095)
        cpu.mem_write(base,self.data[:self.header_size])
        for s in self.sections:
            if s['size']:cpu.mem_write(base+s['rva'],self.data[s['raw']:s['raw']+s['size']])
        if base!=self.base:
            rva,size=self.directory(5);assert rva and size
            at=self.offset(rva);end=at+size
            while at<end:
                page,n=struct.unpack_from('<II',self.data,at);assert n>=8
                for entry in struct.unpack_from('<'+'H'*((n-8)//2),self.data,at+8):
                    if entry>>12==0:continue
                    assert entry>>12==3
                    target=base+page+(entry&4095)
                    value=struct.unpack('<I',cpu.mem_read(target,4))[0]
                    cpu.mem_write(target,struct.pack('<I',(value+base-self.base)&0xffffffff))
                at+=n

    def protect(self,cpu,base):
        pages={}
        for at in range(0,self.header_size,4096):pages[base+at]=2
        for s in self.sections:
            flags=s['flags'];prot=(0x20 if flags&0x20000000 else 2)
            if flags&0x80000000:prot=0x40 if flags&0x20000000 else 4
            for at in range(s['rva']&~4095,(s['rva']+max(s['size'],s['vsize'])+4095)&~4095,4096):pages[base+at]=prot
        for at,prot in pages.items():cpu.mem_protect(at,4096,{2:1,4:3,0x20:5,0x40:7}[prot])
        return pages
