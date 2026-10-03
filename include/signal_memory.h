#pragma once
#include <stdint.h>
#include <math.h>

namespace field {

struct SignalFingerprint {
    uint32_t frequency_khz=0;
    int16_t peak_rssi=-120;
    int16_t noise_rssi=-120;
    uint16_t duration_ms=0;
    uint16_t repeat_ms=0;
    uint16_t bandwidth_khz=0;
    uint16_t samples=0, sample_gap_ms=0;
    uint8_t evidence_version=0; // Zero = legacy/unknown, never silently inferred.
};

inline bool qualified(const SignalFingerprint& fp) {
    return fp.evidence_version==1 && fp.duration_ms>=4 && fp.samples>=3 && fp.sample_gap_ms<=20 &&
        fp.peak_rssi>=-140 && fp.peak_rssi<=20 && fp.noise_rssi>=-140 && fp.noise_rssi<=20 &&
        int(fp.peak_rssi)-int(fp.noise_rssi)>=10;
}
// Version 2 denotes sampled sustained-activity windows, not burst fingerprints.
inline bool activityWindow(const SignalFingerprint& fp) {
    return fp.evidence_version==2 && fp.duration_ms>=5000 && fp.samples>=100 && fp.sample_gap_ms<=250 &&
        fp.peak_rssi>=-140 && fp.peak_rssi<=20 && fp.noise_rssi>=-140 && fp.noise_rssi<=20 &&
        int(fp.peak_rssi)-int(fp.noise_rssi)>=10;
}
inline bool observed(const SignalFingerprint& fp) {return qualified(fp)||activityWindow(fp);}
inline uint8_t rawSimilarity(const SignalFingerprint& a,const SignalFingerprint& b) {
    if(!a.frequency_khz || !b.frequency_khz) return 0;
    uint32_t fd = a.frequency_khz>b.frequency_khz ? a.frequency_khz-b.frequency_khz : b.frequency_khz-a.frequency_khz;
    if(fd>250) return 0;
    int score=100;
    score -= int(fd/5);
    if(score<0) score=0;
    int rd=abs(int(a.peak_rssi)-int(b.peak_rssi));
    score -= rd>20 ? 20 : rd;
    if(a.duration_ms && b.duration_ms) {
        uint32_t dd=a.duration_ms>b.duration_ms ? a.duration_ms-b.duration_ms : b.duration_ms-a.duration_ms;
        uint32_t base=a.duration_ms>b.duration_ms?a.duration_ms:b.duration_ms;
        int penalty=base ? int((dd*20)/base) : 0;
        score -= penalty>20?20:penalty;
    }
    if(a.bandwidth_khz && b.bandwidth_khz) {
        uint32_t bd=a.bandwidth_khz>b.bandwidth_khz ? a.bandwidth_khz-b.bandwidth_khz : b.bandwidth_khz-a.bandwidth_khz;
        int penalty=int(bd/5);
        score -= penalty>15?15:penalty;
    }
    if(score<0) score=0;
    if(score>100) score=100;
    return uint8_t(score);
}

// Similarity is a comparison score, not a probability or transmitter identity.
inline uint8_t similarity(const SignalFingerprint& a,const SignalFingerprint& b) {
    uint8_t score=rawSimilarity(a,b);
    return (!qualified(a)||!qualified(b)) && score>60?60:score;
}
inline bool likelySameSignal(const SignalFingerprint& a,const SignalFingerprint& b) {
    return similarity(a,b)>=75;
}

}
