#pragma once
#include <Arduino.h>
#include <SD.h>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace satrf {
constexpr uint32_t issFrequency=437800;
inline bool allowed(uint32_t khz){return khz>=387000&&khz<=464000;}
struct Record {
    uint32_t frequency=0,utc=0,span=0,samples=0,gap=0,above=0;
    int gate=-75,mean10=0,peak10=0;
    bool located=false;int32_t latitude=0,longitude=0;unsigned target=0;
    uint32_t session=0,sequence=0,phase=0,excessMs=UINT32_MAX,late=0,fixAge=UINT32_MAX,hdop100=0,priorSaveUs=0;
};
// Ten-second sampled RSSI summaries, never satellite-identification events.
struct Window {
    uint32_t first=0,last=0,count=0,gap=0,above=0,excessMs=0,late=0;double sum=0;float peak=-140;
    void reset(){*this=Window{};}
    void add(uint32_t now,float value,int gate){
        if(!std::isfinite(value)||value<-140||value>20)return;
        if(count){uint32_t d=now-last;if(d>gap)gap=d;if(d>50)excessMs+=d-50;if(d>75)++late;}else first=now;
        last=now;++count;sum+=value;if(value>peak)peak=value;if(value>=gate)++above;
    }
    bool ready()const{return count&&uint32_t(last-first)>=10000;}
    bool valid()const{return ready()&&count>=100&&gap<=250;}
    Record record(uint32_t frequency,unsigned target,int gate)const{
        Record r;r.frequency=frequency;r.target=target;r.gate=gate;r.span=last-first;r.samples=count;r.gap=gap;r.above=above;
        r.mean10=count?int(lround(sum*10/count)):0;r.peak10=int(lround(peak*10));r.excessMs=excessMs;r.late=late;return r;
    }
};
inline uint32_t checksum(const char* s){uint32_t hash=2166136261u;for(;*s;++s)hash=(hash^uint8_t(*s))*16777619u;return hash;}
inline String encodeV27(const Record& r){
    char b[230];snprintf(b,sizeof(b),"%lu,%lu,%lu,%lu,%lu,%lu,%d,%d,%d,%u,%ld,%ld,%u",(unsigned long)r.frequency,(unsigned long)r.utc,(unsigned long)r.span,(unsigned long)r.samples,(unsigned long)r.gap,(unsigned long)r.above,r.gate,r.mean10,r.peak10,r.located?1:0,(long)r.latitude,(long)r.longitude,r.target);
    char crc[16];snprintf(crc,sizeof(crc),",%08lx",(unsigned long)checksum(b));return String(b)+crc;
}
inline bool decodeV27(const String& line,Record& r){
    if(line.length()>245)return false;char b[250];strcpy(b,line.c_str());char* end=strrchr(b,',');if(!end||strlen(end+1)!=8)return false;
    unsigned long crc=0;int used=0;if(sscanf(end+1,"%8lx%n",&crc,&used)!=1||used!=8)return false;*end=0;if(checksum(b)!=crc)return false;
    unsigned long frequency,utc,span,samples,gap,above;long lat,lon;unsigned located,target;int gate,mean,peak,consumed=0;
    if(sscanf(b,"%lu,%lu,%lu,%lu,%lu,%lu,%d,%d,%d,%u,%ld,%ld,%u%n",&frequency,&utc,&span,&samples,&gap,&above,&gate,&mean,&peak,&located,&lat,&lon,&target,&consumed)!=13||b[consumed])return false;
    if(!allowed(frequency)||target>1||(target==0&&(frequency<issFrequency-15||frequency>issFrequency+15))||span<10000||span>20000||samples<100||samples>10000||gap>250||above>samples||gate<-140||gate>20||mean<-1400||peak>200||mean>peak||located>1||lat<-90000000||lat>90000000||lon<-180000000||lon>180000000)return false;
    r.frequency=frequency;r.utc=utc;r.span=span;r.samples=samples;r.gap=gap;r.above=above;r.gate=gate;r.mean10=mean;r.peak10=peak;r.located=located;r.latitude=lat;r.longitude=lon;r.target=target;
    return encodeV27(r)==line.c_str();
}
inline String encode(const Record& r){
    String body=encodeV27(r)+"|"+String(r.session)+","+String(r.sequence)+","+String(r.phase)+","+String(r.excessMs)+","+String(r.late)+","+String(r.fixAge)+","+String(r.hdop100)+","+String(r.priorSaveUs);
    char crc[16];snprintf(crc,sizeof(crc),"|%08lx",(unsigned long)checksum(body.c_str()));return body+crc;
}
inline bool decode(const String& line,Record& r){
    r=Record{};int split=line.indexOf('|');if(split<0)return decodeV27(line,r);
    int end=line.indexOf('|',split+1);if(end<0||line.length()!=unsigned(end+9)||!decodeV27(line.substring(0,split),r))return false;
    unsigned long id,seq,phase,excess,late,age,hdop,save;int used=0;String values=line.substring(split+1,end);
    if(sscanf(values.c_str(),"%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu%n",&id,&seq,&phase,&excess,&late,&age,&hdop,&save,&used)!=8||unsigned(used)!=values.length()||phase>2||late>r.samples||hdop>65535||(excess!=UINT32_MAX&&excess>r.span)||(id&&!seq))return false;
    r.session=id;r.sequence=seq;r.phase=phase;r.excessMs=excess;r.late=late;r.fixAge=age;r.hdop100=hdop;r.priorSaveUs=save;
    return encode(r)==line.c_str();
}
class Archive {
    Record rows[64]{};unsigned size=0;bool mounted=false,failed=false,dirty=false;
    static const char* path(){return "/rfexplorer/satellite-rf-v28.csv";}
    static const char* backup(){return "/rfexplorer/satellite-rf-v28.bak";}
    static const char* temp(){return "/rfexplorer/satellite-rf-v28.tmp";}
    void insert(const Record& r){if(size==64){for(unsigned i=1;i<64;++i)rows[i-1]=rows[i];--size;}rows[size++]=r;}
public:
    unsigned count()const{return size;}
    bool ready()const{return mounted;}
    bool writeFailed()const{return failed;}
    const Record* get(unsigned newest)const{return newest<size?&rows[size-1-newest]:nullptr;}
    bool begin(){size=0;failed=dirty=false;mounted=SD.exists("/rfexplorer");if(!mounted)return false;
        const char* source=SD.exists(path())?path():SD.exists(backup())?backup():"/rfexplorer/satellite-rf-v27.csv";if(!SD.exists(source))return true;File f=SD.open(source,FILE_READ);if(!f){mounted=false;return false;}
        while(f.available()){String line=f.readStringUntil('\n');line.trim();Record r;if(decode(line,r))insert(r);}f.close();return true;
    }
    bool save(){if(!mounted)return false;if(!dirty)return true;failed=true;
        if(SD.exists(temp())&&!SD.remove(temp()))return false;File f=SD.open(temp(),FILE_WRITE);if(!f)return false;
        bool ok=true;for(unsigned i=0;i<size;++i){String line=encode(rows[i])+"\n";if(f.print(line)!=line.length())ok=false;}f.flush();ok=ok&&!f.getWriteError();f.close();if(!ok)return false;
        if(SD.exists(path())){if(SD.exists(backup())&&!SD.remove(backup()))return false;if(!SD.rename(path(),backup()))return false;}
        if(!SD.rename(temp(),path()))return false;dirty=failed=false;return true;
    }
    bool add(const Record& r){Record checked;if(!decode(encode(r),checked))return false;if(!mounted)return false;insert(r);dirty=true;return save();}
};
}
