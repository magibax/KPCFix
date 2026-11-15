#!/usr/bin/env python3
"""CPU integration checks for InputRate, the settings menu and cursor surfaces.

Runs the linked PE and exact original game routines. Win32, CRT conversion,
bitmap pixels and final UI backend methods are mocked; no native playtest.
"""
from fixture_layout import rva, italian, crt_address
import argparse, hashlib, json, struct, sys
from pathlib import Path
import test_runtime

p32=lambda n:struct.pack('<I',n&0xffffffff)
f32=lambda n:struct.unpack('<I',struct.pack('<f',n))[0]

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--expose-ui-rig',action='store_true',help=argparse.SUPPRESS)
    for name in ('fixtures','symbols','dependency-dir','output'):ap.add_argument('--'+name,type=Path,required=True)
    args=ap.parse_args();saved=sys.argv
    sys.argv=[saved[0],'--fixtures',str(args.fixtures),'--symbols',str(args.symbols),'--dependency-dir',str(args.dependency_dir),'--platform','windows','--expose-rig']
    try:Rig,dll,syms,uc,x,_=test_runtime.main()
    finally:sys.argv=saved
    rows=[]

    class R(Rig):
        def r32(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
        def w32(self,a,n):self.u.mem_write(a,p32(n))
        def symbol(self,name):return self.dllbase+syms['_'+name]
        def run(self,address,argv=(),this=None,cdecl=False,floating=False):
            code=bytearray()
            if this is not None:code+=b'\xb9'+p32(this)
            for a in reversed(argv):code+=b'\x68'+p32(a)
            code+=b'\xb8'+p32(address)+b'\xff\xd0'
            if cdecl and argv:code+=b'\x83\xc4'+bytes([len(argv)*4])
            if floating:code+=b'\xdd\x1d'+p32(0x5f0000)
            code+=b'\xc3';self.u.mem_write(0x6200e000,bytes(code))
            self.u.ctl_remove_cache(0x6200e000,0x6200e100)
            self.u.reg_write(x.UC_X86_REG_FPTAG,0xffff);self.u.reg_write(x.UC_X86_REG_FPSW,0)
            try:v=self.invoke_address(0x6200e000,limit=1000000)
            except Exception:
                print('CPU failure: entry',hex(address),'EIP',hex(self.u.reg_read(x.UC_X86_REG_EIP)),'ESP',hex(self.u.reg_read(x.UC_X86_REG_ESP)),'stack',bytes(self.u.mem_read(self.u.reg_read(x.UC_X86_REG_ESP),32)).hex(),flush=True)
                raise
            assert self.u.reg_read(x.UC_X86_REG_FPTAG)==0xffff,'leaked x87 operand'
            return struct.unpack('<d',self.u.mem_read(0x5f0000,8))[0] if floating else v
        def intercept(self,address,callback,nargs=0,stdcall=False):
            def h(u,at,size,user):
                sp=u.reg_read(x.UC_X86_REG_ESP);a=tuple(self.r32(sp+4+i*4) for i in range(nargs))
                value=callback(a)
                if value is not None:u.reg_write(x.UC_X86_REG_EAX,value)
                u.reg_write(x.UC_X86_REG_EIP,self.r32(sp))
                u.reg_write(x.UC_X86_REG_ESP,sp+4+(nargs*4 if stdcall else 0))
            return self.u.hook_add(uc.UC_HOOK_CODE,h,begin=address,end=address)
        def atof(self,address):
            # Keep genuine x87 ABI: mock string conversion, then run an FLD/RET.
            def h(u,at,size,user):
                value=float(self.cstr(self.r32(u.reg_read(x.UC_X86_REG_ESP)+4)) or '0')
                u.mem_write(0x5f0100,struct.pack('<d',value));u.reg_write(x.UC_X86_REG_EIP,0x6200d000)
            self.u.mem_write(0x6200d000,b'\xdd\x05'+p32(0x5f0100)+b'\xc3')
            return self.u.hook_add(uc.UC_HOOK_CODE,h,begin=address,end=address)

    if args.expose_ui_rig:return R,dll,syms,uc,x
    for db,sb in ((0x68000000,0x20000000),(0x03000000,0x10000000)):
        r=R(dllbase=db,shellbase=sb,settings={'HUD.ScalePercent':200,'NormalizeInputRateConfigs':0});r.factory();assert not r.messages,r.messages
        u=r.u
        # Engine variable create and update take both patched low-level paths.
        context,variable,name=0x580000,0x581000,0x582000
        r.w32(context+8,0x1d41b90);r.w32(context+12,76)
        h_atof=r.atof(0x1d214ff)
        for creating in (False,True):
            lookup=r.intercept(0x1cbb010,lambda a:0 if creating else variable,2)
            allocate=r.intercept(0x1cbaa40,lambda a:variable,3)
            for spelling,value,want in (('InputRate','60','0'),('inputrate','120.5','0'),('INPUTRATE','-25','0'),('InputRate','12345678901234567890','0'),('InputRateExtra','9.5','9.5'),('Sensitivity','2.5','2.5')):
                u.mem_write(variable,b'prior\0'+bytes(0x100));u.mem_write(name,spelling.encode()+b'\0')
                r.w32(variable+0x10,name);r.w32(variable+0x14,variable)
                u.mem_write(name+0x100,value.encode()+b'\0')
                r.run(0x1cbabd0,(context,name,name+0x100,0,0,0,0),cdecl=True)
                assert r.cstr(r.r32(variable+0x14))==want
                assert abs(struct.unpack('<f',u.mem_read(variable+0x18,4))[0]-float(want))<1e-5
                if spelling.lower()=='inputrate':assert r.r32(0x1d4b668)==0
            u.hook_del(lookup);u.hook_del(allocate)
            rows.append({'console':'create' if creating else 'update','dll_base':hex(db),'case_variants_and_unrelated_variables':'pass'})
        u.hook_del(h_atof)

        # Full original profile refresh, reading the locked console variable.
        # DB4E0 is DEFAULT REGISTRATION; never write its read-only default text.
        settings,common,vt,client=0x584000,0x586000,0x587000,0x588000
        r.w32(sb+rva(0x1c8de0),common);r.w32(common,vt);r.w32(sb+rva(0x1c8de8),client)
        r.w32(vt+0x34,0x62008000);r.w32(vt+0x38,0x62008020)
        def lookup_var(a):assert r.cstr(a[0]).lower() in ('inputrate','sensitivity');return variable
        reader=r.intercept(0x62008000,lookup_var,1,True)
        u.mem_write(0x62008020,b'\xd9\x05'+p32(variable+0x18)+b'\xc2\x04\x00')
        def format_float(a):
            dest,capacity,fmt,lo,hi=a
            assert r.cstr(fmt) in ('%f','%.f'),r.cstr(fmt)
            value=struct.unpack('<d',p32(lo)+p32(hi))[0];text=(r.cstr(fmt)%value).encode()
            assert len(text)<capacity;u.mem_write(dest,text+b'\0');return len(text)
        formatter=r.intercept(crt_address(r,0xe5429),format_float,5)
        a=r.atof(0x1d214ff)
        for value in ('60','120.5','0.0'):
            u.mem_write(variable,b'prior\0'+bytes(0x100));u.mem_write(name,b'InputRate\0');r.w32(variable+0x10,name);r.w32(variable+0x14,variable)
            u.mem_write(name+0x100,value.encode()+b'\0')
            r.run(r.symbol('kpc_rate_value'),(context,variable,name+0x100),cdecl=True)
            r.run(sb+rva(0xdbb00),(0,3),this=settings)
            assert r.r32(settings+0x124)==0
            assert float(r.cstr(settings+3*0x54+0x2c))==0
        u.hook_del(reader);u.hook_del(formatter);u.hook_del(a)
        # Check the real setter entry clamps only its InputRate value argument;
        # stop before remaining original setter code and inspect the real frame.
        captures=[]
        def setter_tail(cpu,at,size,user):
            sp=cpu.reg_read(x.UC_X86_REG_ESP)+(0x94 if italian() else 0x90)
            captures.append(tuple(r.r32(sp+i*4) for i in range(1,4)))
            cpu.reg_write(x.UC_X86_REG_ESP,sp+16);cpu.reg_write(x.UC_X86_REG_EIP,r.r32(sp));cpu.reg_write(x.UC_X86_REG_EAX,1)
        h=u.hook_add(uc.UC_HOOK_CODE,setter_tail,begin=sb+rva(0xdb806),end=sb+rva(0xdb806))
        for cat,index,value in ((0,3,60.0),(0,2,1.25),(1,3,45.0),(0,3,-5.0)):
            r.run(sb+rva(0xdb800),(cat,index,f32(value)),this=settings)
            assert captures[-1]==(cat,index,0 if (cat,index)==(0,3) else f32(value))
        u.hook_del(h)
        rows.append({'dll_base':hex(db),'original_profile_read_and_setter_entry_zero_lock':'pass'})

        # Build original-shaped widgets, run linked menu-ready hook, then exact
        # original draw/key/cleanup loops with mocked final widget backends.
        menu,base_vt,widgets=0x590000,0x591000,[0x592000+i*0x300 for i in range(7)]
        draw,evt,focus,cleanup,dtor=0x62008100,0x62008120,0x62008140,0x62008160,0x62008180
        traces={'draw':[],'cleanup':[],'dtor':[]}
        def traced(kind,result=0):
            def cb(a):traces[kind].append(u.reg_read(x.UC_X86_REG_ECX));return result
            return cb
        hooks=[r.intercept(draw,traced('draw'),1,True),r.intercept(evt,lambda a:0,2,True),r.intercept(focus,lambda a:0,2,True),r.intercept(cleanup,traced('cleanup'),0,True),r.intercept(dtor,traced('dtor'),1,True)]
        ready=r.intercept(sb+rva(0x25610),lambda a:0x12345601)
        for i in range(16):r.w32(base_vt+i*4,evt)
        for offset,address in ((0,dtor),(0xc,draw),(0x20,cleanup),(0x38,focus)):r.w32(base_vt+offset,address)
        for reopen in range(2):
            u.mem_write(menu,bytes(0x200));r.w32(menu,sb+rva(0xfd80c));r.w32(menu+4,7);u.mem_write(menu+0x5c,b'\x01')
            for i,w in enumerate(widgets):u.mem_write(w,bytes(0x1b0));r.w32(w,base_vt);r.w32(menu+0x64+i*4,w)
            assert r.run(r.symbol('kpc_mouse_menu_ready'),this=menu)==0x12345601
            private=r.r32(widgets[1]);assert private!=base_vt
            assert r.r32(private)==dtor and r.r32(private+0x20)==cleanup
            assert all(r.r32(w)==base_vt for i,w in enumerate(widgets) if i!=1)
            for slot,nargs in ((0xc,1),(0x24,2),(0x28,2),(0x2c,2),(0x34,1),(0x38,2)):
                r.run(r.r32(private+slot),(0,)*nargs,this=widgets[1])
            traces['draw'].clear();r.run(sb+rva(0x25110),(0x599000,),this=menu)
            assert traces['draw']==widgets[:1]+widgets[2:]
            r.w32(menu+0x58,0x591f00);r.w32(menu+0x88,0)
            u.mem_write(widgets[1]+0x24,b'\x01')
            r.run(sb+rva(0x25150),this=menu);assert r.r32(menu+0x88)==0
            for old,key,want in ((0,0x28,2),(2,0x26,0),(0,0x26,6),(6,0x28,0),(3,0x28,4)):
                r.w32(menu+0x88,old);r.run(r.r32(sb+rva(0xfd824)),(key,0),this=menu);assert r.r32(menu+0x88)==want
            traces['cleanup'].clear();traces['dtor'].clear();r.run(sb+rva(0x250b0),this=menu)
            assert traces['cleanup']==widgets and traces['dtor']==widgets
            assert all(r.r32(menu+0x64+i*4)==0 for i in range(7))
        for h in hooks+[ready]:u.hook_del(h)
        for site,y in ((rva(0x24e44),220),(rva(0x24ef3),240),(rva(0x24f76),280),(rva(0x2501a),320)):assert r.r32(sb+site+3)==y
        rows.append({'dll_base':hex(db),'mouse_menu_hidden_draw_input_keyboard_skip_cleanup_reopen_reflow':'pass'})

        # Execute original cursor loading, GetDims and background-allocation
        # routines with two bitmaps of different dimensions and two backbuffers.
        cursor,api=0x5a0000,0x5a1000;r.w32(sb+rva(0x1c8dfc),api);r.w32(api+0x21c,2)
        dims={};deleted=[];scaled=[];created=[];counter=[0];frame=[0];failure=[''];loads=[];blits=[]
        def load(a):
            frame[0]+=1;handle=0x100+frame[0];dims[handle]=(12,16) if frame[0]%2 else (16,12);loads.append(handle);return handle
        def getdims(a):
            handle,w,h=a;r.w32(w,dims[handle][0]);r.w32(h,dims[handle][1]);return 0xdeadbeef
        def create(a):
            counter[0]+=1;handle=0x200+counter[0];dims[handle]=a;created.append(handle);return handle
        def delete(a):assert a[0] not in deleted;deleted.append(a[0]);return 0
        def scale(a):
            dest,src,rect,src_rect,color=a;assert src_rect==0 and dest==0x599000 and src in loads and color==0
            rectangle=tuple(struct.unpack('<4i',u.mem_read(rect,16)));scaled.append((dest,src,rectangle))
            blits.append(('scaled',dest,src,rectangle));return 1 if failure[0]=='scale' else 0
        def sprintf(a):u.mem_write(a[0],b'cursor-frame.pcx\0');return 16
        def raw(a):
            dest,src,rect,xx,yy=a;blits.append(('raw',dest,src,tuple(struct.unpack('<4i',u.mem_read(rect,16))) if rect else None,xx,yy));return 0
        def keyed(a):blits.append(('keyed',*a));return 0
        h=[r.intercept(crt_address(r,0xe57f4),sprintf,4)]
        functions=((0x14c,1,load),(0x174,3,getdims),(0x154,2,create),(0x158,1,delete),(0x190,5,scale),(0x124,6,lambda a:0),(0x12c,4,lambda a:0),(0x184,5,raw),(0x188,6,keyed))
        for i,(slot,n,fn) in enumerate(functions):
            address=0x62008200+i*32;r.w32(api+slot,address);h.append(r.intercept(address,fn,n))
        for percent,mode in ((200,''),(150,''),(100,''),(200,'scale'),(50,'')):
            r.w32(r.symbol('g_cursor_percent'),percent);u.mem_write(cursor,bytes(0x100));frame[0]=0;counter[0]=0
            dims.clear();deleted.clear();scaled.clear();created.clear();loads.clear();failure[0]=mode
            assert r.run(sb+rva(0x23fc0),(api,0,2),this=cursor)&255==1
            target=((16*percent+50)//100,)*2
            assert (r.r32(cursor+0x48),r.r32(cursor+0x4c))==target
            backbuffers=[r.r32(cursor+0x50+i*4) for i in range(2)]
            assert all(dims[a]==target for a in backbuffers) and len(created)==2
            for i in range(2):
                surface=r.r32(cursor+0xc+i*4);assert surface==loads[i]
                assert r.r32(cursor+0x14+i*4)==(dims[surface][0]*percent+50)//100
                assert r.r32(cursor+0x1c+i*4)==(dims[surface][1]*percent+50)//100
            assert (r.r32(cursor+4),r.r32(cursor+8))==(0,0),'cursor coordinates changed'
            r.w32(cursor+4,f32(100.75));r.w32(cursor+8,f32(200.5));r.w32(api+0x220,0);blits.clear()
            r.run(sb+rva(0x242c0),(0x599000,),this=cursor)
            assert blits[-1]==('raw',backbuffers[0],0x599000,(100,200,100+target[0],200+target[1]),0,0)
            r.run(sb+rva(0x24440),(0x599000,),this=cursor)
            if percent==100 or mode:
                assert blits[-1]==('keyed',0x599000,r.r32(cursor+0xc),0,100,200,0)
            else:assert blits[-1]==('scaled',0x599000,r.r32(cursor+0xc),(100,200,100+r.r32(cursor+0x14),200+r.r32(cursor+0x1c)))
            r.w32(cursor+4,f32(150));r.w32(cursor+8,f32(250))
            r.run(sb+rva(0x24250),(0x599000,),this=cursor)
            assert blits[-1]==('raw',0x599000,backbuffers[0],None,100,200)
            r.run(sb+rva(0x24110),this=cursor)
            assert all(handle in deleted for handle in dims)
            rows.append({'dll_base':hex(db),'cursor_percent':percent,'failure':mode or 'none','original_sources_and_screen_scale_with_matching_backgrounds':'pass'})

        for hh in h:u.hook_del(hh)

    # A live InputRate variable may already exist before DirectInput creation.
    # The startup migration must reset its canonical value and integer binding.
    r=R(settings={'NormalizeInputRateConfigs':0});u=r.u;ctx=0x1d4b3d8;var=0x581000;name=0x582000
    u.mem_write(var,b'60\0'+bytes(0x100));u.mem_write(name,b'InputRate\0')
    for at,value in ((ctx+8,0x1d41b90),(ctx+12,76),(var+0x10,name),(var+0x14,var),(var+0x18,f32(60)),(0x1d4b668,60)):r.w32(at,value)
    h=r.intercept(0x1cbb010,lambda a:var,2);a=r.atof(0x1d214ff)
    r.factory();assert not r.messages,r.messages
    assert r.cstr(r.r32(var+0x14))=='0' and r.r32(var+0x18)==0 and r.r32(0x1d4b668)==0
    u.hook_del(h);u.hook_del(a);rows.append({'startup_preexisting_console_variable_reset':'pass'})

    # New options retain a clean off path; expected-byte failure is atomic for
    # the relevant module and must not turn an unknown instruction into a hook.
    for settings in ({'LockInputRate':0,'Cursor.ScalePercent':100},{'HideInputRate':0,'Cursor.ScalePercent':100}):
        r=R(settings=settings);r.factory();assert not r.messages
        for at,n in ((rva(0x2508b),5),(rva(0xfd824),4),(rva(0x24e44),7),(rva(0x24ef3),7),(rva(0x24f76),7),(rva(0x2501a),7),(rva(0x24065),6),(rva(0x2456c),6)):
            assert bytes(r.u.mem_read(r.shellbase+at,n))==r.initial_shell[at:at+n]
        if settings.get('LockInputRate')==0:
            assert bytes(r.u.mem_read(r.shellbase+rva(0xdb800),6))==r.initial_shell[rva(0xdb800):rva(0xdb806)]
            for at in (0x1aa0e,0x1ac90):assert bytes(r.u.mem_read(0x1ca0000+at,5))==r.initial_client[at:at+5]
        rows.append({'options':settings,'reviewed_original_instructions_retained':'pass'})
    for module,at in (('client',0x1aa0e),('client',0x1ac90),('shell',rva(0x24065)),('shell',rva(0xdb800))):
        r=R(settings={'HUD.ScalePercent':200,'NormalizeInputRateConfigs':0});base=0x1ca0000 if module=='client' else r.shellbase
        r.u.mem_write(base+at,b'\x90');before=r.client_bytes() if module=='client' else r.shell_bytes()
        r.factory();assert r.messages and r.invoke('KPCFixStatus')&128
        assert (r.client_bytes() if module=='client' else r.shell_bytes())==before
        rows.append({'mismatch_module':module,'rva':hex(at),'module_transaction_preserved':'pass'})
    for percent in (49,401,0xffffffff):
        r=R(settings={'Cursor.ScalePercent':percent});r.factory()
        assert r.messages and r.client_bytes()==r.initial_client and r.shell_bytes()==r.initial_shell
        rows.append({'invalid_cursor_percent':percent,'no_code_changes':'pass'})

    # The compiled config normalizer, including literal canonical spelling.
    r=R(settings={'LockInputRate':0});r.factory();assert not r.messages
    cases=[b'"InputRate" "60"\r\n"Other" "value"\r\n',b'InputRate 50\n',b'\t"inputrate"\t"0.0" # comment\n',b'// InputRate 60\n"Other" "InputRate"\n',b'"InputRate" "60"\n"INPUTRATE" "30"',b'\xef\xbb\xbf"InputRate" "60"\r\n',b'"InputRateExtra" "5"\n',b'',b'"InputRate" "0"\n']
    for source in cases:
        r.u.mem_write(0x5b0000,source or b'\0')
        n=r.run(r.symbol('kpc_normalize_text'),(0x5b0000,len(source),0x5d1000,131136),cdecl=True)
        assert n!=0xffffffff
        out=bytes(r.u.mem_read(0x5d1000,n));assert b'"InputRate" "0"' in out
        if b'Other' in source:assert b'"Other" "value"' in out or b'"Other" "InputRate"' in out
        r.u.mem_write(0x5b0000,out)
        nn=r.run(r.symbol('kpc_normalize_text'),(0x5b0000,n,0x5d1000,131136),cdecl=True)
        assert nn==n and bytes(r.u.mem_read(0x5d1000,n))==out
        rows.append({'config_text_sha256':hashlib.sha256(source).hexdigest(),'canonical_zero_idempotent_preserves_other_values':'pass'})
    for bad in (b'\xff\xfeI\0n\0',b'"InputRate" "unterminated',b'x'*131073):
        r.u.mem_write(0x5b0000,bad)
        assert r.run(r.symbol('kpc_normalize_text'),(0x5b0000,len(bad),0x5d1000,131136),cdecl=True)==0xffffffff
    rows.append({'config_unsupported_encoding_malformed_quote_size_limit':'pass'})

    report={'status':'pass','dll_sha256':hashlib.sha256((Path(__file__).parents[1]/'dinput.dll').read_bytes()).hexdigest(),'groups':len(rows),'results':rows,'scope':__doc__}
    args.output.write_text(json.dumps(report,indent=2)+'\n');print('PASS:',len(rows),'UI/input/cursor integration groups',flush=True)

if __name__=='__main__':main()
