#!/usr/bin/env python3
"""Launch the already-built native game; never rebuild or alter existing saves."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--saves',type=Path,help='Optional isolated save directory')
    args=parser.parse_args()
    executable=ROOT/'.build/native'/('RelWithDebInfo/sonnheide.exe' if os.name=='nt' else 'sonnheide')
    if not executable.is_file():
        raise SystemExit('Build first: python3 tools/build.py --bundle')
    command=[str(executable),'--resources',str(ROOT/'.build/runtime')]
    if args.saves:command+=['--saves',str(args.saves.resolve())]
    raise SystemExit(subprocess.call(command,cwd=ROOT))

if __name__=='__main__':main()
