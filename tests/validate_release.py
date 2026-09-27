"""RFExplorer 2.1 release validation. Run with Python 3 from the source tree."""
from pathlib import Path
import hashlib
import json
import re
import struct

root=Path(__file__).resolve().parents[1]
source='\n'.join(p.read_text(errors='ignore') for p in (root/'src').glob('*.cpp'))
source=re.sub(r'/\*.*?\*/|//[^\n]*','',source,flags=re.S)

# Application-layer safety/architecture audit.
assert not re.search(r'\b(?:radio|rx)\s*\.\s*(?:transmit\w*|startTransmit|startChannelScan)\s*\(',source)
assert 'nfcA.identify(' not in source
assert 'nfcA.detect(picc)' in source and 'nfc.disableField()' in source
assert 'radio.receiveDirectAsync()' in source
assert source.index('radio.receiveDirectAsync()') < source.index('radio.setDIOMapping(2,map)')
assert 'SatelliteLogbook' in source
assert 'System::SBAS' in source
assert 'WAYPOINT' in source
assert 'GNSS DIAGNOSTICS' in source

pkg=(root/'scripts/package_firmware.py').read_text(errors='ignore')
assert 'RFExplorer-v2.1-app-only.bin' in pkg
assert 'RFExplorer-v2.1-cardputer-adv-merged.bin' in pkg
assert '"version":"2.1.0"' in pkg

manifest_path=root/'firmware/manifest.json'
if manifest_path.exists():
    manifest=json.loads(manifest_path.read_text())
    assert manifest['version']=='2.1.0'
    for name,record in manifest['images'].items():
        data=(root/'firmware'/name).read_bytes()
        assert len(data)==record['bytes'], name
        assert hashlib.sha256(data).hexdigest()==record['sha256'], name
    merged=(root/'firmware/RFExplorer-v2.1-cardputer-adv-merged.bin').read_bytes()
    app=(root/'firmware/RFExplorer-v2.1-app-only.bin').read_bytes()
    assert merged[0]==0xe9 and app[0]==0xe9, 'ESP image magic'
    assert struct.unpack_from('<H',app,12)[0]==9, 'ESP32-S3 chip id'
    assert merged[0x10000:0x10000+len(app)]==app, 'application at 0x10000'
    for marker in (b'2.1.0',b'UNDERSTAND',b'DOSSIER / HISTORY',b'DOSSIER / IDENTITY',b'INSPECT / MATCH',b'signal-memory-v20.csv',b'RF ANALYST',b'FAMILY / BASELINE',b'FIELD JOURNAL',b'baseline_deviations='):
        assert marker in app and marker in merged, ('missing v2.1 implementation marker',marker)
    assert len(app)<=0x330000, 'application fits its slot'
    assert merged[2]==2, 'DIO flash mode'
    entries={}
    for off in range(0x8000,0x9000,32):
        magic,kind,subtype,address,size,label,flags=struct.unpack_from('<HBBII16sI',merged,off)
        if magic!=0x50aa: break
        entries[label.split(b'\0')[0].decode()]=(kind,subtype,address,size)
    assert entries['nvs']==(1,2,0x9000,0x5000)
    assert entries['otadata']==(1,0,0xe000,0x2000)
    assert entries['app0']==(0,16,0x10000,0x330000)
    assert entries['app1']==(0,17,0x340000,0x330000)
    assert entries['spiffs']==(1,130,0x670000,0x190000)
    ordered=sorted(entries.values(),key=lambda row:row[2])
    for a,b in zip(ordered,ordered[1:]): assert a[2]+a[3]<=b[2], 'partition overlap'
    assert ordered[-1][2]+ordered[-1][3]==0x800000
    print('PASS: source audit + RFExplorer 2.1 binary/package validation')
else:
    raise AssertionError('Release validation requires built binaries and manifest')
