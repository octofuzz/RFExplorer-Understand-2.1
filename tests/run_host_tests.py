"""Run hardware-independent contracts and actual SignalMemory with an in-memory SD.
Windows: requires Visual Studio Build Tools; run with Python 3.
"""
from pathlib import Path
import subprocess
import os
import shutil
root=Path(__file__).resolve().parents[1]
out=root/'tests/.build'; out.mkdir(exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Auxiliary/Build/vcvars64.bat')
import sys
names=sys.argv[1:] or ['core','analysis','finder','satellites','field_intelligence','signal_memory','signal_store','investigation','understand','gps','gnss_logbook','event_csv']
for name in names:
    src=root/f'tests/test_{name}.cpp'
    if name in ['core','analysis','finder'] and 'int main(' not in src.read_text():
        wrapper=out/f'run_{name}.cpp'; wrapper.write_text(f'#include "{src.as_posix()}"\nint main() {{return 0;}}\n'); src=wrapper
    extra=[root/f'src/{name}.cpp'] if name in ('signal_store','gps','gnss_logbook') else []
    if os.name=='nt':
        rsp=out/f'{name}.rsp'
        rsp.write_text('\n'.join(['/nologo','/std:c++14','/EHsc','/O2','/D_CRT_SECURE_NO_WARNINGS',f'/I"{root/"tests/host"}"',f'/I"{root/"include"}"',f'/Fe:"{out/name}.exe"',f'"{src}"']+[f'"{s}"' for s in extra]))
        cmd=out/f'{name}.cmd'; cmd.write_text(f'@call "{vc}" >nul\n@cl @"{rsp}"\n@if errorlevel 1 exit /b 1\n@"{out/name}.exe"\n')
        subprocess.run(['cmd','/c',str(cmd)],cwd=out,check=True)
    else:
        subprocess.run(['c++','-std=c++14','-O2','-I'+str(root/'tests/host'),'-I'+str(root/'include'),str(src),*[str(s) for s in extra],'-o',str(out/name)],check=True)
        subprocess.run([str(out/name)],check=True)
    print('PASS:',name,flush=True)
