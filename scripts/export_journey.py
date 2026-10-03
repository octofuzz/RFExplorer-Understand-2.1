"""Convert an Expedition session CSV to GPX without joining route gaps.
Usage: python scripts/export_journey.py path/to/session.csv [--output route.gpx]
The original CSV is never modified. Only fresh POINT fixes become track points.
"""
import argparse
import csv
import math
from pathlib import Path
import xml.etree.ElementTree as ET

NS = 'http://www.topografix.com/GPX/1/1'
ET.register_namespace('', NS)
def tag(name): return '{'+NS+'}'+name
def location(row):
    try:
        lat, lon = float(row['latitude']), float(row['longitude'])
        return (lat, lon) if math.isfinite(lat) and math.isfinite(lon) and abs(lat)<=90 and abs(lon)<=180 else None
    except (ValueError, KeyError, TypeError): return None

def convert(source, target):
    root=ET.Element(tag('gpx'),version='1.1',creator='RFExplorer 2.4 Expedition')
    track=ET.Element(tag('trk'));segment=None
    points=bookmarks=0
    with open(source, newline='', encoding='utf-8-sig') as file:
        for row in csv.DictReader(file):
            kind=row.get('type')
            if kind in ('GAP','START','END','RECOVERED'):
                segment=None
                continue
            loc=location(row)
            if kind=='BOOKMARK' and loc:
                node=ET.SubElement(root,tag('wpt'),lat=str(loc[0]),lon=str(loc[1]))
                ET.SubElement(node,tag('name')).text=row.get('note') or 'Field bookmark'
                bookmarks+=1
            elif kind=='POINT' and row.get('state')=='fresh_fix' and loc:
                if segment is None: segment=ET.SubElement(track,tag('trkseg'))
                node=ET.SubElement(segment,tag('trkpt'),lat=str(loc[0]),lon=str(loc[1]))
                try:
                    altitude=float(row.get('altitude_m',''))
                    if math.isfinite(altitude): ET.SubElement(node,tag('ele')).text=str(altitude)
                except (TypeError,ValueError): pass
                points+=1
            else:
                if kind=='POINT': segment=None
                continue
            utc=row.get('utc','')
            if utc.endswith('Z') and 'T' in utc: ET.SubElement(node,tag('time')).text=utc
    root.append(track)
    ET.indent(root)
    ET.ElementTree(root).write(target,encoding='utf-8',xml_declaration=True)
    return points,bookmarks

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv',type=Path);parser.add_argument('--output',type=Path)
    args=parser.parse_args();output=args.output or args.csv.with_suffix('.gpx')
    if output.resolve()==args.csv.resolve(): parser.error('Output must differ from input CSV')
    points,bookmarks=convert(args.csv,output)
    print(f'Saved {output}: {points} track points, {bookmarks} geotagged bookmarks')
