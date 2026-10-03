"""Guard the firmware integration boundaries that pure detector tests cannot cover."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=(root/'src/main.cpp').read_text()
def body(start,end):return source.split(start,1)[1].split(end,1)[0]
sample=body('void analyseRadio() {','void sampleSweep()')
assert sample.index('lastComparison=signalMemory.bestMatch')<sample.index('saveObservation("BURST")')
update=body('void updateLiveMatch() {','void draw()')
assert 'bestMatch(' not in update and 'liveMatch=lastComparison' in update
save=body('void saveObservation(const char* kind) {','void startObservationMonitor()')
assert 'qualified?lastBurstUTC' in save and 'qualified?lastBurstGeo' in save
remember=body('void rememberCurrentSignal(float savedStrength) {','void saveObservation(const char* kind) {')
assert remember.index('rememberedBurst==envelope.last.sequence')<remember.index('signalMemory.remember(')
assert 'envelope.last.end, lastBurstEpoch' in remember
assert 'LIKELY MATCH' not in source
assert 'lastBurstContext=encounterContext(now,100)' in sample
assert 'lastBurstEpoch, lastBurstContext' in remember
assert 'quietReference.fresh(now) || envelope.active' in sample
assert 'activityReady && autoLog && !rfTrace.active' in sample
assert 'frameInterval=screen==Screen::SatRFLive?250:observing?' in source
assert 'signalMemory.setFlag' in source and 'signalMemory.setReviewed' in source
print('PASS: pre-save comparison, no self-rematch, immutable event tags and no duplicate sightings from repeated saves')
start=body('void startSatelliteRF(){','void sampleSatelliteRF(){')
assert start.index('if(gnssOnly)')<start.index('tuneSatelliteRF()')
sample=body('void sampleSatelliteRF(){','void stopRadio()')
assert 'screen!=Screen::SatRFLive||satelliteRFPaused' in sample
assert sample.index('satelliteRFWindow.valid()')<sample.index('observations.append(r)')
assert 'signalMemory' not in sample and 'satLog.' not in sample
assert 'if(gps.freshFix())' in sample and 'identity=unknown' in sample
assert 'satelliteRFWindow.reset();' in sample
assert 'if(screen==Screen::SatRFLive){rx.stop();satelliteRFWindow.reset();' in source
print('PASS: experimental RF isolation, timing admission, fresh geotags and radio stop on exit')

assert 'r.session=rfActiveSession;r.phase=rfPhase' in sample
assert sample.index('observations.append(r)')<sample.index('satelliteRFArchive.add(r)')
assert 'r.fixAge=' in sample and 'gps.freshHdop()' in sample
assert 'rfTimeline.add(now,value)' in sample
print('PASS: session persistence before rolling archive, frozen phase and receiver quality context')
