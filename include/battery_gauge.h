#pragma once
#include <stdint.h>
namespace ui {
// Sample once per second. Six-second low-pass + two-point hysteresis prevents
// voltage-derived percentage oscillation from redrawing the header every frame.
class BatteryGauge {
    uint32_t sampled=0, published=0;
    bool started=false, sampledOnce=false;
    float average=0;
    int shown=-1, criticalSamples=0, invalidSamples=0;
public:
    bool due(uint32_t now) const { return !sampledOnce || uint32_t(now-sampled)>=1000; }
    int value() const { return shown; }
    void update(int raw,uint32_t now) {
        if(!due(now)) return;
        sampled=now;sampledOnce=true;
        if(raw<0 || raw>100) { if(++invalidSamples>=3) {shown=-1;started=false;} return; }
        invalidSamples=0;
        if(!started) { started=true;average=raw;shown=raw;published=now;return; }
        average+=(raw-average)/6.0f;
        criticalSamples=raw<=5?criticalSamples+1:0;
        if(criticalSamples>=3) {shown=raw;average=raw;published=now;return;}
        int next=int(average+0.5f),delta=next-shown;
        if(uint32_t(now-published)>=10000 && (delta>=2 || delta<=-2)) {shown=next;published=now;}
    }
};
}
