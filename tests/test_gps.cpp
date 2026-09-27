#include "gps.h"
#include "gnss_time.h"
#include <cassert>
#include <cmath>
#include <iostream>
uint32_t hostMillis=1000;
static void feed(gnss::Receiver& gps,const std::string& body,bool bad=false) {
    unsigned sum=0;for(char c:body) sum^=uint8_t(c);
    char suffix[8];snprintf(suffix,sizeof(suffix),"*%02X\r\n",bad?(sum^1):sum);
    HardwareSerial::input()+="$"+body+suffix;gps.poll();
}
int main() {
    gnss::Receiver gps;gps.begin();
    const char* rmc="GNRMC,123456.250,A,6325.8300,N,01023.7000,E,0.1,42.0,270926,,,A";
    feed(gps,rmc,true);assert(!gps.fix().valid&&!gps.fix().timeValid&&gps.diagnostics().checksumBad==1);
    HardwareSerial::input()="$GNRMC,123456,A,6325.8300,N,01023.7000,E,0,0,270926\n";gps.poll();assert(!gps.fix().timeValid);
    feed(gps,rmc);assert(gps.freshFix()&&gps.fix().timeValid);
    assert(fabs(gps.fix().latitude-63.4305)<1e-7&&fabs(gps.fix().longitude-10.395)<1e-7);
    assert(gps.fix().utcFraction==250&&gps.fix().utc=="2026-09-27T12:34:56Z");
    auto epoch=gps.fix().utcEpoch;
    feed(gps,"GNRMC,123456,A,6399.000,N,01023.700,E,0,0,270926,,,A");assert(!gps.freshFix());
    feed(gps,rmc);feed(gps,"GNRMC,123457,V,,,,,,,270926,,,N");assert(!gps.freshFix());
    assert(gps.csv()==",,,,,,,");
    feed(gps,rmc);hostMillis+=3100;feed(gps,"GPTXT,01,01,02,still connected");assert(!gps.freshFix());
    assert(gps.csv()==",,,,,,,");
    feed(gps,"GNZDA,123457.500,27,09,2026,00,00");
    assert(gps.fix().utcEpoch==epoch+1&&gps.fix().utcFraction==500);
    auto previous=gps.fix().utcEpoch;
    feed(gps,"GNZDA,123457,31,02,2026,00,00");assert(gps.fix().utcEpoch==previous);
    hostMillis+=5100;gps.poll();assert(!gps.fix().connected&&!gps.fix().timeValid);
    feed(gps,"GPGSV,1,1,01,01,45,180,35");assert(gps.count()==1);
    gps.cycleRawFilter(); // GSV
    feed(gps,"GPGSV,1,1,01,01,45,180,35");feed(gps,"GPTXT,01,01,02,hello");
    assert(gps.rawCountLines()==1&&gps.raw(0).startsWith("$GPGSV,"));
    hostMillis+=16000;gps.poll();assert(gps.count()==0&&gps.fix().visible==0);
    HardwareSerial::input()=std::string(150,'x')+"$GNZDA,123457,27,09,2026,00,00*00\n";gps.poll();
    assert(gps.diagnostics().truncated==1&&!gps.fix().timeValid);
    feed(gps,"GNZDA,123458,27,09,2026,00,00");assert(gps.fix().timeValid);
    uint32_t e;uint16_t ms;
    assert(gnss::utcEpoch("235959.999",2024,2,29,e,ms)&&ms==999);
    assert(!gnss::utcEpoch("235960",2024,2,29,e,ms));
    assert(!gnss::utcEpoch("240000",2026,9,27,e,ms));
    assert(!gnss::utcEpoch("120000",2025,2,29,e,ms));
    assert(!gnss::utcEpoch("12x000",2026,9,27,e,ms));
    gnss::ClockSync clock;assert(clock.due(1000,true,1000));clock.applied(1000);
    assert(!clock.due(2000,true,2000));assert(clock.due(61000,true,61000));
    assert(!clock.due(62000,true,50000));assert(!clock.due(62000,false,62000));
    clock.applied(0xfffffff0);assert(clock.due(60000,true,60000));
    // Time and fix expiry are rollover-safe, even with uninterrupted TXT traffic.
    hostMillis=0xfffffff0;feed(gps,rmc);hostMillis=4000;gps.poll();assert(!gps.freshFix());
    std::cout<<"PASS: checksum gate, coordinate/UTC validation, RMC invalidation, ZDA, stale fix/time/sky, filters, truncation, boot sync and rollover\n";
}
