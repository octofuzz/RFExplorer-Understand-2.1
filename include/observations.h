#pragma once
#include "satellite_rf.h"
namespace observe {
inline String seal(const String& body){char b[16];snprintf(b,sizeof(b),"|%08lx",(unsigned long)satrf::checksum(body.c_str()));return body+b;}
inline bool unseal(String line,String& body){int end=-1;for(unsigned i=0;i<line.length();++i)if(line[i]=='|')end=i;if(end<0)return false;body=line.substring(0,end);return seal(body)==line.c_str();}
inline String hex(const String& value){String out;const char* digits="0123456789abcdef";for(unsigned i=0;i<value.length();++i){out+=digits[uint8_t(value[i])>>4];out+=digits[uint8_t(value[i])&15];}return out;}
inline bool unhex(const String& encoded,String& value){value="";if(encoded.length()>96||encoded.length()%2)return false;for(unsigned i=0;i<encoded.length();i+=2){unsigned v=0;for(unsigned j=0;j<2;++j){char c=encoded[i+j];if(c>='0'&&c<='9')v=v*16+c-'0';else if(c>='a'&&c<='f')v=v*16+c-'a'+10;else return false;}if(v<32||v>126)return false;value+=char(v);}return true;}
inline bool number(const String& s,uint32_t& n){if(!s.length())return false;n=0;for(unsigned i=0;i<s.length();++i){char c=s[i];if(c<'0'||c>'9'||n>UINT32_MAX/10||(n==UINT32_MAX/10&&unsigned(c-'0')>UINT32_MAX%10))return false;n=n*10+c-'0';}return String(n)==s.c_str();}
inline unsigned split(const String& body,String* fields,unsigned capacity){unsigned start=0,n=0;for(unsigned i=0;i<=body.length();++i)if(i==body.length()||body[i]=='|'){if(n==capacity)return 0;fields[n++]=body.substring(start,i);start=i+1;}return n;}
inline bool near(const satrf::Record& a,const satrf::Record& b){if(!a.located||!b.located)return false;double dy=(b.latitude-a.latitude)*0.11132,lon=(double(b.longitude)-a.longitude)/1e6;if(lon>180)lon-=360;if(lon<-180)lon+=360;double dx=lon*111320*cos(a.latitude/1e6*3.141592653589793/180);return dx*dx+dy*dy<=10000;}
inline const char* comparable(const satrf::Record& a,const satrf::Record& b){
    if(a.frequency!=b.frequency)return "Different tuning";if(a.gate!=b.gate)return "Different RSSI gates";
    if(a.excessMs==UINT32_MAX||b.excessMs==UINT32_MAX)return "Legacy timing unknown";
    if(a.gap>150||b.gap>150||a.excessMs>a.span/3||b.excessMs>b.span/3)return "Sampling quality differs";
    if(!a.located||!b.located)return "Receiver location missing";
    if(a.fixAge>5000||b.fixAge>5000||!a.hdop100||!b.hdop100||a.hdop100>300||b.hdop100>300)return "Fix quality insufficient";
    if(!near(a,b))return "Receiver moved >100m";return nullptr;
}
struct Phase {
    uint32_t windows=0;uint64_t samples=0;int64_t total10=0;satrf::Record first;const char* issue=nullptr;
    void add(const satrf::Record& r){if(!windows){first=r;issue=comparable(r,r);}else if(!issue)issue=comparable(first,r);++windows;samples+=r.samples;total10+=int64_t(r.mean10)*r.samples;}
    float mean()const{return samples?float(double(total10)/samples/10):NAN;}
};
struct Profile {String name="Custom UHF",source="Unknown",checked="Unknown";uint32_t frequency=433920;};
struct Session {
    uint32_t id=0,utc=0,ended=0,nominal=0,total=0,lastSequence=0;bool closed=false;
    String name,antenna,notes,target,source,checked;Phase phases[3];
};
class Store {
    Profile profiles[4];Session sessions[12];satrf::Record pins[32];unsigned size=0,pinCount=0;
    bool mounted=false,failed=false;
    static const char* meta(){return "/rfexplorer/observations-v28.meta";}
    static const char* backup(){return "/rfexplorer/observations-v28.bak";}
    static const char* temp(){return "/rfexplorer/observations-v28.tmp";}
    String filename(uint32_t id)const{return String("/rfexplorer/observation-")+String(id)+".csv";}
    bool metadata(File& f)const{
        auto write=[&](const String& body){String row=seal(body)+"\n";return f.print(row)==row.length();};
        for(unsigned i=0;i<4;++i){const auto& p=profiles[i];if(!write("P|"+String(i)+"|"+String(p.frequency)+"|"+hex(p.name)+"|"+hex(p.source)+"|"+hex(p.checked)))return false;}
        for(unsigned i=0;i<size;++i){const auto& s=sessions[i];if(!write("S|"+String(s.id)+"|"+String(s.utc)+"|"+String(s.ended)+"|"+String(s.nominal)+"|"+String(s.closed?1:0)+"|"+hex(s.name)+"|"+hex(s.antenna)+"|"+hex(s.notes)+"|"+hex(s.target)+"|"+hex(s.source)+"|"+hex(s.checked)))return false;}
        for(unsigned i=0;i<pinCount;++i)if(!write("B|"+hex(satrf::encode(pins[i]))))return false;
        return true;
    }
    bool persist(){if(!mounted)return false;failed=true;if(SD.exists(temp())&&!SD.remove(temp()))return false;File f=SD.open(temp(),FILE_WRITE);if(!f)return false;
        bool ok=metadata(f);f.flush();ok=ok&&!f.getWriteError();f.close();if(!ok)return false;
        if(SD.exists(meta())){if(SD.exists(backup())&&!SD.remove(backup()))return false;if(!SD.rename(meta(),backup()))return false;}
        if(!SD.rename(temp(),meta()))return false;failed=false;return true;
    }
    void scan(Session& s){s.total=s.lastSequence=0;for(auto& phase:s.phases)phase=Phase{};File f=SD.open(filename(s.id).c_str(),FILE_READ);if(!f)return;
        while(f.available()){String line=f.readStringUntil('\n');line.trim();satrf::Record r;if(satrf::decode(line,r)&&r.session==s.id&&r.sequence>s.lastSequence){s.lastSequence=r.sequence;++s.total;s.phases[r.phase].add(r);}}f.close();
    }
public:
    Store(){profiles[0].name="ISS UHF";profiles[0].frequency=satrf::issFrequency;profiles[0].source="ARISS status";profiles[0].checked="2026-09-30";}
    bool ready()const{return mounted;}bool writeFailed()const{return failed;}
    unsigned count()const{return size;}unsigned bookmarks()const{return pinCount;}
    Profile profile(unsigned i)const{return profiles[i<4?i:0];}
    const Session* session(unsigned i)const{return i<size?&sessions[i]:nullptr;}
    Session* find(uint32_t id){for(unsigned i=0;i<size;++i)if(sessions[i].id==id)return &sessions[i];return nullptr;}
    const satrf::Record* pin(unsigned i)const{return i<pinCount?&pins[pinCount-1-i]:nullptr;}
    bool begin(){size=pinCount=0;failed=false;mounted=SD.exists("/rfexplorer");if(!mounted)return false;const char* source=SD.exists(meta())?meta():backup();if(!SD.exists(source))return true;File f=SD.open(source,FILE_READ);if(!f){mounted=false;return false;}
        while(f.available()){String line=f.readStringUntil('\n');line.trim();String body,fields[12];if(!unseal(line,body))continue;unsigned n=split(body,fields,12);uint32_t a,b,c,d,e;
            if(n==6&&fields[0]=="P"&&number(fields[1],a)&&a<4&&number(fields[2],b)&&satrf::allowed(b)){Profile p;p.frequency=b;if(unhex(fields[3],p.name)&&unhex(fields[4],p.source)&&unhex(fields[5],p.checked)&&p.name.length()&&(a||b==satrf::issFrequency))profiles[a]=p;}
            if(n==12&&fields[0]=="S"&&size<12&&number(fields[1],a)&&a&&a<=999999&&!find(a)&&number(fields[2],b)&&number(fields[3],c)&&number(fields[4],d)&&satrf::allowed(d)&&number(fields[5],e)&&e<=1){Session s;s.id=a;s.utc=b;s.ended=c;s.nominal=d;s.closed=e;
                if(unhex(fields[6],s.name)&&unhex(fields[7],s.antenna)&&unhex(fields[8],s.notes)&&unhex(fields[9],s.target)&&unhex(fields[10],s.source)&&unhex(fields[11],s.checked))sessions[size++]=s;}
            if(n==2&&fields[0]=="B"&&pinCount<32){String data;if(fields[1].length()<=700&&!(fields[1].length()%2)){for(unsigned i=0;i<fields[1].length();i+=2){String part; if(!unhex(fields[1].substring(i,i+2),part)){data="";break;}data=data+part;}satrf::Record r;if(satrf::decode(data,r))pins[pinCount++]=r;}}
        }f.close();for(unsigned i=0;i<size;++i)scan(sessions[i]);return true;
    }
    bool saveProfile(unsigned index,const Profile& p){if(index>=4||!p.name.length()||p.name.length()>24||p.source.length()>24||p.checked.length()>24||!satrf::allowed(p.frequency)||(!index&&p.frequency!=satrf::issFrequency))return false;auto old=profiles[index];profiles[index]=p;if(persist())return true;profiles[index]=old;return false;}
    uint32_t create(const String& name,const String& antenna,const String& notes,const Profile& profile,uint32_t utc){if(!mounted||size==12||!name.length()||name.length()>24||antenna.length()>24||notes.length()>24)return 0;
        uint32_t id=1;for(unsigned i=0;i<size;++i)if(sessions[i].id>=id)id=sessions[i].id+1;while(id<=999999&&SD.exists(filename(id).c_str()))++id;if(id>999999)return 0;
        Session s;s.id=id;s.name=name;s.antenna=antenna;s.notes=notes;s.target=profile.name;s.source=profile.source;s.checked=profile.checked;s.nominal=profile.frequency;s.utc=utc;sessions[size++]=s;
        if(persist())return id;--size;sessions[size]=Session{};return 0;
    }
    bool finish(uint32_t id,uint32_t utc){auto* s=find(id);if(!s)return false;s->closed=true;s->ended=utc;return persist();}
    bool note(uint32_t id,const String& note){auto* s=find(id);if(!s||note.length()>24)return false;s->notes=note;return persist();}
    bool append(satrf::Record& r){auto* s=find(r.session);if(!s||s->closed||!mounted)return false;r.sequence=++s->lastSequence;satrf::Record valid;if(!satrf::decode(satrf::encode(r),valid))return false;
        File f=SD.open(filename(s->id).c_str(),FILE_APPEND);if(!f){failed=true;return false;}String row="\n"+satrf::encode(r)+"\n";bool ok=f.print(row)==row.length();f.flush();ok=ok&&!f.getWriteError();f.close();failed=!ok;if(ok){++s->total;s->phases[r.phase].add(r);}return ok;
    }
    unsigned page(uint32_t id,unsigned start,satrf::Record* output,unsigned limit){auto* s=find(id);if(!s)return 0;unsigned count=0,index=0;uint32_t last=0;File f=SD.open(filename(id).c_str(),FILE_READ);if(!f)return 0;
        while(f.available()){String line=f.readStringUntil('\n');line.trim();satrf::Record r;if(!satrf::decode(line,r)||r.session!=id||r.sequence<=last)continue;last=r.sequence;unsigned newest=s->total>index?s->total-1-index:UINT32_MAX;++index;if(newest>=start&&newest<start+limit){output[newest-start]=r;++count;}}f.close();return count;
    }
    bool bookmark(const satrf::Record& r){if(!mounted)return false;for(unsigned i=0;i<pinCount;++i)if(satrf::encode(pins[i])==satrf::encode(r).c_str())return true;if(pinCount==32)return false;satrf::Record valid;if(!satrf::decode(satrf::encode(r),valid))return false;pins[pinCount++]=r;if(persist())return true;--pinCount;return false;}
    const char* comparison(const Session& s,unsigned phase)const{const auto& a=s.phases[0];const auto& b=s.phases[phase];if(!a.windows||!b.windows)return "Need Before + selected phase";if(!s.antenna.length()||s.antenna=="Unknown")return "Antenna not recorded";if(a.issue)return a.issue;if(b.issue)return b.issue;return comparable(a.first,b.first);}
};
struct Point {uint32_t at=0;int16_t rssi10=0;uint8_t event=0;};
struct Timeline {
    Point points[128]{};unsigned head=0,count=0;uint32_t last=0;bool started=false;
    void add(uint32_t now,float value,uint8_t event=0){if(!event&&started&&uint32_t(now-last)<250)return;last=now;started=true;points[head]={now,int16_t(std::isfinite(value)?lround(value*10):0),uint8_t(event?event:std::isfinite(value)?0:1)};head=(head+1)%128;if(count<128)++count;}
    const Point& get(unsigned i)const{return points[(head+128-count+i)%128];}
};
}
