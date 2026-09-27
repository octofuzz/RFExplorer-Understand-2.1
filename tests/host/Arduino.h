#pragma once
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdio>
class String {
    std::string s;
public:
    String()=default;
    String(const char* p):s(p?p:""){}
    String(char* p):s(p?p:""){}
    String(std::string v):s(v){}
    template<class T> String(T v):s(std::to_string(v)){}
    String(float v,int places) { std::ostringstream o; o<<std::fixed<<std::setprecision(places)<<v; s=o.str(); }
    String(double v,int places) { std::ostringstream o; o<<std::fixed<<std::setprecision(places)<<v; s=o.str(); }
    unsigned length() const { return unsigned(s.size()); }
    const char* c_str() const { return s.c_str(); }
    char operator[](unsigned i) const { return s[i]; }
    String substring(unsigned start,unsigned end=UINT32_MAX) const {
        start=std::min(start,length()); end=std::min(end,length());
        if(start>end) std::swap(start,end); return s.substr(start,end-start);
    }
    void replace(const char* a,const char* b) {
        size_t at=0; while((at=s.find(a,at))!=s.npos) { s.replace(at,std::string(a).size(),b); at+=std::string(b).size(); }
    }
    void trim() {
        auto a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");
        s=a==s.npos?"":s.substr(a,b-a+1);
    }
    bool startsWith(const char* p) const { return s.find(p)==0; }
    long toInt() const { return strtol(s.c_str(),nullptr,10); }
    int indexOf(char c,unsigned start=0) const { auto p=s.find(c,start); return p==s.npos?-1:int(p); }
    friend bool operator==(const String& a,const char* b) { return a.s==b; }
    friend String operator+(const String& a,const String& b) { return a.s+b.s; }
    void remove(unsigned at) { if(at<s.size()) s.erase(at); }
    String& operator+=(char c) { s+=c; return *this; }
};
extern uint32_t hostMillis;
inline uint32_t millis() { return hostMillis; }
constexpr int SERIAL_8N1=0;
class HardwareSerial {
public:
    HardwareSerial(int){}
    static std::string& input() {static std::string queue;return queue;}
    void setRxBufferSize(unsigned){}
    void begin(int,int,int,int){}
    int available() {return int(input().size());}
    int read() {char c=input()[0];input().erase(0,1);return c;}
};
