"""Run the DLL's own file reader and SHA-256 through DirectInputCreateA."""
import argparse, json, sys
from pathlib import Path
import test_runtime

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('fixtures','symbols','dependency-dir','output'):
        ap.add_argument('--'+name,required=True,type=Path)
    ap.add_argument('--corrupt',action='store_true')
    args=ap.parse_args()
    sys.argv=[sys.argv[0],'--fixtures',str(args.fixtures),'--symbols',str(args.symbols),
              '--dependency-dir',str(args.dependency_dir),'--expose-rig']
    Rig,dll,*_=test_runtime.main()
    rig=Rig(fast_hash=False,settings={'HUD.ScalePercent':150,'Multiplayer.EmptyBindIPFix':1})
    # Hashing over three megabytes of client and shell code can exceed the
    # small-call instruction budget. Use Unicorn's wall-clock timeout here.
    from functools import partial
    rig.invoke_address=partial(rig.invoke_address,limit=0)
    if args.corrupt:
        name=r'C:\Games\Psycho\cshell.dll'
        data=bytearray(rig.file_data[name]);data[-1]^=1;rig.file_data[name]=bytes(data)
    rig.factory()
    if args.corrupt:
        assert rig.messages and rig.invoke('KPCFixStatus')&128
        assert rig.client_bytes()==rig.initial_client and rig.shell_bytes()==rig.initial_shell
    else:
        assert not rig.messages and rig.invoke('KPCFixStatus')&0x87==7,rig.messages
    assert 'ReadFile' in rig.calls
    args.output.write_text(json.dumps({'status':'pass','region':args.fixtures.name,
        'compiled_sha256':True,'same_size_altered_file':args.corrupt})+'\n')
    print('PASS: compiled file gate',args.fixtures.name,'corrupt' if args.corrupt else 'stock')

if __name__=='__main__':main()
