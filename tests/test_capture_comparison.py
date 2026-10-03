import importlib.util,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('comparison',root/'scripts/compare_captures.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
with tempfile.TemporaryDirectory() as temp:
    p=Path(temp)/'data.csv';p.write_text('uptime_ms,rssi_dbm\n0,-100\n50,-60\n100,-100\n150,nan\n200,-60\n1000,-60\n1050,-100\n1100,-60\n')
    result=module.analyse(p,'uptime_ms','rssi_dbm',-75)
    assert result['rising_threshold_crossings_ms']==[50,1100] and result['invalid_samples']==1 and result['long_gaps']==1
    assert 'cardputer_minus_shifted_reference_ms' not in module.compare(result,result)
    assert module.compare(result,result,0)['cardputer_minus_shifted_reference_ms']==[0,0]
    p.write_text('uptime_ms,rssi_dbm\n10,-100\n5,-60\n')
    try:module.analyse(p,'uptime_ms','rssi_dbm',-75)
    except ValueError:pass
    else:raise AssertionError('Backwards time accepted')
print('PASS: independent units, explicit alignment, gaps, invalid data and counter-order checks')
