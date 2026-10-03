"""Replay an exported RF trace through the firmware's C++ detector.
Requires Visual Studio C++ Build Tools on Windows, or c++ on other systems.
Usage: python scripts/replay_trace.py path/to/trace.csv
"""
from pathlib import Path
import argparse
import os
import subprocess
root=Path(__file__).resolve().parents[1]
def build():
    out=root/'tests/.build';out.mkdir(exist_ok=True)
    exe=out/('replay_trace.exe' if os.name=='nt' else 'replay_trace')
    source=root/'tests/replay_trace.cpp'
    if os.name=='nt':
        vc=Path('C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Auxiliary/Build/vcvars64.bat')
        rsp=out/'replay.rsp'
        rsp.write_text('\n'.join(['/nologo','/std:c++14','/EHsc','/O2',f'/I"{root/"include"}"',f'/Fe:"{exe}"',f'"{source}"']))
        command=out/'replay.cmd'
        command.write_text(f'@call "{vc}" >nul\n@cl @"{rsp}"\n@exit /b %errorlevel%\n')
        subprocess.run(['cmd','/c',str(command)],cwd=out,check=True)
    else:
        subprocess.run(['c++','-std=c++14','-O2','-I'+str(root/'include'),str(source),'-o',str(exe)],check=True)
    return exe
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('csv',type=Path);args=parser.parse_args()
    if not args.csv.is_file():parser.error('Trace CSV does not exist')
    subprocess.run([str(build()),str(args.csv.resolve())],check=True)
