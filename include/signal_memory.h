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
};

inline uint8_t similarity(const SignalFingerprint& a,const SignalFingerprint& b) {
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

inline bool likelySameSignal(const SignalFingerprint& a,const SignalFingerprint& b) {
    return similarity(a,b)>=75;
}

}
