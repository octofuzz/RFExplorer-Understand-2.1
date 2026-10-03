// Build with the same C++14 host compiler as run_host_tests.py, include ../include.
// Usage: replay_trace path/to/trace.csv
#include "analysis.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
int main(int argc,char** argv) {
    if(argc!=2){std::cerr<<"Usage: replay_trace trace.csv\n";return 2;}
    std::ifstream input(argv[1]);if(!input)return 2;
    std::string line;std::getline(input,line);
    rf::Envelope e;unsigned rows=0;uint32_t frequency=0;
    try {
        while(std::getline(input,line)) {
            if(line.empty())continue;std::stringstream ss(line);std::vector<std::string> c;std::string part;
            while(std::getline(ss,part,','))c.push_back(part);
            if(c.size()!=7 || std::stoi(c[4])!=10 || std::stoi(c[5])!=6 || std::stoi(c[6])!=20)return 3;
            uint32_t at=std::stoul(c[0]),f=std::stoul(c[1]);
            if(frequency && frequency!=f)e.interrupt();frequency=f;
            float r=c[2].empty()?NAN:std::stof(c[2]),n=c[3].empty()?NAN:std::stof(c[3]);
            e.sample(at,r,n);++rows;
        }
    } catch(...) {std::cerr<<"Invalid trace row\n";return 3;}
    std::cout<<"samples="<<rows<<" qualified="<<e.count<<" unresolved="<<e.unresolved<<" max_sample_gap_ms="<<e.maxGap<<" interval_count="<<e.recurrence.count<<"\n";
    return 0;
}
