#pragma once
#include <stdint.h>

namespace rf {
struct Band { const char* name; uint32_t low, high, centre, scanLow, scanHigh; bool sw0, sw1; };
constexpr Band bands[] = {
    {"315", 300000, 348000, 315000, 314000, 316000, false, false},
    {"433", 387000, 464000, 433920, 433050, 434790, false, true},
    {"868", 779000, 928000, 868300, 868000, 869000, true, true},
    {"915", 779000, 928000, 915000, 902000, 928000, true, true}
};
constexpr bool inBand(unsigned b, uint32_t f) {
    return b < 4 && f >= bands[b].low && f <= bands[b].high;
}
constexpr bool validRange(unsigned b, uint32_t lo, uint32_t hi, uint32_t step) {
    return inBand(b,lo) && inBand(b,hi) && lo <= hi && step >= 10 && step <= 1000;
}
// Include the upper endpoint even if the step does not divide the span.
constexpr uint32_t nextFrequency(uint32_t f, uint32_t lo, uint32_t hi, uint32_t step) {
    return f >= hi ? lo : (hi - f < step ? hi : f + step);
}
constexpr bool elapsed(uint32_t now, uint32_t then, uint32_t ms) { return uint32_t(now-then) >= ms; }
struct Threshold {
    bool high = false;
    bool update(float rssi, int limit) {
        if (high && rssi < limit - 3) high = false;
        if (!high && rssi >= limit) { high = true; return true; }
        return false;
    }
};
static_assert(inBand(0,300000) && inBand(0,348000), "315 endpoints");
static_assert(!inBand(0,348001) && !inBand(1,386999), "reject gaps");
static_assert(!inBand(2,118000) && !inBand(3,1090000), "no airband or ADS-B");
static_assert(!validRange(1,433000,868000,100), "cannot cross gaps");
static_assert(!validRange(1,434000,433000,100), "ordered range");
static_assert(!validRange(1,433000,434000,0), "no zero step");
static_assert(nextFrequency(434750,433050,434790,100)==434790, "include endpoint");
static_assert(nextFrequency(434790,433050,434790,100)==433050, "wrap scan");
static_assert(elapsed(10,0xfffffff0u,26), "millis rollover");
}
