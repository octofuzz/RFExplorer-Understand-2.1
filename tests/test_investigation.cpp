#include "investigation.h"
#include <cassert>
int main() {
    field::SignalFingerprint a,b;a.frequency_khz=b.frequency_khz=433920;
    a.peak_rssi=b.peak_rssi=-60;
    for(int df=0;df<=250;df+=5) for(int dr=0;dr<=30;dr+=5) {
        b.frequency_khz=a.frequency_khz+df;b.peak_rssi=a.peak_rssi+dr;
        a.duration_ms=120;b.duration_ms=230;a.bandwidth_khz=25;b.bandwidth_khz=70;
        auto e=field::evidence(a,b);
        int score=100-e.frequencyPenalty-e.strengthPenalty-e.durationPenalty-e.bandwidthPenalty;
        if(score<0) score=0;
        assert(field::similarity(a,b)==score);
        assert(e.durationKnown&&e.bandwidthKnown);
    }
    b=a;b.duration_ms=0;b.bandwidth_khz=0;
    auto e=field::evidence(a,b);assert(!e.durationKnown&&!e.bandwidthKnown);
    assert(e.durationPenalty==0&&e.bandwidthPenalty==0);
    b.frequency_khz+=251;assert(field::similarity(a,b)==0);
    assert(field::bandFor(348000)==0&&field::bandFor(348001)==-1);
    assert(field::bandFor(386999)==-1&&field::bandFor(387000)==1);
    assert(field::bandFor(928000)==3&&field::bandFor(928001)==-1);
}
