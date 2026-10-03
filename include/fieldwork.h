#pragma once
#include <stdint.h>
#include <algorithm>
#include <cmath>
namespace rf {
// Relative quiet reference, not a calibrated noise-power or SNR measurement.
struct QuietReference {
    float values[128]{},level=-120;
    unsigned head=0,count=0,added=0;
    uint32_t started=0,updated=0,lastQuiet=0;
    bool ready=false,begun=false;
    void sample(uint32_t now,float rssi,bool busy) {
        if(!std::isfinite(rssi)||rssi<-140||rssi>20)return;
        if(!begun){begun=true;started=updated=now;}
        if(!ready || (!busy && rssi<level+6)) {
            values[head]=rssi;head=(head+1)%128;if(count<128)++count;++added;lastQuiet=now;
        }
        if(!busy && count>=32 && added>=16 && uint32_t(now-updated)>=1000) {
            float sorted[128];std::copy(values,values+count,sorted);std::sort(sorted,sorted+count);
            float candidate=sorted[count/5];
            level=ready?std::min(candidate,level+1.0f):candidate;
            ready=true;updated=now;added=0;
        }
    }
    uint32_t age(uint32_t now) const {return ready?uint32_t(now-lastQuiet):UINT32_MAX;}
    bool fresh(uint32_t now) const {return ready && age(now)<=10000;}
};
struct SamplingQuality {
    uint32_t previous=0,maxGap=0,intervals=0,late=0,drawUs=0,sdUs=0;
    uint64_t totalGap=0;bool begun=false;
    void sample(uint32_t now) {
        if(begun){uint32_t gap=now-previous;++intervals;totalGap+=gap;if(gap>maxGap)maxGap=gap;if(gap>20)++late;}
        previous=now;begun=true;
    }
    void interrupt(){begun=false;}
    uint32_t mean() const {return intervals?uint32_t(totalGap/intervals):0;}
};
// Five-second sampled activity windows; never counted as protocol packets/bursts.
struct ActivityWindow {
    bool active=false;uint32_t start=0,previous=0,covered=0,samples=0,maxGap=0;
    float peak=-120,quiet=-120;
    uint32_t duration=0,lastSamples=0,lastGap=0;uint8_t coverage=0;float lastPeak=-120,lastQuiet=-120;
    void interrupt(){active=false;}
    bool sample(uint32_t now,float rssi,float reference,bool canStart) {
        if(!std::isfinite(rssi)||rssi<-140||rssi>20){interrupt();return false;}
        if(active && (uint32_t(now-previous)>250 || rssi<quiet+6))interrupt();
        if(!active){if(!canStart||rssi<reference+10)return false;active=true;start=previous=now;covered=0;samples=1;maxGap=0;peak=rssi;quiet=reference;return false;}
        uint32_t gap=now-previous;previous=now;if(gap<=20)covered+=gap;
        maxGap=std::max(maxGap,gap);++samples;peak=std::max(peak,rssi);
        uint32_t span=now-start;if(span<5000)return false;
        coverage=uint8_t(std::min(uint64_t(100),uint64_t(covered)*100/span));
        duration=span;lastSamples=samples;lastGap=maxGap;lastPeak=peak;lastQuiet=quiet;
        // Continue using the frozen reference; its age is saved with each window.
        start=previous=now;covered=0;samples=1;maxGap=0;peak=rssi;
        return coverage>=80 && lastSamples>=100 && duration<=65535 && lastPeak-lastQuiet>=10;
    }
};
}
