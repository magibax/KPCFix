#!/usr/bin/env python3
"""Execute the linked DLL under Unicorn with synthetic Win32 services."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

from fixture_layout import rva, italian, crt_address
from pe_tools import PE

ROOT=Path(__file__).resolve().parents[1]


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--fixtures',type=Path,required=True)
    ap.add_argument('--symbols',type=Path,required=True)
    ap.add_argument('--dependency-dir',type=Path)
    ap.add_argument('--output',type=Path)
    ap.add_argument('--quick',action='store_true')
    ap.add_argument('--expose-rig',action='store_true',help=argparse.SUPPRESS)
    ap.add_argument('--platform',choices=('windows',),default='windows')
    args=ap.parse_args()
    if args.dependency_dir:sys.path.insert(0,str(args.dependency_dir.resolve()))
    import unicorn as uc
    import unicorn.x86_const as x
    pack=lambda value:struct.pack('<I',value&0xffffffff)
    syms=json.loads(args.symbols.read_text())
    dll=PE((ROOT/'dinput.dll').read_bytes())
    fixtures=args.fixtures.resolve()
    from fixture_layout import select
    select(fixtures)
    clients={'original':(fixtures/'client.exe').read_bytes()}
    shells={'original':(fixtures/'cshell.dll').read_bytes()}

    class Rig:
        def __init__(self,client='original',shell='original',dllbase=0x68000000,shellbase=0x20000000,pending=False,scenario='',fast_hash=True,settings=None):
            self.u=uc.Uc(uc.UC_ARCH_X86,uc.UC_MODE_32);self.dllbase=dllbase;self.shellbase=shellbase
            self.client_name=client;self.shell_name=shell;self.scenario=scenario;self.fast_hash=fast_hash
            self.settings={"Multiplayer.EmptyBindIPFix":0,**(settings or {})}
            self.loaded=not pending;self.last_error=0x11223344;self.tick=1000;self.messages=[];self.logs=bytearray();self.calls=[];self.input_calls=[];self.writes=[];self.vprotect_calls=0;self.flush_calls=0;self.hash_calls=0
            self.file_data={'C:\\Games\\Psycho\\client.exe':clients[client],'C:\\Games\\Psycho\\cshell.dll':shells[shell]}
            if scenario=='unknown_client':self.file_data['C:\\Games\\Psycho\\client.exe']+=b'unknown'
            if scenario=='unknown_shell':self.file_data['C:\\Games\\Psycho\\cshell.dll']+=b'unknown'
            self.files={};self.next_file=100
            self.client=PE(clients[client]);self.shell=PE(shells[shell]);self.client.map(self.u,0x1ca0000);self.shell.map(self.u,shellbase);dll.map(self.u,dllbase)
            self.u.mem_map(0x400000,0x20000);self.u.mem_map(0x500000,0x100000)
            self.u.mem_map(0x60000000,0x10000);self.u.mem_map(0x62000000,0x10000)
            self.u.mem_write(0x62000000,b'\xdd\x05'+pack(0x560000))
            self.stub_names={};self.stub_args={}
            argn={'GetModuleHandleW':1,'GetModuleFileNameW':3,'GetModuleHandleExW':3,'GetSystemDirectoryW':2,
                'LoadLibraryW':1,'FreeLibrary':1,'GetProcAddress':2,'DisableThreadLibraryCalls':1,'GetCurrentProcess':0,
                'GetCurrentThreadId':0,'GetTickCount':0,'Sleep':1,'GetLastError':0,'SetLastError':1,'VirtualProtect':4,
                'FlushInstructionCache':3,'CreateFileW':7,'GetFileSizeEx':2,'ReadFile':5,'WriteFile':5,'CloseHandle':1,
                'GetPrivateProfileIntW':4,'WideCharToMultiByte':8,'MessageBoxA':4,'RealLoadLibraryA':1,
                'CopyFileW':3,'MoveFileExW':3,'DeleteFileW':1,'FlushFileBuffers':1,
                'FindFirstFileW':2,'FindNextFileW':2,'FindClose':1,
                'RealDirectInputCreateA':4,'RealDirectInputCreateW':4,'RealDirectInputCreateEx':5,
                'RealDllGetClassObject':3,'RealDllCanUnloadNow':0,'RealDllRegisterServer':0,'RealDllUnregisterServer':0,
                'PhysicsUpdate':1,'PhysicsMove':3,'TimeGetTime':0}
            for i,(name,n) in enumerate(argn.items()):
                address=0x60000000+i*32;self.stub_names[address]=name;self.stub_args[name]=n
                self.u.mem_write(address,b'\xc2'+struct.pack('<H',n*4) if n else b'\xc3')
            self.stub_address={v:k for k,v in self.stub_names.items()}
            for library,name,iat in dll.imports():self.u.mem_write(dllbase+iat,pack(self.stub_address[name]))
            self.u.mem_write(0x1d371f0,pack(self.stub_address['RealLoadLibraryA']))
            self.u.mem_write(0x1d37278,pack(self.stub_address['TimeGetTime']))
            self.u.mem_write(0x1d37188,pack(self.stub_address['GetTickCount']))
            if italian():
                # Imported CRT helpers in the Italian build correspond to CRT
                # routines linked into the English and German cshell images.
                for slot,address in ((0xec07c,0x6200f000),(0xec090,0x6200f020),
                                     (0xec0b0,0x6200f040),(0xec088,0x01d21028)):
                    self.u.mem_write(shellbase+slot,pack(address))
            self.clock_probe=None
            if scenario=='input_clock_import_missing':self.u.mem_write(0x1d37278,pack(0))
            self.pages={**self.client.protect(self.u,0x1ca0000),**self.shell.protect(self.u,shellbase),**dll.protect(self.u,dllbase)}
            self.initial_protection=dict(self.pages)
            if scenario=='memory_mismatch':
                at=0x1d064b2;page=at&~4095;self.u.mem_protect(page,4096,7);self.u.mem_write(at,b'\x90');self.u.mem_protect(page,4096,5)
            if scenario=='input_clock_memory_mismatch':
                at=0x1cd42c3;page=at&~4095;self.u.mem_protect(page,4096,7);self.u.mem_write(at,b'\x90');self.u.mem_protect(page,4096,5)
            if scenario=='input_interval_memory_mismatch':
                at=0x1cd44a5;page=at&~4095;self.u.mem_protect(page,4096,7);self.u.mem_write(at,b'\x90');self.u.mem_protect(page,4096,5)
            self.initial_client=self.client_bytes();self.initial_shell=self.shell_bytes()
            self.u.hook_add(uc.UC_HOOK_CODE,self.api,begin=0x60000000,end=0x6000ffff)
            if fast_hash:self.u.hook_add(uc.UC_HOOK_CODE,self.fake_hash,begin=dllbase+syms['_kpc_hash_file'],end=dllbase+syms['_kpc_hash_file'])
            self.u.reg_write(x.UC_X86_REG_CR4,self.u.reg_read(x.UC_X86_REG_CR4)|0x200)
            self.u.reg_write(x.UC_X86_REG_FPCW,0x037f);self.u.reg_write(x.UC_X86_REG_FPTAG,0xffff)
            self.invoke_address(dllbase+dll.entry,[dllbase,1,0])
            assert self.calls==['DisableThreadLibraryCalls'],'DllMain did more than minimal setup'

        def cstr(self,address):
            out=bytearray()
            for i in range(4096):
                b=self.u.mem_read(address+i,1)[0]
                if not b:return out.decode('utf8')
                out.append(b)
            raise AssertionError('unterminated string')
        def wstr(self,address):
            out=bytearray()
            for i in range(1024):
                b=bytes(self.u.mem_read(address+i*2,2))
                if b==b'\0\0':return out.decode('utf-16le')
                out.extend(b)
            raise AssertionError('unterminated wide string')
        def put_w(self,address,text,capacity):
            encoded=text.encode('utf-16le')+b'\0\0';assert len(encoded)//2<=capacity
            self.u.mem_write(address,encoded)
        def client_bytes(self):return bytes(self.u.mem_read(0x1ca0000,self.client.size))
        def shell_bytes(self):return bytes(self.u.mem_read(self.shellbase,self.shell.size))
        def fake_hash(self,u,address,size,user):
            sp=u.reg_read(x.UC_X86_REG_ESP);ret,path,out,sizeout=struct.unpack('<4I',u.mem_read(sp,16))
            if self.fast_hash=='shell_only' and self.wstr(path).lower().endswith('client.exe'):
                return  # execute the real compiled C file reader and SHA-256
            data=self.file_data.get(self.wstr(path));self.hash_calls+=1
            if data:
                u.mem_write(out,hashlib.sha256(data).digest());u.mem_write(sizeout,pack(len(data)))
            u.reg_write(x.UC_X86_REG_EAX,int(bool(data)));u.reg_write(x.UC_X86_REG_ESP,sp+4);u.reg_write(x.UC_X86_REG_EIP,ret)

        def api(self,u,address,size,user):
            name=self.stub_names.get(address)
            if not name:return
            sp=u.reg_read(x.UC_X86_REG_ESP);n=self.stub_args[name]
            a=struct.unpack('<'+'I'*n,u.mem_read(sp+4,n*4)) if n else ()
            self.calls.append(name);result=1
            if name=='GetLastError':result=self.last_error
            elif name=='SetLastError':self.last_error=a[0]
            elif name=='GetCurrentThreadId':result=1
            elif name=='GetTickCount':
                self.tick+=1;result=self.clock_probe['coarse'] if self.clock_probe else self.tick
            elif name=='TimeGetTime':result=self.clock_probe['fine'] if self.clock_probe else self.tick+5
            elif name=='GetCurrentProcess':result=0xffffffff
            elif name=='GetModuleHandleW':
                result=0x1ca0000 if a[0]==0 else 0x71000000 if self.wstr(a[0]).lower()=='ntdll.dll' else self.shellbase if self.loaded and self.wstr(a[0]).lower().endswith('cshell.dll') else 0
            elif name=='GetModuleFileNameW':
                module,dest,capacity=a
                text='C:\\Games\\Psycho\\client.exe' if module in (0,0x1ca0000) else 'C:\\Games\\Psycho\\cshell.dll' if module==self.shellbase else 'C:\\Games\\Psycho\\dinput.dll' if module==self.dllbase else 'C:\\Windows\\System32\\other.dll'
                self.put_w(dest,text,capacity);result=len(text)
            elif name=='GetModuleHandleExW':
                flags,path,out=a
                module=self.dllbase if flags==5 and self.scenario!='pin_failure' else self.shellbase if flags==0 and self.loaded else 0
                u.mem_write(out,pack(module));result=int(bool(module))
            elif name=='GetSystemDirectoryW':self.put_w(a[0],'C:\\Windows\\System32',a[1]);result=19
            elif name=='LoadLibraryW':
                assert self.wstr(a[0])=='C:\\Windows\\System32\\dinput.dll'
                result=self.dllbase if self.scenario=='proxy_recursion' else 0x70000000
            elif name=='GetProcAddress':
                assert a[0]==0x70000000
                result=self.stub_address['Real'+self.cstr(a[1])]
            elif name=='GetPrivateProfileIntW':
                section,key=self.wstr(a[0]),self.wstr(a[1]);assert section in ('KPCFix','HUD','Cursor','Tooltip','Multiplayer'),(section,key,a)
                if section!='KPCFix':key=section+'.'+key
                # Existing-fix suites isolate the original core; test_nettrace
                # explicitly enables the new observer and supplies Winsock APIs.
                result=0 if (key=='Enabled' and self.scenario=='disabled') or (key=='Log' and self.scenario=='log_off') or (key=='InputClock' and self.scenario=='input_clock_off') else self.settings.get(key,0 if key=='Multiplayer.NetTrace' else a[2])
            elif name=='CreateFileW':
                path=self.wstr(a[0]);assert a[3]==0 and a[6]==0
                if a[1]==4:
                    assert path=='C:\\Games\\Psycho\\KPCFix.log' and a[4]==4
                    result=99
                else:
                    assert a[1]==0x80000000 and a[4]==3
                    data=self.file_data.get(path)
                    if data is None:result=0xffffffff
                    else:
                        result=self.next_file;self.next_file+=1;self.files[result]={'data':data,'at':0}
            elif name=='GetFileSizeEx':u.mem_write(a[1],struct.pack('<Q',len(self.files[a[0]]['data'])))
            elif name=='ReadFile':
                f=self.files[a[0]];data=f['data'][f['at']:f['at']+a[2]];f['at']+=len(data)
                if data:u.mem_write(a[1],data)
                u.mem_write(a[3],pack(len(data)));assert a[4]==0
            elif name=='WriteFile':
                assert a[0]==99 and a[4]==0
                self.logs.extend(bytes(u.mem_read(a[1],a[2])));u.mem_write(a[3],pack(a[2]))
            elif name=='CloseHandle':self.files.pop(a[0],None)
            elif name=='FindFirstFileW':result=0xffffffff
            elif name=='FindNextFileW':result=0
            elif name in ('FindClose','CopyFileW','MoveFileExW','DeleteFileW','FlushFileBuffers'):
                raise AssertionError('Unexpected config mutation in baseline fixture: '+name)
            elif name=='WideCharToMultiByte':
                assert a[0]==65001 and a[3]==0xffffffff
                data=self.wstr(a[2]).encode('utf8')+b'\0';assert len(data)<=a[5]
                u.mem_write(a[4],data);result=len(data)
            elif name=='MessageBoxA':self.messages.append(self.cstr(a[1]))
            elif name=='VirtualProtect':
                self.vprotect_calls+=1
                if self.scenario=='protect_failure' and self.vprotect_calls==3:result=0
                elif self.scenario=='restore_protect_failure' and self.vprotect_calls==2:result=0
                else:
                    at,length,prot,out=a;page=at&~4095
                    u.mem_write(out,pack(self.pages[page]))
                    for p in range(page,(at+length+4095)&~4095,4096):
                        self.pages[p]=prot;u.mem_protect(p,4096,{2:1,4:3,0x20:5,0x40:7}[prot])
                    self.writes.append((at,length,prot))
            elif name=='FlushInstructionCache':
                self.flush_calls+=1
                assert a[0]==0xffffffff
                if self.scenario=='flush_failure' and self.flush_calls==1:result=0
            elif name=='RealLoadLibraryA':
                text=self.cstr(a[0])
                if text=='missing.dll':result=0;self.last_error=126
                else:
                    self.loaded=True;result=self.shellbase if text.lower().endswith('cshell.dll') else 0x70020000
                    self.last_error=0x55667788
            elif name.startswith('RealDirectInput') or name=='RealDllGetClassObject':
                self.input_calls.append((name,a,self.last_error))
                out=a[3] if name=='RealDirectInputCreateEx' else a[2]
                u.mem_write(out,pack(0x00abc000));self.last_error=0x42;result=0
            elif name=='RealDllCanUnloadNow':result=1
            elif name in ('RealDllRegisterServer','RealDllUnregisterServer'):result=0
            elif name=='PhysicsUpdate':
                u.mem_write(a[0]+8,struct.pack('<fff',*self.physics_delta))
            elif name=='PhysicsMove':
                assert a[0]==0x510000 and a[2]==0
                assert u.reg_read(x.UC_X86_REG_ECX)==0x540000 and u.reg_read(x.UC_X86_REG_EDX)==0x540100
                self.physics_targets.append(struct.unpack('<fff',u.mem_read(a[1],12)))
            elif name in ('DisableThreadLibraryCalls','FreeLibrary','Sleep'):pass
            else:raise AssertionError(name)
            if name not in ('GetLastError','SetLastError','RealLoadLibraryA') and not name.startswith('RealDirectInput') and name!='RealDllGetClassObject':self.last_error=0xaabbccdd
            u.reg_write(x.UC_X86_REG_EAX,result&0xffffffff)

        def invoke_address(self,address,args=(),this=None,limit=250000000):
            sp=0x418000
            self.u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),0x62000100,*args))
            for reg,value in ((x.UC_X86_REG_ESP,sp),(x.UC_X86_REG_EBX,0x12345678),(x.UC_X86_REG_ESI,0x23456789),(x.UC_X86_REG_EDI,0x3456789a),(x.UC_X86_REG_EBP,0x456789ab)):
                self.u.reg_write(reg,value)
            if this is not None:self.u.reg_write(x.UC_X86_REG_ECX,this)
            self.u.emu_start(address,0x62000100,timeout=120_000_000 if limit==0 else 0,count=limit)
            assert self.u.reg_read(x.UC_X86_REG_EIP)==0x62000100,'instruction budget exhausted'
            assert self.u.reg_read(x.UC_X86_REG_ESP)==sp+4+len(args)*4
            for reg,value in ((x.UC_X86_REG_EBX,0x12345678),(x.UC_X86_REG_ESI,0x23456789),(x.UC_X86_REG_EDI,0x3456789a),(x.UC_X86_REG_EBP,0x456789ab)):
                assert self.u.reg_read(reg)==value
            return self.u.reg_read(x.UC_X86_REG_EAX)
        def invoke(self,name,args=()):return self.invoke_address(self.dllbase+dll.exports()[name],args)
        def factory(self):
            self.last_error=0x11223344
            result=self.invoke('DirectInputCreateA',(0x11111111,0x700,0x550000,0))
            if self.scenario!='proxy_recursion':
                assert result==0 and struct.unpack('<I',self.u.mem_read(0x550000,4))[0]==0xabc000
                assert self.input_calls[-1]==('RealDirectInputCreateA',(0x11111111,0x700,0x550000,0),0x11223344)
                assert self.last_error==0x42
            return result
        def load_shell(self,name='cshell.dll'):
            self.u.mem_write(0x550100,name.encode()+b'\0')
            fn=struct.unpack('<I',self.u.mem_read(0x1d371f0,4))[0]
            value=self.invoke_address(fn,(0x550100,))
            assert self.last_error==(126 if name=='missing.dll' else 0x55667788)
            return value
        def physics(self,delta=(1,-.0006,0),ground=1,jump=0,mode=0,vy=-1.25):
            self.physics_delta=delta;self.physics_targets=[]
            u=self.u;base=self.shellbase
            for address,value in ((base+rva(0x1c8de4),0x540000),(0x540000,0x540100),(0x54015c,self.stub_address['PhysicsUpdate']),(0x540140,self.stub_address['PhysicsMove']),
                (base+rva(0x1c8dfc),0x530000),(0x530268,0x1cae360),(0x500038,0x510000),(0x510028,0x20000),(0x500044,jump),(0x50004c,0),(0x500054,ground|(mode<<8))):u.mem_write(address,pack(value))
            u.mem_write(0x5000a8,struct.pack('<f',.001));u.mem_write(0x510040,struct.pack('<fff',16,-81.96875,200));u.mem_write(0x510080,struct.pack('<f',vy))
            self.invoke_address(base+rva(0xbca10),this=0x500000)
            return self.physics_targets
        def timing_call(self,index,value):
            self.u.mem_write(0x560000,struct.pack('<d',value));self.u.reg_write(x.UC_X86_REG_FPTAG,0xffff);self.u.reg_write(x.UC_X86_REG_FPSW,0)
            self.u.reg_write(x.UC_X86_REG_ESP,0x418000);self.u.emu_start(0x62000000,0x62000006,count=3)
            site=0x1ca0000+(0x4742,0x4854,0x4a03,0x4a83,0x4b52,0x4c65,0x4d33,0x4db3)[index]
            self.u.emu_start(site,site+5,count=10000)
            assert self.u.reg_read(x.UC_X86_REG_EIP)==site+5 and self.u.reg_read(x.UC_X86_REG_ESP)==0x418000
            lo=self.u.reg_read(x.UC_X86_REG_EAX);hi=self.u.reg_read(x.UC_X86_REG_EDX)
            return (hi<<32)|lo
        def interval(self,previous,event,pending,frame,minimum):
            u=self.u;axis=0x570000;record=0x571000;sp=0x418000
            for at,value in ((axis+0xfc,previous),(axis+0x100,pending),(record+4,event),(sp+0x24,frame),(0x1d4b668,minimum)):u.mem_write(at,pack(value))
            for reg,value in ((x.UC_X86_REG_ESP,sp),(x.UC_X86_REG_ESI,axis),(x.UC_X86_REG_EDI,record),(x.UC_X86_REG_EBX,3),(x.UC_X86_REG_EBP,0x12345678)):u.reg_write(reg,value)
            cw=u.reg_read(x.UC_X86_REG_FPCW);tag=u.reg_read(x.UC_X86_REG_FPTAG)
            u.emu_start(0x1cd44a5,0x1cd44d1,count=1000)
            assert u.reg_read(x.UC_X86_REG_EIP)==0x1cd44d1 and u.reg_read(x.UC_X86_REG_ESP)==sp
            for reg,value in ((x.UC_X86_REG_ESI,axis),(x.UC_X86_REG_EDI,record),(x.UC_X86_REG_EBX,3),(x.UC_X86_REG_EBP,0x12345678),(x.UC_X86_REG_EDX,frame)):assert u.reg_read(reg)==value
            assert u.reg_read(x.UC_X86_REG_FPCW)==cw and u.reg_read(x.UC_X86_REG_FPTAG)==tag
            return u.reg_read(x.UC_X86_REG_EAX)

        def sensitivity(self,position):
            # Execute the real slider callback; mock only its generic settings write.
            u=self.u;widget=0x580000;settings=0x584000;sp=0x418000;captured=[]
            u.mem_write(widget+0x114,struct.pack('<f',position))
            u.mem_write(self.shellbase+rva(0x1d30e4),pack(settings))
            def save(cpu,address,size,user):
                stack=cpu.reg_read(x.UC_X86_REG_ESP)
                ret,category,index,value=struct.unpack('<4I',cpu.mem_read(stack,16))
                assert category==0 and index==2 and cpu.reg_read(x.UC_X86_REG_ECX)==settings
                captured.append(struct.unpack('<f',pack(value))[0])
                cpu.reg_write(x.UC_X86_REG_ESP,stack+16);cpu.reg_write(x.UC_X86_REG_EIP,ret)
            hook=u.hook_add(uc.UC_HOOK_CODE,save,begin=self.shellbase+rva(0xdb800),end=self.shellbase+rva(0xdb800))
            # Callback is cdecl and overwrites its stack argument with a float.
            u.mem_write(sp,pack(0x62000100)+pack(widget));u.reg_write(x.UC_X86_REG_ESP,sp)
            u.emu_start(self.shellbase+rva(0x24650),0x62000100,count=20000)
            u.hook_del(hook);assert len(captured)==1 and u.reg_read(x.UC_X86_REG_ESP)==sp+4
            # Execute original readback/clamp block through the slider-value store.
            u.mem_write(settings+0xd0,struct.pack('<f',captured[0]))
            u.reg_write(x.UC_X86_REG_EDX,settings);u.reg_write(x.UC_X86_REG_ESI if italian() else x.UC_X86_REG_EBP,widget)
            u.reg_write(x.UC_X86_REG_ESP,sp);u.reg_write(x.UC_X86_REG_FPTAG,0xffff);u.reg_write(x.UC_X86_REG_FPSW,0)
            u.emu_start(self.shellbase+rva(0x24bcc),self.shellbase+rva(0x24c13),count=20000)
            back=struct.unpack('<f',u.mem_read(widget+0x114,4))[0]
            u.reg_write(x.UC_X86_REG_FPTAG,0xffff);u.reg_write(x.UC_X86_REG_FPSW,0)
            return captured[0],back

    return Rig,dll,syms,uc,x,args
