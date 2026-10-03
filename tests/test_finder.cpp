#include "finder.h"

constexpr bool boundedSweep() {
    rf::Sweep s;
    if(!s.begin(1,433050,434150,10) || s.count!=111 || s.current()!=433050) return false;
    for(unsigned i=0;i<111;++i) s.add(-100.0f+float(i));
    return s.sweeps==1 && s.index==0 && s.strongestFrequency==434150 && s.strongest==10.0f;
}
constexpr bool invalidSweep() {
    rf::Sweep s;
    return !s.begin(1,433100,433050,10) && !s.begin(1,386900,387100,10);
}
constexpr bool resetPeak() {
    rf::Sweep s; s.begin(1,433900,433920,10);
    s.add(-40); s.add(-50); s.add(-60); s.resetHeld();
    return s.strongest==-120 && s.strongestFrequency==0 && s.held[0]==-120;
}
static_assert(boundedSweep(),"112-bin display must include both endpoints and wrap");
static_assert(invalidSweep(),"finder rejects reversed and out-of-band spans");
static_assert(resetPeak(),"peak reset must clear all held bins");
constexpr bool resetFloor() {rf::Sweep s;s.floor=-40;s.begin(1,433050,433100,25);return s.floor==-110;}
static_assert(resetFloor(),"new scan must not inherit a different band's floor");
static_assert(rf::waterfallColour(-110,-100)==0,"below floor");
static_assert(rf::waterfallColour(-99,-100)==0,"1 dB must not saturate");
static_assert(rf::waterfallColour(-70,-100)==15,"30 dB midpoint");
static_assert(rf::waterfallColour(-40,-100)==31,"60 dB ceiling");
static_assert(rf::waterfallColour(10,-100)==31,"clamp high");
