#include "navigation.h"
#include "battery_gauge.h"
#include <cassert>
#include <set>
int main() {
    ui::Navigation n;
    std::set<unsigned> actions;
    for(unsigned g=0;g<7;++g) {
        n.open(g);
        for(unsigned i=0;i<n.count();++i) actions.insert(n.action(i));
        n.remember(n.count()-1);
        auto saved=n.selection();n.back();assert(n.selection()==g);
        n.open(g);assert(n.selection()==saved);
    }
    assert(actions.size()==35); // Every old destination plus diagnostics.
    ui::BatteryGauge gauge;
    gauge.update(80,0);
    for(unsigned t=1000;t<=60000;t+=1000) gauge.update(t%2000?79:81,t);
    assert(gauge.value()==80);
    for(unsigned t=61000;t<=100000;t+=1000) gauge.update(70,t);
    assert(gauge.value()>=70 && gauge.value()<=72);
    for(unsigned t=101000;t<=103000;t+=1000) gauge.update(4,t);
    assert(gauge.value()==4);
    for(unsigned t=104000;t<=106000;t+=1000) gauge.update(-1,t);
    assert(gauge.value()==-1);
    gauge.update(50,0xfffff000u);gauge.update(50,2000);assert(gauge.value()==50);
}
