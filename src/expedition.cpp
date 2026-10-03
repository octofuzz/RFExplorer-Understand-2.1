#include "expedition.h"

#include <esp_timer.h>
#include <cmath>

namespace expedition {

String JourneyRecorder::slug(const String& value) {
    String out;

    for(unsigned i=0;i<value.length() && out.length()<28;++i) {
        char c=value[i];

        if((c>='a'&&c<='z') ||
           (c>='A'&&c<='Z') ||
           (c>='0'&&c<='9')) {
            out += c;
        }
        else if(c==' ' || c=='-' || c=='_') {
            if(!out.length() || out[out.length()-1]!='_') out += '_';
        }
    }

    while(out.endsWith("_")) out.remove(out.length()-1);

    if(!out.length()) out="journey";

    return out;
}

String JourneyRecorder::csvSafe(const String& value) {
    String out=value;
    out.replace("\"","\"\"");
    out.replace("\r"," ");
    out.replace("\n"," ");
    return out;
}

bool JourneyRecorder::ensureDirectory(const String& path) {
    if(SD.exists(path.c_str())) return true;

    if(!SD.mkdir(path.c_str())) {
        error="Cannot create "+path;
        return false;
    }

    return true;
}

bool JourneyRecorder::append(const String& row) {
    if(!ready) return false;

    File f=SD.open(filePath.c_str(),FILE_APPEND);

    if(!f) {
        error="Expedition SD open failed";ready=false;recording=false;
        return false;
    }

    bool ok=f.print(row)==row.length();
    f.flush();
    ok=ok && f.getWriteError()==0;
    f.close();

    if(!ok) {
        error="Expedition SD write failed";
        ready=false;
        recording=false;
    }

    return ok;
}

bool JourneyRecorder::begin(uint32_t boot) {
    bootId=boot;

    if(!SD.exists("/rfexplorer")) {
        error="RFExplorer SD folder unavailable";
        ready=false;
        return false;
    }

    if(!ensureDirectory("/rfexplorer/expeditions")) {
        ready=false;
        return false;
    }

    ready=true;
    error="";
    return true;
}

bool JourneyRecorder::start(const String& journey,
                            const String& session,
                            uint32_t sampleIntervalMs) {
    if(!ready) {
        error="Expedition recorder unavailable";
        return false;
    }

    if(recording) {
        error="Finish current session first";
        return false;
    }

    journeyName=journey.length()?journey:"Unnamed journey";
    sessionName=session.length()?session:"Unnamed session";
    journeySlug=slug(journeyName);

    String dir="/rfexplorer/expeditions/"+journeySlug;

    if(!ensureDirectory(dir)) return false;

    intervalMs=constrain(sampleIntervalMs,1000u,60000u);

    do {
        ++sessionIndex;

        char name[48];
        snprintf(name,sizeof(name),
                 "/%010lu-%03lu.csv",
                 (unsigned long)bootId,
                 (unsigned long)sessionIndex);

        filePath=dir+String(name);
    }
    while(SD.exists(filePath.c_str()));

    File f=SD.open(filePath.c_str(),FILE_WRITE);

    if(!f) {
        error="Cannot create expedition session";
        return false;
    }

    const char* header=
        "type,state,utc,uptime_ms,journey,session,"
        "latitude,longitude,altitude_m,speed_kmh,course_deg,"
        "fix_satellites,visible_satellites,hdop,note\n";

    bool ok=f.print(header)==strlen(header);
    f.flush();
    ok=ok && f.getWriteError()==0;
    f.close();

    if(!ok) {
        error="Expedition header write failed";
        return false;
    }

    recording=true;
    previousFix=false;
    pointsWritten=0;
    gapsWritten=0;
    trailCount=trailHead=0;

    sessionStartedAt=millis();
    sessionFinishedDuration=0;
    samplesAttempted=0;
    samplesWithFix=0;

    haveTrackPoint=false;
    lastLatitude=0.0;
    lastLongitude=0.0;
    lastTrackAt=0;
    distanceMetres=0.0;

    lastSampleAt=millis()-intervalMs;

    String row=
        "START,session_start,UNSET,"+
        String((unsigned long long)(esp_timer_get_time()/1000))+
        ",\""+csvSafe(journeyName)+"\",\""+
        csvSafe(sessionName)+"\",,,,,,,,,\n";

    if(!append(row)) {recording=false;return false;}

    error="";
    return true;
}

bool JourneyRecorder::writePoint(const gnss::Receiver& gps) {
    const auto& f=gps.fix();
    uint32_t now=millis();

    const bool goodDistance=gps.freshMotion() && isfinite(f.speedKmh) && f.speedKmh>=2.0 &&
        gps.freshHdop() && isfinite(f.hdop) && f.hdop>0 && f.hdop<=5;
    if(haveTrackPoint && goodDistance) {
        constexpr double earthRadiusM=6371000.0;
        constexpr double degToRad=0.017453292519943295;

        double lat1=lastLatitude*degToRad;
        double lat2=f.latitude*degToRad;
        double dLat=(f.latitude-lastLatitude)*degToRad;
        double dLon=(f.longitude-lastLongitude)*degToRad;

        double a=
            sin(dLat/2.0)*sin(dLat/2.0)+
            cos(lat1)*cos(lat2)*
            sin(dLon/2.0)*sin(dLon/2.0);

        a=fmax(0.0,fmin(1.0,a));
        double c=2.0*atan2(sqrt(a),sqrt(1.0-a));
        double segmentM=earthRadiusM*c;

        double elapsedSeconds=
            double(uint32_t(now-lastTrackAt))/1000.0;

        double maximumPlausibleM=
            50.0+(elapsedSeconds*400.0);

        if(gps.freshMotion() && isfinite(f.speedKmh) && f.speedKmh>=2.0 &&
           gps.freshHdop() && isfinite(f.hdop) && f.hdop>0 && f.hdop<=5 &&
           segmentM>=5.0 &&
           segmentM<=maximumPlausibleM) {
            distanceMetres+=segmentM;
        }
    }

    lastLatitude=f.latitude;
    lastLongitude=f.longitude;
    lastTrackAt=now;
    haveTrackPoint=goodDistance;

    String utc=gps.freshTime()?f.utc:String("UNSET");

    String altitude=
        gps.freshAltitude() && isfinite(f.altitude)
        ? String(f.altitude,1)
        : String("");

    String speed=
        gps.freshMotion() && isfinite(f.speedKmh)
        ? String(f.speedKmh,1)
        : String("");

    String course=
        gps.freshMotion() && isfinite(f.course)
        ? String(f.course,1)
        : String("");

    String used=
        gps.freshUsed()
        ? String(f.used)
        : String("");

    String hdop=
        gps.freshHdop() && isfinite(f.hdop)
        ? String(f.hdop,1)
        : String("");

    String row=
        "POINT,fresh_fix,"+
        utc+","+
        String((unsigned long long)(esp_timer_get_time()/1000))+
        ",\""+csvSafe(journeyName)+"\",\""+
        csvSafe(sessionName)+"\","+
        String(f.latitude,6)+","+
        String(f.longitude,6)+","+
        altitude+","+
        speed+","+
        course+","+
        used+","+
        String(f.visible)+","+
        hdop+",\n";

    if(!append(row)) return false;

    trail[trailHead]={f.latitude,f.longitude,!previousFix};
    trailHead=(trailHead+1)%64;if(trailCount<64)++trailCount;
    ++pointsWritten;
    previousFix=true;
    return true;
}

bool JourneyRecorder::writeGap(const gnss::Receiver& gps) {
    const auto& f=gps.fix();

    String state;

    if(!f.connected) state="no_uart";
    else if(f.lastFix) state="stale_fix";
    else state="no_fix";

    String utc=gps.freshTime()?f.utc:String("UNSET");

    String row=
        "GAP,"+state+","+
        utc+","+
        String((unsigned long long)(esp_timer_get_time()/1000))+
        ",\""+csvSafe(journeyName)+"\",\""+
        csvSafe(sessionName)+"\",,,,,,,,,\n";

    if(!append(row)) return false;

    ++gapsWritten;
    previousFix=false;
    haveTrackPoint=false;
    lastTrackAt=0;
    return true;
}

void JourneyRecorder::poll(const gnss::Receiver& gps) {
    if(!recording || !ready) return;

    uint32_t now=millis();

    if(previousFix && !gps.freshFix()) {if(!writeGap(gps)) return;}
    if(uint32_t(now-lastSampleAt)<intervalMs) return;

    lastSampleAt=now;
    ++samplesAttempted;

    if(gps.freshFix()) {
        ++samplesWithFix;
        writePoint(gps);
        return;
    }

    if(previousFix) writeGap(gps);
}

bool JourneyRecorder::bookmark(const gnss::Receiver& gps,
                               const String& note) {
    if(!recording) {
        error="No active expedition session";
        return false;
    }

    if(!ready) {
        error="Expedition recorder unavailable";
        return false;
    }

    const auto& f=gps.fix();

    String utc=
        gps.freshTime()
        ? f.utc
        : String("UNSET");

    String latitude="";
    String longitude="";

    if(gps.freshFix()) {
        latitude=String(f.latitude,6);
        longitude=String(f.longitude,6);
    }

    String row=
        "BOOKMARK,field_note,"+
        utc+","+
        String((unsigned long long)(esp_timer_get_time()/1000))+
        ",\""+csvSafe(journeyName)+"\",\""+
        csvSafe(sessionName)+"\","+
        latitude+","+
        longitude+
        ",,,,,,,\""+
        csvSafe(note.length()?note:String("Field bookmark"))+
        "\"\n";

    if(!append(row)) {recording=false;return false;}

    error="";
    return true;
}
bool JourneyRecorder::recoverInterrupted(const String& path,
                                         const String& journey,
                                         const String& session) {
    if(!ready) {
        error="Expedition recorder unavailable";
        return false;
    }

    if(!path.length() ||
       !path.startsWith("/rfexplorer/expeditions/") || path.indexOf('.')!=int(path.length()-4) || !path.endsWith(".csv")) {
        error="Invalid recovery path";
        return false;
    }

    if(!SD.exists(path.c_str())) {
        error="Recovery session file missing";
        return false;
    }

    File f=SD.open(path.c_str(),FILE_APPEND);

    if(!f) {
        error="Cannot open recovery session";
        return false;
    }

    String row=
        "\nRECOVERED,unexpected_shutdown,UNSET,"+
        String((unsigned long long)(esp_timer_get_time()/1000))+
        ",\""+csvSafe(journey)+"\",\""+
        csvSafe(session)+"\",,,,,,,,,\n";

    bool ok=f.print(row)==row.length();
    f.flush();
    ok=ok && f.getWriteError()==0;
    f.close();

    if(!ok) {
        error="Recovery write failed";
        return false;
    }

    error="";
    return true;
}
bool JourneyRecorder::finish() {
    if(!recording) {
        error="No active expedition session";
        return false;
    }

    String row=
        "END,session_end,UNSET,"+
        String((unsigned long long)(esp_timer_get_time()/1000))+
        ",\""+csvSafe(journeyName)+"\",\""+
        csvSafe(sessionName)+"\",,,,,,,,,\"elapsed_ms="+String(uint32_t(millis()-sessionStartedAt))+
        ";distance_m="+String(distanceMetres,1)+";fix_sample_percent="+String(coveragePercent(),1)+
        ";points="+String(pointsWritten)+";gaps="+String(gapsWritten)+"\"\n";

    bool ok=append(row);

    sessionFinishedDuration=
        uint32_t(millis()-sessionStartedAt);

    recording=false;
    previousFix=false;
    haveTrackPoint=false;
    lastTrackAt=0;

    if(ok) error="";

    return ok;
}

}









