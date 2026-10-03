#pragma once
#include <stdint.h>
namespace ui {
struct MenuGroup { const char* name; const char* caption; uint8_t count; uint8_t actions[10]; };
constexpr MenuGroup groups[]={
    {"RF Explorer","SPECTRUM / RECEIVE",10,{0,1,7,8,13,2,3,4,5,6}},
    {"Signal Memory","EVIDENCE / DISCOVERY",5,{24,20,17,18,9}},
    {"GNSS & Travel","ORBIT / EXPEDITION",4,{10,11,12,19}},
    {"Satellites","RECEIVER / SKY",10,{31,25,26,27,28,29,30,32,33,34}},
    {"NFC","NEAR FIELD / READ",1,{14}},
    {"Journal & Files","OBSERVE / REMEMBER",2,{21,15}},
    {"Settings & Diagnostics","INSTRUMENT / STATUS",3,{16,23,22}}
};
struct Navigation {
    bool home=true;
    unsigned group=0, positions[7]{};
    unsigned count() const { return home?7:groups[group].count; }
    void remember(unsigned selection) { if(home) group=selection%7; else positions[group]=selection%count(); }
    unsigned selection() const { return home?group:positions[group]; }
    void open(unsigned selection) { group=selection%7;home=false; }
    unsigned action(unsigned selection) const { return groups[group].actions[selection%count()]; }
    void back() { home=true; }
};
}
