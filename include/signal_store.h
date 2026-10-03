#pragma once
#include <Arduino.h>
#include "signal_memory.h"
#include "encounter_context.h"

namespace field {

struct MemoryEntry {
    SignalFingerprint fingerprint;
    String label;
    uint32_t firstSeen=0;
    uint32_t lastSeen=0;
    uint32_t sightings=0;
    // Legacy first/lastSeen are boot-relative milliseconds, never UTC.
    uint32_t firstUTC=0, lastUTC=0;
    int16_t strongest=-120;
    uint8_t category=0;
    String notes;
    bool flagged=false, reviewed=false;
    EncounterContext context[12]{};
    int16_t encounterRSSI[12]{};
    uint8_t encounterCount=0;
    uint32_t encounterUTC[12]{};
    uint16_t encounterDuration[12]{};
    int16_t baselineRSSI=-120;
    uint8_t baselineCount=0;
    uint32_t baselineUTC=0;
};

class SignalMemory {
    static constexpr uint8_t capacity=48;
    MemoryEntry entries[capacity];
    uint8_t size_=0;
    bool dirty_=false;
    bool writeFailed_=false;
    uint32_t lastSaveAttempt_=0;

public:
    bool begin();
    uint8_t count() const { return size_; }
    const MemoryEntry& entry(uint8_t i) const { return entries[i]; }

    int bestMatch(const SignalFingerprint& fp,uint8_t& score) const;
    int remember(const SignalFingerprint& fp,const String& label,uint32_t now,uint32_t utc=0,const EncounterContext& context=EncounterContext{});
    bool rename(uint8_t index,const String& label);
    bool annotate(uint8_t index,const String& notes);
    bool classify(uint8_t index,uint8_t category);
    bool pending() const { return dirty_; }
    bool writeFailed() const { return writeFailed_; }

    bool setFlag(uint8_t index,bool value);
    bool setReviewed(uint8_t index,bool value);
    bool captureBaseline(uint8_t index,uint32_t utc);
    bool load();
    bool save();
    // Returns true when clean or successfully flushed; false when pending/failed.
    bool flushIfDue(uint32_t now,uint32_t intervalMs);
};

}
