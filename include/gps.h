#pragma once
#include <Arduino.h>
#include <math.h>
#include "satellites.h"

namespace gnss {

struct Fix {
    bool connected=false, valid=false, timeValid=false;
    uint8_t fixType=0;
    double latitude=0, longitude=0;
    float altitude=NAN, speedKmh=NAN, course=NAN, hdop=NAN, pdop=NAN, vdop=NAN;
    uint8_t used=0, visible=0;
    uint8_t gps=0, glonass=0, galileo=0, beidou=0, qzss=0, sbas=0;
    uint32_t lastSentence=0, lastFix=0, lastTime=0, utcEpoch=0;
    uint16_t utcFraction=0;
    String utc="UNSET";
};

struct Diagnostics {
    uint32_t lines=0, checksumBad=0, truncated=0;
    uint32_t gga=0, rmc=0, gsa=0, gsv=0, zda=0, txt=0, other=0;
    uint32_t lastGGA=0, lastRMC=0, lastGSA=0, lastGSV=0;
};

const char* name(System value);

class Receiver {
    HardwareSerial port{1};
    Fix state;
    Diagnostics diagnostic;
    SatelliteStore store;

    char line[128]{};
    uint8_t length=0;
    bool discarding=false;
    uint32_t refreshedAt=0;

    static constexpr uint8_t rawCapacity=64;
    String rawLines[rawCapacity];
    uint8_t rawHead=0;
    uint8_t rawCount=0;
    uint8_t rawFilter=0; // ALL, GSV, GSA, GGA, RMC, ZDA, TXT

    void parse(char* sentence);
    void recount();
    bool checksumValid(const char* sentence) const;

public:
    void begin();
    void poll();

    const Fix& fix() const { return state; }
    const Diagnostics& diagnostics() const { return diagnostic; }
    const Satellite* satellites() const { return store.satellites(); }
    uint8_t count() const { return store.count(); }

    uint32_t age() const;
    bool freshFix() const { return state.valid && uint32_t(millis()-state.lastFix)<=3000; }
    uint32_t sentenceAge(uint32_t at) const { return at ? millis()-at : UINT32_MAX; }

    const String& raw(uint8_t i) const {
        static String empty;
        return i<rawCount
            ? rawLines[(rawHead+rawCapacity-rawCount+i)%rawCapacity]
            : empty;
    }

    uint8_t rawCountLines() const { return rawCount; }

    void cycleRawFilter() {
        rawFilter=uint8_t((rawFilter+1)%7);
        rawHead=rawCount=0;
    }

    uint8_t filter() const { return rawFilter; }

    const char* filterName() const {
        static const char* n[]={"ALL","GSV","GSA","GGA","RMC","ZDA","TXT"};
        return n[rawFilter];
    }

    String csv() const;
};

}
