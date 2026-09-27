#include "storage.h"
#include "event_csv.h"
#include "receiver.h"
#include <time.h>
#include <esp_timer.h>
#include <algorithm>
#include <cmath>

String utcTimestamp() {
    time_t t=time(nullptr);
    if(t<1704067200) return "UNSET";
    tm utc{}; gmtime_r(&t,&utc);
    char b[24]; strftime(b,sizeof(b),"%Y-%m-%dT%H:%M:%SZ",&utc); return b;
}
bool EventStore::begin(uint32_t boot) {
    session=boot;
    ready=SD.begin(pins::sdCS,SPI,4000000);
    mounted=ready;
    if(!ready) { error="No SD / mount failed"; return false; }
    if(!SD.exists("/rfexplorer") && !SD.mkdir("/rfexplorer")) {
        ready=false; error="Cannot create log folder"; return false;
    }
    return makeFile();
}
bool EventStore::makeFile() {
    do { char name[64]; snprintf(name,sizeof(name),"/rfexplorer/%010lu-%04lu.csv",
        (unsigned long)session,(unsigned long)part++); current=name;
    } while(SD.exists(current));
    File f=SD.open(current,FILE_WRITE);
    const char* header="utc,uptime_ms,event,frequency_mhz,rssi_dbm,note,coarse_mhz,refined_mhz,refinement_valid,noise_dbm,delta_db,duration_ms,burst_count,label,status,max_sample_gap_ms,latitude,longitude,altitude_m,speed_kmh,course_deg,fix_satellites,visible_satellites,hdop\n";
    if(!f || f.print(header)!=strlen(header)) {
        if(f) f.close(); ready=false; error="SD create failed"; return false;
    }
    f.flush(); bool ok=f.getWriteError()==0; f.close();
    if(!ok) {ready=false;error="SD header flush failed";} return ok;
}
bool EventStore::save(const char* kind,uint32_t khz,float rssi,const String& note,const String& geo) {
    if(!ready) return false;
    File f=SD.open(current,FILE_APPEND);
    if(!f) { ready=false; error="SD open failed"; return false; }
    if(f.size()>=1024*1024) { f.close(); if(!makeFile()) return false; f=SD.open(current,FILE_APPEND); }
    if(!f) { ready=false; error="SD rollover failed"; return false; }
    String safe=note; safe.replace("\"","\"\""); safe.replace("\r"," "); safe.replace("\n"," ");
    char numbers[112]; snprintf(numbers,sizeof(numbers),",%llu,%s,%.3f,%.1f,\"",
        (unsigned long long)(esp_timer_get_time()/1000),kind,khz/1000.0,rssi);
    String row=utcTimestamp()+numbers+safe+"\""+eventLocationTail(geo);
    bool ok=f.print(row)==row.length(); f.flush(); ok=ok&&f.getWriteError()==0; f.close();
    if(!ok) { ready=false; error="SD full / write failed"; } else ++events;
    return ok;
}

bool EventStore::saveObservation(const char* kind,const String& utc,uint64_t at,
    uint32_t coarse,uint32_t refined,bool valid,float rssi,float noise,uint32_t duration,
    uint32_t count,const String& label,const String& status,uint32_t maxGap,const String& geo) {
    if(!ready) { error="SD unavailable"; return false; }
    File f=SD.open(current,FILE_APPEND);
    if(f && f.size()>=1024*1024) { f.close(); if(!makeFile()) return false; f=SD.open(current,FILE_APPEND); }
    if(!f) { ready=false; error="SD open failed"; return false; }
    String safe=label; safe.replace("\"","\"\""); safe.replace("\r"," "); safe.replace("\n"," ");
    String row=utc+","+String((unsigned long long)at)+","+kind+","+String((valid?refined:coarse)/1000.0,3)+","+
        String(rssi,1)+",\"observation\","+String(coarse/1000.0,3)+","+String(refined/1000.0,3)+","+
        String(valid?1:0)+","+(isfinite(noise)?String(noise,1):String(""))+","+
        (isfinite(noise)?String(rssi-noise,1):String(""))+","+String(duration)+","+String(count)+
        ",\""+safe+"\",\""+status+"\","+String(maxGap)+","+(geo.length()?geo:String(",,,,,,,"))+"\n";
    bool ok=f.print(row)==row.length(); f.flush(); ok=ok&&f.getWriteError()==0; f.close();
    if(!ok) { ready=false; error="SD full / write failed"; } else ++events;
    return ok;
}

std::vector<String> EventStore::files() {
    std::vector<String> result;
    if(!mounted) return result;
    File dir=SD.open("/rfexplorer");
    if(!dir) return result;
    while(true) {
        File f=dir.openNextFile(); if(!f) break;
        String name=f.name(); bool isFile=!f.isDirectory(); f.close();
        if(!isFile || !name.endsWith(".csv")) continue;
        int slash=name.lastIndexOf('/'); if(slash>=0) name=name.substring(slash+1);
        result.push_back(name);
        std::sort(result.begin(),result.end(),[](const String& a,const String& b){return a>b;});
        if(result.size()>128) result.pop_back(); // bounded RAM: show the newest 128 sessions/parts
    }
    dir.close(); return result;
}
bool EventStore::readPage(const String& path,uint32_t offset,String* lines,unsigned count,uint32_t& next) {
    File f=SD.open("/rfexplorer/"+path,FILE_READ);
    if(!f || !f.seek(offset)) { if(f) f.close(); error="Cannot read log"; return false; }
    for(unsigned i=0;i<count;++i) {
        lines[i]="";
        while(f.available()) { char c=f.read(); if(c=='\n') break; if(c!='\r' && lines[i].length()<512) lines[i]+=c; }
    }
    next=f.available()?f.position():0; f.close(); return true;
}
