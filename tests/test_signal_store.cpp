#include "signal_store.h"
#include "investigation.h"
#include "understand.h"
#include <SD.h>
#include <cassert>
#include <iostream>
uint32_t hostMillis=100;
HostDisk disk; SDClass SD;
const char* legacy="/rfexplorer/signal-memory.csv";
const char* modern="/rfexplorer/signal-memory-v20.csv";
const char* backup="/rfexplorer/signal-memory-v20.bak";
int main() {
    const std::string original="frequency_khz,peak_rssi,noise_rssi,duration_ms,repeat_ms,bandwidth_khz,first_seen,last_seen,sightings,label\n433920,-60,-105,25,0,0,3000000000,4000000000,8,Old remote\n";
    disk.files[legacy]=original;
    field::SignalMemory m; assert(m.begin() && m.count()==1 && m.pending());
    assert(m.entry(0).firstSeen==3000000000u && m.entry(0).lastSeen==4000000000u);
    assert(m.entry(0).firstUTC==0 && m.entry(0).encounterCount==0);
    assert(m.entry(0).label=="Old remote");
    assert(!m.flushIfDue(200,30000)); hostMillis=30100;
    assert(m.flushIfDue(hostMillis,30000)); assert(disk.files[legacy]==original);
    field::SignalFingerprint fp=m.entry(0).fingerprint; fp.peak_rssi=-55;
    unsigned writes=disk.writes;
    assert(m.remember(fp,"",31000,1790400000)==0 && disk.writes==writes);
    assert(m.entry(0).sightings==9 && m.entry(0).strongest==-55);
    assert(m.entry(0).firstUTC==0 && m.entry(0).lastUTC==1790400000);
    for(int i=0;i<15;++i) { fp.peak_rssi=-60+i; m.remember(fp,"",32000+i,1790400001+i); }
    assert(m.entry(0).encounterCount==12 && m.entry(0).encounterRSSI[0]==-57 && m.entry(0).encounterRSSI[11]==-46);
    assert(m.rename(0,"Garden,remote\nlabel")); assert(m.annotate(0,"near,gate")); assert(m.classify(0,5));
    field::SignalMemory reboot; assert(reboot.begin());
    assert(reboot.entry(0).label=="Garden remote label" && reboot.entry(0).notes=="near gate");
    assert(reboot.entry(0).category==5 && reboot.entry(0).encounterCount==12);
    assert(reboot.entry(0).encounterRSSI[11]==-46 && reboot.entry(0).sightings==24);
    assert(reboot.entry(0).encounterUTC[11]==1790400015);
    assert(reboot.entry(0).encounterDuration[11]==25);
    assert(reboot.captureBaseline(0,1790400100));
    field::SignalMemory baseline;assert(baseline.begin());
    assert(baseline.entry(0).baselineCount==12 && baseline.entry(0).baselineUTC==1790400100);
    assert(baseline.entry(0).baselineRSSI==-51);
    assert(!baseline.captureBaseline(47,1));
    // Failed data write cannot destroy the last complete file.
    auto good=disk.files[modern]; disk.failWrite=true;
    assert(!reboot.rename(0,"Pending") && reboot.pending() && reboot.writeFailed());
    assert(disk.files[modern]==good); disk.failWrite=false;
    // Interrupted replacement: .bak remains recoverable on reboot.
    disk.failCommit=true; assert(!reboot.save()); assert(!disk.files.count(modern)&&disk.files.count(backup));
    field::SignalMemory recovery; assert(recovery.begin() && recovery.count()==1 && recovery.pending());
    assert(recovery.entry(0).label=="Garden remote label");
    disk.failCommit=false; assert(recovery.save()); assert(disk.files[legacy]==original);
    // Millisecond rollover: delayed flush remains bounded and completes.
    hostMillis=0xfffffff0u; assert(recovery.rename(0,"Wrap")); fp.peak_rssi=-60;
    recovery.remember(fp,"",hostMillis); assert(!recovery.flushIfDue(10,30000));
    hostMillis=30000; assert(recovery.flushIfDue(hostMillis,30000));
    // Unknown frequency rejected, capacity never silently evicts a dossier.
    fp.frequency_khz=118000; assert(recovery.remember(fp,"",0)==-1);
    for(unsigned i=1;i<48;++i) { fp.frequency_khz=300000+i*700; assert(recovery.remember(fp,"",i)>=0); }
    fp.frequency_khz=460000; assert(recovery.remember(fp,"",1)==-1 && recovery.count()==48);
    uint8_t score=0; fp.frequency_khz=433920; assert(recovery.bestMatch(fp,score)>=0 && score>=75);
    assert(field::bandFor(315000)==0 && field::bandFor(433920)==1 && field::bandFor(868300)==2 && field::bandFor(915000)==3);
    assert(field::bandFor(1090000)==-1);
    disk.files.clear();disk.files[legacy]=original.substr(0,original.size()-1)+",side gate\n";
    field::SignalMemory commaLabel;assert(commaLabel.begin());
    assert(commaLabel.entry(0).label=="Old remote side gate");
    // Candidate grouping is anchored: no transitive chain across a wide span.
    disk.files.clear(); field::SignalMemory familiesMemory;familiesMemory.begin();
    fp.duration_ms=100;fp.peak_rssi=-60;
    fp.frequency_khz=434300;familiesMemory.remember(fp,"C",1);
    fp.frequency_khz=433900;familiesMemory.remember(fp,"A",2);
    fp.frequency_khz=434100;familiesMemory.remember(fp,"B",3);
    field::Families groups;groups.build(familiesMemory);
    assert(groups.anchor[1]==1 && groups.anchor[2]==1 && groups.anchor[0]==0);
    assert(groups.members(1)==2);
    // v1.9 data remains untouched and is migrated with unknown historical times.
    disk.files.clear();
    disk.files["/rfexplorer/signal-memory-v19.csv"]="433920,-60,-100,25,0,0,10,20,8,Imported,0,1790400000,-55,4,note,2,-60|-55\n";
    field::SignalMemory v19;assert(v19.begin()&&v19.count()==1&&v19.pending());
    assert(v19.entry(0).encounterCount==2&&v19.entry(0).encounterUTC[0]==0);
    assert(v19.save()&&disk.files.count("/rfexplorer/signal-memory-v19.csv"));
    std::cout<<"PASS: migration, persistence, history, write failure, interrupted commit, rollover, capacity, RF band selection\n";
}
