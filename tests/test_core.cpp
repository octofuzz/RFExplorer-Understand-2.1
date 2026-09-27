// Compile-time contract tests. Run with a C++14 compiler (no hardware required).
#include "core.h"
constexpr bool bandMatrix() {
    for(unsigned b=0;b<4;++b) {
        if(!rf::inBand(b,rf::bands[b].low) || !rf::inBand(b,rf::bands[b].high)) return false;
        if(rf::inBand(b,rf::bands[b].low-1) || rf::inBand(b,rf::bands[b].high+1)) return false;
        if(!rf::validRange(b,rf::bands[b].scanLow,rf::bands[b].scanHigh,100)) return false;
        if(!rf::inBand(b,rf::bands[b].centre)) return false;
    }
    return !rf::inBand(4,433920) && !rf::validRange(4,433000,434000,100);
}
constexpr bool sweep(uint32_t lo,uint32_t hi,uint32_t step) {
    uint32_t f=lo, count=0;
    while(f<hi && count++<20000) {
        uint32_t next=rf::nextFrequency(f,lo,hi,step);
        if(next<=f || next>hi) return false;
        f=next;
    }
    return f==hi && rf::nextFrequency(f,lo,hi,step)==lo;
}
static_assert(bandMatrix(),"all band boundaries and presets");
static_assert(sweep(779000,928000,10),"largest range / smallest step terminates");
static_assert(sweep(433050,434790,100),"fractional final step terminates");
static_assert(sweep(433920,433920,100),"one-bin scan wraps");
static_assert(sweep(433000,433010,1000),"step wider than span reaches upper endpoint");
static_assert(!rf::validRange(1,433000,434000,9),"minimum step");
static_assert(!rf::validRange(1,433000,434000,1001),"maximum step");
static_assert(!rf::elapsed(9,0xfffffff0u,26),"no premature event at clock wrap");
static_assert(rf::elapsed(10,0xfffffff0u,26),"event at clock wrap");
static_assert(!rf::bands[0].sw0 && !rf::bands[0].sw1,"315 path");
static_assert(!rf::bands[1].sw0 && rf::bands[1].sw1,"433 path");
static_assert(rf::bands[2].sw0 && rf::bands[2].sw1 && rf::bands[3].sw0 && rf::bands[3].sw1,"868/915 path");
