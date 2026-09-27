#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Hardware-independent NMEA satellite store; exercised by tests/test_satellites.cpp.
namespace gnss {
constexpr uint8_t maxSatellites=96;
constexpr uint32_t satelliteTimeout=15000, usageTimeout=5000;
enum class System : uint8_t { GPS, GLONASS, Galileo, BeiDou, QZSS, SBAS, Unknown };
struct Satellite {
    uint16_t prn=0; int16_t azimuth=-1, elevation=-1, snr=-1;
    System system=System::Unknown;
    bool used=false, usageKnown=false;
    uint32_t lastSeen=0;
};
inline System talkerSystem(const char* s) {
    if(!strncmp(s,"$GL",3)) return System::GLONASS;
    if(!strncmp(s,"$GA",3)) return System::Galileo;
    if(!strncmp(s,"$GB",3)||!strncmp(s,"$BD",3)) return System::BeiDou;
    if(!strncmp(s,"$GQ",3)||!strncmp(s,"$QZ",3)) return System::QZSS;
    if(!strncmp(s,"$GP",3)) return System::GPS;
    return System::Unknown;
}
inline System systemFor(System talker,unsigned id) {
    if(talker==System::GPS && id>=33 && id<=64) return System::SBAS;
    if(talker!=System::Unknown) return talker;
    if(id>=1&&id<=32) return System::GPS;
    if(id>=33&&id<=64) return System::SBAS;
    if(id>=65&&id<=96) return System::GLONASS;
    if(id>=193&&id<=200) return System::QZSS;
    if(id>=201&&id<=237) return System::BeiDou;
    if(id>=301&&id<=336) return System::Galileo;
    return System::Unknown;
}
// Only standard local/global ID aliases are joined for GSA membership. Keep the
// exact GSV reported ID on screen; vendor-specific encodings are not guessed.
inline unsigned usageId(System s,unsigned id) {
    if(s==System::GLONASS&&id>=65&&id<=96) return id-64;
    if(s==System::Galileo&&id>=301&&id<=336) return id-300;
    if(s==System::BeiDou&&id>=201&&id<=237) return id-200;
    if(s==System::QZSS&&id>=193&&id<=200) return id-192;
    return id;
}
inline int number(const char* s,int maximum) {
    if(!s||!*s) return -1;
    unsigned v=0;
    for(;*s;++s) { if(*s<'0'||*s>'9') return -1; v=v*10+unsigned(*s-'0'); if(v>unsigned(maximum)) return -1; }
    return int(v);
}
inline bool checksum(char* s) {
    if(s[0]!='$') return false;
    char* star=strchr(s,'*');
    if(!star||strlen(star+1)!=2) return false;
    unsigned sum=0; for(char* p=s+1;p<star;++p) sum^=uint8_t(*p);
    char* end=nullptr; unsigned given=strtoul(star+1,&end,16);
    if(end!=star+3||sum!=given) return false;
    *star=0; return true;
}
class SatelliteStore {
    // Each talker/signal stream commits complete GSV cycles independently.
    // Incomplete cycles never replace the last complete snapshot.
    struct Stream {
        char talker[4]{}; int signal=-1; bool occupied=false, valid=false, pending=false;
        uint8_t pages=0,next=0,count=0,stagedCount=0,total=0;
        uint32_t at=0,started=0;
        Satellite live[64]{},staged[64]{};
    } streams[16];
    struct Usage { bool valid=false,complete=false; uint32_t at=0; uint8_t count=0; uint16_t ids[12]{}; } usage[7];
    Satellite sky[maxSatellites]{};
    uint8_t size=0;
public:
    void reset() {memset(streams,0,sizeof(streams));clearUsage();size=0;}
    const Satellite* satellites() const { return sky; }
    uint8_t count() const { return size; }
    void clearUsage() { for(auto& u:usage) u=Usage{}; }
    void refresh(uint32_t now) {
        size=0;
        for(auto& stream:streams) {
            if(!stream.valid||uint32_t(now-stream.at)>satelliteTimeout) continue;
            for(unsigned k=0;k<stream.count;++k) {
                const auto& incoming=stream.live[k]; unsigned i=0;
                for(;i<size;++i) if(sky[i].system==incoming.system&&sky[i].prn==incoming.prn) break;
                if(i==size) { if(size==maxSatellites) continue; sky[size++]=incoming; }
                else { // Strongest currently reported signal, not a historical maximum.
                    if(incoming.snr>sky[i].snr) sky[i]=incoming;
                }
            }
        }
        for(unsigned i=0;i<size;++i) {
            auto& sat=sky[i]; auto& u=usage[unsigned(sat.system)]; sat.used=false; sat.usageKnown=false;
            if(sat.system!=System::Unknown&&u.valid&&uint32_t(now-u.at)<=usageTimeout) {
                for(unsigned j=0;j<u.count;++j) if(u.ids[j]==usageId(sat.system,sat.prn)) sat.used=true;
                sat.usageKnown=sat.used||u.complete;
            }
        }
        // Stable constellation + reported ID ordering, independent of arrival order.
        for(unsigned i=1;i<size;++i) { Satellite s=sky[i]; unsigned j=i;
            while(j&&(unsigned(sky[j-1].system)>unsigned(s.system)||(sky[j-1].system==s.system&&sky[j-1].prn>s.prn))) { sky[j]=sky[j-1]; --j; } sky[j]=s;
        }
    }
    void gsv(char** f,unsigned n,uint32_t now) {
        if(n<4||(n-4)%4>1) return;
        int pages=number(f[1],16),page=number(f[2],16),total=number(f[3],64);
        if(pages<1||page<1||page>pages||total<0) return;
        unsigned groups=(n-4)/4; if(groups>4) return;
        int signal=(n-4)%4?number(f[n-1],255):-1;
        Stream* stream=nullptr; Stream* freeSlot=nullptr;
        for(auto& st:streams) {
            if(st.occupied&&!strncmp(st.talker,f[0],3)&&st.signal==signal) { stream=&st; break; }
            if(!st.occupied||(!st.pending&&uint32_t(now-st.at)>satelliteTimeout)|| (st.pending&&uint32_t(now-st.started)>satelliteTimeout)) freeSlot=&st;
        }
        if(!stream) { if(!freeSlot||page!=1) return; stream=freeSlot; *stream=Stream{}; stream->occupied=true; memcpy(stream->talker,f[0],3); stream->signal=signal; }
        auto& st=*stream;
        if(page==1) { st.stagedCount=0; st.total=total; st.pages=pages; st.next=1; st.pending=true; st.started=now; }
        if(!st.pending||st.pages!=pages||st.total!=total||st.next!=page||uint32_t(now-st.started)>3000) { st.pending=false; return; }
        for(unsigned k=0;k<groups;++k) {
            unsigned pos=4+k*4; int id=number(f[pos],999); if(id<=0) continue;
            Satellite sat; sat.prn=id; sat.system=systemFor(talkerSystem(f[0]),id);
            sat.elevation=number(f[pos+1],90); sat.azimuth=number(f[pos+2],359); sat.snr=number(f[pos+3],99); sat.lastSeen=now;
            if(st.stagedCount<64) st.staged[st.stagedCount++]=sat;
        }
        ++st.next;
        if(page==pages) { if(st.stagedCount!=st.total) {st.pending=false;return;} st.count=st.stagedCount; memcpy(st.live,st.staged,sizeof(Satellite)*st.count); st.at=now; st.valid=true; st.pending=false; refresh(now); }
    }
    void gsa(char** f,unsigned n,uint32_t now) {
        if(n<18) return;
        int fix=number(f[2],3); if(fix<1) return;
        System declared=talkerSystem(f[0]);
        if(n>18&&*f[18]) { int id=number(f[18],6); declared=id>=1&&id<=5?System(id-1):System::Unknown; }
        Usage fresh[7]{}; bool touched[7]{};
        if(declared!=System::Unknown) touched[unsigned(declared)]=true;
        unsigned entries=0;
        for(unsigned k=3;k<15;++k) {
            int id=number(f[k],999); if(id<=0) continue; ++entries;
            System sys=systemFor(declared,id); if(sys==System::Unknown) continue;
            auto idx=unsigned(sys); touched[idx]=true;
            if(fix>1) fresh[idx].ids[fresh[idx].count++]=usageId(sys,id);
        }
        // A GN sentence without system ID is ambiguous when empty. Invalidate
        // membership instead of falsely claiming that every satellite is unused.
        if(declared==System::Unknown&&entries==0) { clearUsage(); refresh(now); return; }
        for(unsigned i=0;i<7;++i) if(touched[i]) { fresh[i].valid=true; fresh[i].at=now; fresh[i].complete=declared!=System::Unknown&&entries<12; usage[i]=fresh[i]; }
        refresh(now);
    }
};
}
