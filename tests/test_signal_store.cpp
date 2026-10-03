#include "review_queue.h"
#include "signal_store.h"
#include "investigation.h"
#include "understand.h"
#include <SD.h>
#include <cassert>
#include <iostream>
uint32_t hostMillis=100;
HostDisk disk; SDClass SD;
const char* legacy="/rfexplorer/signal-memory.csv";
const char* modern="/rfexplorer/signal-memory-v25.csv";
const char* backup="/rfexplorer/signal-memory-v25.bak";
int main() {
    const std::string original="frequency_khz,peak_rssi,noise_rssi,duration_ms,repeat_ms,bandwidth_khz,first_seen,last_seen,sightings,label\n433920,-60,-105,25,0,0,3000000000,4000000000,8,Old remote\n";
    disk.files[legacy]=original;
    field::SignalMemory m; assert(m.begin() && m.count()==1 && m.pending());
    assert(m.entry(0).firstSeen==3000000000u && m.entry(0).lastSeen==4000000000u);
    assert(m.entry(0).firstUTC==0 && m.entry(0).encounterCount==0);
    assert(m.entry(0).label=="Old remote");
    assert(!m.flushIfDue(200,30000)); hostMillis=30100;
    assert(m.flushIfDue(hostMillis,30000)); assert(disk.files[legacy]==original);
    field::SignalFingerprint fp=m.entry(0).fingerprint;
    assert(!field::qualified(fp));assert(m.remember(fp,"",0)==-2);
    auto old=fp;old.peak_rssi=-100;old.noise_rssi=-101;old.duration_ms=0;
    assert(m.remember(old,"",0)==-2 && m.count()==1);
    uint8_t legacyScore=0;m.bestMatch(fp,legacyScore);assert(legacyScore<=60);
    // Establish a new qualified fixture for durability/history regressions.
    m=field::SignalMemory{};fp.samples=20;fp.sample_gap_ms=10;fp.evidence_version=1;
    for(unsigned i=0;i<8;++i)assert(m.remember(fp,"Old remote",3000000000u+i)==0);
    fp.peak_rssi=-55;
    unsigned writes=disk.writes;
    assert(m.remember(fp,"",31000,1790400000)==0 && disk.writes==writes);
    assert(m.entry(0).sightings==9 && m.entry(0).strongest==-55);
    assert(m.entry(0).firstUTC==0 && m.entry(0).lastUTC==1790400000);
    for(int i=0;i<15;++i) { fp.peak_rssi=-60+i; m.remember(fp,"",32000+i,1790400001+i); }
    assert(m.entry(0).encounterCount==12 && m.entry(0).encounterRSSI[0]==-57 && m.entry(0).encounterRSSI[11]==-46);
    assert(m.rename(0,"Garden,remote\nlabel")); assert(m.annotate(0,"near,gate")); assert(m.classify(0,5));
    field::SignalMemory reboot; assert(reboot.begin());
    assert(reboot.entry(0).label=="Garden remote label" && reboot.entry(0).notes=="near gate");
    assert(field::qualified(reboot.entry(0).fingerprint));
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
    disk.files.clear();disk.files["/rfexplorer/signal-memory-v20.csv"]="433050,-100,-101,0,0,0,10,20,134,Noise,0,0,-98,0,,1,-100,-100,0,0,0,0\n";
    auto untouched=disk.files["/rfexplorer/signal-memory-v20.csv"];
    field::SignalMemory v20;assert(v20.begin()&&v20.count()==1&&v20.pending());
    assert(!field::qualified(v20.entry(0).fingerprint));assert(v20.save());
    assert(disk.files["/rfexplorer/signal-memory-v20.csv"]==untouched);
    disk.files[modern]="433050,-70,-102,10,0,0,10,20,3,Corrupt,0,0,-70,0,,1,-70,-100,0,0,0,10,-1,2,1\n";
    field::SignalMemory corrupt;assert(corrupt.begin()&&corrupt.count()==1);
    assert(!field::qualified(corrupt.entry(0).fingerprint));
    // Review eligibility survives SD reload; legacy evidence is never promoted.
    assert(!field::reviewReason(corrupt.entry(0)));
    assert(corrupt.setFlag(0,true));
    field::SignalMemory reviewed;assert(reviewed.begin());
    assert(field::reviewReason(reviewed.entry(0)));
    assert(!field::qualified(reviewed.entry(0).fingerprint));
    assert(reviewed.setFlag(0,false));
    field::SignalMemory unmarked;assert(unmarked.begin());
    assert(!field::reviewReason(unmarked.entry(0)));
    field::MemoryEntry candidate;candidate.fingerprint=fp;candidate.category=0;
    assert(field::qualified(fp));assert(field::reviewReason(candidate));
    candidate.category=4;assert(!field::reviewReason(candidate));
    candidate.baselineCount=6;candidate.baselineRSSI=-70;candidate.encounterCount=1;
    candidate.encounterRSSI[0]=-59;assert(!field::reviewReason(candidate));
    candidate.encounterRSSI[0]=-58;assert(field::reviewReason(candidate));
    candidate.fingerprint.evidence_version=0;assert(!field::reviewReason(candidate));
    disk.files.clear();field::SignalMemory rich;assert(rich.begin());
    field::EncounterContext c;c.located=true;c.latitudeE6=0;c.longitudeE6=-3123456;c.fixAgeMs=120;c.quietAgeMs=200;c.hdop100=95;c.rxBandwidth10=580;c.coverage=100;
    fp.frequency_khz=433920;fp.evidence_version=1;fp.duration_ms=100;fp.samples=30;fp.sample_gap_ms=4;
    assert(rich.remember(fp,"Sensor",0,1790400010,c)==0);assert(rich.classify(0,4));assert(rich.setFlag(0,true));
    field::SignalMemory loaded;assert(loaded.begin());assert(loaded.entry(0).flagged&&loaded.entry(0).category==4);
    auto saved=loaded.entry(0).context[0];assert(saved.located&&saved.latitudeE6==0&&saved.longitudeE6==-3123456&&saved.hdop100==95&&saved.rxBandwidth10==580);
    assert(loaded.setReviewed(0,true));assert(field::reviewReason(loaded.entry(0))); // Explicit flag wins.
    assert(loaded.setFlag(0,false));assert(!field::reviewReason(loaded.entry(0)));
    fp.evidence_version=2;fp.duration_ms=5000;fp.samples=2000;fp.sample_gap_ms=20;
    assert(loaded.remember(fp,"Activity",5000,1790400020,c)==1);assert(!field::qualified(loaded.entry(1).fingerprint));
    assert(loaded.remember(fp,"",10000,1790400025,c)==1 && loaded.entry(1).sightings==2);
    assert(loaded.save());field::SignalMemory activity;assert(activity.begin());assert(field::activityWindow(activity.entry(1).fingerprint));
    assert(field::reviewReason(activity.entry(1)));c.coverage=79;assert(activity.remember(fp,"",15000,0,c)==-2);
    // Context ring and missing fixes: a later observation must not reuse a location.
    fp.evidence_version=1;fp.duration_ms=100;fp.samples=30;fp.sample_gap_ms=4;c={};
    for(unsigned i=0;i<14;++i)activity.remember(fp,"",20000+i,1790400100+i,c);
    assert(activity.save());field::SignalMemory missing;assert(missing.begin());assert(!missing.entry(0).context[11].located);
    disk.files.clear();disk.files["/rfexplorer/signal-memory-v24.csv"]="433920,-60,-100,100,0,0,1,2,2,Old,0,0,-60,2,,1,-60,-120,0,0,0,100,30,4,1\n";
    auto v24original=disk.files["/rfexplorer/signal-memory-v24.csv"];
    field::SignalMemory migration;assert(migration.begin()&&migration.count()==1&&migration.pending());
    assert(migration.entry(0).flagged&&migration.entry(0).category==2&&!migration.entry(0).context[0].located);
    assert(migration.save());assert(disk.files["/rfexplorer/signal-memory-v24.csv"]==v24original);
    std::cout<<"PASS: migration, persistence, history, write failure, interrupted commit, rollover, capacity, RF band selection\n";
}
