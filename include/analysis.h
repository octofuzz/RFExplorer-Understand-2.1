#pragma once
#include <algorithm>
#include <cmath>
#include <stdint.h>
#include "core.h"
#include "evidence.h"
namespace rf {
template<class T> constexpr void sortValues(T* a,unsigned n) {
    for(unsigned i=1;i<n;++i) {
        T value=a[i]; unsigned j=i;
        while(j && a[j-1]>value) { a[j]=a[j-1]; --j; }
        a[j]=value;
    }
}
// Integer kHz grid, clipped to the selected hardware region.
struct FineScan {
    uint32_t coarse=0, frequency[41]{};
    float levels[3][41]{};
    unsigned count=0, pass=0, index=0;
    constexpr void begin(unsigned band,uint32_t centre) {
        coarse=centre; count=pass=index=0;
        for(int offset=-100;offset<=100;offset+=5) {
            int64_t f=int64_t(centre)+offset;
            if(f>=0 && inBand(band,uint32_t(f))) frequency[count++]=uint32_t(f);
        }
    }
    constexpr unsigned bin() const { return pass==1 ? count-1-index : index; }
    constexpr uint32_t current() const { return frequency[bin()]; }
    constexpr bool add(float dbm) {
        levels[pass][bin()]=dbm;
        if(++index==count) { index=0; ++pass; }
        return pass==3;
    }
    constexpr uint32_t result(float& peak,float& noise,bool& valid) const {
        if(!count || pass!=3) {peak=noise=-120;valid=false;return coarse;}
        float all[123]{}; unsigned n=0, best[3]{};
        for(unsigned p=0;p<3;++p) for(unsigned i=0;i<count;++i) {
            all[n++]=levels[p][i];
            if(levels[p][i]>levels[p][best[p]]) best[p]=i;
        }
        sortValues(all,n); noise=all[n/5];
        float score[41]{}; unsigned b=0;
        for(unsigned i=0;i<count;++i) {
            float a[3]={levels[0][i],levels[1][i],levels[2][i]};
            sortValues(a,3); score[i]=a[1];
            if(score[i]>score[b]) b=i;
        }
        peak=score[b];
        // A candidate is repeatable only if all sweep maxima agree within 15 kHz.
        unsigned lo=std::min(best[0],std::min(best[1],best[2]));
        unsigned hi=std::max(best[0],std::max(best[1],best[2]));
        valid=peak-noise>=8 && frequency[hi]-frequency[lo]<=15 && b>0 && b+1<count;
        return frequency[b];
    }
};
struct Envelope {
    bool active=false, sampled=false, candidateTruncated=false;
    float peak=-120,quiet=-120;
    uint32_t start=0,lastHigh=0,count=0,duration=0,maxGap=0,lastSample=0;
    uint32_t candidateSamples=0,candidateGap=0,unresolved=0,gapCount=0,recentGap=0;
    BurstEvidence last{};
    Recurrence recurrence{};
    constexpr void interrupt() {
        if(active) ++unresolved;
        active=false;sampled=false;recurrence.breakContinuity();
    }
    constexpr void sample(uint32_t now,float level,float noise) {
        // Ordered comparisons reject NaN and out-of-range receiver values.
        if(!(level>=-140 && level<=20 && noise>=-140 && noise<=20)) {interrupt();return;}
        uint32_t gap=sampled?uint32_t(now-lastSample):0;
        recentGap=gap;if(gap>maxGap)maxGap=gap;
        if(sampled && gap>maximumSampleGapMs) {++gapCount;interrupt();}
        sampled=true;lastSample=now;
        if(active && gap>candidateGap)candidateGap=gap;
        const float gate=active?quiet+6:noise+10;
        if(level>=gate) {
            if(!active) {active=true;start=now;peak=level;quiet=noise;candidateSamples=0;candidateGap=0;candidateTruncated=false;}
            if(level>peak)peak=level;
            if(candidateSamples<65535)++candidateSamples;lastHigh=now;duration=now-start;
            if(duration>65535)candidateTruncated=true;
        } else if(active && elapsed(now,lastHigh,12)) {
            active=false;
            bool good=!candidateTruncated && candidateSamples>=minimumBurstSamples && duration>=minimumBurstMs && duration<=65535 && candidateGap<=maximumSampleGapMs && peak-quiet>=10;
            if(good) {
                ++count;last={true,count,start,lastHigh,duration,candidateSamples,candidateGap,peak,quiet};
                recurrence.add(start);
            } else {++unresolved;recurrence.breakContinuity();}
        }
    }
};
}
