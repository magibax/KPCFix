#!/usr/bin/env python3
"""CPU/ABI checks for the linked, observational Winsock import wrappers.

Network APIs are mocked: this does not contact the server or reproduce the
user's native failure. It verifies forwarding, errors, buffers and bounded logs.
"""
from fixture_layout import rva, italian, crt_address
import argparse,json,struct,sys
from pathlib import Path
import test_runtime

SLOTS={0x97284:('WSAStartup',2,'kpc_net_startup'),0x972a0:('WSACleanup',0,'kpc_net_cleanup'),
       0x972a8:('Socket',3,'kpc_net_socket'),0x9728c:('CloseSocket',1,'kpc_net_close'),
       0x972a4:('Bind',3,'kpc_net_bind'),0x97290:('SendTo',6,'kpc_net_send'),
       0x9729c:('RecvFrom',6,'kpc_net_receive')}
p32=lambda n:struct.pack('<I',n&0xffffffff)
ADDRESS,BUFFER,LENGTH,WSADATA=0x588000,0x589000,0x58a000,0x58b000
SOCKADDR=struct.pack('<H',2)+(42972).to_bytes(2,'big')+bytes((192,168,1,3))+bytes(8)
PAYLOAD=bytes.fromhex('0102030405060708')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('fixtures','symbols','dependency-dir','output'):ap.add_argument('--'+name,type=Path,required=True)
    args=ap.parse_args();saved=sys.argv
    sys.argv=[saved[0],'--fixtures',str(args.fixtures),'--symbols',str(args.symbols),'--dependency-dir',str(args.dependency_dir),'--platform','windows','--expose-rig']
    try:Rig,dll,syms,uc,x,_=test_runtime.main()
    finally:sys.argv=saved
    rows=[]

    class NetRig(Rig):
        def __init__(self,enabled=True,alias=False,missing_getter=False,**kw):
            self.alias=alias;self.wsa_error=0;self.missing_getter=missing_getter
            self.network_calls=[];self.network_results={}
            settings=kw.pop('settings',{})
            settings.update({'Multiplayer.NetTrace':int(enabled),'HUD.ScalePercent':200,'NormalizeInputRateConfigs':0})
            super().__init__(settings=settings,**kw)
            definitions={v[0]:v[1] for v in SLOTS.values()};definitions.update({'WSAGetLastError':0,'WSASetLastError':1})
            for index,(name,nargs) in enumerate(definitions.items()):
                address=0x60002000+index*32;self.stub_names[address]='Net'+name;self.stub_args['Net'+name]=nargs
                self.stub_address['Net'+name]=address
                self.u.mem_write(address,b'\xc2'+struct.pack('<H',nargs*4) if nargs else b'\xc3')
            page=0x1d37000;self.u.mem_protect(page,4096,7)
            for slot,(name,_,_) in SLOTS.items():self.u.mem_write(0x1ca0000+slot,p32(self.stub_address['Net'+name]))
            self.u.mem_write(0x1d372ac,p32(0 if missing_getter else self.stub_address['NetWSAGetLastError']))
            self.u.mem_protect(page,4096,{2:1,4:3,0x20:5,0x40:7}[self.pages[page]])
            self.u.mem_write(ADDRESS,SOCKADDR);self.u.mem_write(BUFFER,PAYLOAD+bytes(24));self.u.mem_write(LENGTH,p32(16))
        def api(self,u,address,size,user):
            name=self.stub_names.get(address);sp=u.reg_read(x.UC_X86_REG_ESP)
            n=self.stub_args.get(name,0);a=list(struct.unpack('<'+'I'*n,u.mem_read(sp+4,n*4))) if n else []
            if name=='GetModuleHandleW' and a[0] and self.wstr(a[0]).upper()=='WSOCK32.DLL':
                u.reg_write(x.UC_X86_REG_EAX,0x72000000);return
            if name=='GetProcAddress' and a[0]==0x72000000:
                assert self.cstr(a[1])=='WSASetLastError'
                u.reg_write(x.UC_X86_REG_EAX,self.stub_address['NetWSASetLastError']);return
            if name and name.startswith('Net'):
                key=name[3:]
                if key=='WSAGetLastError':result=self.last_error if self.alias else self.wsa_error
                elif key=='WSASetLastError':
                    self.wsa_error=a[0]
                    if self.alias:self.last_error=a[0]
                    result=0
                else:
                    self.network_calls.append((key,tuple(a)))
                    result,error=self.network_results.get(key,(0,0))
                    self.last_error=error if self.alias else 0x76543210
                    self.wsa_error=error
                    if key=='WSAStartup' and result==0:u.mem_write(a[1],b'WSA!')
                    if key=='RecvFrom' and result>=0:
                        u.mem_write(a[1],PAYLOAD[:result])
                        if a[4] and a[5]:u.mem_write(a[4],SOCKADDR);u.mem_write(a[5],p32(16))
                u.reg_write(x.UC_X86_REG_EAX,result&0xffffffff);return
            return super().api(u,address,size,user)
        def call_slot(self,slot,argv,result,error):
            name,_,_=SLOTS[slot];self.network_results[name]=(result,error)
            target=struct.unpack('<I',self.u.mem_read(0x1ca0000+slot,4))[0]
            before_count=len(self.network_calls);cw=self.u.reg_read(x.UC_X86_REG_FPCW)
            returned=self.invoke_address(target,argv)
            assert returned==result&0xffffffff,(name,returned,result)
            assert self.network_calls[before_count:]==[(name,tuple(argv))]
            assert self.wsa_error==error
            assert self.last_error==(error if self.alias else 0x76543210)
            assert self.u.reg_read(x.UC_X86_REG_FPCW)==cw
            return returned

    calls={0x97284:(0x101,WSADATA),0x972a0:(),0x972a8:(2,2,0),0x9728c:(77,),
           0x972a4:(77,ADDRESS,16),0x97290:(77,BUFFER,8,0,ADDRESS,16),
           0x9729c:(77,BUFFER,8,0,ADDRESS,LENGTH)}
    for base,shellbase,clock in ((0x68000000,0x20000000,1),(0x03000000,0x10000000,0)):
        for alias in (False,True):
            r=NetRig(alias=alias,dllbase=base,shellbase=shellbase,settings={'InputClock':clock});r.factory()
            assert not r.messages,r.messages;assert r.invoke('KPCFixStatus')&7==7
            assert b'NET trace v1 active' in r.logs
            for slot,(name,nargs,symbol) in SLOTS.items():
                target=struct.unpack('<I',r.u.mem_read(0x1ca0000+slot,4))[0]
                assert target==base+syms['_'+symbol]
                success=0x123456 if name=='Socket' else 8 if name in ('SendTo','RecvFrom') else 0
                r.call_slot(slot,calls[slot],success,1234)
                failure=10093 if name=='WSAStartup' else -1
                r.call_slot(slot,calls[slot],failure,10093)
            assert bytes(r.u.mem_read(WSADATA,4))==b'WSA!'
            assert bytes(r.u.mem_read(BUFFER,8))==PAYLOAD
            assert bytes(r.u.mem_read(ADDRESS,16))==SOCKADDR
            browser_sites = ((0x18164,0x18176,0x1896b,0x189cc,0x189e9,0x18db7) if italian() else
                             (0x18083,0x18095,0x1888b,0x188ec,0x18909,0x18ced))
            for site,register in zip(browser_sites,(0x91,0x92,0x91,0x91,0x92,0x90 if italian() else 0x91)):
                assert bytes(r.u.mem_read(shellbase+site,6))==bytes((0xff,register,0x10,2,0,0))
            assert b'endpoint=192.168.1.3:42972' in r.logs
            assert b'header8=0102030405060708' in r.logs
            rows.append({'dll_base':hex(base),'shell_base':hex(shellbase),'InputClock':clock,'error_slots_alias':alias,'all_seven_calls_forwarded_once_with_registers_stack_errors_and_buffers_preserved':'pass','original_browser_calls':'pass'})

    r=NetRig();r.factory();assert not r.messages
    r.call_slot(0x97290,(77,0xdeadbeef,8,0,0xdeadbeef,16),-1,10014)
    r.call_slot(0x9729c,(77,0xdeadbeef,8,0,0xdeadbeef,0xdeadbeef),-1,10014)
    r.call_slot(0x972a4,(77,0xdeadbeef,16),-1,10014)
    rows.append({'failed_calls_do_not_dereference_rejected_pointers':'pass'})

    r=NetRig();r.factory();assert not r.messages
    for _ in range(80):r.call_slot(0x97290,calls[0x97290],8,0)
    assert r.logs.count(b' sendto ')==48
    for _ in range(128):r.call_slot(0x9729c,calls[0x9729c],-1,10035)
    assert r.logs.count(b' wouldblock_count=')==8
    rows.append({'send_budget_48_and_wouldblock_power_of_two_sampling':'pass'})

    r=NetRig();r.factory();assert not r.messages
    before=bytes(r.logs)
    r.u.mem_write(r.dllbase+syms['_net_log_lock'],p32(1))
    r.call_slot(0x97290,calls[0x97290],8,0)
    assert bytes(r.logs)==before
    rows.append({'busy_logger_skips_observation_without_blocking_or_changing_network_result':'pass'})

    for enabled,missing in ((False,False),(True,True)):
        r=NetRig(enabled=enabled,missing_getter=missing);r.factory();assert not r.messages,r.messages
        assert r.invoke('KPCFixStatus')&7==7
        for slot,(name,_,_) in SLOTS.items():
            assert struct.unpack('<I',r.u.mem_read(0x1ca0000+slot,4))[0]==r.stub_address['Net'+name]
        rows.append({'enabled':enabled,'missing_error_getter':missing,'no_network_hooks_other_fixes_active':'pass'})

    report={'status':'pass','groups':rows,'scope':'Actual linked x86 Winsock observers with mocked network/Win32 services. ABI, errors, forwarding, buffers, bounds and original browser calls verified. No native packet exchange or connection fix claimed.'}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS:',len(rows),'network observer integration groups')

if __name__=='__main__':main()
