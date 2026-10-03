#include "satellite_rf.h"
#include <cassert>
uint32_t hostMillis=0;HostDisk disk;SDClass SD;
int main(){
    using namespace satrf;
    assert(allowed(437800)&&!allowed(145800)&&!allowed(464001));
    Window w;for(uint32_t t=0;t<=10000;t+=50)w.add(t,-100,-75);
    assert(w.valid()&&w.count==201&&w.above==0);auto r=w.record(issFrequency,0,-75);
    assert(r.mean10==-1000&&r.peak10==-1000);Record restored;assert(decode(encode(r),restored));
    assert(!decode(encode(r)+"0",restored));r.frequency=145800;assert(!decode(encode(r),restored));r.frequency=437800;
    r.located=true;r.latitude=63430000;r.longitude=10390000;r.utc=1790400000;assert(decode(encode(r),restored)&&restored.latitude==63430000);
    r.latitude=91000000;assert(!decode(encode(r),restored));r.latitude=0;r.longitude=0;assert(decode(encode(r),restored)&&restored.located);
    w.reset();for(uint32_t t=0;t<=10000;t+=500)w.add(t,-60,-75);assert(w.ready()&&!w.valid());
    w.reset();for(uint32_t t=0;t<=10000;t+=50)w.add(0xfffffff0u+t,-60,-75);assert(w.valid()&&w.above==201);
    w.reset();w.add(0,NAN,-75);w.add(1,-141,-75);assert(!w.count);w.add(2,-60,-75);w.reset();assert(!w.count&&!w.valid());
    disk.files["/rfexplorer"]="";Archive a;assert(a.begin()&&a.count()==0);
    assert(a.add(r));Archive b;assert(b.begin()&&b.count()==1&&b.get(0)->mean10==-1000);
    auto good=disk.files["/rfexplorer/satellite-rf-v28.csv"];
    disk.failWrite=true;r.utc++;assert(!a.add(r)&&a.writeFailed());assert(disk.files["/rfexplorer/satellite-rf-v28.csv"]==good);
    disk.failWrite=false;disk.failCommit=true;assert(!a.save());Archive recovery;assert(recovery.begin()&&recovery.count()==1);
    disk.failCommit=false;assert(a.save()&&!a.writeFailed());Archive c;assert(c.begin()&&c.count()==2);
    for(unsigned i=0;i<70;++i){r.utc=i;assert(a.add(r));}assert(a.count()==64&&a.get(0)->utc==69&&a.get(63)->utc==6);
    Archive d;assert(d.begin()&&d.count()==64&&d.get(0)->utc==69);
    disk.files["/rfexplorer/satellite-rf-v28.csv"]="broken\n"+std::string(encode(r).c_str())+"\npartial";Archive e;assert(e.begin()&&e.count()==1);
    disk.files.erase("/rfexplorer/satellite-rf-v28.csv");disk.files.erase("/rfexplorer/satellite-rf-v28.bak");
    auto legacy=std::string(encodeV27(r).c_str())+"\n";disk.files["/rfexplorer/satellite-rf-v27.csv"]=legacy;
    Archive imported;assert(imported.begin()&&imported.count()==1&&imported.get(0)->excessMs==UINT32_MAX);
    assert(disk.files["/rfexplorer/satellite-rf-v27.csv"]==legacy);
    disk.unavailable=true;Archive absent;assert(!absent.begin()&&!absent.add(r)&&absent.count()==0);
}
