#pragma once
#include <Arduino.h>
#include <cmath>
// Non-RF events have no frequency/RSSI measurement; zero is not a measurement.
inline String eventMeasurementFields(uint32_t khz,float rssi) {
    if(!khz)return ",";
    return String(khz/1000.0,3)+","+(std::isfinite(rssi)?String(rssi,1):String(""));
}
// Ordinary event rows end at note (column 6). Latitude is column 17.
// Keep ten empty observation columns and then all eight GNSS columns.
inline String eventLocationTail(const String& geo) {
    return String(",,,,,,,,,,,")+(geo.length()?geo:String(",,,,,,,"))+"\n";
}
