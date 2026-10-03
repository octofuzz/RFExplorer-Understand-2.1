#pragma once
#include <stdint.h>
namespace rf {
// Observational limits of this sampled-RSSI instrument, not protocol claims.
constexpr unsigned minimumBurstSamples=3;
constexpr uint32_t minimumBurstMs=4, maximumSampleGapMs=20;
struct BurstEvidence {
    bool qualified=false;
    uint32_t sequence=0,start=0,end=0,duration=0,samples=0,maxGap=0;
    float peak=-120,quiet=-120;
};
struct Recurrence {
    uint32_t values[8]{},previous=0;unsigned count=0,head=0;bool anchored=false;
    constexpr void breakContinuity() {anchored=false;count=head=0;}
    constexpr void add(uint32_t start) {
        if(anchored) {uint32_t dt=start-previous;if(dt && dt<=60000) {values[head]=dt;head=(head+1)%8;if(count<8)++count;} else count=head=0;}
        previous=start;anchored=true;
    }
    constexpr uint32_t mean() const {uint64_t sum=0;for(unsigned i=0;i<count;++i)sum+=values[i];return count?uint32_t(sum/count):0;}
    constexpr uint32_t span() const {if(!count)return 0;uint32_t lo=values[0],hi=lo;for(unsigned i=1;i<count;++i){if(values[i]<lo)lo=values[i];if(values[i]>hi)hi=values[i];}return hi-lo;}
};
}
