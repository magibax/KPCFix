#!/usr/bin/env python3
"""Original cursor/tooltip routines plus real engine screen-scale/clipper path.

Mocks Win32/fonts/surface storage and the final driver. Checks the actual source
and destination rectangles delivered by client+0x11660 through its clipper,
then rasterizes a small synthetic image to check all four source corners.
This does not simulate the native DDrawCompat driver or Windows font rasterizer.
"""
from fixture_layout import rva, italian, crt_address
import argparse,hashlib,json,struct,sys
from pathlib import Path
import test_ui
p32=lambda n:struct.pack('<I',n&0xffffffff)
f32=lambda n:struct.unpack('<I',struct.pack('<f',n))[0]

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('fixtures','symbols','dependency-dir','output'):ap.add_argument('--'+name,type=Path,required=True)
    args=ap.parse_args();saved=sys.argv
    sys.argv=[saved[0],'--fixtures',str(args.fixtures),'--symbols',str(args.symbols),'--dependency-dir',str(args.dependency_dir),'--output','unused','--expose-ui-rig']
    try:R,dll,syms,uc,x=test_ui.main()
    finally:sys.argv=saved
    rows=[]
    cases=((200,200,0x68000000,0x20000000),(200,200,0x03000000,0x10000000),(100,100,0x68000000,0x20000000),(150,150,0x68000000,0x20000000),(400,400,0x68000000,0x20000000),(200,100,0x68000000,0x20000000),(100,200,0x68000000,0x20000000))
    for pc,pt,db,sb in cases:
        r=R(dllbase=db,shellbase=sb,settings={'HUD.ScalePercent':200,'Cursor.ScalePercent':pc,'Tooltip.ScalePercent':pt,'NormalizeInputRateConfigs':0});r.factory();assert not r.messages,r.messages
        u=r.u;cursor,api,screen=0x5a0000,0x5a1000,0x1d4ab60
        r.w32(sb+rva(0x1c8dfc),api);r.w32(api+0x21c,2);r.w32(api+0x220,0)
        root=0x5d0000;r.w32(0x1d4aca8,root);r.w32(root+0xb0,0x6200a000);r.w32(0x1d4c6e0,0x6200a020)
        r.w32(screen+0x10,2560);r.w32(screen+0x14,1440);r.w32(api+0x210,2560);r.w32(api+0x214,1440)
        surfaces={};textures={};fonts={};strings={};deleted=[];loads=[];created=[];draws=[];fills=[];texts=[];font_args=[];serial=[0];string_id=[0];frame=[0]
        def surface(w,h,pixels=None):
            serial[0]+=1;address=0x5c0000+serial[0]*0x100;texture=0x9000+serial[0]
            surfaces[address]=(w,h);r.w32(address+0x10,w);r.w32(address+0x14,h);r.w32(address+0xc,texture)
            if pixels is not None:textures[texture]=pixels
            return address
        def load(a):
            frame[0]+=1;w,h=(10,14) if frame[0]==1 else (14,10)
            image=[[1+xx+yy*w for xx in range(w)] for yy in range(h)]
            handle=surface(w,h,image);loads.append(handle);return handle
        def create(a):
            handle=surface(*a);created.append(handle);return handle
        def dims(a):
            handle,w,h=a;r.w32(w,surfaces[handle][0]);r.w32(h,surfaces[handle][1]);return 0xbadc0de # void
        def delete(a):assert a[0] not in deleted;deleted.append(a[0]);return 0
        def font(a):
            name,w,h,*style=a;assert r.cstr(name)=='Arial' and style==[0,0,0]
            font_args.append((w,h));fonts[0xa001]=(w,h);return 0xa001
        def delete_font(a):fonts.pop(a[0]);return 0
        def make_string(a):string_id[0]+=1;handle=0xb000+string_id[0];strings[handle]=r.cstr(a[0]);return handle
        def delete_string(a):strings.pop(a[0]);return 0
        def string_dims(a):
            font_handle,string_handle,w,h=a;fw,fh=fonts[font_handle];r.w32(w,len(strings[string_handle])*fw);r.w32(h,fh);return 0
        def raw(a):
            dest,src,rect,xx,yy=a;draws.append(('raw',dest,src,tuple(struct.unpack('<4i',u.mem_read(rect,16))) if rect else None,xx,yy));return 0
        def keyed(a):draws.append(('keyed',*a));return 0
        def fill(a):dest,rect,color=a;fills.append(tuple(struct.unpack('<4i',u.mem_read(rect,16))));return 0
        def text(a):
            dest,font_handle,string_handle,rect,foreground,background=a
            texts.append((tuple(struct.unpack('<4i',u.mem_read(rect,16))),strings[string_handle],fonts[font_handle]));return 0
        def format_name(a):u.mem_write(a[0],b'cursor-frame.pcx\0');return 16
        def no_offscreen(a):raise AssertionError('Offscreen bitmap resampling was called')
        def driver(a):
            call=a[0];texture=r.r32(call);src_rect=tuple(struct.unpack('<4i',u.mem_read(r.r32(call+12),16)));dst_rect=tuple(struct.unpack('<4i',u.mem_read(r.r32(call+16),16)))
            image=textures[texture];w,h=len(image[0]),len(image)
            assert src_rect==(0,0,w,h),src_rect
            left,top,right,bottom=dst_rect;ww,hh=right-left,bottom-top
            expected=((w*pc+50)//100,(h*pc+50)//100);assert (ww,hh)==expected
            # Reference final driver: every source edge is represented at the
            # supported >=100% scales tested here; no cropped replacement image.
            output=[[image[min(h-1,yy*h//hh)][min(w-1,xx*w//ww)] for xx in range(ww)] for yy in range(hh)]
            assert (output[0][0],output[0][-1],output[-1][0],output[-1][-1])==(image[0][0],image[0][-1],image[-1][0],image[-1][-1])
            draws.append(('driver',texture,src_rect,dst_rect));return 0
        hooks=[r.intercept(crt_address(r,0xe57f4),format_name,4),r.intercept(0x6200a000,lambda a:0),r.intercept(0x6200a020,driver,1)]
        functions=((0x14c,1,load),(0x154,2,create),(0x174,3,dims),(0x158,1,delete),(0x124,6,font),(0x128,1,delete_font),(0x12c,4,lambda a:0),(0x104,1,make_string),(0x108,1,delete_string),(0x16c,4,string_dims),(0x170,6,text),(0x134,4,lambda a:0xffffff),(0x1ac,3,fill),(0x184,5,raw),(0x188,6,keyed),(0x18c,4,no_offscreen))
        for i,(slot,n,fn) in enumerate(functions):
            address=0x6200b000+i*32;r.w32(api+slot,address);hooks.append(r.intercept(address,fn,n))
        r.w32(api+0x190,0x1cb1660) # genuine original engine function, not a mock
        r.run(sb+rva(0x23fc0),(api,0,2),this=cursor)
        assert font_args==[((5*pt+50)//100,(12*pt+50)//100)]
        assert len(loads)==2 and len(created)==2 and all(r.r32(cursor+0xc+i*4)==loads[i] for i in range(2))
        assert bytes(u.mem_read(sb+rva(0x24024),6))==(bytes.fromhex('ff914c010000') if italian() else bytes.fromhex('ff904c010000')),'bitmap loader was modified'
        maximum=(14*pc+50)//100
        assert (r.r32(cursor+0x48),r.r32(cursor+0x4c))==(maximum,maximum)
        r.w32(cursor+4,f32(100.75));r.w32(cursor+8,f32(200.5))
        for selected in (0,1):
            r.w32(cursor+0x24,selected);draws.clear();r.run(sb+rva(0x242c0),(screen,),this=cursor);r.run(sb+rva(0x24440),(screen,),this=cursor)
            if pc!=100:
                assert draws[-1][0]=='driver'
                assert draws[-1][3][:2]==(100,200)
            else:assert draws[-1]==('keyed',screen,loads[selected],0,100,200,0)
            rows.append({'cursor_percent':pc,'tooltip_percent':pt,'dll_base':hex(db),'frame':selected,'native_engine_clip_and_full_source_corner_coverage':'pass'})
        # Full tooltip lifecycle: set text, allocate its own matching backup,
        # paint border/text, restore it, then repeat with changed/longer text.
        lift=(12*pt+50)//100;fw,fh=font_args[0]
        for message in ('Start a game','Configure the game for your system.'):
            u.mem_write(0x5b0000,message.encode()+b'\0');r.run(sb+rva(0x24410),(0x5b0000,),this=cursor)
            draws.clear();fills.clear();texts.clear();r.run(sb+rva(0x242c0),(screen,),this=cursor)
            width,height=len(message)*fw+2,fh+2
            assert (r.r32(cursor+0x84),r.r32(cursor+0x88))==(width,height)
            background=r.r32(cursor+0x6c);assert surfaces[background]==(width,height)
            saved=('raw',background,screen,(100,200-lift,100+width,200-lift+height),0,0)
            assert saved in draws,draws
            r.run(sb+rva(0x24440),(screen,),this=cursor)
            assert fills[-1]==(100,200-lift,100+width,200-lift+height)
            assert texts[-1]==((101,201-lift,99+width,199-lift+height),message,(fw,fh))
            assert (r.r32(cursor+4),r.r32(cursor+8))==(f32(100.75),f32(200.5)),'menu coordinates changed'
            r.run(sb+rva(0x24250),(screen,),this=cursor)
            assert ('raw',screen,background,None,100,200-lift) in draws
            rows.append({'cursor_percent':pc,'tooltip_percent':pt,'dll_base':hex(db),'message_characters':len(message),'native_metrics_backplate_text_insets_save_restore':'pass'})
        r.run(sb+rva(0x24110),this=cursor)
        # Original rva(0x24110) frees frames/base buffers/font/HSTRING, while its
        # 0x23F50 helper leaves the final tooltip backup at cursor+0x6C. Verify
        # that unchanged lifetime; this update adds no replacement cursor images.
        remaining=set(surfaces)-set(deleted)
        assert not fonts and not strings and remaining=={r.r32(cursor+0x6c)}
        assert all(handle in deleted for handle in loads+created[:2])
        for h in hooks:u.hook_del(h)
        rows.append({'cursor_percent':pc,'tooltip_percent':pt,'original_cursor_cleanup_and_tooltip_resource_lifetime_retained':'pass'})
    for percent in (49,401,0xffffffff):
        r=R(settings={'Tooltip.ScalePercent':percent});r.factory()
        assert r.messages and r.client_bytes()==r.initial_client and r.shell_bytes()==r.initial_shell
        rows.append({'invalid_tooltip_percent':percent,'no_changes':'pass'})
    report={'status':'pass','dll_sha256':hashlib.sha256((Path(__file__).parents[1]/'dinput.dll').read_bytes()).hexdigest(),'groups':len(rows),'results':rows,'scope':__doc__}
    args.output.write_text(json.dumps(report,indent=2)+'\n');print('PASS:',len(rows),'cursor-pixel/tooltip original-code groups',flush=True)

if __name__=='__main__':main()
