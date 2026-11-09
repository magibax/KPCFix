"""Replay the packaged 1.13 Windows proxy checks with the user's original discs."""
import argparse
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile

ROOT=Path(__file__).resolve().parent
REGIONS=('Europe','USA','Germany','Italy-Covermount')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--dependency-dir',type=Path,default=Path('.'))
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp, tarfile.open(args.archive,'r:gz') as archive:
        for region in REGIONS:
            for stage in ('unpatched','patched to 1.13'):
                directory=Path(tmp)/stage/region
                directory.mkdir(parents=True)
                for filename in ('client.exe','cshell.dll'):
                    key=f'KPCFix-region-research/{stage}/{region}/{filename}'
                    with archive.extractfile(key) as f:
                        (directory/filename).write_bytes(f.read())
            common=['--fixtures',directory,'--symbols',ROOT/'SYMBOLS.json',
                    '--dependency-dir',args.dependency_dir.resolve()]
            def run(script,label,options):
                subprocess.run([sys.executable,ROOT/script,*options,'--output',
                                args.output/f'{region}_{label}.json'],check=True)
            for suite in ('regional','hud','hud_loop','carousel','ui','tooltip_cursor','bindfix','config_io','nettrace'):
                options=list(common)
                if suite=='regional':
                    options+=['--originals',Path(tmp)/'unpatched'/region]
                if suite in ('hud_loop','carousel'):
                    options+=['--expect','fixed','--package',ROOT.parent]
                run('test_'+suite+'.py',suite,options)
            for suite in ('enemy_hud','enemy_label','cinematic','movie_filter'):
                options = list(common)
                if suite == 'enemy_hud':
                    options.append('--expose-hud-rig')
                with (args.output/f'{region}_{suite}.log').open('w') as log:
                    subprocess.run([sys.executable, ROOT/f'test_{suite}.py', *options,
                                    '--output', args.output/f'{region}_{suite}.json'],
                                   stdout=log, stderr=subprocess.STDOUT, check=True)
            run('audit_sites.py','audit',['--fixtures',directory])
            run('test_file_gate.py','filegate',common)
            if region=='Germany':
                run('test_file_gate.py','altered',[*common,'--corrupt'])

if __name__=='__main__':
    main()
