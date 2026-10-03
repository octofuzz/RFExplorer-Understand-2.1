#pragma once
#include <Arduino.h>
#include "analysis.h"
namespace rf {
struct TraceSample {uint32_t at=0,frequency=0;float rssi=0,quiet=0;};
class TraceRecorder {
public:
    static constexpr unsigned capacity=1024;
    TraceSample samples[capacity]{};
    unsigned count=0;bool active=false;String error,path;
    void start() {count=0;active=true;error="";path="";}
    void stop() {active=false;}
    void add(uint32_t at,uint32_t frequency,float rssi,float quiet) {
        if(!active)return;
        samples[count++]={at,frequency,rssi,quiet};
        if(count==capacity)active=false;
    }
    bool save();
};
}
