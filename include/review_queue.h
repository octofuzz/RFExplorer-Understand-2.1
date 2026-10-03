#pragma once
#include "signal_store.h"
namespace field {
// Review hints, not signal identification or proof of an anomaly.
inline const char* reviewReason(const MemoryEntry& e) {
    if(e.flagged) return "Flagged for investigation";
    if(e.reviewed)return nullptr;
    if(!observed(e.fingerprint)) return nullptr;
    if(activityWindow(e.fingerprint))return "Sustained activity / sampled";
    if(e.baselineCount>=6 && e.encounterCount &&
       abs(int(e.encounterRSSI[e.encounterCount-1])-int(e.baselineRSSI))>=12)
        return "Changed >=12 dB from baseline";
    if(e.category==0) return "Qualified / unidentified";
    return nullptr;
}
}
