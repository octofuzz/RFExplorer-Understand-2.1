#include "analysis.h"
#include "signal_memory.h"
#include <cassert>
#include <cmath>
#include <iostream>
static void burst(rf::Envelope& e,uint32_t t,float peak=-80) {
    e.sample(t,peak,-102);e.sample(t+2,peak,-102);e.sample(t+4,peak,-102);e.sample(t+16,-102,-102);
}
int main() {
    // Reported case: near-floor samples and a recurring 1019 ms scheduling delay.
    rf::Envelope noise;
    for(uint32_t t=0;t<134*1019;t+=1019) noise.sample(t,-103+float((t/1019)%7),-102);
    assert(noise.count==0 && !noise.last.qualified && noise.maxGap==1019);
    assert(noise.gapCount==133 && noise.recurrence.count==0);
    rf::Envelope spikes;
    for(uint32_t t=0;t<10000;t+=100) {spikes.sample(t,-85,-102);spikes.sample(t+2,-102,-102);spikes.sample(t+14,-102,-102);}
    assert(spikes.count==0 && spikes.unresolved>0);
    rf::Envelope valid;burst(valid,100);assert(valid.count==1&&valid.last.duration==4&&valid.last.samples==3);
    auto first=valid.last;valid.sample(118,-103,-102);
    assert(valid.last.peak==first.peak && valid.last.quiet==first.quiet && valid.last.duration==4);
    // Repetition requires uninterrupted observation between valid events.
    for(uint32_t t=120;t<200;t+=2)valid.sample(t,-102,-102);burst(valid,200);
    for(uint32_t t=218;t<300;t+=2)valid.sample(t,-102,-102);burst(valid,300);
    assert(valid.recurrence.count==2 && valid.recurrence.mean()==100 && valid.recurrence.span()==0);
    // Perfectly regular legitimate bursts are retained, not declared local artefacts.
    assert(valid.count==3);
    valid.sample(1500,-102,-102);assert(valid.recurrence.count==0);
    rf::Envelope interrupted;interrupted.sample(0,-80,-102);interrupted.sample(1019,-80,-102);interrupted.sample(1031,-102,-102);
    assert(interrupted.count==0 && interrupted.unresolved==2);
    rf::Envelope rollover;burst(rollover,0xfffffffcu);assert(rollover.count==1&&rollover.last.duration==4);
    rf::Envelope carrier;for(unsigned t=0;t<2000;t+=2)carrier.sample(t,-70,-102);
    assert(carrier.active&&carrier.count==0);carrier.sample(2012,-102,-102);assert(carrier.count==1);
    rf::Envelope longCarrier;for(unsigned t=0;t<70000;t+=10)longCarrier.sample(t,-70,-102);
    longCarrier.sample(70002,-102,-102);assert(longCarrier.count==0&&longCarrier.unresolved==1);
    rf::Envelope invalid;invalid.sample(10,-80,-102);invalid.sample(12,NAN,-102);invalid.sample(14,-80,-102);invalid.sample(26,-102,-102);assert(invalid.count==0);
    field::SignalFingerprint a;a.frequency_khz=433050;a.peak_rssi=-103;a.noise_rssi=-102;
    assert(field::similarity(a,a)<=60 && !field::qualified(a));
    a.peak_rssi=-80;a.duration_ms=4;a.samples=3;a.sample_gap_ms=12;a.evidence_version=1;
    assert(field::qualified(a) && field::similarity(a,a)==100);
    a.duration_ms=0;assert(!field::qualified(a) && field::similarity(a,a)<=60);
    a.duration_ms=4;a.sample_gap_ms=1019;assert(!field::qualified(a));
    std::cout<<"PASS: reported near-floor case, isolated samples, valid bursts, carrier, gaps, recurrence, rollover and evidence gates\n";
}
