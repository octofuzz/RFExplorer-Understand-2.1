#pragma once
#include "Arduino.h"
#include <map>
#include <memory>
constexpr int FILE_READ=0,FILE_WRITE=1,FILE_APPEND=2;
struct HostDisk {
    std::map<std::string,std::string> files;
    bool failWrite=false, failCommit=false, unavailable=false;
    unsigned writes=0;
};
extern HostDisk disk;
class File {
    std::string path; size_t at=0; bool valid=false, writing=false;
public:
    File()=default;
    File(const char* p,int mode):path(p),valid(!disk.unavailable&&(mode!=FILE_READ||disk.files.count(p))),writing(mode!=FILE_READ) {
        if(valid&&writing) { if(mode==FILE_WRITE) disk.files[p]=""; ++disk.writes; }
    }
    explicit operator bool() const { return valid; }
    bool available() const { return valid && at<disk.files[path].size(); }
    String readStringUntil(char c) { auto& s=disk.files[path]; auto end=s.find(c,at); if(end==s.npos) end=s.size(); auto out=s.substr(at,end-at); at=end+1; return out; }
    size_t print(const String& s) { if(valid&&!disk.failWrite) {disk.files[path]+=s.c_str();return s.length();}return 0; }
    size_t print(const char* s) { return print(String(s)); }
    void print(char c) { if(valid&&!disk.failWrite) disk.files[path]+=c; }
    template<class T> void print(T v) { print(String(v)); }
    void println(const String& s) { print(s); print('\n'); }
    void println(const char* s) { print(s); print('\n'); }
    void println() { print('\n'); }
    void flush(){}
    int getWriteError() const { return disk.failWrite?1:0; }
    void close() { valid=false; }
};
struct SDClass {
    bool exists(const char* p) { return !disk.unavailable && disk.files.count(p); }
    bool mkdir(const char*) { return !disk.unavailable; }
    bool remove(const char* p) { return !disk.unavailable && disk.files.erase(p); }
    bool rename(const char* a,const char* b) {
        if(disk.unavailable || !disk.files.count(a) || (disk.failCommit&&std::string(a).find(".tmp")!=std::string::npos)) return false;
        disk.files[b]=disk.files[a]; disk.files.erase(a); return true;
    }
    File open(const char* p,int mode) { return File(p,mode); }
};
extern SDClass SD;
