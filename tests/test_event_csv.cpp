#include "event_csv.h"
#include <cassert>
#include <vector>
#include <string>
uint32_t hostMillis=0;
std::vector<std::string> split(const String& row) {
    std::string s=row.c_str();std::vector<std::string> fields;size_t a=0;
    for(size_t i=0;i<s.size();++i) if(s[i]==','||s[i]=='\n') {fields.push_back(s.substr(a,i-a));a=i+1;}
    return fields;
}
int main() {
    assert(eventMeasurementFields(0,0)==",");
    assert(eventMeasurementFields(433920,-55)=="433.920,-55.0");
    assert(eventMeasurementFields(433920,NAN)=="433.920,");
    String prefix="2026-09-27T12:00:00Z,100,SIGNAL,433.920,-55,plain note";
    auto fields=split(prefix+eventLocationTail("63.430500,10.395000,20,0,42,8,12,1.0"));
    assert(fields.size()==24&&fields[16]=="63.430500"&&fields[17]=="10.395000"&&fields[23]=="1.0");
    for(unsigned i=6;i<16;++i) assert(fields[i].empty());
    fields=split(prefix+eventLocationTail(""));assert(fields.size()==24);
    for(unsigned i=6;i<24;++i) assert(fields[i].empty());
    fields=split(prefix+eventLocationTail(",,,,,,,"));assert(fields.size()==24);
}
