from pathlib import Path
import csv
import importlib.util
import subprocess
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('replay',root/'scripts/replay_trace.py')
replay=importlib.util.module_from_spec(spec);spec.loader.exec_module(replay)
exe=replay.build();out=root/'tests/.build'
def run(name,rows,expected):
    path=out/(name+'.csv')
    with path.open('w',newline='') as f:
        w=csv.writer(f);w.writerow(['uptime_ms','frequency_khz','rssi_dbm','quiet_dbm','start_margin_db','hold_margin_db','max_allowed_gap_ms'])
        for t,r in rows:w.writerow([t,433050,r,-102,10,6,20])
    result=subprocess.run([str(exe),str(path)],check=True,capture_output=True,text=True).stdout.strip()
    assert f'qualified={expected} ' in result,result
    print(name+': '+result)
run('reported-noise',[(i*1019,-103+i%7) for i in range(134)],0)
run('regular-real-bursts',[(t,-80 if 10<=t%100<=20 else -102) for t in range(0,500,2)],5)
run('one-sample-spikes',[(t,-80 if t%100==10 else -102) for t in range(0,500,2)],0)
print('PASS: actual firmware detector replay distinguishes floor noise, spikes and periodic valid bursts')
