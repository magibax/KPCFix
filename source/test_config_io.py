#!/usr/bin/env python3
"""Exercise compiled config migration with mocked Win32 files and failures."""
from fixture_layout import rva, italian, crt_address
import argparse,hashlib,json,struct,sys
from pathlib import Path
import test_runtime
p32=lambda n:struct.pack('<I',n&0xffffffff)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('fixtures','symbols','dependency-dir','output'):ap.add_argument('--'+name,type=Path,required=True)
    args=ap.parse_args();saved=sys.argv
    sys.argv=[saved[0],'--fixtures',str(args.fixtures),'--symbols',str(args.symbols),'--dependency-dir',str(args.dependency_dir),'--platform','windows','--expose-rig']
    try:Rig,dll,syms,uc,x,_=test_runtime.main()
    finally:sys.argv=saved
    root='C:\\Games\\Psycho\\'
    originals={root+'autoexec.cfg':b'"InputRate" "60"\r\n"Unrelated" "1"\r\n',root+'Profiles\\Player.cfg':b'"InputRate" "120"\n"Name" "Player"\n',root+'Profiles\\Other.cfg':b'"Name" "Other"',root+'Profiles\\Already.cfg':b'"InputRate" "0"\n',root+'Profiles\\Linked.cfg':b'"InputRate" "25"\n',root+'Profiles\\DoNotTouch.txt':b'InputRate 60'}

    class F(Rig):
        def __init__(self,failure='',**kw):
            self.storage=dict(originals);self.handles={};self.searches={};self.serial=800;self.io=[];self.failure=failure
            if failure=='stale_temp':self.storage[root+'autoexec.cfg.kpc-inputrate.tmp']=b'previous temporary'
            super().__init__(**kw)
        def api(self,u,address,size,user):
            name=self.stub_names.get(address);sp=u.reg_read(x.UC_X86_REG_ESP)
            n=self.stub_args.get(name,0);a=struct.unpack('<'+'I'*n,u.mem_read(sp+4,n*4)) if n else ()
            if name in ('FindFirstFileW','FindNextFileW','FindClose','CopyFileW','MoveFileExW','DeleteFileW','FlushFileBuffers') or name in ('CreateFileW',) and self.wstr(a[0]).lower().endswith(('.cfg','.tmp')) or name in ('ReadFile','WriteFile','GetFileSizeEx','CloseHandle') and a[0] in self.handles:
                result,error=1,0;self.calls.append(name);self.io.append(name)
                if name=='CreateFileW':
                    path=self.wstr(a[0]);read=a[1]==0x80000000
                    assert path.startswith(root) and a[3]==0 and a[6]==0
                    if read and path not in self.storage:result,error=0xffffffff,2
                    elif not read and path in self.storage:result,error=0xffffffff,80
                    else:
                        assert (a[4],a[2])==((3,1) if read else (1,0))
                        if not read:self.storage[path]=b''
                        self.serial+=1;result=self.serial;self.handles[result]={'path':path,'at':0,'read':read}
                elif name=='GetFileSizeEx':u.mem_write(a[1],struct.pack('<Q',len(self.storage[self.handles[a[0]]['path']])))
                elif name=='ReadFile':
                    h=self.handles[a[0]];data=self.storage[h['path']][h['at']:h['at']+a[2]];h['at']+=len(data)
                    if data:u.mem_write(a[1],data)
                    u.mem_write(a[3],p32(len(data)))
                elif name=='WriteFile':
                    path=self.handles[a[0]]['path'];assert path.endswith('.kpc-inputrate.tmp')
                    data=bytes(u.mem_read(a[1],a[2]));count=len(data)-(1 if self.failure=='short_write' else 0)
                    self.storage[path]=data[:count];u.mem_write(a[3],p32(count))
                elif name=='CloseHandle':self.handles.pop(a[0])
                elif name=='CopyFileW':
                    source,dest=self.wstr(a[0]),self.wstr(a[1]);assert a[2]==1 and dest==source+'.kpc-inputrate.bak'
                    if self.failure=='copy':result,error=0,5
                    elif dest in self.storage:result,error=0,80
                    else:self.storage[dest]=self.storage[source]
                elif name=='FlushFileBuffers':
                    if self.failure=='flush':result,error=0,5
                elif name=='MoveFileExW':
                    source,dest=self.wstr(a[0]),self.wstr(a[1]);assert a[2]==9 and source==dest+'.kpc-inputrate.tmp'
                    if self.failure=='rename':result,error=0,5
                    else:self.storage[dest]=self.storage.pop(source)
                elif name=='DeleteFileW':self.storage.pop(self.wstr(a[0]),None)
                elif name=='FindFirstFileW':
                    assert self.wstr(a[0])==root+'Profiles\\*.cfg'
                    self.serial+=1;result=self.serial
                    items=[p for p in self.storage if p.startswith(root+'Profiles\\') and p.lower().endswith('.cfg')]
                    self.searches[result]=[items,0];self.find_data(u,a[1],items[0])
                elif name=='FindNextFileW':
                    items,i=self.searches[a[0]];i+=1;self.searches[a[0]][1]=i
                    if i==len(items):result,error=0,18
                    else:self.find_data(u,a[1],items[i])
                elif name=='FindClose':self.searches.pop(a[0])
                self.last_error=error;u.reg_write(x.UC_X86_REG_EAX,result);return
            return super().api(u,address,size,user)
        def find_data(self,u,dest,path):
            u.mem_write(dest,bytes(592));u.mem_write(dest,p32(0x400 if path.endswith('Linked.cfg') else 0x80))
            u.mem_write(dest+44,path.rsplit('\\',1)[1].encode('utf-16le')+b'\0\0')
        def migrate(self):
            self.put_w(0x550000,root+'client.exe',260)
            code=b'\x68'+p32(0x550000)+b'\xb8'+p32(self.dllbase+syms['_kpc_normalize_configs'])+b'\xff\xd0\x83\xc4\x04\xc3'
            self.u.mem_write(0x6200f000,code);self.invoke_address(0x6200f000)

    rows=[]
    r=F(settings={'HUD.ScalePercent':200});r.factory();assert not r.messages,r.messages
    for path,source in originals.items():
        if path.endswith(('Linked.cfg','.txt','Already.cfg')):assert r.storage[path]==source and path+'.kpc-inputrate.bak' not in r.storage
        else:
            assert b'"InputRate" "0"' in r.storage[path] and b'"60"' not in r.storage[path] and b'"120"' not in r.storage[path]
            assert r.storage[path+'.kpc-inputrate.bak']==source
    assert b'"Unrelated" "1"' in r.storage[root+'autoexec.cfg']
    assert b'"Name" "Player"' in r.storage[root+'Profiles\\Player.cfg']
    assert not r.handles and not r.searches and not any(p.endswith('.tmp') for p in r.storage)
    snapshot=dict(r.storage);writes=r.io.count('WriteFile');r.migrate();assert snapshot==r.storage and r.io.count('WriteFile')==writes
    # Existing backups are retained on a subsequent repair, including a changed
    # external config between game sessions.
    r.storage[root+'autoexec.cfg']=b'"InputRate" "35"\n';r.migrate()
    assert r.storage[root+'autoexec.cfg']==b'"InputRate" "0"\n'
    assert r.storage[root+'autoexec.cfg.kpc-inputrate.bak']==originals[root+'autoexec.cfg']
    rows.append({'startup_migration_multiple_profiles_backup_idempotence_and_later_repair':'pass'})
    for failure in ('copy','short_write','flush','rename','stale_temp'):
        r=F(failure=failure,settings={'NormalizeInputRateConfigs':0});r.factory();assert not r.messages
        r.migrate()
        if failure=='stale_temp':
            assert r.storage[root+'autoexec.cfg']==originals[root+'autoexec.cfg']
            assert r.storage[root+'autoexec.cfg.kpc-inputrate.tmp']==b'previous temporary'
        else:
            assert all(r.storage[p]==v for p,v in originals.items())
            assert not any(p.endswith('.tmp') for p in r.storage)
        assert not r.handles and not r.searches and b'skipped/failed' in r.logs
        rows.append({'failure':failure,'originals_and_prior_temp_preserved_handles_closed':'pass'})
    for settings in ({'LockInputRate':0},{'Enabled':0},{'NormalizeInputRateConfigs':0}):
        r=F(settings=settings);r.factory();assert not r.messages
        assert r.storage==originals and not r.io
        rows.append({'settings':settings,'no_disk_changes':'pass'})
    report={'status':'pass','dll_sha256':hashlib.sha256((Path(__file__).parents[1]/'dinput.dll').read_bytes()).hexdigest(),'groups':len(rows),'results':rows,'scope':'Compiled PE32 migration code, synthetic Win32 file system. Not native Windows file-lock/antivirus testing.'}
    args.output.write_text(json.dumps(report,indent=2)+'\n');print('PASS:',len(rows),'config I/O integration groups',flush=True)

if __name__=='__main__':main()
