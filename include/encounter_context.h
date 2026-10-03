#pragma once
#include <stdint.h>
namespace field {
struct EncounterContext {
    int32_t latitudeE6=0,longitudeE6=0;
    uint32_t fixAgeMs=0,quietAgeMs=0;
    uint16_t hdop100=0,rxBandwidth10=0;
    uint8_t coverage=0;
    bool located=false;
};
}
