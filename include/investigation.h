#pragma once
#include "signal_memory.h"
#include "core.h"
namespace field {
inline const char* categoryName(uint8_t category) {
    const char* names[]={"Unknown","Known","Interesting","Interference","Sensor","Remote"};
    return names[category<6?category:0];
}
inline int bandFor(uint32_t f) {
    if(rf::inBand(0,f)) return 0;
    if(rf::inBand(1,f)) return 1;
    if(rf::inBand(2,f)) return f>=902000?3:2;
    return -1;
}
struct MatchEvidence {
    int frequencyPenalty=0, strengthPenalty=0, durationPenalty=0, bandwidthPenalty=0;
    bool durationKnown=false, bandwidthKnown=false;
};
inline MatchEvidence evidence(const SignalFingerprint& a,const SignalFingerprint& b) {
    MatchEvidence e;
    uint32_t fd=a.frequency_khz>b.frequency_khz?a.frequency_khz-b.frequency_khz:b.frequency_khz-a.frequency_khz;
    e.frequencyPenalty=int(fd/5);
    int rd=abs(int(a.peak_rssi)-int(b.peak_rssi)); e.strengthPenalty=rd>20?20:rd;
    e.durationKnown=a.duration_ms && b.duration_ms;
    if(e.durationKnown) {
        uint32_t hi=a.duration_ms>b.duration_ms?a.duration_ms:b.duration_ms;
        uint32_t lo=a.duration_ms<b.duration_ms?a.duration_ms:b.duration_ms;
        e.durationPenalty=(hi-lo)*20/hi;
    }
    e.bandwidthKnown=a.bandwidth_khz && b.bandwidth_khz;
    if(e.bandwidthKnown) {
        int d=abs(int(a.bandwidth_khz)-int(b.bandwidth_khz))/5;
        e.bandwidthPenalty=d>15?15:d;
    }
    return e;
}
}
