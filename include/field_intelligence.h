#pragma once
#include <stdint.h>
namespace field {
struct GnssFix { bool valid=false; double lat=0,lon=0,alt=0; float hdop=0; uint32_t utc=0; };
struct Encounter { uint32_t frequency_khz=0,first_ms=0,last_ms=0,duration_ms=0,detections=0; int16_t peak_rssi=-120,weakest_rssi=-120; GnssFix fix{}; char label[25]{}; uint8_t category=0; bool watched=false; };
struct WatchStats { uint32_t frequency_khz=0,started_ms=0,last_ms=0,detections=0; int16_t strongest=-120,weakest=-120; uint16_t timeline[60]{}; uint8_t timeline_count=0; void reset(uint32_t f,uint32_t now){frequency_khz=f;started_ms=last_ms=now;detections=0;strongest=weakest=-120;timeline_count=0;} void sample(int16_t r,uint32_t now,int16_t gate){last_ms=now;if(r>=gate){++detections;if(r>strongest)strongest=r;if(r<weakest)weakest=r;if(timeline_count<60)timeline[timeline_count++]=(uint16_t)((now-started_ms)/1000);}} };
inline bool supported(uint32_t k){return(k>=300000&&k<=348000)||(k>=387000&&k<=464000)||(k>=779000&&k<=928000);}
inline uint8_t rssiBar(int16_t r){if(r<=-110)return 0;if(r>=-30)return 100;return(uint8_t)((r+110)*100/80);}
}
