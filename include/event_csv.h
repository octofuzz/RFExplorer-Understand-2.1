#pragma once
#include <Arduino.h>
// Ordinary event rows end at note (column 6). Latitude is column 17.
// Keep ten empty observation columns and then all eight GNSS columns.
inline String eventLocationTail(const String& geo) {
    return String(",,,,,,,,,,,")+(geo.length()?geo:String(",,,,,,,"))+"\n";
}
