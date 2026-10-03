#include "rf_trace.h"
#include <SD.h>
#include <cassert>
#include <iostream>
uint32_t hostMillis=100;HostDisk disk;SDClass SD;
int main() {
    disk.files["/rfexplorer"]="";rf::TraceRecorder recorder;recorder.start();
    for(unsigned i=0;i<1100;++i) recorder.add(i*2,433050,-100,-102);
    assert(!recorder.active && recorder.count==1024);
    assert(recorder.save());auto path=std::string(recorder.path.c_str());
    assert(disk.files[path].find("0,433050,-100.0000,-102.0000,10,6,20")!=std::string::npos);
    disk.failWrite=true;assert(!recorder.save());assert(recorder.count==1024);
    std::cout<<"PASS: bounded capture, precise CSV export and retained buffer on SD failure\n";
}
