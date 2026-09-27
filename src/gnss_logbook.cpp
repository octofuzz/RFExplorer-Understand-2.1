#include "gnss_logbook.h"
#include <time.h>
#include <algorithm>
#include <cmath>

namespace gnss {

static const char* token(System s) {
    switch(s) {
        case System::GPS:return "GPS";
        case System::GLONASS:return "GLO";
        case System::Galileo:return "GAL";
        case System::BeiDou:return "BDS";
        case System::QZSS:return "QZS";
        case System::SBAS:return "SBA";
        default:return "UNK";
    }
}

static System parseToken(const String& s) {
    if(s=="GPS") return System::GPS;
    if(s=="GLO") return System::GLONASS;
    if(s=="GAL") return System::Galileo;
    if(s=="BDS") return System::BeiDou;
    if(s=="QZS") return System::QZSS;
    if(s=="SBA") return System::SBAS;
    return System::Unknown;
}

int SatelliteLogbook::find(System system,uint16_t prn) const {
    for(uint16_t i=0;i<size;++i) if(entries[i].system==system&&entries[i].prn==prn) return int(i);
    return -1;
}


static const char* legacyPath="/rfexplorer/satellite-log.csv";
static const char* temporaryPath="/rfexplorer/satellite-log-v21.tmp";
static const char* backupPath="/rfexplorer/satellite-log-v21.bak";
bool SatelliteLogbook::begin() {
    size=0;dirty=failed=false;lastSave=millis();
    mounted=SD.exists("/rfexplorer");
    if(!mounted) return false;
    if(!load()) {failed=true;mounted=false;return false;}
    return true;
}
bool SatelliteLogbook::load() {
    bool modern=SD.exists(path)||SD.exists(backupPath);
    const char* source=SD.exists(path)?path:SD.exists(backupPath)?backupPath:legacyPath;
    if(!SD.exists(source)) return true;
    File f=SD.open(source,FILE_READ);if(!f) return false;
    while(f.available() && size<maxEntries) {
        String line=f.readStringUntil('\n');line.trim();
        if(!line.length()||line.startsWith("system,")) continue;
        String col[15];unsigned n=0,start=0;
        for(unsigned i=0;i<=line.length()&&n<15;++i) if(i==line.length()||line[i]==',') {col[n++]=line.substring(start,i);start=i+1;}
        if(n!=(modern?14:7)) continue;
        LogEntry e;e.system=parseToken(col[0]);e.prn=col[1].toInt();
        if(e.system==System::Unknown||!e.prn||e.prn>999||find(e.system,e.prn)>=0) continue;
        e.firstSeen=strtoul(col[2].c_str(),nullptr,10);e.lastSeen=strtoul(col[3].c_str(),nullptr,10);
        e.bestSnr=col[4].toInt();e.maxElevation=col[5].toInt();e.sightings=strtoul(col[6].c_str(),nullptr,10);
        if(modern && col[7]=="1") {
            e.firstLatitude=strtol(col[8].c_str(),nullptr,10);e.firstLongitude=strtol(col[9].c_str(),nullptr,10);
            e.lastLatitude=strtol(col[10].c_str(),nullptr,10);e.lastLongitude=strtol(col[11].c_str(),nullptr,10);
            e.firstLocationUTC=strtoul(col[12].c_str(),nullptr,10);e.lastLocationUTC=strtoul(col[13].c_str(),nullptr,10);
            e.located=abs(int64_t(e.firstLatitude))<=90000000 && abs(int64_t(e.lastLatitude))<=90000000 &&
                abs(int64_t(e.firstLongitude))<=180000000 && abs(int64_t(e.lastLongitude))<=180000000;
        }
        entries[size++]=e;
    }
    f.close();dirty=size && (!modern || !SD.exists(path));return true;
}
bool SatelliteLogbook::update(const Satellite* sats,uint8_t count,System& newSystem,uint16_t& newPrn,
                              bool fixValid,double latitude,double longitude) {
    newSystem=System::Unknown;newPrn=0;
    if(!mounted||!sats) return false;
    uint32_t epoch=time(nullptr);if(epoch<1704067200) epoch=0;
    fixValid=fixValid && isfinite(latitude)&&isfinite(longitude)&&latitude>=-90&&latitude<=90&&longitude>=-180&&longitude<=180;
    bool changed=false;
    for(unsigned i=0;i<count;++i) {
        const auto& sat=sats[i];
        // A snapshot may remain on the sky plot for 15 s; only fresh reports are sightings.
        if(sat.system==System::Unknown||!sat.prn||uint32_t(millis()-sat.lastSeen)>3000) continue;
        int idx=find(sat.system,sat.prn);
        if(idx<0) {
            if(size>=maxEntries) continue;
            idx=size++;entries[idx]=LogEntry{};
            entries[idx].system=sat.system;entries[idx].prn=sat.prn;entries[idx].firstSeen=epoch;
            if(newSystem==System::Unknown) {newSystem=sat.system;newPrn=sat.prn;}
        }
        auto& e=entries[idx];
        if(e.reportedThisBoot && e.reportStamp==sat.lastSeen) continue;
        e.reportedThisBoot=true;e.reportStamp=sat.lastSeen;
        // Unknown first-seen UTC is preserved; later fixes are not backdated.
        e.lastSeen=epoch;
        if(sat.snr>e.bestSnr) e.bestSnr=sat.snr;
        if(sat.elevation>e.maxElevation) e.maxElevation=sat.elevation;
        if(e.sightings!=UINT32_MAX) ++e.sightings;
        if(fixValid) {
            int32_t lat=int32_t(lround(latitude*1e6)),lon=int32_t(lround(longitude*1e6));
            if(!e.located) {e.firstLatitude=lat;e.firstLongitude=lon;e.firstLocationUTC=epoch;}
            e.located=true;e.lastLatitude=lat;e.lastLongitude=lon;e.lastLocationUTC=epoch;
        }
        changed=true;
    }
    if(changed) dirty=true;
    return changed;
}
bool SatelliteLogbook::save(bool force) {
    if(!mounted||!dirty) return mounted;
    uint32_t now=millis();
    if((!force||failed)&&uint32_t(now-lastSave)<30000) return !failed;
    lastSave=now;failed=true;
    if(SD.exists(temporaryPath)&&!SD.remove(temporaryPath)) return false;
    File f=SD.open(temporaryPath,FILE_WRITE);if(!f) return false;
    f.println("system,prn,first_seen_epoch,last_seen_epoch,best_snr_db,max_elevation_deg,sightings,located,first_lat_e6,first_lon_e6,last_lat_e6,last_lon_e6,first_location_utc,last_location_utc");
    for(unsigned i=0;i<size;++i) {
        const auto& e=entries[i];
        f.print(token(e.system));f.print(',');f.print(e.prn);f.print(',');f.print(e.firstSeen);f.print(',');f.print(e.lastSeen);
        f.print(',');f.print(e.bestSnr);f.print(',');f.print(e.maxElevation);f.print(',');f.print(e.sightings);
        f.print(',');f.print(e.located?1:0);f.print(',');f.print(e.firstLatitude);f.print(',');f.print(e.firstLongitude);
        f.print(',');f.print(e.lastLatitude);f.print(',');f.print(e.lastLongitude);
        f.print(',');f.print(e.firstLocationUTC);f.print(',');f.print(e.lastLocationUTC);f.println();
    }
    f.flush();bool ok=f.getWriteError()==0;f.close();if(!ok) return false;
    if(SD.exists(path)) {
        if(SD.exists(backupPath)&&!SD.remove(backupPath)) return false;
        if(!SD.rename(path,backupPath)) return false;
    }
    if(!SD.rename(temporaryPath,path)) return false;
    dirty=failed=false;return true;
}
}
