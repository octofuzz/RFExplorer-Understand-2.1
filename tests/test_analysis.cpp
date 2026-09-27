#include "analysis.h"
constexpr bool garage() {
    rf::FineScan s; s.begin(1,433950);
    if(s.count!=41 || s.current()!=433850) return false;
    for(unsigned n=0;n<123;++n) {
        auto f=s.current();
        float distance=f>433920?f-433920:433920-f;
        s.add(-45.0f-distance*0.5f);
    }
    float peak=0,noise=0; bool valid=false;
    return s.result(peak,noise,valid)==433920 && valid;
}
constexpr bool noSignal() {
    rf::FineScan s; s.begin(1,433950);
    for(unsigned n=0;n<123;++n) s.add(-110);
    float peak=0,noise=0; bool valid=true;
    s.result(peak,noise,valid); return !valid;
}
constexpr bool transient() {
    rf::FineScan s; s.begin(1,433950);
    for(unsigned n=0;n<123;++n) s.add(n==14?-40:-110);
    float peak=0,noise=0; bool valid=true;
    s.result(peak,noise,valid); return !valid;
}
constexpr bool edge() {
    rf::FineScan s; s.begin(1,387000);
    if(s.count!=21 || s.current()!=387000) return false;
    for(unsigned n=0;n<63;++n) s.add(s.current()==387000?-40:-110);
    float peak=0,noise=0; bool valid=true;
    s.result(peak,noise,valid); return !valid;
}
constexpr bool bursts() {
    rf::Envelope e;
    e.sample(0xfffffff0u,-40,-100);
    e.sample(0xfffffffeu,-40,-100);
    e.sample(3,-100,-100); // Gap shorter than 12 ms belongs to the same burst.
    if(!e.active || e.count!=1) return false;
    e.sample(10,-100,-100);
    if(e.active || e.duration!=14) return false;
    e.sample(20,-40,-100); e.sample(30,-40,-100); e.sample(42,-100,-100);
    return e.count==2 && e.duration==10 && !e.active;
}
static_assert(garage(),"433.950 coarse must recover synthetic 433.920 peak");
static_assert(noSignal(),"flat noise is not a frequency estimate");
static_assert(transient(),"one transient must not pass repeatability");
static_assert(edge(),"clip scan at hardware boundary and reject edge maximum");
static_assert(bursts(),"hysteresis, burst gaps, durations and millis rollover");
