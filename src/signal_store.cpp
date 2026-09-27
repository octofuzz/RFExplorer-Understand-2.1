#include "signal_store.h"
#include "investigation.h"
#include <SD.h>

namespace field {
static const char* legacyPath="/rfexplorer/signal-memory.csv";
static const char* memoryPath="/rfexplorer/signal-memory-v20.csv";
static const char* tempPath="/rfexplorer/signal-memory-v20.tmp";
static const char* backupPath="/rfexplorer/signal-memory-v20.bak";
static const char* previousPath="/rfexplorer/signal-memory-v19.csv";
static String clean(String s) {
    s.replace(","," "); s.replace("\n"," "); s.replace("\r"," ");
    return s.substring(0,24);
}
bool SignalMemory::begin() { return load(); }
int SignalMemory::bestMatch(const SignalFingerprint& fp,uint8_t& score) const {
    int best=-1; score=0;
    for(uint8_t i=0;i<size_;++i) {
        uint8_t s=similarity(fp,entries[i].fingerprint);
        if(s>score) { score=s; best=i; }
    }
    return best;
}
int SignalMemory::remember(const SignalFingerprint& fp,const String& label,uint32_t now,uint32_t utc) {
    if(bandFor(fp.frequency_khz)<0) return -1;
    uint8_t score=0; int match=bestMatch(fp,score);
    bool fresh=match<0 || score<75;
    if(fresh && size_>=capacity) return -1;
    int index=fresh?size_++:match;
    MemoryEntry& e=entries[index];
    if(fresh) {
        e=MemoryEntry{}; e.fingerprint=fp; e.label=clean(label);
        e.firstSeen=now; e.firstUTC=utc; e.strongest=fp.peak_rssi;
    }
    // Keep the original matching fingerprint stable; strongest/history are separate.
    e.lastSeen=now; e.lastUTC=utc;
    if(e.sightings!=UINT32_MAX) ++e.sightings;
    if(fp.peak_rssi>e.strongest) e.strongest=fp.peak_rssi;
    if(e.encounterCount==12) {
        for(unsigned i=1;i<12;++i) {
            e.encounterRSSI[i-1]=e.encounterRSSI[i];
            e.encounterUTC[i-1]=e.encounterUTC[i];
            e.encounterDuration[i-1]=e.encounterDuration[i];
        }
    } else ++e.encounterCount;
    e.encounterRSSI[e.encounterCount-1]=fp.peak_rssi;
    e.encounterUTC[e.encounterCount-1]=utc;
    e.encounterDuration[e.encounterCount-1]=fp.duration_ms;
    dirty_=true;
    if(fresh) save();
    return index;
}
bool SignalMemory::rename(uint8_t index,const String& label) {
    if(index>=size_) return false;
    entries[index].label=clean(label); dirty_=true; return save();
}
bool SignalMemory::annotate(uint8_t index,const String& notes) {
    if(index>=size_) return false;
    entries[index].notes=clean(notes); dirty_=true; return save();
}
bool SignalMemory::classify(uint8_t index,uint8_t category) {
    if(index>=size_ || category>=6) return false;
    entries[index].category=category; dirty_=true; return save();
}
bool SignalMemory::captureBaseline(uint8_t index,uint32_t utc) {
    if(index>=size_ || entries[index].encounterCount<6) return false;
    auto& e=entries[index]; int sum=0;
    for(unsigned i=0;i<e.encounterCount;++i) sum+=e.encounterRSSI[i];
    e.baselineRSSI=sum/e.encounterCount; e.baselineCount=e.encounterCount; e.baselineUTC=utc;
    dirty_=true; return save();
}
bool SignalMemory::load() {
    size_=0; dirty_=writeFailed_=false; lastSaveAttempt_=millis();
    // Recover interrupted replacement before considering the untouched v1.8 file.
    bool modern=SD.exists(memoryPath)||SD.exists(backupPath)||SD.exists(previousPath);
    const char* path=SD.exists(memoryPath)?memoryPath:SD.exists(backupPath)?backupPath:SD.exists(previousPath)?previousPath:legacyPath;
    File f=SD.open(path,FILE_READ);
    if(!f) { writeFailed_=modern; return !modern; }
    while(f.available() && size_<capacity) {
        String line=f.readStringUntil('\n'); line.trim();
        if(!line.length() || line.startsWith("frequency_khz")) continue;
        String col[24]; unsigned n=0,start=0;
        for(unsigned i=0;i<=line.length() && n<24;++i) {
            if(i==line.length() || line[i]==',') { col[n++]=line.substring(start,i); start=i+1; }
        }
        if(n<10 || (modern && n!=17 && n!=22)) continue;
        MemoryEntry e;
        e.fingerprint.frequency_khz=col[0].toInt();
        if(bandFor(e.fingerprint.frequency_khz)<0) continue;
        e.fingerprint.peak_rssi=col[1].toInt(); e.fingerprint.noise_rssi=col[2].toInt();
        e.fingerprint.duration_ms=col[3].toInt(); e.fingerprint.repeat_ms=col[4].toInt();
        e.fingerprint.bandwidth_khz=col[5].toInt();
        e.firstSeen=strtoul(col[6].c_str(),nullptr,10); e.lastSeen=strtoul(col[7].c_str(),nullptr,10);
        e.sightings=strtoul(col[8].c_str(),nullptr,10); e.label=clean(col[9]);
        if(!modern) {
            // The legacy writer did not escape commas in labels: retain the
            // entire tail after the ninth delimiter before sanitising it.
            int delimiter=-1;
            for(unsigned i=0;i<9;++i) delimiter=line.indexOf(',',delimiter+1);
            e.label=clean(line.substring(delimiter+1));
        }
        e.strongest=e.fingerprint.peak_rssi;
        if(modern) {
            e.firstUTC=strtoul(col[10].c_str(),nullptr,10); e.lastUTC=strtoul(col[11].c_str(),nullptr,10);
            e.strongest=col[12].toInt(); e.category=col[13].toInt();
            if(e.category>=6) e.category=0;
            e.notes=clean(col[14]);
            unsigned wanted=col[15].toInt(); if(wanted>12) wanted=12;
            unsigned at=0;
            while(e.encounterCount<wanted && at<col[16].length()) {
                int end=col[16].indexOf('|',at); if(end<0) end=col[16].length();
                e.encounterRSSI[e.encounterCount++]=col[16].substring(at,end).toInt(); at=end+1;
            }
        }
        if(n==22) {
            e.baselineRSSI=col[17].toInt(); e.baselineCount=col[18].toInt();
            if(e.baselineCount>12 || e.baselineCount<6) e.baselineCount=0;
            e.baselineUTC=strtoul(col[19].c_str(),nullptr,10);
            for(unsigned field=20;field<=21;++field) {
                unsigned at=0;
                for(unsigned j=0;j<e.encounterCount && at<col[field].length();++j) {
                    int end=col[field].indexOf('|',at); if(end<0) end=col[field].length();
                    uint32_t value=strtoul(col[field].substring(at,end).c_str(),nullptr,10);
                    if(field==20) e.encounterUTC[j]=value; else e.encounterDuration[j]=value>65535?65535:value;
                    at=end+1;
                }
            }
        }
        entries[size_++]=e;
    }
    f.close();
    // Copy legacy records on the normal timer, even if there are no new encounters.
    dirty_=(!modern && size_) || (modern && !SD.exists(memoryPath));
    return true;
}
bool SignalMemory::flushIfDue(uint32_t now,uint32_t intervalMs) {
    if(!dirty_) return true;
    if(uint32_t(now-lastSaveAttempt_)<intervalMs) return false;
    return save();
}
bool SignalMemory::save() {
    lastSaveAttempt_=millis(); writeFailed_=true;
    if(!SD.exists("/rfexplorer") && !SD.mkdir("/rfexplorer")) return false;
    if(SD.exists(tempPath) && !SD.remove(tempPath)) return false;
    File f=SD.open(tempPath,FILE_WRITE); if(!f) return false;
    f.println("frequency_khz,peak_rssi,noise_rssi,duration_ms,repeat_ms,bandwidth_khz,first_seen,last_seen,sightings,label,first_utc,last_utc,strongest,category,notes,history_count,history_rssi,baseline_rssi,baseline_count,baseline_utc,history_utc,history_duration_ms");
    for(uint8_t i=0;i<size_;++i) {
        const MemoryEntry& e=entries[i];
        f.print(e.fingerprint.frequency_khz); f.print(',');
        f.print(e.fingerprint.peak_rssi); f.print(','); f.print(e.fingerprint.noise_rssi); f.print(',');
        f.print(e.fingerprint.duration_ms); f.print(','); f.print(e.fingerprint.repeat_ms); f.print(',');
        f.print(e.fingerprint.bandwidth_khz); f.print(','); f.print(e.firstSeen); f.print(',');
        f.print(e.lastSeen); f.print(','); f.print(e.sightings); f.print(','); f.print(clean(e.label)); f.print(',');
        f.print(e.firstUTC); f.print(','); f.print(e.lastUTC); f.print(','); f.print(e.strongest); f.print(',');
        f.print(e.category); f.print(','); f.print(clean(e.notes)); f.print(','); f.print(e.encounterCount); f.print(',');
        for(unsigned j=0;j<e.encounterCount;++j) { if(j) f.print('|'); f.print(e.encounterRSSI[j]); }
        f.print(','); f.print(e.baselineRSSI); f.print(','); f.print(e.baselineCount); f.print(','); f.print(e.baselineUTC);
        f.print(','); for(unsigned j=0;j<e.encounterCount;++j) {if(j) f.print('|');f.print(e.encounterUTC[j]);}
        f.print(','); for(unsigned j=0;j<e.encounterCount;++j) {if(j) f.print('|');f.print(e.encounterDuration[j]);}
        f.println();
    }
    f.flush(); bool ok=f.getWriteError()==0; f.close(); if(!ok) return false;
    // Complete temp file first, then retain one previous complete generation.
    if(SD.exists(memoryPath)) {
        if(SD.exists(backupPath) && !SD.remove(backupPath)) return false;
        if(!SD.rename(memoryPath,backupPath)) return false;
    }
    if(!SD.rename(tempPath,memoryPath)) return false;
    dirty_=writeFailed_=false; return true;
}
}
