#include "expedition.h"
#include <cassert>
#include <iostream>
uint32_t hostMillis=1000;HostDisk disk;SDClass SD;
static void feed(gnss::Receiver& gps,const std::string& body) {
    unsigned sum=0;for(char c:body)sum^=uint8_t(c);char suffix[8];snprintf(suffix,sizeof(suffix),"*%02X\r\n",sum);
    HardwareSerial::input()+="$"+body+suffix;gps.poll();
}
static void position(gnss::Receiver& gps,const char* latitude) {
    feed(gps,std::string("GNRMC,123456,A,")+latitude+",N,01023.7000,E,450,42,270926,,,A");
    feed(gps,std::string("GNGGA,123456,")+latitude+",N,01023.7000,E,1,08,0.9,10000,M,40,M,,");
}
int main() {
    disk.files["/rfexplorer"]="";
    gnss::Receiver gps;gps.begin();expedition::JourneyRecorder r;assert(r.begin(1));
    assert(r.start("Journey","Flight",5000));auto path=std::string(r.path().c_str());
    position(gps,"6325.8300");r.poll(gps);assert(r.points()==1);
    hostMillis+=5000;position(gps,"6326.4600");r.poll(gps);
    assert(r.distanceM()>1100 && r.distanceM()<1200); // ~840 km/h is legitimate travel.
    hostMillis+=3100;gps.poll();r.poll(gps);assert(r.gaps()==1); // Detect loss between samples.
    assert(r.bookmark(gps,"No fix, \"unknown\""));
    assert(disk.files[path].find("BOOKMARK,field_note")!=std::string::npos);
    hostMillis+=1900;position(gps,"6425.8300");r.poll(gps);
    assert(r.distanceM()<1200); // Never join across the missing-fix segment.
    assert(r.finish());assert(disk.files[path].find("END,session_end")!=std::string::npos);
    assert(disk.files[path].find("distance_m=")!=std::string::npos);
    unsigned columns=1;bool quoted=false;
    for(char c:disk.files[path]) {
        if(c=='"')quoted=!quoted;
        if(c==','&&!quoted)++columns;
        if(c=='\n'&&!quoted){assert(columns==15);columns=1;}
    }
    assert(r.trailCount==3 && r.trailPoint(2).start);
    assert(r.recoverInterrupted(path.c_str(),"Journey","Flight"));
    assert(!r.recoverInterrupted("/rfexplorer/expeditions/../secret.csv","x","y"));
    assert(r.start("Journey","Next"));disk.unavailable=true;r.poll(gps);
    assert(!r.active() && !r.ready);disk.unavailable=false;
    assert(r.begin(2));disk.failWrite=true;assert(!r.start("Journey","Failed"));assert(!r.active());
    std::cout<<"PASS: route, aircraft distance, gaps, bookmarks, recovery and SD failures\n";
}
