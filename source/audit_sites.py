"""Check instruction boundaries and incoming direct branches in the verified 1.13 shell."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path
from fixture_layout import select, rva
from test_hud import SITES

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    select(args.fixtures)
    path=args.fixtures/'cshell.dll'
    # Data writes (sensitivity constants, vtable entry, Y immediates) preserve
    # their enclosing instructions. Check the executable detours separately.
    detours=[(rva(at),len(bytes.fromhex(before))) for at,before,_ in SITES]
    detours.extend((rva(at),size) for at,size in (
        (0xbcac2,7),(0xdb800,6),(0x2508b,5),(0x24065,6),(0x2456c,6),(0x240ad,6)))
    from fixture_layout import REGION
    if REGION in ('Europe','USA'):
        # Campaign loading art call redirects (Europe/USA shell profile only).
        detours.extend(((0xcbba6,5),(0xcdb92,5)))
        # Spawn hold: the 0x17 handler reply calls (loading.c).
        detours.extend(((0xbeadc,5),(0xbeae3,5)))
        # Chat message time: MOV [ESP+0x10], imm32 (immediate only).
        detours.extend(((0xcb38d,8),))
        # Smooth scripted cameras: sample store call.
        detours.extend(((0xab104,5),))
    asm=subprocess.check_output(['objdump','-dw','-Mintel',str(path)],text=True)
    boundaries=set()
    transfers=[]
    for line in asm.splitlines():
        fields=line.split('\t')
        if len(fields)<3:continue
        address=re.fullmatch(r'\s*([a-f0-9]+):',fields[0])
        if not address:continue
        start=int(address[1],16)-0x20000000
        boundaries.add(start)
        branch=re.match(r'\s*(?:j[a-z]+|loop[a-z]*|call)\s+(?:0x)?([a-f0-9]+)(?:\s|$)',fields[-1])
        if branch:transfers.append((start,int(branch[1],16)-0x20000000))
    assert transfers
    for at,size in detours:
        assert at in boundaries and at+size in boundaries,(hex(at),size)
        assert not any(at<dest<at+size for _,dest in transfers),(hex(at),size)
    # client.exe (shared by every edition): the 14-byte server-timeout window.
    client=args.fixtures/'client.exe'
    casm=subprocess.check_output(['objdump','-dw','-Mintel',str(client)],text=True)
    cbound=set();ctransfers=[]
    for line in casm.splitlines():
        fields=line.split('\t')
        if len(fields)<3:continue
        address=re.fullmatch(r'\s*([a-f0-9]+):',fields[0])
        if not address:continue
        start=int(address[1],16)-0x1ca0000
        cbound.add(start)
        branch=re.match(r'\s*(?:j[a-z]+|loop[a-z]*|call)\s+(?:0x)?([a-f0-9]+)(?:\s|$)',fields[-1])
        if branch:ctransfers.append((start,int(branch[1],16)-0x1ca0000))
    for at,size in ((0x4b4fc,14),):
        assert at in cbound and at+size in cbound,(hex(at),size)
        assert not any(at<dest<at+size for _,dest in ctransfers),(hex(at),size)
        detours.append((at,size))
    args.output.write_text(json.dumps({'status':'pass','region':args.fixtures.name,
        'cshell_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'detours':len(detours),'direct_transfers_examined':len(transfers),
        'scope':'Instruction boundaries and direct branch targets. Indirect runtime transfers are outside this static check.'},indent=2)+'\n')

if __name__=='__main__':main()
