#include "signal_memory.h"

int main() {
    field::SignalFingerprint a;
    a.frequency_khz=433920;
    a.peak_rssi=-52;
    a.noise_rssi=-102;
    a.duration_ms=340;
    a.bandwidth_khz=25;

    field::SignalFingerprint b=a;
    b.frequency_khz=433925;
    b.peak_rssi=-55;
    b.duration_ms=360;

    field::SignalFingerprint c=a;
    c.frequency_khz=868300;

    if(field::similarity(a,b)<75) return 1;
    if(!field::likelySameSignal(a,b)) return 2;
    if(field::similarity(a,c)!=0) return 3;

    return 0;
}
