#include "gps.h"
#include <math.h>
#include "gnss_time.h"

namespace gnss {

const char* name(System v) {
    switch(v) {
        case System::GPS:     return "GPS";
        case System::GLONASS: return "GLO";
        case System::Galileo: return "GAL";
        case System::BeiDou:  return "BDS";
        case System::QZSS:    return "QZS";
        case System::SBAS:    return "SBA";
        default:              return "?";
    }
}

static bool coordinate(const char* s,char hemisphere,bool latitude,double& result) {
    if(!s||!*s|| (latitude?hemisphere!='N'&&hemisphere!='S':hemisphere!='E'&&hemisphere!='W')) return false;
    char* end=nullptr; double raw=strtod(s,&end);
    if(*end||!isfinite(raw)||raw<0) return false;
    double degrees=floor(raw/100.0),minute=raw-degrees*100.0;
    if(minute>=60||degrees>(latitude?90:180)||(degrees==(latitude?90:180)&&minute>0)) return false;
    result=degrees+minute/60.0;if(hemisphere=='S'||hemisphere=='W') result=-result;return true;
}
static float optionalNumber(const char* s) {
    if(!s||!*s) return NAN;char* end=nullptr;float v=strtof(s,&end);return *end||!isfinite(v)?NAN:v;
}
static String optionalCSV(float v,int places=1) {return isfinite(v)?String(v,places):String("");}

bool Receiver::checksumValid(const char* s) const {
    if(!s || s[0]!='$') return false;
    const char* star=strchr(s,'*');
    if(!star || strlen(star+1)!=2) return false;
    for(unsigned i=1;i<=2;++i) if(!((star[i]>='0'&&star[i]<='9')||(star[i]>='A'&&star[i]<='F')||(star[i]>='a'&&star[i]<='f'))) return false;
    uint8_t sum=0;
    for(const char* p=s+1;p<star;++p) sum^=uint8_t(*p);
    char hex[3]={star[1],star[2],0};
    char* end=nullptr;
    unsigned expected=strtoul(hex,&end,16);
    return end==hex+2 && sum==expected;
}

void Receiver::begin() {
    port.setRxBufferSize(2048);
    port.begin(115200,SERIAL_8N1,1,2);
    state=Fix{};
    diagnostic=Diagnostics{};
    length=0; discarding=false;
    store.reset(); refreshedAt=millis();
    rawHead=0;
    rawCount=0;
}

uint32_t Receiver::age() const {
    return state.connected ? millis()-state.lastSentence : UINT32_MAX;
}

void Receiver::recount() {
    state.gps=0;
    state.glonass=0;
    state.galileo=0;
    state.beidou=0;
    state.qzss=0;
    state.sbas=0;

    const Satellite* sats=store.satellites();
    uint8_t n=store.count();

    for(uint8_t i=0;i<n;i++) {
        switch(sats[i].system) {
            case System::GPS:     ++state.gps; break;
            case System::GLONASS: ++state.glonass; break;
            case System::Galileo: ++state.galileo; break;
            case System::BeiDou:  ++state.beidou; break;
            case System::QZSS:    ++state.qzss; break;
            case System::SBAS:    ++state.sbas; break;
            default: break;
        }
    }

    state.visible=n;
}

void Receiver::poll() {
    bool received=false;
    unsigned budget=2048; // Bound UART work so RF and keyboard cannot be starved.
    while(budget-- && port.available()) {
        char c=char(port.read());

        if(discarding) {if(c=='\n') {discarding=false;length=0;}continue;}
        if(c=='\n') {
            line[length]=0;

            if(length>6) {
                ++diagnostic.lines;
                bool validChecksum=checksumValid(line);
                if(!validChecksum) ++diagnostic.checksumBad;

                const char* t=(line[0]=='$'&&length>=6)?line+3:"";
                bool keep=rawFilter==0;

                if(!keep) {
                    static const char* f[]={
                        "","GSV","GSA","GGA","RMC","ZDA","TXT"
                    };
                    keep=!strncmp(t,f[rawFilter],3) && t[3]==',';
                }

                if(keep) {
                    rawLines[rawHead]=String(line);
                    rawHead=(rawHead+1)%rawCapacity;
                    if(rawCount<rawCapacity) ++rawCount;
                }

                if(validChecksum) {parse(line);received=true;}
            }

            length=0;
        }
        else if(c!='\r'&&length<sizeof(line)-1) {
            line[length++]=c;
        }
        else if(length>=sizeof(line)-1) {
            ++diagnostic.truncated; discarding=true;
            line[sizeof(line)-1]=0;

            if(rawFilter==0) {
                rawLines[rawHead]=String(line)+" [TRUNC]";
                rawHead=(rawHead+1)%rawCapacity;
                if(rawCount<rawCapacity) ++rawCount;
            }

            length=0;
        }
    }
    uint32_t now=millis();
    if(state.connected && uint32_t(now-state.lastSentence)>5000) state.connected=false;
    if(state.valid && uint32_t(now-state.lastFix)>3000) state.valid=false;
    if(state.timeValid && uint32_t(now-state.lastTime)>5000) state.timeValid=false;
    if(received || uint32_t(now-refreshedAt)>=250) {store.refresh(now);recount();refreshedAt=now;}
}

void Receiver::parse(char* s) {
    if(s[0]!='$'||strlen(s)<7||s[6]!=',') return;

    char* star=strchr(s,'*');
    if(star) *star=0;

    char* fields[24]{};
    uint8_t n=0;

    for(char* p=s;p&&n<24;) {
        fields[n++]=p;
        char* comma=strchr(p,',');
        if(!comma) break;
        *comma=0;
        p=comma+1;
    }

    if(n<2) return;

    const char* type=fields[0]+3;
    uint32_t now=millis();

    state.connected=true;
    state.lastSentence=now;

    if(!strcmp(type,"GGA")) { ++diagnostic.gga; diagnostic.lastGGA=now; }
    else if(!strcmp(type,"RMC")) { ++diagnostic.rmc; diagnostic.lastRMC=now; }
    else if(!strcmp(type,"GSA")) { ++diagnostic.gsa; diagnostic.lastGSA=now; }
    else if(!strcmp(type,"GSV")) { ++diagnostic.gsv; diagnostic.lastGSV=now; }
    else if(!strcmp(type,"ZDA")) ++diagnostic.zda;
    else if(!strcmp(type,"TXT")) ++diagnostic.txt;
    else ++diagnostic.other;

    if(!strcmp(type,"GGA")&&n>9) {
        double lat=0,lon=0;
        int quality=number(fields[6],8);
        bool good=quality>0 && coordinate(fields[2],fields[3][0],true,lat) && coordinate(fields[4],fields[5][0],false,lon);
        state.valid=good;
        if(good) {
            state.latitude=lat;state.longitude=lon;state.lastFix=now;
            state.altitude=optionalNumber(fields[9]);
            int used=number(fields[7],99);state.used=used<0?0:used;
            state.hdop=optionalNumber(fields[8]);
        }
    }
    else if(!strcmp(type,"RMC")&&n>9) {
        double lat=0,lon=0;
        state.valid=fields[2][0]=='A' && coordinate(fields[3],fields[4][0],true,lat) && coordinate(fields[5],fields[6][0],false,lon);
        if(state.valid) {
            state.latitude=lat;state.longitude=lon;state.lastFix=now;
            state.speedKmh=optionalNumber(fields[7])*1.852f;state.course=optionalNumber(fields[8]);
        }
        if(fields[2][0]=='A' && strlen(fields[9])==6) {
            int d=digits(fields[9],2),m=digits(fields[9]+2,2),y=digits(fields[9]+4,2);
            uint32_t epoch;uint16_t fraction;
            if(y>=0 && utcEpoch(fields[1],2000+y,m,d,epoch,fraction)) {
                state.utcEpoch=epoch;state.utcFraction=fraction;state.lastTime=now;state.timeValid=true;
                char b[24];snprintf(b,sizeof(b),"%04d-%02d-%02dT%.2s:%.2s:%.2sZ",2000+y,m,d,fields[1],fields[1]+2,fields[1]+4);state.utc=b;
            }
        }
    }
    else if(!strcmp(type,"ZDA")&&n>4) {
        int d=number(fields[2],31),m=number(fields[3],12),y=number(fields[4],2099);
        uint32_t epoch;uint16_t fraction;
        if(utcEpoch(fields[1],y,m,d,epoch,fraction)) {
            state.utcEpoch=epoch;state.utcFraction=fraction;state.lastTime=now;state.timeValid=true;
            char b[24];snprintf(b,sizeof(b),"%04d-%02d-%02dT%.2s:%.2s:%.2sZ",y,m,d,fields[1],fields[1]+2,fields[1]+4);state.utc=b;
        }
    }
    else if(!strcmp(type,"GSA")&&n>16) {
        state.fixType=uint8_t(atoi(fields[2]));
        state.pdop=atof(fields[15]);
        state.hdop=atof(fields[16]);
        if(n>17) state.vdop=atof(fields[17]);
        store.gsa(fields,n,now);
    }
    else if(!strcmp(type,"GSV")&&n>3) {
        store.gsv(fields,n,now);
    }

}

String Receiver::csv() const {
    if(!freshFix()) return ",,,,,,,";

    return String(state.latitude,6)+","+
           String(state.longitude,6)+","+
           optionalCSV(state.altitude)+","+
           optionalCSV(state.speedKmh)+","+
           optionalCSV(state.course)+","+
           String(state.used)+","+
           String(state.visible)+","+
           optionalCSV(state.hdop);
}

}
