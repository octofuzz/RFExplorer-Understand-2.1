#pragma once
#include <Arduino.h>
#include <SD.h>
#include <vector>
class EventStore {
    String current;
    uint32_t session=0, part=0;
    bool makeFile();
    bool mounted=false;
public:
    bool ready=false;
    String error;
    uint32_t events=0;
    bool begin(uint32_t boot);
    bool save(const char* kind,uint32_t khz,float rssi,const String& note="",const String& geo="");
    bool saveObservation(const char* kind,const String& utc,uint64_t at,uint32_t coarse,
        uint32_t refined,bool valid,float rssi,float noise,uint32_t duration,uint32_t count,
        const String& label,const String& status,uint32_t maxGap,const String& geo="");
    std::vector<String> files();
    bool readPage(const String& path,uint32_t offset,String* lines,unsigned count,uint32_t& next);
    const String& path() const { return current; }
};
String utcTimestamp();
