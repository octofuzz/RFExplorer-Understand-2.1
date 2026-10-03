#include "satellites.h"
#include <cassert>
#include <string>
using namespace gnss;
void sentence(SatelliteStore& store,std::string body,uint32_t now,bool usage=false) {
    char buffer[160];strcpy(buffer,body.c_str());char* fields[24]{};unsigned n=0;
    for(char* p=buffer;p && n<24;) {fields[n++]=p;auto comma=strchr(p,',');if(!comma)break;*comma=0;p=comma+1;}
    if(usage)store.gsa(fields,n,now);else store.gsv(fields,n,now);
}
int main() {
    assert(unsigned(System::GPS)==0);
    assert(talkerSystem("$GPGSV")==System::GPS);
    assert(talkerSystem("$GLGSV")==System::GLONASS);
    assert(talkerSystem("$GAGSV")==System::Galileo);
    assert(talkerSystem("$GBGSV")==System::BeiDou);
    assert(systemFor(System::Unknown,65)==System::GLONASS);
    assert(systemFor(System::Unknown,201)==System::BeiDou);
    assert(systemFor(System::Unknown,301)==System::Galileo);
    assert(systemFor(System::GPS,40)==System::SBAS);
    assert(usageId(System::GLONASS,65)==1);
    assert(usageId(System::Galileo,301)==1);
    SatelliteStore store;
    sentence(store,"$GPGSV,1,1,01,01,45,180,35",1000);assert(store.count()==1);
    sentence(store,"$GPGSV,2,1,05,02,45,180,35,03,45,180,35,04,45,180,35,05,45,180,35",2000);
    assert(store.count()==1&&store.satellites()[0].prn==1); // Incomplete cycle preserves snapshot.
    sentence(store,"$GPGSV,2,2,05,06,45,180,36",2100);assert(store.count()==5);
    sentence(store,"$GPGSV,1,1,02,07,45,180,35",3000);assert(store.count()==5); // Declared two, only one.
    sentence(store,"$GPGSV,1,1,01,02,45,180,20",4000);assert(store.count()==1&&store.satellites()[0].snr==20);
    store.refresh(20001);assert(store.count()==0);
    sentence(store,"$GPGSV,1,1,01,01,45,180,35",0xfffffff0);
    store.refresh(100);assert(store.count()==1);store.refresh(16000);assert(store.count()==0);
    store.reset();assert(store.count()==0);

    // Conservative detection: hardware-supported IDs, signals and fresh positive C/N0.
    const char* reports[]={"$GPGSV,1,1,01,01,45,180,35,1","$GLGSV,1,1,01,65,45,180,35,1","$GAGSV,1,1,01,01,45,180,35,7","$GBGSV,1,1,01,01,45,180,35,3","$GQGSV,1,1,01,01,45,180,35,1","$GPGSV,1,1,01,33,45,180,35,1"};
    for(unsigned i=0;i<6;++i){store.reset();sentence(store,reports[i],100);assert(store.count()==1);auto sat=store.satellites()[0];assert(unsigned(sat.system)==i&&detected(sat,3100)&&!detected(sat,3101));assert(skyPosition(sat));sat.snr=0;assert(!detected(sat,100));sat.snr=-1;assert(!detected(sat,100));sat.snr=35;sat.lastSeen=0xfffffff0;assert(detected(sat,100));sat.signalId=15;assert(!detected(sat,100));}
    store.reset();sentence(store,"$GNGSV,1,1,01,01,45,180,35",100);assert(!detected(store.satellites()[0],100));
    store.reset();sentence(store,"$GPGSV,1,1,01,01,,,35",100);assert(detected(store.satellites()[0],100)&&!skyPosition(store.satellites()[0]));
    sentence(store,"$GPGSV,1,1,02,02,45,180,40,02,45,180,40",200);assert(store.count()==1&&store.satellites()[0].prn==1);
    store.reset();sentence(store,"$GPGSV,1,1,01,01,45,180,50,1",100);sentence(store,"$GPGSV,1,1,01,01,45,180,20",3200);assert(store.satellites()[0].snr==20&&detected(store.satellites()[0],3200));
    store.reset();sentence(store,"$GPGSV,1,1,01,01,45,180,35",100);
    sentence(store,"$GNGSA,A,3,01,,,,,,,,,,,,1.0,1.0,1.0,6",101,true);assert(!store.satellites()[0].used);
    sentence(store,"$GPGSV,1,1,01,01,45,180,0",200);assert(!detected(store.satellites()[0],200));
    assert(!supportedId(System::SBAS,56)&&!supportedId(System::GLONASS,89)&&!supportedId(System::QZSS,8));
    return 0;
}
