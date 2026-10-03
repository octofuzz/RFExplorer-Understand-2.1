"""Compare manually aligned threshold crossings; never calibrate dBm against dBFS.
Inputs: Cardputer diagnostic CSV uptime_ms,rssi_dbm; external CSV elapsed_ms,level_dbfs.
External CSV must be prepared from an independent receiver's measurements; this does
not capture HackRF IQ or convert its amplitude into calibrated power.
"""
import argparse,csv,json,math,statistics
from pathlib import Path

def analyse(path,time_column,value_column,gate,max_gap=250):
    previous=None;high=False;intervals=[];rises=[];valid=0;invalid=0;gaps=0
    with Path(path).open(newline='',encoding='utf-8-sig') as f:
        reader=csv.DictReader(f)
        if not {time_column,value_column}.issubset(reader.fieldnames or []):
            raise ValueError(f'{path}: required columns {time_column}, {value_column}')
        for row in reader:
            t=float(row[time_column]);v=float(row[value_column] or 'nan')
            if not math.isfinite(t) or (previous is not None and t<=previous):
                raise ValueError('Times must be finite and strictly increasing; unwrap counters first')
            gap=t-previous if previous is not None else None
            if gap is not None:
                intervals.append(gap)
                if gap>max_gap:gaps+=1;high=False
            if not math.isfinite(v):invalid+=1;high=False
            else:
                valid+=1
                # First sample and samples after gaps only establish state, not an observed edge.
                if v>=gate and not high and previous is not None and gap<=max_gap and prior_valid:
                    rises.append(t)
                high=v>=gate
            prior_valid=math.isfinite(v);previous=t
    return dict(unit=value_column,gate=gate,valid_samples=valid,invalid_samples=invalid,
                max_interval_ms=max(intervals,default=None),median_interval_ms=statistics.median(intervals) if intervals else None,
                long_gaps=gaps,rising_threshold_crossings_ms=rises)

def compare(dut,reference,offset=None):
    result={'cardputer':dut,'reference':reference,'reference_offset_ms':offset,
            'interpretation':'Threshold crossings only; no satellite identity, packet decoding or absolute power calibration.'}
    a=dut['rising_threshold_crossings_ms'];b=reference['rising_threshold_crossings_ms']
    if offset is None:result['timing_comparison']='Not compared: independent time axes have not been aligned.'
    elif not a or len(a)!=len(b):result['timing_comparison']='Not paired: no crossings or unequal crossing counts.'
    else:
        result['cardputer_minus_shifted_reference_ms']=[x-(y+offset) for x,y in zip(a,b)]
        result['timing_comparison']='Order-paired crossings after user-supplied alignment; manually verify corresponding events. Sampling intervals limit interpretation.'
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('cardputer');p.add_argument('reference');p.add_argument('--dut-gate',type=float,required=True,help='RSSI threshold in dBm')
    p.add_argument('--reference-gate',type=float,required=True,help='Independent threshold in the external CSV dBFS units')
    p.add_argument('--offset-ms',type=float,help='Known offset added to reference times; omitted means no timing comparison')
    p.add_argument('--output',required=True);args=p.parse_args()
    if not math.isfinite(args.dut_gate) or not math.isfinite(args.reference_gate) or (args.offset_ms is not None and not math.isfinite(args.offset_ms)):p.error('Thresholds/offset must be finite')
    data=compare(analyse(args.cardputer,'uptime_ms','rssi_dbm',args.dut_gate),analyse(args.reference,'elapsed_ms','level_dbfs',args.reference_gate),args.offset_ms)
    Path(args.output).write_text(json.dumps(data,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    print('Saved comparison; independent units retained. No hardware calibration claimed.')
if __name__=='__main__':main()
