#include "gnss_logbook.h"
#include <cassert>
#include <iostream>
uint32_t hostMillis=1000;HostDisk disk;SDClass SD;
int main() {
    disk.files["/rfexplorer"]="";
    const char* legacy="/rfexplorer/satellite-log.csv";
    const char* path="/rfexplorer/satellite-log-v26.csv";
    disk.files[legacy]="system,prn,first_seen_epoch,last_seen_epoch,best_snr_db,max_elevation_deg,sightings\nGPS,1,0,0,30,40,3\n";
    auto original=disk.files[legacy];gnss::SatelliteLogbook book;assert(book.begin()&&book.count()==1);
    gnss::Satellite sat;sat.system=gnss::System::GPS;sat.prn=1;sat.snr=35;sat.elevation=45;sat.lastSeen=hostMillis;
    gnss::System system;uint16_t prn;
    assert(book.update(&sat,1,system,prn,false,63.4,10.4));assert(!book.get(0)->located);
    assert(!book.update(&sat,1,system,prn,true,63.4,10.4));assert(book.get(0)->sightings==4);
    hostMillis=2000;sat.lastSeen=hostMillis;
    assert(book.update(&sat,1,system,prn,true,63.4305,10.395));
    assert(book.get(0)->located&&book.get(0)->firstLatitude==63430500&&book.get(0)->firstLongitude==10395000);
    hostMillis=3000;sat.lastSeen=hostMillis;book.update(&sat,1,system,prn,true,0,0);
    assert(book.get(0)->lastLatitude==0&&book.get(0)->firstLatitude==63430500);
    hostMillis=4000;sat.lastSeen=hostMillis;book.update(&sat,1,system,prn,false,70,20);
    assert(book.get(0)->lastLatitude==0);assert(book.save(true));assert(disk.files[legacy]==original);
    gnss::SatelliteLogbook reboot;assert(reboot.begin());
    assert(reboot.get(0)->located&&reboot.get(0)->firstLatitude==63430500&&reboot.get(0)->lastLatitude==0);
    assert(reboot.get(0)->firstSeen==0); // Never invent first-seen UTC during migration.
    auto good=disk.files[path];hostMillis=35000;sat.lastSeen=hostMillis;book.update(&sat,1,system,prn,true,-33.8,151.2);
    disk.failWrite=true;assert(!book.save(true)&&book.writeFailed()&&disk.files[path]==good);
    disk.failWrite=false;disk.failCommit=true;hostMillis+=30000;assert(!book.save(true));
    gnss::SatelliteLogbook recovery;assert(recovery.begin()&&recovery.count()==1);
    assert(recovery.get(0)->lastLatitude==0);disk.failCommit=false;assert(recovery.save(true));
    assert(disk.files[legacy]==original);
    hostMillis+=16000;assert(!recovery.update(&sat,1,system,prn,true,1,1));
    sat.lastSeen=hostMillis;assert(recovery.update(&sat,1,system,prn,true,91,181));assert(recovery.get(0)->lastLatitude==0);

    assert(reboot.get(0)->confirmedReports==4&&reboot.get(0)->lastCn0==35&&reboot.get(0)->confirmedGeotag);
    auto before=recovery.get(0)->confirmedReports;sat.snr=0;sat.lastSeen=++hostMillis;assert(!recovery.update(&sat,1,system,prn));assert(recovery.get(0)->confirmedReports==before);
    sat.snr=-1;assert(!recovery.update(&sat,1,system,prn));sat.snr=35;sat.signalId=15;assert(!recovery.update(&sat,1,system,prn));
    disk.files.clear();disk.files["/rfexplorer"]="";
    const char* previous="/rfexplorer/satellite-log-v21.csv";
    disk.files[previous]="GPS,1,0,0,35,45,999,1,bad,0,0,0,0,0\n";
    auto old=disk.files[previous];gnss::SatelliteLogbook migrated;assert(migrated.begin()&&migrated.count()==1);assert(!migrated.get(0)->confirmedReports&&!migrated.get(0)->located);
    sat.signalId=1;sat.lastSeen=hostMillis;assert(migrated.update(&sat,1,system,prn));assert(migrated.get(0)->confirmedReports==1&&!migrated.get(0)->confirmedGeotag);assert(system==gnss::System::GPS);assert(migrated.save(true)&&disk.files[previous]==old);
    std::cout<<"PASS: legacy migration, valid/invalid/zero geotags, report deduplication, persistence, SD failure and recovery\n";
}
