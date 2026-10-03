#include "observations.h"
#include <cassert>
uint32_t hostMillis=0;HostDisk disk;SDClass SD;
int main(){
    disk.files["/rfexplorer"]="";observe::Store store;assert(store.begin());auto profile=store.profile(0);
    auto id=store.create("ISS attempt","UHF antenna","Garden",profile,1790400000);assert(id);
    satrf::Window w;for(unsigned t=0;t<=10000;t+=50)w.add(t,-95,-75);auto r=w.record(437800,0,-75);
    r.session=id;r.located=true;r.latitude=63430000;r.longitude=10390000;r.fixAge=100;r.hdop100=110;
    assert(store.append(r)&&r.sequence==1);assert(store.bookmark(r));assert(store.bookmark(r)&&store.bookmarks()==1);
    r.phase=1;r.mean10=-900;r.peak10=-880;assert(store.append(r));
    auto s=store.find(id);assert(s->total==2&&!store.comparison(*s,1));assert(std::abs(s->phases[1].mean()-s->phases[0].mean()-5)<0.01);
    satrf::Record page[4];assert(store.page(id,0,page,4)==2&&page[0].sequence==2&&page[1].sequence==1);
    observe::Store loaded;assert(loaded.begin()&&loaded.count()==1&&loaded.bookmarks()==1&&loaded.find(id)->total==2);
    assert(loaded.pin(0)->session==id&&loaded.pin(0)->sequence==1);
    auto changed=r;changed.frequency++;assert(observe::comparable(r,changed));changed=r;changed.gate++;assert(observe::comparable(r,changed));
    changed=r;changed.located=false;assert(observe::comparable(r,changed));changed=r;changed.longitude+=100000;assert(observe::comparable(r,changed));
    changed=r;changed.excessMs=UINT32_MAX;assert(observe::comparable(r,changed));changed=r;changed.hdop100=0;assert(observe::comparable(r,changed));
    r.phase=2;r.longitude+=100000;assert(store.append(r)&&store.comparison(*store.find(id),2));
    disk.failWrite=true;assert(!store.bookmark(r)&&store.bookmarks()==1);auto before=store.find(id)->total;assert(!store.append(r)&&store.find(id)->total==before);
    disk.failWrite=false;assert(store.append(r));
    for(unsigned i=0;i<80;++i)assert(store.append(r));
    auto total=store.find(id)->total;assert(total>64);observe::Store full;assert(full.begin()&&full.find(id)->total==total&&full.bookmarks()==1);
    assert(full.page(id,total-1,page,1)==1&&page[0].sequence==1);
    disk.files["/rfexplorer/observation-1.csv"]+="partial bad row";assert(store.append(r));
    observe::Store recovered;assert(recovered.begin()&&recovered.find(id)->total==total+1);
    assert(store.finish(id,1790401000));assert(!store.append(r));
    profile.frequency=145800;assert(!store.saveProfile(1,profile));profile.frequency=435250;profile.name="Custom test";assert(store.saveProfile(1,profile));
    assert(!store.saveProfile(0,profile));observe::Store reboot;assert(reboot.begin()&&reboot.profile(1).frequency==435250&&reboot.find(id)->closed);
    for(unsigned i=1;i<12;++i)assert(store.create("Session","UHF","",profile,0));assert(!store.create("Overflow","UHF","",profile,0));
    observe::Timeline timeline;timeline.add(0xfffffff0u,-100);timeline.add(100,-90);assert(timeline.count==1);timeline.add(250,-90);assert(timeline.count==2);timeline.add(251,NAN,2);assert(timeline.count==3&&timeline.get(2).event==2);
    for(unsigned i=0;i<200;++i)timeline.add(1000+i*250,-90);assert(timeline.count==128);
    satrf::Record legacy;assert(satrf::decode(satrf::encodeV27(r),legacy)&&legacy.excessMs==UINT32_MAX&&!legacy.session);
}
