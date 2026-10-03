#pragma once

#include <Arduino.h>
#include <SD.h>
#include "gps.h"

namespace expedition {

class JourneyRecorder {
    uint32_t bootId=0;
    uint32_t sessionIndex=0;
    uint32_t intervalMs=5000;
    uint32_t lastSampleAt=0;

    bool recording=false;
    bool previousFix=false;

    String journeyName;
    String sessionName;
    String journeySlug;
    String filePath;

    uint32_t pointsWritten=0;
    uint32_t gapsWritten=0;

    uint32_t sessionStartedAt=0;
    uint32_t sessionFinishedDuration=0;
    uint32_t samplesAttempted=0;
    uint32_t samplesWithFix=0;

    bool haveTrackPoint=false;
    double lastLatitude=0.0;
    double lastLongitude=0.0;
    uint32_t lastTrackAt=0;
    double distanceMetres=0.0;

    static String slug(const String& value);
    static String csvSafe(const String& value);

    bool ensureDirectory(const String& path);
    bool append(const String& row);
    bool writePoint(const gnss::Receiver& gps);
    bool writeGap(const gnss::Receiver& gps);

public:
    struct TrailPoint { double latitude,longitude; bool start; };
    TrailPoint trail[64]{};
    unsigned trailCount=0,trailHead=0;
    const TrailPoint& trailPoint(unsigned i) const {return trail[(trailHead+64-trailCount+i)%64];}
    bool ready=false;
    String error;

    bool begin(uint32_t boot);

    bool start(const String& journey,
               const String& session,
               uint32_t sampleIntervalMs=5000);

    void poll(const gnss::Receiver& gps);

    bool finish();

    bool bookmark(const gnss::Receiver& gps,
                  const String& note);

    bool recoverInterrupted(const String& path,
                            const String& journey,
                            const String& session);

    bool active() const { return recording; }

    const String& journey() const { return journeyName; }
    const String& session() const { return sessionName; }
    const String& path() const { return filePath; }

    uint32_t points() const { return pointsWritten; }
    uint32_t gaps() const { return gapsWritten; }
    uint32_t interval() const { return intervalMs; }

    uint32_t elapsedMs() const {
        return recording
            ? uint32_t(millis()-sessionStartedAt)
            : sessionFinishedDuration;
    }

    double distanceM() const { return distanceMetres; }

    float coveragePercent() const {
        return samplesAttempted
            ? (100.0f*float(samplesWithFix)/float(samplesAttempted))
            : 0.0f;
    }
};

}




