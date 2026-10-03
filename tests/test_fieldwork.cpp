#include "fieldwork.h"
#include <cassert>
#include <limits>
int main(){
    rf::QuietReference q;
    for(uint32_t t=0;t<=1200;t+=2)q.sample(t,-100,false);
    assert(q.ready&&q.level==-100&&q.fresh(1200));
    const auto update=q.updated;
    for(uint32_t t=1202;t<=13000;t+=2)q.sample(t,-60,true);
    assert(q.updated==update&&q.level==-100&&!q.fresh(13000));
    for(uint32_t t=13002;t<=14500;t+=2)q.sample(t,-104,false);
    assert(q.fresh(14500)&&q.level<=-103);
    q={};for(uint32_t t=0;t<20;++t)q.sample(t*100,-100,false);assert(!q.ready);
    q={};for(uint32_t i=0;i<=600;++i)q.sample(0xfffffff0u+i*2,-100,false);assert(q.fresh(0xfffffff0u+1200));
    rf::ActivityWindow a;unsigned windows=0;
    for(uint32_t t=0;t<=15000;t+=2)if(a.sample(t,-60,-100,true)){++windows;assert(a.coverage==100&&a.duration==5000);}
    assert(windows==3);assert(!a.sample(15002,-100,-100,true)&&!a.active);
    a={};windows=0;
    for(uint32_t t=0;t<=10000;t+=2)windows+=a.sample(t,-95,-100,true);
    assert(!windows);a={};
    for(uint32_t t=0;t<4000;t+=2)a.sample(t,-60,-100,true);
    a.interrupt();assert(!a.sample(5000,-60,-100,true));
    a={};windows=0;for(uint32_t t=0;t<=10000;t+=100)windows+=a.sample(t,-60,-100,true);assert(!windows);
    a={};for(uint32_t t=0;t<4998;t+=2)a.sample(t,-60,-100,true);assert(!a.sample(5400,-60,-100,true));
    a={};windows=0;for(uint32_t t=0;t<=5000;t+=2)windows+=a.sample(0xfffffff0u+t,-60,-100,true);assert(windows==1);
    a.sample(6000,std::numeric_limits<float>::quiet_NaN(),-100,true);assert(!a.active);
    a={};assert(!a.sample(0,-60,-100,true));unsigned accepted=0;
    for(uint32_t t=2;t<=15000;t+=2)accepted+=a.sample(t,-93,-100,true);
    assert(accepted==1); // First window contains the real start; later 7 dB windows do not qualify.
    rf::SamplingQuality s;s.sample(0xfffffff0u);s.sample(4);s.sample(30);assert(s.maxGap==26&&s.late==1&&s.mean()==23);
    s.interrupt();s.sample(1000);assert(s.intervals==2);
}
