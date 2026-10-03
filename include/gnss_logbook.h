#pragma once
#include <Arduino.h>
#include <SD.h>
#include "satellites.h"

namespace gnss {

struct LogEntry {
    System system=System::Unknown;
    uint16_t prn=0;
    int16_t bestSnr=-1;
    int16_t maxElevation=-1;
    uint32_t firstSeen=0;
    uint32_t lastSeen=0;
    uint32_t sightings=0;
    bool located=false;
    int32_t firstLatitude=0,firstLongitude=0,lastLatitude=0,lastLongitude=0; // degrees * 1e6
    uint32_t firstLocationUTC=0,lastLocationUTC=0;
    uint32_t reportStamp=0;
    bool reportedThisBoot=false;
    uint32_t confirmedReports=0,firstConfirmedUTC=0,lastConfirmedUTC=0;
    int16_t lastAzimuth=-1,lastElevation=-1,lastCn0=-1;
    int8_t lastSignal=-1;uint8_t lastUse=0;
    bool confirmedGeotag=false;
};

class SatelliteLogbook {
    static constexpr uint16_t maxEntries=192;
    LogEntry entries[maxEntries]{};
    uint16_t size=0;
    bool mounted=false, dirty=false, failed=false;
    uint32_t lastSave=0;
    const char* path="/rfexplorer/satellite-log-v26.csv";
    int find(System system,uint16_t prn) const;
    bool load();
public:
    bool begin();
    bool update(const Satellite* sats,uint8_t count,System& newSystem,uint16_t& newPrn,
                bool fixValid=false,double latitude=0,double longitude=0);
    bool save(bool force=false);
    uint16_t count() const { return size; }
    const LogEntry* data() const { return entries; }
    const LogEntry* get(uint16_t i) const { return i<size?&entries[i]:nullptr; }
    bool ready() const { return mounted; }
    bool full() const { return size>=maxEntries; }
    bool writeFailed() const { return failed; }
};

}
