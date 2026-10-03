#pragma once
#include "signal_store.h"
#include "investigation.h"
namespace field {
// These are candidate families, not protocol or transmitter identifications.
inline bool familyCandidate(const SignalFingerprint& a,const SignalFingerprint& b) {
    if(!qualified(a) || !qualified(b)) return false;
    if(bandFor(a.frequency_khz)<0 || bandFor(a.frequency_khz)!=bandFor(b.frequency_khz)) return false;
    uint32_t d=a.frequency_khz>b.frequency_khz?a.frequency_khz-b.frequency_khz:b.frequency_khz-a.frequency_khz;
    if(d>250 || !a.duration_ms || !b.duration_ms) return false;
    return uint32_t(a.duration_ms)<=uint32_t(b.duration_ms)*2 && uint32_t(b.duration_ms)<=uint32_t(a.duration_ms)*2;
}
struct Families {
    uint8_t anchor[48]{},count=0;
    void build(const SignalMemory& memory) {
        count=memory.count(); uint8_t order[48]{};
        for(unsigned i=0;i<count;++i) order[i]=i;
        for(unsigned i=0;i<count;++i) for(unsigned j=i+1;j<count;++j)
            if(memory.entry(order[j]).fingerprint.frequency_khz<memory.entry(order[i]).fingerprint.frequency_khz) {
                auto t=order[i];order[i]=order[j];order[j]=t;
            }
        for(unsigned i=0;i<count;++i) {
            unsigned n=order[i]; anchor[n]=n;
            for(unsigned j=0;j<i;++j) {
                unsigned a=order[j];
                if(anchor[a]==a && familyCandidate(memory.entry(n).fingerprint,memory.entry(a).fingerprint)) {anchor[n]=a;break;}
            }
        }
    }
    unsigned members(unsigned a) const { unsigned n=0;for(unsigned i=0;i<count;++i) if(anchor[i]==a) ++n;return n; }
};
inline bool deviation(const MemoryEntry& entry,int rssi) {
    return entry.baselineCount>=6 && abs(rssi-int(entry.baselineRSSI))>=12;
}
struct Session {
    uint32_t activityRows=0,observations=0,discoveries=0,repeats=0,familyCandidates=0,anomalies=0,geotagged=0,unstored=0;
    int16_t strongest=-120; uint32_t strongestFrequency=0;
    void activity(uint32_t f,int16_t rssi) {
        if((!activityRows&&!observations) || rssi>strongest) {strongest=rssi;strongestFrequency=f;}
        ++activityRows;
    }
    void record(const SignalFingerprint& fp,bool fresh,bool family,bool odd,bool geo,bool stored) {
        if((!observations&&!activityRows) || fp.peak_rssi>strongest) {strongest=fp.peak_rssi;strongestFrequency=fp.frequency_khz;}
        ++observations;
        if(stored) {if(fresh) ++discoveries;else ++repeats;} else ++unstored;
        if(family) ++familyCandidates;if(odd) ++anomalies;if(geo) ++geotagged;
    }
};
}
