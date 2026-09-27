#pragma once
#include <stdint.h>
#include "core.h"

namespace rf {
constexpr uint8_t waterfallColour(float value, float floor) {
    // 32 colours across 60 dB above each completed sweep's estimated floor.
    // Clamp BEFORE conversion: NaN and infinity must never enter float-to-int conversion.
    if(!(value>floor)) return 0;
    if(value-floor>=60.0f) return 31;
    return uint8_t((value-floor)*31.0f/60.0f);
}
struct Sweep {
    static constexpr unsigned maxBins=112;
    uint32_t frequencies[maxBins]{};
    float live[maxBins]{}, held[maxBins]{};
    unsigned count=0,index=0,sweeps=0;
    float floor=-110, strongest=-120;
    uint32_t strongestFrequency=0;
    constexpr bool begin(unsigned band,uint32_t lo,uint32_t hi,uint32_t spacing) {
        count=index=sweeps=0; strongest=-120; strongestFrequency=0;
        if(!validRange(band,lo,hi,spacing)) return false;
        for(uint32_t f=lo;;f=nextFrequency(f,lo,hi,spacing)) {
            if(count==maxBins) return false;
            frequencies[count]=f; live[count]=held[count]=-120; ++count;
            if(f==hi) break;
        }
        return count>0;
    }
    constexpr uint32_t current() const { return frequencies[index]; }
    constexpr bool add(float value) {
        live[index]=value;
        if(value>held[index]) held[index]=value;
        if(value>strongest) { strongest=value; strongestFrequency=frequencies[index]; }
        if(++index==count) { index=0; ++sweeps; return true; }
        return false;
    }
    constexpr void resetHeld() {
        strongest=-120; strongestFrequency=0;
        for(unsigned i=0;i<count;++i) held[i]=-120;
    }
};
}
