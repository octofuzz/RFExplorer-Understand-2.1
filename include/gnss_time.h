#pragma once
#include <stdint.h>
#include <string.h>
namespace gnss {
inline int digits(const char* s,unsigned count) {
    if(!s || strlen(s)<count) return -1;
    int n=0;for(unsigned i=0;i<count;++i) {if(s[i]<'0'||s[i]>'9') return -1;n=n*10+s[i]-'0';}return n;
}
inline bool leap(int y) {return y%4==0 && (y%100!=0||y%400==0);}
inline bool utcEpoch(const char* clock,int y,int m,int d,uint32_t& epoch,uint16_t& fraction) {
    static const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(y<2024||y>2099||m<1||m>12||d<1||d>lengths[m-1]+(m==2&&leap(y))) return false;
    int h=digits(clock,2),mi=clock&&strlen(clock)>=4?digits(clock+2,2):-1;
    int sec=clock&&strlen(clock)>=6?digits(clock+4,2):-1;
    if(h<0||h>23||mi<0||mi>59||sec<0||sec>59) return false;
    fraction=0;
    if(clock[6]) {
        if(clock[6]!='.'||!clock[7]) return false;
        unsigned scale=100;
        for(unsigned i=7;clock[i];++i) {if(clock[i]<'0'||clock[i]>'9') return false;if(scale) {fraction+=(clock[i]-'0')*scale;scale/=10;}}
    }
    uint32_t days=0;for(int year=1970;year<y;++year) days+=leap(year)?366:365;
    for(int month=1;month<m;++month) days+=lengths[month-1]+(month==2&&leap(y));
    epoch=(days+d-1)*86400u+h*3600u+mi*60u+sec;return true;
}
struct ClockSync {
    bool synced=false;uint32_t lastSync=0;
    bool due(uint32_t now,bool valid,uint32_t received) const {
        return valid && uint32_t(now-received)<=2000 && (!synced||uint32_t(now-lastSync)>=60000);
    }
    void applied(uint32_t now) {synced=true;lastSync=now;}
};
}
