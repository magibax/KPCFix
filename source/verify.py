"""Replay the packaged 1.13 Windows proxy checks with the user's original discs.

Every check is a separate process, so they run in parallel (--jobs, default:
one per CPU). Each check writes <region>_<label>.json and .log to --output; a
summary lists every failure and the script exits non-zero if any check failed.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import time

ROOT=Path(__file__).resolve().parent
REGIONS=('Europe','USA','Germany','Italy-Covermount')
SUITES=('regional','hud','hud_loop','carousel','ui','tooltip_cursor','bindfix','config_io','nettrace',
        'enemy_hud','enemy_label','cinematic','movie_filter','minstep','console')

def jobs_for(region,tmp,args):
    directory=Path(tmp)/'patched to 1.13'/region
    common=['--fixtures',directory,'--symbols',ROOT/'SYMBOLS.json',
            '--dependency-dir',args.dependency_dir.resolve()]
    jobs=[]
    for suite in SUITES:
        options=list(common)
        if suite=='regional':
            options+=['--originals',Path(tmp)/'unpatched'/region]
        if suite in ('hud_loop','carousel'):
            options+=['--expect','fixed','--package',ROOT.parent]
        if suite=='enemy_hud':
            options.append('--expose-hud-rig')
        jobs.append((region,suite,'test_'+suite+'.py',options))
    if not args.skip_audit:
        jobs.append((region,'audit','audit_sites.py',['--fixtures',directory]))
    jobs.append((region,'filegate','test_file_gate.py',common))
    if region=='Germany':
        jobs.append((region,'altered','test_file_gate.py',[*common,'--corrupt']))
    return jobs

def run(job,output):
    region,label,script,options=job
    with (output/f'{region}_{label}.log').open('w') as log:
        result=subprocess.run([sys.executable,ROOT/script,*options,'--output',output/f'{region}_{label}.json'],
                              stdout=log,stderr=subprocess.STDOUT)
    return job,result.returncode

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--dependency-dir',type=Path,default=Path('.'))
    parser.add_argument('--jobs',type=int,default=os.cpu_count() or 1,help='checks run at once (default: one per CPU)')
    parser.add_argument('--skip-audit',action='store_true',help='skip audit_sites.py (it needs objdump)')
    args=parser.parse_args()
    args.output=args.output.resolve();args.output.mkdir(parents=True,exist_ok=True)
    started=time.monotonic()
    with tempfile.TemporaryDirectory() as tmp, tarfile.open(args.archive,'r:gz') as archive:
        jobs=[]
        for region in REGIONS:
            for stage in ('unpatched','patched to 1.13'):
                directory=Path(tmp)/stage/region
                directory.mkdir(parents=True)
                for filename in ('client.exe','cshell.dll'):
                    key=f'KPCFix-region-research/{stage}/{region}/{filename}'
                    with archive.extractfile(key) as f:
                        (directory/filename).write_bytes(f.read())
            jobs+=jobs_for(region,tmp,args)
        failed=[]
        with ThreadPoolExecutor(max_workers=max(1,args.jobs)) as pool:
            for (region,label,_,_),code in pool.map(lambda j:run(j,args.output),jobs):
                print(f"{'ok  ' if code==0 else 'FAIL'} {region} {label}",flush=True)
                if code:failed.append(f'{region}_{label}')
    print(f'{len(jobs)-len(failed)}/{len(jobs)} checks passed in {time.monotonic()-started:.0f} s'
          +(' (audit skipped)' if args.skip_audit else ''))
    if failed:
        print('Failed (see the .log files in the output folder): '+', '.join(failed))
        sys.exit(1)

if __name__=='__main__':
    main()
