#include "rf_trace.h"
#include <SD.h>
namespace rf {
bool TraceRecorder::save() {
    active=false;
    if(!count){error="No captured samples";return false;}
    if(!SD.exists("/rfexplorer") || (!SD.exists("/rfexplorer/diagnostics") && !SD.mkdir("/rfexplorer/diagnostics"))) {error="Diagnostic SD unavailable";return false;}
    unsigned suffix=0;
    do {path="/rfexplorer/diagnostics/trace-"+String(millis())+"-"+String(suffix++)+".csv";}while(SD.exists(path.c_str()));
    File f=SD.open(path.c_str(),FILE_WRITE);if(!f){error="Trace open failed";return false;}
    const char* header="uptime_ms,frequency_khz,rssi_dbm,quiet_dbm,start_margin_db,hold_margin_db,max_allowed_gap_ms\n";
    bool ok=f.print(header)==strlen(header);
    for(unsigned i=0;i<count && ok;++i) {
        const auto& s=samples[i];
        String row=String(s.at)+","+String(s.frequency)+","+(isfinite(s.rssi)?String(s.rssi,4):String(""))+","+(isfinite(s.quiet)?String(s.quiet,4):String(""))+",10,6,20\n";
        ok=f.print(row)==row.length();
    }
    f.flush();ok=ok&&f.getWriteError()==0;f.close();
    error=ok?String(""):String("Trace write failed; buffer retained");return ok;
}
}
