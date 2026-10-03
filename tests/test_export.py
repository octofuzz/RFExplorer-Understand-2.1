import csv
import importlib.util
from pathlib import Path
import tempfile
import xml.etree.ElementTree as ET
spec=importlib.util.spec_from_file_location('exporter',Path(__file__).resolve().parents[1]/'scripts/export_journey.py')
e=importlib.util.module_from_spec(spec);spec.loader.exec_module(e)
with tempfile.TemporaryDirectory() as temp:
    source=Path(temp)/'route.csv';out=Path(temp)/'route.gpx'
    with source.open('w',newline='') as f:
        writer=csv.writer(f);writer.writerow(['type','state','latitude','longitude','altitude_m','utc','note'])
        writer.writerows([['POINT','fresh_fix',63,10,50,'2026-09-28T12:00:00Z',''],['GAP','stale_fix','','','','',''],['POINT','fresh_fix',64,11,'','UNSET',''],['BOOKMARK','field_note',64,11,'','UNSET','Hill & valley'],['BOOKMARK','field_note','','','','','unknown'],['POINT','fresh_fix','nan',10,'','','']])
    assert e.convert(source,out)==(2,1)
    tree=ET.parse(out);ns={'g':e.NS}
    assert len(tree.findall('.//g:trkseg',ns))==2
    assert len(tree.findall('.//g:time',ns))==1
    assert len(tree.findall('.//g:ele',ns))==1
    assert tree.find('.//g:name',ns).text=='Hill & valley'
print('PASS: GPX segments, missing data, bookmarks and XML escaping')
