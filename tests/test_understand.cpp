#include "understand.h"
#include <cassert>
int main() {
    field::SignalFingerprint a,b;a.frequency_khz=433900;b.frequency_khz=434150;
    a.duration_ms=100;b.duration_ms=200;
    a.peak_rssi=b.peak_rssi=-60;a.noise_rssi=b.noise_rssi=-110;
    a.samples=b.samples=20;a.evidence_version=b.evidence_version=1;
    assert(field::familyCandidate(a,b));
    b.frequency_khz++;assert(!field::familyCandidate(a,b));b.frequency_khz--;
    b.duration_ms=201;assert(!field::familyCandidate(a,b));b.duration_ms=0;assert(!field::familyCandidate(a,b));
    b.duration_ms=100;b.frequency_khz=868000;assert(!field::familyCandidate(a,b));
    field::MemoryEntry e;e.baselineRSSI=-70;e.baselineCount=5;
    assert(!field::deviation(e,-40));e.baselineCount=6;
    assert(!field::deviation(e,-59));assert(field::deviation(e,-58));assert(field::deviation(e,-82));
    field::Session s;a.peak_rssi=-65;s.record(a,true,false,false,true,true);
    s.record(a,false,false,true,false,true);s.record(a,true,true,false,false,false);
    assert(s.observations==3&&s.discoveries==1&&s.repeats==1&&s.unstored==1);
    assert(s.anomalies==1&&s.geotagged==1&&s.familyCandidates==1&&s.strongestFrequency==433900);
}
