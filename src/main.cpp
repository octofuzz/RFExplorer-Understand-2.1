#include <M5Cardputer.h>
#include <Wire.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <Preferences.h>
#include <sys/time.h>
#include <cmath>
#include "receiver.h"
#include "storage.h"
#include "analysis.h"
#include "finder.h"
#include "gps.h"
#include "expedition.h"
#include "gnss_time.h"
#include "gnss_logbook.h"
#include "signal_store.h"
#include "investigation.h"
#include "understand.h"
#include "menu_art.h"
#include "ui_theme.h"
#include "navigation.h"
#include "battery_gauge.h"
#include "rf_trace.h"
#include "review_queue.h"
#include "fieldwork.h"
#include "satellite_rf.h"
#include "observations.h"
#include <esp_timer.h>
// RFExplorer 2.8 Observe: supported GNSS detections and honest saved evidence.

namespace {
constexpr uint16_t BG=ui::theme.background, PANEL=ui::theme.panel, CYAN=ui::theme.accent,
    MUTED=ui::theme.muted, WHITE=ui::theme.ink, AMBER=ui::theme.warning,
    GREEN=ui::theme.good, ERROR_COLOR=ui::theme.error;
M5Canvas canvas(&M5.Display);
Receiver rx;
EventStore logs;
gnss::Receiver gps;
expedition::JourneyRecorder journeyRecorder;
String expeditionJourney="New Journey";
String expeditionSession="Session 1";
bool expeditionInterrupted=false;
gnss::ClockSync clockSync;
gnss::SatelliteLogbook satLog;
field::SignalMemory signalMemory;
field::Session journal;
uint32_t journalAt=0;
bool journalDirty=false;
unsigned encounterSelected=0;
void saveJournal();
Preferences prefs;
m5::unit::UnitUnified units;
m5::unit::CapCC1101NFC nfc{pins::nfcCS};
m5::nfc::NFCLayerA nfcA{nfc};
bool nfcReady=false, nfcAttempted=false, prefsReady=false;
enum class Screen { Menu, Bands, Live, Antenna, Edit, Settings, NFC, Files, Log, About, Fine, Inspector, RfDiagnostics, Glass, Finder, Bookmarks, GPS, Expedition, Trail, TravelStatus, Satellites, SatDetail, SatCategory, SatArchive, SatRecord, SatCapabilities, SatRFMenu, SatRFSetup, SatRFLive, SatRFHistory, SatRFRecord, SatRFLimits, RFProfiles, RFSessions, RFSession, NMEA, GnssHistory, GnssDiag, GnssLogbook, SatLocation, BuildInfo, Catalogue, Hunt, Watch, Survey, Library, Dossier, Summary };
enum class Field { RFName, RFSource, RFChecked, RFSessionName, RFAntenna, RFNote, SatFrequency, Frequency, Low, High, Step, Threshold, Dwell, Clock, Label, Bookmark, MemoryLabel, MemoryNote, JourneyName, SessionName, ExpeditionBookmark };
Screen screen=Screen::Menu, returnScreen=Screen::Menu;
Field field=Field::Frequency;
unsigned selected=0, band=1, bwIndex=2;
ui::Navigation navigation;
ui::BatteryGauge batteryGauge;
bool charging=false, gnssOnly=false;
unsigned routeInterval=5000;
Screen gnssParent=Screen::GPS;
constexpr float bandwidths[]={58.0f,101.6f,203.1f,406.3f};
uint32_t low=433050, high=434790, fixed=433920, step=100, dwell=40;
uint32_t frequency=433920, sampledFrequency=433920, tunedAt=0, sampleAt=0, drawAt=0, lastEvent=0;
int threshold=-75, brightness=170, volume=70;
bool scan=false, paused=false, sound=true, autoLog=true, haveSample=false;
float rssi=-120, peak=-120;
uint32_t peakFrequency=0;
float history[112]{}; unsigned historyHead=0, historyCount=0;
rf::Threshold trigger;
rf::FineScan fine;
rf::Envelope envelope;
rf::TraceRecorder rfTrace;
rf::QuietReference quietReference;rf::SamplingQuality samplingQuality;rf::ActivityWindow activityWindow;
field::EncounterContext lastBurstContext;
unsigned diagnosticPage=0;
uint32_t rememberedBurst=0,lastBurstEpoch=0;
int lastComparison=-1;uint8_t lastComparisonScore=0;
uint16_t lastBurstInterval=0;
uint64_t lastBurstUptime=0;
String lastBurstUTC="UNSET",lastBurstGeo;
bool lastBurstLocated=false;
bool observation=false, refinedValid=false, observing=false;
uint32_t coarseFrequency=0, refinedFrequency=0, observationAt=0;
float refinedRSSI=-120, noiseEstimate=-120;
String observationUTC, userLabel, refinementStatus;
int signalMemoryMatch=-1;
uint8_t signalMemoryScore=0;
bool signalMemoryNew=false;

uint32_t noiseStarted=0, monitorAt=0;
bool noiseReady=false;
rf::Sweep sweep;
bool sweepActive=false, finderMode=false, peakHold=false, markerBeep=false;
uint32_t sweepAt=0, markerFrequency=0, finderHits=0;
uint8_t waterfall[72][112]{}; unsigned waterfallHead=0, markerIndex=0;
struct Bookmark { uint32_t frequency=0; String label; };
Bookmark bookmarks[8]; unsigned bookmarkIndex=0;
struct Profile { const char* name; const char* detail; uint8_t band; uint32_t low,high,step; };
constexpr Profile profiles[]={{"315 ISM survey","Remotes / sensors - RSSI only",0,314000,316000,25},{"433 remote survey","433.92 MHz vicinity - RSSI only",1,433750,434100,5},{"433 sensor survey","Weather / sensors - RSSI only",1,433050,434790,25},{"433 ISM wide","General activity survey",1,433050,434790,50},{"868 EU ISM","General activity survey",2,868000,869000,25},{"915 ISM","General activity survey",3,902000,928000,100}};
void beginFine(uint32_t centre);
void saveObservation(const char* kind);
field::SignalFingerprint currentFingerprint();
void rememberCurrentSignal(float savedStrength);
void startObservationMonitor();
void startSweep(bool finder);
void saveBookmark();
String message, edit, editError, nfcUID="No tag read", nfcType="Type A / ISO 14443A", nfcDetail;
uint32_t messageAt=0;
bool editFresh=true;
constexpr const char* firmwareVersion="2.8.0";
unsigned dossierIndex=0, dossierPage=0, inspectorPage=0;
uint8_t memoryOrder[48]{}; unsigned memorySort=0, memoryCount=0; bool reviewOnly=false;
int liveMatch=-1; uint8_t liveScore=0;
uint32_t liveMatchAt=0; bool liveMatchSample=false;
void refreshMemoryOrder();
void monitorMemory(unsigned index);
constexpr const char* buildLabel=__DATE__ " " __TIME__;
uint32_t gnssSampleAt=0, gnssLogAt=0;
static constexpr uint8_t gnssHistoryCapacity=60;
uint8_t gnssVisibleHistory[gnssHistoryCapacity]{};
uint8_t gnssSnrHistory[gnssHistoryCapacity]{};
uint16_t gnssHdopHistory[gnssHistoryCapacity]{};
uint8_t gnssHistoryHead=0, gnssHistoryCount=0;

std::vector<String> fileNames;
unsigned fileIndex=0;
String logLines[4]; uint32_t logNext=0;
std::vector<uint32_t> logPages{0}; unsigned logPage=0, logColumn=0;

void toast(const String& s) { message=s; messageAt=millis(); }
void saveSettings() {
    if(!prefsReady) return;
    prefs.putUInt("band",band); prefs.putUInt("low",low); prefs.putUInt("high",high);
    prefs.putUInt("fixed",fixed); prefs.putUInt("step",step); prefs.putUInt("dwell",dwell);
    prefs.putUInt("bw",bwIndex); prefs.putInt("threshold",threshold);
    prefs.putInt("brightness",brightness); prefs.putInt("volume",volume);
    prefs.putBool("sound",sound); prefs.putBool("autolog",autoLog);
    prefs.putBool("markbeep",markerBeep);
    for(unsigned i=0;i<8;++i) {
        prefs.putUInt(("bmF"+String(i)).c_str(),bookmarks[i].frequency);
        prefs.putString(("bmL"+String(i)).c_str(),bookmarks[i].label);
    }
}
void defaultsForBand(unsigned b) {
    band=b; fixed=rf::bands[b].centre; low=rf::bands[b].scanLow; high=rf::bands[b].scanHigh;
}
void loadSettings() {
    prefsReady=prefs.begin("rfexplorer",false);
    if(!prefsReady) return;
    band=prefs.getUInt("band",1); if(band>3) band=1;
    defaultsForBand(band);
    low=prefs.getUInt("low",low); high=prefs.getUInt("high",high);
    fixed=prefs.getUInt("fixed",fixed); step=prefs.getUInt("step",100);
    if(!rf::validRange(band,low,high,step)) { defaultsForBand(band); step=100; }
    if(!rf::inBand(band,fixed)) fixed=rf::bands[band].centre;
    dwell=constrain(prefs.getUInt("dwell",40),20u,1000u);
    bwIndex=prefs.getUInt("bw",2); if(bwIndex>3) bwIndex=2;
    threshold=constrain(prefs.getInt("threshold",-75),-110,-20);
    brightness=constrain(prefs.getInt("brightness",170),30,250);
    volume=constrain(prefs.getInt("volume",70),0,200);
    sound=prefs.getBool("sound",true); autoLog=prefs.getBool("autolog",true);
    markerBeep=prefs.getBool("markbeep",false);
    for(unsigned i=0;i<8;++i) {
        bookmarks[i].frequency=prefs.getUInt(("bmF"+String(i)).c_str(),0);
        bookmarks[i].label=prefs.getString(("bmL"+String(i)).c_str(),"");
        if(bookmarks[i].frequency && !rf::inBand(band,bookmarks[i].frequency)) bookmarks[i].frequency=0;
    }
}
String fitLine(const String& value,int maxWidth,bool large=false) {
    if(large) canvas.setFont(&fonts::Font2); else canvas.setFont(&fonts::Font0); canvas.setTextSize(1);
    if(canvas.textWidth(value)<=maxWidth) return value;
    String out=value;
    while(out.length()>1 && canvas.textWidth(out+"...")>maxWidth) out.remove(out.length()-1);
    return out+"...";
}
void tiny(const String& s,int x,int y,uint16_t color=WHITE,int maxWidth=-1) {
    canvas.setFont(&fonts::Font0); canvas.setTextSize(1); canvas.setTextColor(color);
    if(maxWidth<0) maxWidth=238-x;
    canvas.drawString(fitLine(s,maxWidth,false),x,y);
}
void text(const String& s,int x,int y,uint16_t color=WHITE,int maxWidth=-1) {
    canvas.setFont(&fonts::Font2); canvas.setTextSize(1); canvas.setTextColor(color);
    if(maxWidth<0) maxWidth=238-x;
    canvas.drawString(fitLine(s,maxWidth,true),x,y);
}
void header(const String& title) {
    canvas.fillScreen(BG); canvas.fillRect(0,0,240,20,PANEL);
    canvas.drawFastHLine(0,19,240,ui::theme.edge);
    canvas.fillRect(0,0,3,20,CYAN);
    ui::headerTexture(canvas);
    tiny(title,7,6,CYAN,143);
    // G: fresh GNSS fix; S: SD; L: automatic event logging; battery outline.
    tiny("G",155,6,gps.freshFix()?GREEN:MUTED,7);
    tiny("S",167,6,logs.ready?GREEN:AMBER,7);
    tiny("L",179,6,logs.ready&&autoLog?GREEN:MUTED,7);
    int battery=batteryGauge.value();
    canvas.drawRect(193,5,15,9,battery>=0&&battery<20?AMBER:MUTED);
    canvas.fillRect(208,8,2,3,MUTED);
    if(battery>=0) canvas.fillRect(195,7,constrain(battery,0,100)*11/100,5,battery<20?AMBER:GREEN);
    tiny(battery<0?"?":String(battery),213,6,WHITE,25);
    if(charging) canvas.drawFastHLine(195,15,11,CYAN);
}
void footer(const String& help) {
    canvas.fillRect(0,121,240,14,PANEL); canvas.drawFastHLine(0,121,240,ui::theme.edge);
    tiny(help,4,123,MUTED,232);
}
void monitorFooter(const String& first,const String& second) {
    canvas.fillRect(0,111,240,24,PANEL);
    canvas.drawFastHLine(0,111,240,ui::theme.edge);
    tiny(first,4,114,MUTED,232);
    tiny(second,4,124,MUTED,232);
}
void row(const String& label,unsigned i,unsigned start,bool chosen) {
    int y=24+(i-start)*18;
    if(chosen) { canvas.fillRoundRect(4,y,232,18,3,ui::theme.raised); canvas.fillRect(4,y+3,2,12,CYAN); }
    text(label,11,y,chosen?CYAN:WHITE,216);
}
void card(int x,int y,int w,int h,bool active=false) { ui::card(canvas,x,y,w,h,active); }
String matchCaption() {
    if(!liveMatchSample) return "No qualified completed burst";
    if(liveMatch<0) return "Last burst: no stored candidate";
    if(liveScore<75) return "Last burst: weak/legacy similarity";
    return "Last burst similarity "+String(liveScore)+"/100";
}
String mhz(uint32_t f) { return String(f/1000.0f,3); }
satrf::Archive satelliteRFArchive;satrf::Window satelliteRFWindow;
uint32_t satelliteRFNominal=satrf::issFrequency,satelliteRFTuned=0,satelliteRFAt=0,satelliteRFSettle=0;
unsigned satelliteRFTarget=0,satelliteRFRow=0,satelliteRFPage=0;
int satelliteRFOffset=0,satelliteRFGate=-75;
bool satelliteRFPaused=false,satelliteRFAuto=true,satelliteRFHave=false;
float satelliteRFValue=NAN;
String satelliteRFStatus="No completed window";
observe::Store observations;observe::Timeline rfTimeline;observe::Profile rfProfile;
String rfSessionName="Observation",rfAntenna="Unknown",rfNote;
uint32_t rfActiveSession=0,rfSelectedSession=0,rfEditSession=0,rfHistorySession=0,rfLastSaveUs=0;
unsigned rfProfileIndex=0,rfPhase=0,rfSetupPage=0,rfLivePage=0,rfSessionPage=0,rfComparePhase=1;
bool rfPinnedOnly=false,rfHaveCompleted=false;satrf::Record rfCompleted,rfCache[4];unsigned rfCacheStart=0,rfCacheCount=0;
const char* rfPhaseName(unsigned phase){return phase==0?"Before":phase==1?"During":"After";}
uint32_t rfUTC(){time_t t=time(nullptr);return t>=1704067200?uint32_t(t):0;}
unsigned rfHistoryCount(){if(rfPinnedOnly)return observations.bookmarks();if(rfHistorySession){auto* session=observations.find(rfHistorySession);return session?session->total:0;}return satelliteRFArchive.count();}
void refreshRFHistory(){
    unsigned count=rfHistoryCount();if(count&&satelliteRFRow>=count)satelliteRFRow=count-1;rfCacheStart=satelliteRFRow/4*4;rfCacheCount=0;
    if(rfHistorySession&&!rfPinnedOnly){rfCacheCount=observations.page(rfHistorySession,rfCacheStart,rfCache,4);return;}
    for(unsigned i=rfCacheStart;i<count&&i<rfCacheStart+4;++i){const auto* r=rfPinnedOnly?observations.pin(i):satelliteRFArchive.get(i);if(r)rfCache[rfCacheCount++]=*r;}
}
const satrf::Record* rfHistoryRecord(unsigned index){return index>=rfCacheStart&&index<rfCacheStart+rfCacheCount?&rfCache[index-rfCacheStart]:nullptr;}
void loadRFProfile(unsigned index){rfProfileIndex=index;rfProfile=observations.profile(index);satelliteRFNominal=rfProfile.frequency;satelliteRFTarget=index?1:0;rfSetupPage=0;}
bool tuneSatelliteRF(){
    rfTimeline.add(millis(),NAN,2);
    satelliteRFTuned=uint32_t(int64_t(satelliteRFNominal)+satelliteRFOffset);
    satelliteRFWindow.reset();satelliteRFHave=false;
    if(!satrf::allowed(satelliteRFTuned)||!rx.tune(1,satelliteRFTuned,58.0f)){rx.stop();satelliteRFPaused=true;toast("RF tune failed / reception stopped");return false;}
    satelliteRFSettle=millis();return true;
}
void startSatelliteRF(){
    if(gnssOnly){toast("Disable GNSS-only in Expedition");return;}
    if(!rx.available()){toast("CC1101 unavailable / check Cap");return;}
    observing=false;sweepActive=false;rx.stop();satelliteRFPaused=false;satelliteRFOffset=0;satelliteRFWindow.reset();satelliteRFStatus="No completed window";
    rfProfile.frequency=satelliteRFNominal;rfActiveSession=observations.create(rfSessionName,rfAntenna,rfNote,rfProfile,rfUTC());
    if(!rfActiveSession){satelliteRFStatus="LIVE ONLY / no session saved";toast(observations.count()>=12?"12 sessions / live only":"SD unavailable / live only");}
    rfPhase=0;rfHaveCompleted=false;rfTimeline={};rfLivePage=0;
    if(tuneSatelliteRF())screen=Screen::SatRFLive;else {observations.finish(rfActiveSession,rfUTC());rfActiveSession=0;}
}
void sampleSatelliteRF(){
    if(screen!=Screen::SatRFLive||satelliteRFPaused)return;uint32_t now=millis();
    if(uint32_t(now-satelliteRFSettle)<50||uint32_t(now-satelliteRFAt)<50)return;satelliteRFAt=now;
    float value=rx.rssi();satelliteRFHave=std::isfinite(value);satelliteRFValue=value;
    if(!satelliteRFHave){rfTimeline.add(now,NAN,1);satelliteRFWindow.reset();satelliteRFStatus="Invalid sample / window discarded";return;}
    rfTimeline.add(now,value);
    satelliteRFWindow.add(now,value,satelliteRFGate);
    if(!satelliteRFWindow.ready())return;
    if(!satelliteRFWindow.valid()){satelliteRFStatus="Sampling gap / window not saved";satelliteRFWindow.reset();return;}
    if(satelliteRFAuto&&rfActiveSession){
        auto r=satelliteRFWindow.record(satelliteRFTuned,satelliteRFTarget,satelliteRFGate);
        time_t utc=time(nullptr);r.utc=utc>=1704067200?uint32_t(utc):0;r.session=rfActiveSession;r.phase=rfPhase;r.priorSaveUs=rfLastSaveUs;
        if(gps.freshFix()){r.located=true;r.latitude=int32_t(lround(gps.fix().latitude*1e6));r.longitude=int32_t(lround(gps.fix().longitude*1e6));r.fixAge=uint32_t(now-gps.fix().lastFix);if(gps.freshHdop()&&isfinite(gps.fix().hdop)&&gps.fix().hdop>0&&gps.fix().hdop<655.35f)r.hdop100=uint32_t(lround(gps.fix().hdop*100));}
        uint32_t saving=micros();bool sessionOK=observations.append(r);bool ok=sessionOK&&satelliteRFArchive.add(r);if(sessionOK){rfCompleted=r;rfHaveCompleted=true;}satelliteRFStatus=ok?"RF window saved / source unknown":"SD SAVE FAILED / pending or absent";
        if(logs.ready)logs.save("SAT_RF_OBSERVATION",r.frequency,r.mean10/10.0f,"experimental;identity=unknown;rssi_stat=mean;target="+String(r.target?"custom_UHF":"ISS_UHF_candidate")+";peak_dbm="+String(r.peak10/10.0f,1)+";samples="+String(r.samples)+";above_gate="+String(r.above)+";gate_dbm="+String(r.gate)+";max_gap_ms="+String(r.gap)+";session="+String(r.session)+";phase="+String(r.phase)+";excess_interval_ms="+String(r.excessMs),gps.csv());
        rfLastSaveUs=uint32_t(micros()-saving);
    }else satelliteRFStatus=rfActiveSession?"Window measured / auto logging off":"LIVE ONLY / no session saved";
    satelliteRFWindow.reset();
}
void stopRadio() { rx.stop(); haveSample=false;rfTrace.stop();envelope.interrupt();activityWindow.interrupt();samplingQuality.interrupt(); }
bool tune() {
    if(!rx.tune(band,frequency,observing?58.0f:bandwidths[bwIndex])) {
        toast("Radio error "+String(rx.lastError())); screen=Screen::Menu; selected=navigation.selection(); haveSample=false; return false;
    }
    tunedAt=millis(); return true;
}
void startRadio() {
    if(gnssOnly) { toast("GNSS-only mode: disable in Expedition"); return; }
    if(!rx.available()) { toast("CC1101 unavailable: reboot/check Cap"); screen=Screen::Menu; selected=navigation.selection(); return; }
    if(!rf::validRange(band,low,high,step) || !rf::inBand(band,fixed)) {
        toast("Invalid frequency settings"); screen=Screen::Menu; selected=navigation.selection(); return;
    }
    observation=false; observing=false; coarseFrequency=0; refinedValid=false; envelope={};rememberedBurst=0;lastComparison=-1;lastComparisonScore=0;lastBurstInterval=0;rfTrace.stop(); liveMatch=-1; liveScore=0; liveMatchSample=false;
    frequency=scan?low:fixed; sampledFrequency=frequency;
    peak=-120; peakFrequency=frequency; haveSample=false;
    historyHead=historyCount=0; trigger.high=false; paused=false; lastEvent=millis()-1000;
    screen=Screen::Live; tune();
}
void promptRadio(bool scanning) { if(gnssOnly) {toast("GNSS-only mode: radio suspended");return;} scan=scanning; screen=Screen::Antenna; }
void enterEdit(Field f,Screen back) {
    field=f; returnScreen=back; editError=""; editFresh=true;
    switch(f) {
        case Field::RFName: edit=rfProfile.name; break;
        case Field::RFSource: edit=rfProfile.source; break;
        case Field::RFChecked: edit=rfProfile.checked; break;
        case Field::RFSessionName: edit=rfSessionName; break;
        case Field::RFAntenna: edit=rfAntenna; break;
        case Field::RFNote: edit=rfEditSession&&observations.find(rfEditSession)?observations.find(rfEditSession)->notes:rfNote; break;
        case Field::SatFrequency: edit=mhz(satelliteRFNominal); break;
        case Field::Frequency: edit=mhz(fixed); break;
        case Field::Low: edit=mhz(low); break;
        case Field::High: edit=mhz(high); break;
        case Field::Step: edit=String(step); break;
        case Field::Threshold: edit=String(threshold); break;
        case Field::Dwell: edit=String(dwell); break;
        case Field::Clock: edit=""; break;
        case Field::Label: edit=userLabel; break;
        case Field::MemoryLabel: edit=signalMemory.entry(dossierIndex).label; break;
        case Field::MemoryNote: edit=signalMemory.entry(dossierIndex).notes; break;
        case Field::JourneyName: edit=expeditionJourney; break;
        case Field::SessionName: edit=expeditionSession; break;
        case Field::ExpeditionBookmark: edit=""; break;
        case Field::Bookmark: edit=bookmarks[bookmarkIndex].label; break;
    }
    screen=Screen::Edit;
}
const char* editTitle() {
    switch(field) {
        case Field::RFName: return "TARGET NAME";
        case Field::RFSource: return "FREQUENCY SOURCE";
        case Field::RFChecked: return "DATE CHECKED / USER NOTE";
        case Field::RFSessionName: return "OBSERVATION SESSION NAME";
        case Field::RFAntenna: return "ANTENNA / USER DESCRIPTION";
        case Field::RFNote: return "OBSERVATION NOTE";
        case Field::SatFrequency: return "CUSTOM UHF TARGET / MHz";
        case Field::Frequency: return "FIXED FREQUENCY / MHz";
        case Field::Low: return "SCAN START / MHz";
        case Field::High: return "SCAN END / MHz";
        case Field::Step: return "SCAN STEP / kHz";
        case Field::Threshold: return "THRESHOLD / dBm";
        case Field::Dwell: return "TUNE DWELL / ms";
        case Field::Label: return "OBSERVATION LABEL";
        case Field::MemoryLabel: return "RENAME DISCOVERY";
        case Field::MemoryNote: return "DOSSIER NOTE";
        case Field::JourneyName: return "JOURNEY NAME";
        case Field::SessionName: return "SESSION NAME";
        case Field::ExpeditionBookmark: return "FIELD BOOKMARK";
        case Field::Bookmark: return "BOOKMARK LABEL";
        default: return "SET UTC CLOCK";
    }
}
bool setClock(const String& s) {
    int y,m,d,h,mi,se; char rest;
    if(sscanf(s.c_str(),"%d-%d-%d %d:%d:%d%c",&y,&m,&d,&h,&mi,&se,&rest)!=6 ||
       s.length()!=19 || y<2024 || y>2099 || m<1 || m>12 || d<1 || d>31 ||
       h<0 || h>23 || mi<0 || mi>59 || se<0 || se>59) return false;
    tm t{}; t.tm_year=y-1900; t.tm_mon=m-1; t.tm_mday=d; t.tm_hour=h; t.tm_min=mi; t.tm_sec=se;
    time_t epoch=mktime(&t); tm checked{}; gmtime_r(&epoch,&checked);
    if(checked.tm_year!=y-1900 || checked.tm_mon!=m-1 || checked.tm_mday!=d) return false;
    timeval tv{epoch,0}; return settimeofday(&tv,nullptr)==0;
}
void commitEdit() {
    if(field==Field::RFName||field==Field::RFSource||field==Field::RFChecked||field==Field::RFSessionName||field==Field::RFAntenna||field==Field::RFNote){
        if((field==Field::RFName||field==Field::RFSessionName)&&!edit.length()){editError="Name cannot be empty";return;}
        if(field==Field::RFName)rfProfile.name=edit;if(field==Field::RFSource)rfProfile.source=edit;if(field==Field::RFChecked)rfProfile.checked=edit;
        if(field==Field::RFSessionName)rfSessionName=edit;if(field==Field::RFAntenna)rfAntenna=edit.length()?edit:String("Unknown");
        if(field==Field::RFNote){if(rfEditSession){toast(observations.note(rfEditSession,edit)?"Note saved":"SD note save failed");rfEditSession=0;}else rfNote=edit;}
        screen=returnScreen;return;
    }
    if(field==Field::ExpeditionBookmark) {
        if(!journeyRecorder.active()) {
            editError="Start a session first";
            return;
        }

        String note=edit.length()?edit:String("Field bookmark");
        bool ok=journeyRecorder.bookmark(gps,note);

        if(!ok) {
            editError=journeyRecorder.error;
            return;
        }

        screen=returnScreen;
        toast("Expedition bookmark saved");
        return;
    }

    if(field==Field::JourneyName) {
        if(journeyRecorder.active()) {
            editError="Finish active session first";
            return;
        }
        if(edit.length()==0) { editError="Journey name cannot be empty"; return; }
        expeditionJourney=edit;
        if(prefsReady) prefs.putString("expJourney",expeditionJourney);
        screen=returnScreen;
        toast("Journey name saved");
        return;
    }
    if(field==Field::SessionName) {
        if(journeyRecorder.active()) {
            editError="Finish active session first";
            return;
        }
        if(edit.length()==0) { editError="Session name cannot be empty"; return; }
        expeditionSession=edit;
        if(prefsReady) prefs.putString("expSession",expeditionSession);
        screen=returnScreen;
        toast("Session name saved");
        return;
    }
    if(field==Field::MemoryLabel || field==Field::MemoryNote) {
        bool ok=field==Field::MemoryLabel?signalMemory.rename(dossierIndex,edit):signalMemory.annotate(dossierIndex,edit);
        screen=returnScreen; toast(ok?"Dossier saved":"SD failed: change pending"); return;
    }
    if(field==Field::Label) {
        userLabel=edit; screen=returnScreen; saveObservation("SAVE");
        if(!paused) { tune(); monitorAt=millis(); envelope.interrupt();activityWindow.interrupt();samplingQuality.interrupt(); }
        return;
    }
    if(field==Field::Bookmark) {
        bookmarks[bookmarkIndex].label=edit;
        bookmarks[bookmarkIndex].frequency=markerFrequency;
        saveSettings(); screen=returnScreen; toast("Bookmark saved"); return;
    }
    if(field==Field::Clock) {
        if(!setClock(edit)) { editError="Use YYYY-MM-DD HH:MM:SS (UTC)"; return; }
        toast("UTC clock set for this boot"); screen=returnScreen; return;
    }
    char* end=nullptr; double n=strtod(edit.c_str(),&end);
    if(edit.length()==0 || end==edit.c_str() || *end || !std::isfinite(n)) { editError="Enter a valid number"; return; }
    if(field==Field::SatFrequency){if(n<387||n>464){editError="Use 387.000 to 464.000 MHz";return;}satelliteRFNominal=uint32_t(lround(n*1000));rfProfile.frequency=satelliteRFNominal;screen=Screen::SatRFSetup;return;}
    if(field==Field::Frequency || field==Field::Low || field==Field::High) {
        if(n<rf::bands[band].low/1000.0 || n>rf::bands[band].high/1000.0) { editError="Outside selected RF band"; return; }
        uint32_t f=lround(n*1000);
        if(field==Field::Low && f>high) { editError="Start must be <= end"; return; }
        if(field==Field::High && f<low) { editError="End must be >= start"; return; }
        if(field==Field::Frequency) fixed=f;
        if(field==Field::Low) low=f;
        if(field==Field::High) high=f;
    } else {
        if(floor(n)!=n) { editError="Enter a whole number"; return; }
        if(field==Field::Step) { if(n<10||n>1000) { editError="10 to 1000 kHz"; return; } step=n; }
        if(field==Field::Threshold) { if(n< -110||n> -20) { editError="-110 to -20 dBm"; return; } threshold=n; }
        if(field==Field::Dwell) { if(n<20||n>1000) { editError="20 to 1000 ms"; return; } dwell=n; }
    }
    saveSettings(); screen=returnScreen; if(screen==Screen::Menu) selected=navigation.selection(); toast("Saved");
}
void scanNFC() {
    stopRadio();
    if(!nfcAttempted) {
        nfcAttempted=true;
        auto cfg=nfc.config(); cfg.using_irq=true; cfg.irq=pins::nfcIRQ; cfg.emulation=false;
        nfc.config(cfg);
        // SAME SPI object as RadioLib/SD, but NFC transactions use mode 1.
        nfcReady=units.add(nfc,SPI,SPISettings(10000000,MSBFIRST,SPI_MODE1)) && units.begin();
    }
    nfcUID="No tag detected"; nfcType="Type A / ISO 14443A"; nfcDetail="Move tag close, press Enter";
    if(!nfcReady) { nfcUID="NFC init failed"; nfcDetail="Check Cap; reboot to retry"; return; }
    if(!nfc.enableField()) { nfcUID="NFC field error"; nfc.disableField(); return; }
    m5::nfc::a::PICC picc{};
    if(nfcA.detect(picc)) {
        nfcUID=picc.uidAsString().c_str();
        // Use only anticollision/select-derived classification. The library's broader
        // identify() routine includes WritePerso-like probes on some MIFARE Plus tags.
        // Those probes are deliberately excluded from this read-only application.
        nfcType=picc.typeAsString().c_str();
        char b[40]; snprintf(b,sizeof(b),"ATQA %04X   SAK %02X",picc.atqa,picc.sak); nfcDetail=b;
        nfcA.deactivate();
        if(sound) M5.Speaker.tone(1800,50);
    }
    if(!nfc.disableField()) { nfcDetail="Field-off error: power-cycle"; }
}
void readLog() {
    if(fileNames.empty()) return;
    if(!logs.readPage(fileNames[fileIndex],logPages[logPage],logLines,4,logNext)) toast(logs.error);
}
int satelliteFilter=-1;
unsigned satelliteRow=0,satellitePage=0;
gnss::System satelliteSystem=gnss::System::Unknown;
uint16_t satelliteId=0;
bool satelliteFromCategory=false;
bool satelliteMatches(gnss::System system) {return satelliteFilter<0 || int(system)==satelliteFilter;}
unsigned satelliteRows(bool archive) {
    unsigned count=0;
    if(archive){for(unsigned i=0;i<satLog.count();++i){const auto* e=satLog.get(i);if(e->confirmedReports&&satelliteMatches(e->system))++count;}}
    else for(unsigned i=0;i<gps.count();++i){const auto& e=gps.satellites()[i];if(gnss::detected(e,millis())&&satelliteMatches(e.system))++count;}
    return count;
}
const gnss::Satellite* satelliteLive(unsigned row) {
    for(unsigned i=0;i<gps.count();++i){const auto& e=gps.satellites()[i];if(gnss::detected(e,millis())&&satelliteMatches(e.system)){if(!row--)return &e;}}
    return nullptr;
}
const gnss::LogEntry* satelliteSaved(unsigned row) {
    for(unsigned i=0;i<satLog.count();++i){const auto* e=satLog.get(i);if(e->confirmedReports&&satelliteMatches(e->system)){if(!row--)return e;}}
    return nullptr;
}
String satelliteMenuLabel(unsigned action) {
    if(action==34)return "Experimental satellite RF";
    if(action==31)return "All detected / live";
    if(action==32)return "Detection history / SD";
    if(action==33)return "Capabilities / accuracy";
    unsigned count=0;for(unsigned i=0;i<gps.count();++i){const auto& e=gps.satellites()[i];if(int(e.system)==int(action)-25&&gnss::detected(e,millis()))++count;}
    return String(gnss::systemTitle(gnss::System(action-25)))+"  "+String(count)+" detected";
}
void selectMenu() {
    if(navigation.home) { navigation.open(selected);selected=navigation.selection();return; }
    navigation.remember(selected);
    unsigned action=navigation.action(selected);
    if(action==34){screen=Screen::SatRFMenu;satelliteRFRow=0;return;}
    if(action>=25 && action<=33){satelliteRow=0;satellitePage=0;satelliteFilter=action<=30?int(action)-25:-1;
        screen=action==33?Screen::SatCapabilities:action==32?Screen::SatArchive:Screen::SatCategory;return;}
    switch(action) {
        case 0: promptRadio(true); break;
        case 1: promptRadio(false); break;
        case 2: screen=Screen::Bands; selected=band; break;
        case 3: enterEdit(Field::Frequency,Screen::Menu); break;
        case 4: enterEdit(Field::Low,Screen::Menu); break;
        case 5: enterEdit(Field::High,Screen::Menu); break;
        case 6: enterEdit(Field::Step,Screen::Menu); break;
        case 7: startSweep(false); break;
        case 8: startSweep(true); break;
        case 9: screen=Screen::Bookmarks; bookmarkIndex=0; break;
        case 10: screen=Screen::GPS;gnssParent=Screen::GPS;break;
        case 11: screen=Screen::Expedition; break;
        case 12: screen=Screen::NMEA;selected=0;gnssParent=Screen::Menu;break;
        case 13: screen=Screen::Catalogue; selected=0; break;
        case 14: screen=Screen::NFC; break;
        case 15: fileNames=logs.files(); fileIndex=0; screen=Screen::Files; break;
        case 16: screen=Screen::Settings; selected=0; break;
        case 17: screen=Screen::Hunt; break;
        case 18: screen=Screen::Watch; break;
        case 19: screen=Screen::Survey; break;
        case 24: reviewOnly=true; screen=Screen::Library; selected=0; refreshMemoryOrder(); break;
        case 20: reviewOnly=false; screen=Screen::Library; selected=0; refreshMemoryOrder(); break;
        case 21: screen=Screen::Summary; break;
        case 22: screen=Screen::About; break;
        case 23: screen=Screen::GnssDiag;gnssParent=Screen::Menu;break;
    }
}
void selectSetting() {
    switch(selected) {
        case 0: enterEdit(Field::Threshold,Screen::Settings); return;
        case 1: enterEdit(Field::Dwell,Screen::Settings); return;
        case 2: bwIndex=(bwIndex+1)%4; break;
        case 3: sound=!sound; break;
        case 4: autoLog=!autoLog; break;
        case 5: brightness+=40; if(brightness>250) brightness=50; M5.Display.setBrightness(brightness); break;
        case 6: volume+=40; if(volume>200) volume=0; M5.Speaker.setVolume(volume); break;
        case 7: enterEdit(Field::Clock,Screen::Settings); return;
    }
    saveSettings();
}
void startSweep(bool finder) {
    if(gnssOnly) { toast("GNSS-only mode: disable in Expedition"); return; }
    if(!rx.available()) { toast("CC1101 unavailable: reboot/check Cap"); return; }
    // Looking Glass needs a finite screen width. Increase the configured step if needed.
    uint32_t utilityStep=step;
    const uint32_t span=high-low;
    if(span/utilityStep+1>rf::Sweep::maxBins) utilityStep=(span+rf::Sweep::maxBins-2)/(rf::Sweep::maxBins-1);
    if(!sweep.begin(band,low,high,utilityStep)) { toast("Invalid / too wide range"); return; }
    finderMode=finder; sweepActive=true; paused=false; observation=false; observing=false;
    finderHits=0; waterfallHead=0; memset(waterfall,0,sizeof(waterfall)); markerIndex=sweep.count/2; markerFrequency=sweep.frequencies[markerIndex];
    frequency=sweep.current(); screen=finder?Screen::Finder:Screen::Glass;
    if(!rx.tune(band,frequency,58.0f)) { sweepActive=false; screen=Screen::Menu; selected=navigation.selection(); toast("Radio tune failed"); return; }
    tunedAt=sweepAt=millis();
}
void saveBookmark() {
    if(!markerFrequency) return;
    for(unsigned i=0;i<8;++i) if(!bookmarks[i].frequency) { bookmarkIndex=i; enterEdit(Field::Bookmark,screen); return; }
    bookmarkIndex=0; enterEdit(Field::Bookmark,screen);
}
uint16_t constellationColor(gnss::System system) {
    switch(system) {
        case gnss::System::GPS:return GREEN;
        case gnss::System::GLONASS:return 0xf81f;
        case gnss::System::Galileo:return CYAN;
        case gnss::System::BeiDou:return AMBER;
        case gnss::System::QZSS:return WHITE;
        case gnss::System::SBAS:return 0x7bef;
        default:return MUTED;
    }
}
String gnssSnapshotNote() {
    const auto& f=gps.fix();
    String note="LISTED GPS="+String(f.gps)+" GLO="+String(f.glonass)+" GAL="+String(f.galileo)+" BDS="+String(f.beidou)+" QZS="+String(f.qzss)+" SBA="+String(f.sbas)+" REPORTS=ID:CN0_DBHZ:DETECTED=";
    const auto* sats=gps.satellites();
    for(uint8_t i=0;i<gps.count();++i) {
        if(note.length()>420) { note+="..."; break; }
        if(i) note+=';';
        note+=String(gnss::name(sats[i].system))+String(sats[i].prn)+":"+String(sats[i].snr)+":"+String(gnss::detected(sats[i],millis())?1:0);
    }
    return note;
}
void sampleGnssFeatures() {
    uint32_t now=millis();
    if(!rf::elapsed(now,gnssSampleAt,2000)) return;
    gnssSampleAt=now;
    const auto* sats=gps.satellites(); const uint8_t count=gps.count();
    int total=0, valid=0;
    for(uint8_t i=0;i<count;++i) if(gnss::detected(sats[i],now)) { total+=sats[i].snr; ++valid; }
    gnssVisibleHistory[gnssHistoryHead]=count;
    gnssSnrHistory[gnssHistoryHead]=valid?uint8_t(constrain(total/valid,0,99)):255;
    gnssHdopHistory[gnssHistoryHead]=gps.freshHdop()&&isfinite(gps.fix().hdop)?uint16_t(constrain(int(gps.fix().hdop*10.0f),0,999)):65535;
    gnssHistoryHead=(gnssHistoryHead+1)%gnssHistoryCapacity;
    if(gnssHistoryCount<gnssHistoryCapacity) ++gnssHistoryCount;

    gnss::System newSystem; uint16_t newPrn=0;
    if(count && satLog.update(sats,count,newSystem,newPrn,gps.freshFix(),gps.fix().latitude,gps.fix().longitude) && newSystem!=gnss::System::Unknown) {
        toast("NEW "+String(gnss::name(newSystem))+" "+String(newPrn));
        if(sound) M5.Speaker.tone(1450,35);
        satLog.save(true);
    } else satLog.save(false);

    if(logs.ready && count && rf::elapsed(now,gnssLogAt,30000)) {
        gnssLogAt=now;
        logs.save("GNSS",0,0,gnssSnapshotNote(),gps.csv());
    }
}
String epochLabel(uint32_t epoch) {
    if(!epoch) return "UNSET";
    time_t t=epoch; tm utc{}; gmtime_r(&t,&utc); char b[22];
    strftime(b,sizeof(b),"%Y-%m-%d %H:%M",&utc); return String(b);
}
void goBack() {
    if(screen==Screen::RFSession){screen=Screen::RFSessions;satelliteRFRow=0;return;}
    if(screen==Screen::RFProfiles||screen==Screen::RFSessions){screen=Screen::SatRFMenu;satelliteRFRow=0;return;}
    if(screen==Screen::SatRFHistory&&rfHistorySession){screen=Screen::RFSession;return;}
    if(screen==Screen::SatRFRecord){screen=Screen::SatRFHistory;return;}
    if(screen==Screen::SatRFLive){rx.stop();satelliteRFWindow.reset();satelliteRFHave=false;if(rfActiveSession&&!observations.finish(rfActiveSession,rfUTC()))toast("Session close save failed");rfActiveSession=0;screen=Screen::SatRFMenu;satelliteRFRow=0;return;}
    if(screen==Screen::SatRFSetup||screen==Screen::SatRFHistory||screen==Screen::SatRFLimits){screen=Screen::SatRFMenu;satelliteRFRow=0;return;}
    if(screen==Screen::SatRFMenu){screen=Screen::Menu;selected=navigation.selection();return;}
    if(screen==Screen::SatRecord){screen=Screen::SatArchive;return;}
    if(screen==Screen::SatDetail&&satelliteFromCategory){screen=Screen::SatCategory;return;}
    if(screen==Screen::SatArchive&&satelliteFilter>=0){screen=Screen::SatCategory;return;}
    if(screen==Screen::SatCategory||screen==Screen::SatArchive||screen==Screen::SatCapabilities){screen=Screen::Menu;selected=navigation.selection();return;}
    if(screen==Screen::Menu) { navigation.remember(selected);navigation.back();selected=navigation.selection();return; }
    if(journalDirty) saveJournal();
    if(screen==Screen::RfDiagnostics) {screen=Screen::Inspector;return;}
    if(screen==Screen::Trail || screen==Screen::TravelStatus) {screen=Screen::Expedition;return;}
    if(screen==Screen::SatLocation) {screen=Screen::GnssLogbook;return;}
    if(screen==Screen::Dossier) { screen=Screen::Library; refreshMemoryOrder(); return; }
    if(screen==Screen::Edit) { screen=returnScreen; if(screen==Screen::Menu) selected=navigation.selection(); if(field==Field::Label && !paused) { tune(); monitorAt=millis(); envelope.interrupt();activityWindow.interrupt();samplingQuality.interrupt(); } return; }
    if(screen==Screen::Log) { screen=Screen::Files; return; }
    if(screen==Screen::SatDetail) { screen=Screen::Satellites; return; }
    if(screen==Screen::Satellites || screen==Screen::NMEA || screen==Screen::GnssHistory || screen==Screen::GnssDiag || screen==Screen::GnssLogbook || screen==Screen::BuildInfo) { screen=gnssParent; if(screen==Screen::Menu) selected=navigation.selection(); return; }
    if(screen==Screen::Live || screen==Screen::Fine || screen==Screen::Inspector || screen==Screen::Glass || screen==Screen::Finder) { stopRadio(); observing=false; sweepActive=false; }
    if(screen==Screen::NFC && nfcReady) nfc.disableField();
    screen=Screen::Menu; selected=navigation.selection();
}
void handleKeys() {
    if(!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return;
    auto k=M5Cardputer.Keyboard.keysState();
    bool back=false, up=false, down=k.tab, left=false, right=false;
    for(auto h:k.hid_keys) { back|=h==0x29; up|=h==0x52; down|=h==0x51; left|=h==0x50; right|=h==0x4f; }
    for(char c:k.word) {
        back|=c=='`';
        if(screen!=Screen::Edit) { up|=c==';'; down|=c=='.'; left|=c==','; right|=c=='/'; }
    }
    if(back) { goBack(); return; }
    if(screen==Screen::Edit) {
        if(k.del) { edit.remove(edit.length()?edit.length()-1:0); editFresh=false; }
        for(char c:k.word) if(((field==Field::RFName || field==Field::RFSource || field==Field::RFChecked || field==Field::RFSessionName || field==Field::RFAntenna || field==Field::RFNote || field==Field::Label || field==Field::Bookmark || field==Field::MemoryLabel || field==Field::MemoryNote || field==Field::JourneyName || field==Field::SessionName || field==Field::ExpeditionBookmark) && c>=32 && c<=126) || (c>='0'&&c<='9') || c=='.' || c=='-' || c==':' || c==' ') {
            if(editFresh) { edit=""; editFresh=false; }
            if(edit.length()<24) edit+=c;
        }
        if(k.enter) commitEdit(); return;
    }
    if(screen==Screen::SatRFMenu){
        if(up)satelliteRFRow=(satelliteRFRow+5)%6;if(down)satelliteRFRow=(satelliteRFRow+1)%6;
        if(k.enter){unsigned choice=satelliteRFRow;satelliteRFRow=0;rfHistorySession=0;rfPinnedOnly=false;
            if(choice==0){loadRFProfile(0);screen=Screen::SatRFSetup;}
            if(choice==1)screen=Screen::RFProfiles;
            if(choice==2){screen=Screen::SatRFHistory;refreshRFHistory();}
            if(choice==3)screen=Screen::RFSessions;
            if(choice==4){rfPinnedOnly=true;screen=Screen::SatRFHistory;refreshRFHistory();}
            if(choice==5)screen=Screen::SatRFLimits;
        }return;
    }
    if(screen==Screen::RFProfiles){if(up&&satelliteRFRow)--satelliteRFRow;if(down&&satelliteRFRow<3)++satelliteRFRow;if(k.enter){loadRFProfile(satelliteRFRow);screen=Screen::SatRFSetup;}return;}
    if(screen==Screen::SatRFSetup){
        if(left||right)rfSetupPage^=1;if(k.enter){startSatelliteRF();return;}
        for(char c:k.word){if(c=='f'&&rfProfileIndex)enterEdit(Field::SatFrequency,Screen::SatRFSetup);if(c=='n')enterEdit(Field::RFName,Screen::SatRFSetup);if(c=='u')enterEdit(Field::RFSource,Screen::SatRFSetup);if(c=='d')enterEdit(Field::RFChecked,Screen::SatRFSetup);
            if(c=='p'){rfProfile.frequency=satelliteRFNominal;toast(observations.saveProfile(rfProfileIndex,rfProfile)?"Target profile saved":"SD profile save failed");}
            if(c=='j')enterEdit(Field::RFSessionName,Screen::SatRFSetup);if(c=='a')enterEdit(Field::RFAntenna,Screen::SatRFSetup);if(c=='t'){rfEditSession=0;enterEdit(Field::RFNote,Screen::SatRFSetup);}}
        return;
    }
    if(screen==Screen::SatRFLive){
        if(k.enter){satelliteRFPaused=!satelliteRFPaused;satelliteRFHave=false;satelliteRFWindow.reset();rfTimeline.add(millis(),NAN,3);if(satelliteRFPaused)rx.stop();else tuneSatelliteRF();}
        if(left||right){int offset=satelliteRFOffset+(right?1:-1);int64_t khz=int64_t(satelliteRFNominal)+offset;if(offset>=-15&&offset<=15&&khz>=387000&&khz<=464000){satelliteRFOffset=offset;if(!satelliteRFPaused)tuneSatelliteRF();else satelliteRFTuned=uint32_t(khz);}}
        if(up||down){satelliteRFGate=constrain(satelliteRFGate+(up?1:-1),-140,20);satelliteRFWindow.reset();rfTimeline.add(millis(),NAN,4);}
        for(char c:k.word){
            if(c=='a'){satelliteRFAuto=!satelliteRFAuto;satelliteRFWindow.reset();rfTimeline.add(millis(),NAN,1);}
            if(c=='d')rfLivePage^=1;
            if(c=='c'){rfPhase=(rfPhase+1)%3;satelliteRFWindow.reset();rfTimeline.add(millis(),NAN,4);}
            if(c=='b'){satelliteRFWindow.reset();rfTimeline.add(millis(),NAN,1);toast(!rfHaveCompleted?"Wait for a saved window":observations.bookmark(rfCompleted)?"Last saved window bookmarked":"Bookmark full / SD failure");}
            if(c=='l'){rx.stop();satelliteRFWindow.reset();satelliteRFHave=false;rfHistorySession=rfActiveSession;rfSelectedSession=rfActiveSession;if(rfActiveSession&&!observations.finish(rfActiveSession,rfUTC()))toast("Session close save failed");rfActiveSession=0;rfPinnedOnly=false;screen=Screen::SatRFHistory;satelliteRFRow=0;refreshRFHistory();}
        }return;
    }
    if(screen==Screen::SatRFHistory){unsigned old=satelliteRFRow;if(up&&satelliteRFRow)--satelliteRFRow;if(down&&satelliteRFRow+1<rfHistoryCount())++satelliteRFRow;if(old!=satelliteRFRow)refreshRFHistory();if(k.enter&&rfHistoryRecord(satelliteRFRow)){screen=Screen::SatRFRecord;satelliteRFPage=0;}return;}
    if(screen==Screen::SatRFRecord){if(left||right)satelliteRFPage=(satelliteRFPage+1)%3;for(char c:k.word)if(c=='b'){const auto* r=rfHistoryRecord(satelliteRFRow);if(r)toast(observations.bookmark(*r)?"Bookmark saved":"Bookmark full / SD failure");}return;}
    if(screen==Screen::RFSessions){if(up&&satelliteRFRow)--satelliteRFRow;if(down&&satelliteRFRow+1<observations.count())++satelliteRFRow;if(k.enter&&observations.count()){rfSelectedSession=observations.session(observations.count()-1-satelliteRFRow)->id;rfSessionPage=0;rfComparePhase=1;screen=Screen::RFSession;}return;}
    if(screen==Screen::RFSession){if(left||right)rfSessionPage^=1;for(char c:k.word){if(c=='c')rfComparePhase=rfComparePhase==1?2:1;if(c=='h'){rfHistorySession=rfSelectedSession;rfPinnedOnly=false;satelliteRFRow=0;refreshRFHistory();screen=Screen::SatRFHistory;}if(c=='t'){rfEditSession=rfSelectedSession;enterEdit(Field::RFNote,Screen::RFSession);}}return;}
    if(screen==Screen::SatRFLimits)return;
    if(screen==Screen::RfDiagnostics) {
        if(left||right)diagnosticPage=1-diagnosticPage;
        for(char c:k.word) {
            if(c=='n') {quietReference={};noiseReady=false;envelope.interrupt();activityWindow.interrupt();toast("Quiet reference reset");}
            if(c=='c') {if(paused){toast("Resume sampling first");return;}rfTrace.start();envelope.interrupt();activityWindow.interrupt();samplingQuality.interrupt();toast("Capturing up to 1024 samples");}
            if(c=='x') {rfTrace.stop();rx.stop();paused=true;envelope.interrupt();activityWindow.interrupt();samplingQuality.interrupt();toast(rfTrace.save()?"Trace saved to diagnostics folder":rfTrace.error);}
        }
        if(k.enter) {paused=!paused;if(paused){rx.stop();rfTrace.stop();envelope.interrupt();activityWindow.interrupt();samplingQuality.interrupt();}else {tune();monitorAt=millis();}}
        return;
    }
    if(screen==Screen::Expedition) {
        if(k.enter) {
            if(journeyRecorder.active()) {
                bool ok=journeyRecorder.finish();
                if(ok && prefsReady) {
                    prefs.putBool("expActive",false);
                    prefs.remove("expPath");
                }
                toast(ok?"Session saved":"Finish failed: "+journeyRecorder.error);
            } else {
                bool ok=journeyRecorder.start(expeditionJourney,expeditionSession,routeInterval);
                if(ok && prefsReady) {
                    prefs.putBool("expActive",true);
                    prefs.putString("expJourney",expeditionJourney);
                    prefs.putString("expSession",expeditionSession);
                    prefs.putString("expPath",journeyRecorder.path());
                }
                expeditionInterrupted=false;
                toast(ok?
                    "Expedition recording started":
                    "Start failed: "+journeyRecorder.error);
            }
            return;
        }

        for(char c:k.word) {
            if(c=='t') {screen=Screen::Trail;return;}
            if(c=='v') {screen=Screen::TravelStatus;return;}
            if(c=='i') { if(journeyRecorder.active()) toast("Finish session to change interval");
                else {routeInterval=routeInterval==1000?5000:routeInterval==5000?10000:routeInterval==10000?30000:1000;toast("Route interval "+String(routeInterval/1000)+" seconds");} return; }
            if(c=='g') {gnssOnly=!gnssOnly;if(gnssOnly) {stopRadio();sweepActive=false;observing=false;if(nfcReady)nfc.disableField();}toast(gnssOnly?"GNSS-only: RF suspended":"RF available");return;}
            if(c=='b') {
                if(!journeyRecorder.active())
                    toast("Start session before bookmarking");
                else
                    enterEdit(Field::ExpeditionBookmark,Screen::Expedition);
                return;
            }

            if(c=='j') {
                if(journeyRecorder.active()) toast("Finish session before renaming journey");
                else enterEdit(Field::JourneyName,Screen::Expedition);
                return;
            }
            if(c=='s') {
                if(journeyRecorder.active()) toast("Finish session before renaming session");
                else enterEdit(Field::SessionName,Screen::Expedition);
                return;
            }
        }

        return;
    }

    if(screen==Screen::GPS) {
        if(k.enter) { screen=Screen::Satellites; selected=0; return; }
        for(char c:k.word) {
            if(c=='n') { screen=Screen::NMEA; selected=0; return; }
            if(c=='h') { screen=Screen::GnssHistory; return; }
            if(c=='d') { screen=Screen::GnssDiag; return; }
            if(c=='l') { screen=Screen::GnssLogbook; selected=0; return; }
            if(c=='b') { screen=Screen::BuildInfo; return; }
        }
    }
    if(screen==Screen::NMEA) { if(up&&selected) --selected; if(down&&selected+1<gps.rawCountLines()) ++selected; for(char c:k.word) if(c=='f') { gps.cycleRawFilter(); selected=0; toast(String("NMEA filter ")+gps.filterName()); } return; }
    if(screen==Screen::Satellites) {
        if(up&&selected) --selected; if(down&&selected+1<gps.count()) ++selected;
        if(k.enter && selected<gps.count()) { satelliteSystem=gps.satellites()[selected].system;satelliteId=gps.satellites()[selected].prn;satelliteFromCategory=false;screen=Screen::SatDetail; return; }
        return;
    }
    if(screen==Screen::SatCategory||screen==Screen::SatArchive){
        bool archive=screen==Screen::SatArchive;unsigned count=satelliteRows(archive);
        if(up&&satelliteRow)--satelliteRow;if(down&&satelliteRow+1<count)++satelliteRow;
        for(auto c:k.word)if(c=='l'&&!archive){screen=Screen::SatArchive;satelliteRow=0;return;}
        if(k.enter&&count){
            if(archive){const auto* e=satelliteSaved(satelliteRow);if(e){satelliteSystem=e->system;satelliteId=e->prn;satellitePage=0;screen=Screen::SatRecord;}}
            else {const auto* e=satelliteLive(satelliteRow);if(e){satelliteSystem=e->system;satelliteId=e->prn;satelliteFromCategory=true;screen=Screen::SatDetail;}}
        }return;
    }
    if(screen==Screen::SatRecord){for(auto c:k.word)if(c==','||c=='/'||c=='<'||c=='>')satellitePage^=1;return;}
    if(screen==Screen::SatCapabilities)return;
    if(screen==Screen::GnssLogbook) { if(up&&selected) --selected; if(down&&selected+1<satLog.count()) ++selected; if(k.enter&&satLog.count()) screen=Screen::SatLocation; return; }
    if(screen==Screen::SatDetail || screen==Screen::GnssHistory || screen==Screen::GnssDiag || screen==Screen::BuildInfo) return;
    if(screen==Screen::Menu || screen==Screen::Settings || screen==Screen::Bands || screen==Screen::Bookmarks || screen==Screen::Catalogue) {
        unsigned count=screen==Screen::Menu?navigation.count():screen==Screen::Settings?8:screen==Screen::Bookmarks?8:screen==Screen::Catalogue?sizeof(profiles)/sizeof(profiles[0]):4;
        if(up) selected=(selected+count-1)%count;
        if(down) selected=(selected+1)%count;
        if(screen==Screen::Menu) navigation.remember(selected);
        if(k.enter) {
            if(screen==Screen::Menu) selectMenu();
            else if(screen==Screen::Settings) selectSetting();
            else if(screen==Screen::Bookmarks) {
                if(bookmarks[selected].frequency) { fixed=bookmarks[selected].frequency; saveSettings(); scan=false; startRadio(); }
                else toast("Save a marker first");
            } else if(screen==Screen::Catalogue) {
                const Profile& p=profiles[selected]; band=p.band; low=p.low; high=p.high; step=p.step; fixed=rf::bands[band].centre; saveSettings(); startSweep(false);
            } else { defaultsForBand(selected); saveSettings(); screen=Screen::Menu; selected=navigation.selection(); toast("Band selected"); }
        }
    } else if((screen==Screen::Hunt || screen==Screen::Watch || screen==Screen::Survey) && k.enter) {
        scan=false; startRadio();
    } else if(screen==Screen::Summary) {
        if(k.enter) saveJournal();
    } else if(screen==Screen::Dossier) {
        if(left||right) dossierPage=(dossierPage+(right?1:4))%5;
        if(up && encounterSelected) --encounterSelected;
        if(down && encounterSelected+1<signalMemory.entry(dossierIndex).encounterCount) ++encounterSelected;
        if(k.enter) { monitorMemory(dossierIndex); return; }
        for(char c:k.word) {
            if(c=='q') toast(signalMemory.setFlag(dossierIndex,!signalMemory.entry(dossierIndex).flagged)?"Investigation flag saved":"SD failed: flag pending");
            if(c=='r') toast(signalMemory.setReviewed(dossierIndex,!signalMemory.entry(dossierIndex).reviewed)?"Review status saved":"SD failed: review pending");
            if(c=='x') {
                const auto& e=signalMemory.entry(dossierIndex); field::Families f;f.build(signalMemory);
                String note="dossier="+String(dossierIndex+1)+";label="+e.label+";tag="+field::categoryName(e.category)+
                    ";notes="+e.notes+";saved="+String(e.sightings)+";first_utc="+String(e.firstUTC)+";last_utc="+String(e.lastUTC)+
                    ";evidence_version="+String(e.fingerprint.evidence_version)+";candidate_family="+String(f.anchor[dossierIndex]+1)+";family_rule=250kHz_duration_ratio2;identity=unknown;baseline_n="+String(e.baselineCount)+
                    ";baseline_dbm="+String(e.baselineRSSI)+";baseline_utc="+String(e.baselineUTC);
                toast(logs.save("DOSSIER",e.fingerprint.frequency_khz,e.strongest,note)?"Dossier exported to session CSV":logs.error);
            }
            if(c=='b') { const auto& e=signalMemory.entry(dossierIndex); toast(e.encounterCount<6?"Need 6 saved encounters":signalMemory.captureBaseline(dossierIndex,time(nullptr)>=1704067200?uint32_t(time(nullptr)):0)?"Baseline saved":"SD failed: baseline pending"); }
            if(c=='n') enterEdit(Field::MemoryLabel,Screen::Dossier);
            if(c=='t') enterEdit(Field::MemoryNote,Screen::Dossier);
            if(c=='c') toast(signalMemory.classify(dossierIndex,(signalMemory.entry(dossierIndex).category+1)%6)?"Category saved":"SD failed: change pending");
        }
    } else if(screen==Screen::Library) {
        unsigned count=memoryCount;
        if(count) {
            if(up) selected=(selected+count-1)%count;
            if(down) selected=(selected+1)%count;
        if(screen==Screen::Menu) navigation.remember(selected);
            if(k.enter) { monitorMemory(memoryOrder[selected]); return; }
            for(char c:k.word) {
                if(c=='i') { dossierIndex=memoryOrder[selected]; dossierPage=0; encounterSelected=0; screen=Screen::Dossier; }
                if(c=='o') { memorySort=(memorySort+1)%4; selected=0; refreshMemoryOrder(); }
            }
        } else if(k.enter) toast("Signal memory empty");
    } else if(screen==Screen::Antenna && k.enter) startRadio();
    else if(screen==Screen::Live || screen==Screen::Inspector) {
        if(screen==Screen::Inspector && (left||right)) inspectorPage=1-inspectorPage;
        if(k.enter && screen==Screen::Inspector) { screen=Screen::Live; return; }
        if(k.enter) {
            paused=!paused;
            if(paused) { rx.stop(); envelope.interrupt();activityWindow.interrupt();samplingQuality.interrupt(); } else { haveSample=false; trigger.high=false; tune(); monitorAt=millis(); }
        }
        for(char c:k.word) {
            if(c=='d' && observation) {screen=Screen::RfDiagnostics;return;}
            if(c=='s' && haveSample) {
                if(observation) { rx.stop(); envelope.interrupt();activityWindow.interrupt();samplingQuality.interrupt(); enterEdit(Field::Label,screen); }
                else toast(logs.save("SAVE",sampledFrequency,rssi,"",gps.csv())?"Raw RSSI sample saved":logs.error);
            }
            if(c=='w' && haveSample) toast(logs.save("WAYPOINT",sampledFrequency,rssi,"manual waypoint",gps.csv())?"Waypoint saved":logs.error);
            if(c=='i' && observation) screen=Screen::Inspector;
            if(c=='a' && observation) { beginFine(coarseFrequency); return; }
            if(c=='r' && observation) { scan=true; startRadio(); return; }
            if(c=='l') { autoLog=!autoLog; saveSettings(); toast(autoLog?"Event logging on":"Event logging off"); }
            if(c=='m' && observation) { screen=Screen::Live; return; }
            if(c=='m') { if(haveSample) fixed=sampledFrequency; scan=!scan; saveSettings(); startRadio(); }
            if(c=='p' && peakFrequency) { beginFine(peakFrequency); return; }
        }
    } else if(screen==Screen::NFC) {
        if(gnssOnly) {toast("GNSS-only mode: NFC suspended");return;}
        if(k.enter || k.tab) scanNFC();
    } else if(screen==Screen::Files && !fileNames.empty()) {
        if(up) fileIndex=(fileIndex+fileNames.size()-1)%fileNames.size();
        if(down) fileIndex=(fileIndex+1)%fileNames.size();
        if(k.enter) { logPages={0}; logPage=logColumn=0; screen=Screen::Log; readLog(); }
    } else if(screen==Screen::Log) {
        if(right) logColumn+=12;
        if(left) logColumn=logColumn>=12?logColumn-12:0;
        if(logColumn>480) logColumn=480;
        if(down && logNext) {
            if(logPage+1==logPages.size()) logPages.push_back(logNext);
            ++logPage; readLog();
        }
        if(up && logPage) { --logPage; readLog(); }
    } else if(screen==Screen::Glass || screen==Screen::Finder) {
        if(k.enter) { paused=!paused; if(paused) {rx.stop();envelope.interrupt();activityWindow.interrupt();samplingQuality.interrupt();rfTrace.stop();} else { frequency=sweep.current(); rx.tune(band,frequency,58.0f); tunedAt=millis(); } }
        if(left && markerIndex) --markerIndex;
        if(right && markerIndex+1<sweep.count) ++markerIndex;
        markerFrequency=sweep.frequencies[markerIndex];
        for(char c:k.word) {
            if(c=='b') saveBookmark();
            if(c=='p') { peakHold=!peakHold; if(!peakHold) sweep.resetHeld(); }
            if(c=='r') sweep.resetHeld();
            if(c=='m') { finderMode=!finderMode; screen=finderMode?Screen::Finder:Screen::Glass; }
            if(c=='g') { markerBeep=!markerBeep; saveSettings(); toast(markerBeep?"Marker beep on":"Marker beep off"); }
            if(c=='f' && markerFrequency) { fixed=markerFrequency; scan=false; saveSettings(); startRadio(); }
        }
    }
}
void sampleRadio() {
    if(observing || (screen!=Screen::Live) || paused) return;
    uint32_t now=millis();
    if(!rf::elapsed(now,tunedAt,dwell) || !rf::elapsed(now,sampleAt,scan?dwell:50)) return;
    sampleAt=now; float measured=rx.rssi(); if(!isfinite(measured)) {haveSample=false;return;}
    rssi=measured; sampledFrequency=frequency; haveSample=true;
    history[historyHead]=rssi; historyHead=(historyHead+1)%112; if(historyCount<112) ++historyCount;
    if(rssi>peak) { peak=rssi; peakFrequency=sampledFrequency; }
    bool rising=trigger.update(rssi,threshold);
    if(scan) rising=rssi>=threshold;
    bool periodic=!scan && rssi>=threshold && rf::elapsed(now,lastEvent,5000);
    if((rising||periodic) && rf::elapsed(now,lastEvent,1000)) {
        lastEvent=now;
        if(sound && rising) M5.Speaker.tone(2200,35);
        if(autoLog && logs.ready) {
            if(logs.save("SIGNAL",sampledFrequency,rssi,scan?"scan":"monitor",gps.csv())) {
                journal.activity(sampledFrequency,int16_t(lround(rssi)));journalDirty=true;
            } else toast(logs.error);
        }
    }
    if(scan && rssi>=threshold) { beginFine(sampledFrequency); return; }
    if(scan) { frequency=rf::nextFrequency(frequency,low,high,step); tune(); }
}

field::SignalFingerprint currentFingerprint() {
    field::SignalFingerprint fp;
    fp.frequency_khz=observation?(refinedValid?refinedFrequency:coarseFrequency):sampledFrequency;
    if(!observation || !envelope.last.qualified) return fp;
    const auto& event=envelope.last;
    fp.peak_rssi=int16_t(lround(event.peak));fp.noise_rssi=int16_t(lround(event.quiet));
    fp.duration_ms=uint16_t(event.duration);fp.samples=uint16_t(event.samples>65535?65535:event.samples);
    fp.sample_gap_ms=uint16_t(event.maxGap);fp.evidence_version=1;
    fp.repeat_ms=lastBurstInterval;
    // Occupied bandwidth is not available from swept RSSI.
    return fp;
}

void saveJournal() {
    String note="activity_rows="+String(journal.activityRows)+";saved_observations="+String(journal.observations)+";new_dossiers="+String(journal.discoveries)+
        ";repeat_matches="+String(journal.repeats)+";family_candidates="+String(journal.familyCandidates)+
        ";baseline_deviations="+String(journal.anomalies)+";geo_observations="+String(journal.geotagged)+
        ";memory_full="+String(journal.unstored)+";scope=boot;interpretation=heuristic;coverage=not_continuous";
    journalAt=millis();
    bool ok=logs.save("SESSION",journal.strongestFrequency,(journal.observations||journal.activityRows)?float(journal.strongest):NAN,note);
    if(ok) journalDirty=false;
    if(screen==Screen::Summary) toast(ok?"Journal snapshot saved":logs.error);
}
field::EncounterContext encounterContext(uint32_t now,uint8_t coverage) {
    field::EncounterContext c;const auto& fix=gps.fix();
    c.located=gps.freshFix();c.fixAgeMs=c.located?uint32_t(now-fix.lastFix):0;
    if(c.located){c.latitudeE6=int32_t(lround(fix.latitude*1000000));c.longitudeE6=int32_t(lround(fix.longitude*1000000));}
    if(gps.freshHdop() && isfinite(fix.hdop) && fix.hdop>0 && fix.hdop<655.35f)c.hdop100=uint16_t(lround(fix.hdop*100));
    c.rxBandwidth10=580;c.quietAgeMs=quietReference.age(now);c.coverage=coverage;return c;
}
void saveActivityWindow(uint32_t now) {
    field::SignalFingerprint fp;fp.frequency_khz=frequency;fp.peak_rssi=int16_t(lround(activityWindow.lastPeak));fp.noise_rssi=int16_t(lround(activityWindow.lastQuiet));
    fp.duration_ms=uint16_t(activityWindow.duration);fp.samples=uint16_t(std::min(uint32_t(65535),activityWindow.lastSamples));fp.sample_gap_ms=uint16_t(activityWindow.lastGap);fp.evidence_version=2;
    auto context=encounterContext(now,activityWindow.coverage);
    String note="sampled_activity_window;duration_ms="+String(fp.duration_ms)+";coverage_percent="+String(context.coverage)+";max_gap_ms="+String(fp.sample_gap_ms)+";quiet_age_ms="+String(context.quietAgeMs)+";not_packet_count";
    uint32_t started=micros();
    if(logs.save("ACTIVITY_WINDOW",frequency,fp.peak_rssi,note,gps.csv())) {
        unsigned prior=signalMemory.count();time_t utc=time(nullptr);
        int stored=signalMemory.remember(fp,"Sustained activity",now,utc>=1704067200?uint32_t(utc):0,context);
        journal.record(fp,signalMemory.count()>prior,false,false,context.located,stored>=0);journalDirty=true;
        if(stored<0)toast("Activity logged; dossier store full");
        else if(signalMemory.writeFailed())toast("Activity dossier pending SD retry");
    } else toast(logs.error);
    samplingQuality.sdUs=std::max(samplingQuality.sdUs,uint32_t(micros()-started));
}
void rememberCurrentSignal(float savedStrength) {
    field::SignalFingerprint fp=currentFingerprint();
    if(!field::qualified(fp) || rememberedBurst==envelope.last.sequence) return;

    uint8_t score=0;
    int prior=signalMemory.bestMatch(fp,score);

    signalMemoryNew=!(prior>=0 && score>=75);
    signalMemoryScore=signalMemoryNew ? 0 : score;

    bool familyCandidate=false;
    if(signalMemoryNew) for(unsigned i=0;i<signalMemory.count();++i)
        if(field::familyCandidate(fp,signalMemory.entry(i).fingerprint)) { familyCandidate=true; break; }
    bool anomaly=prior>=0 && field::qualified(signalMemory.entry(prior).fingerprint) && !signalMemoryNew && field::deviation(signalMemory.entry(prior),fp.peak_rssi);
    int stored=signalMemory.remember(
        fp,
        userLabel,
        envelope.last.end, lastBurstEpoch, lastBurstContext
    );

    signalMemoryMatch=stored;
    if(stored>=0) rememberedBurst=envelope.last.sequence;
    journal.record(fp,signalMemoryNew,familyCandidate,anomaly,lastBurstLocated,stored>=0);
    journalDirty=true;

    if(stored<0) {
        toast("Signal memory full");
        return;
    }

    if(signalMemory.writeFailed()) { toast("Memory SD failed: pending retry"); return; }
    if(signalMemoryNew) {
        toast("NEW SIGNAL "+mhz(fp.frequency_khz));
        if(sound) M5.Speaker.tone(1650,35);
    } else {
        toast("Saved similarity "+String(signalMemoryScore)+"/100");
    }
}

void saveObservation(const char* kind) {
    if(!observation) return;
    uint32_t storageBegan=micros();
    bool qualified=field::qualified(currentFingerprint());
    const auto& event=envelope.last;
    uint64_t uptime=qualified?lastBurstUptime:uint64_t(esp_timer_get_time()/1000);
    String status=qualified?"qualified_sampled_burst":"unqualified_manual_sample";
    status+=";samples="+String(qualified?event.samples:0)+";sampling_gap_ms="+String(qualified?event.maxGap:envelope.recentGap);
    bool saved=logs.saveObservation(qualified?kind:"RAW",qualified?lastBurstUTC:utcTimestamp(),uptime,
        coarseFrequency,refinedFrequency,refinedValid,qualified?event.peak:rssi,
        qualified?event.quiet:(noiseReady?noiseEstimate:NAN),qualified?event.duration:0,envelope.count,
        userLabel,status,qualified?event.maxGap:envelope.recentGap,
        qualified?lastBurstGeo:gps.csv());
    // A delayed save must not attach today's receiver position to an old burst.
    if(saved && qualified) rememberCurrentSignal(event.peak);
    else if(saved) toast("Raw sample saved; evidence insufficient");
    else toast(logs.error);
    samplingQuality.sdUs=std::max(samplingQuality.sdUs,uint32_t(micros()-storageBegan));
}
void startObservationMonitor() {
    observing=true; paused=false; screen=Screen::Inspector;
    frequency=refinedValid?refinedFrequency:coarseFrequency;
    if(!rx.tune(band,frequency,58.0f)) {
        observing=false; screen=Screen::Menu; selected=navigation.selection(); toast("Radio tune failed"); return;
    }
    tunedAt=monitorAt=noiseStarted=millis();
    quietReference={};samplingQuality={};activityWindow={};noiseReady=false; envelope={};rememberedBurst=0;lastComparison=-1;lastComparisonScore=0;lastBurstInterval=0;rfTrace.stop();
    historyHead=historyCount=0; haveSample=false;
}
void beginFine(uint32_t centre) {
    observing=false; paused=false; observation=true; signalMemoryMatch=-1; liveMatch=-1; liveScore=0; liveMatchSample=false; inspectorPage=0;
    coarseFrequency=centre; observationAt=millis(); observationUTC=utcTimestamp();
    refinedValid=false; userLabel=""; fine.begin(band,centre);
    screen=Screen::Fine; frequency=fine.current();
    if(!rx.tune(band,frequency,58.0f)) { screen=Screen::Menu; selected=navigation.selection(); toast("Fine tune failed"); return; }
    tunedAt=millis(); refinedRSSI=-120;
}
void analyseRadio() {
    uint32_t now=millis();
    if(screen==Screen::Fine) {
        if(!rf::elapsed(now,tunedAt,15)) return;
        float value=rx.rssi(); if(!isfinite(value)) return;
        if(fine.add(value)) {
            refinedFrequency=fine.result(refinedRSSI,noiseEstimate,refinedValid);
            refinementStatus=refinedValid?"repeatable sampled peak":"uncertain / retry A";
            startObservationMonitor();
        } else {
            frequency=fine.current();
            if(!rx.tune(band,frequency,58.0f)) { screen=Screen::Menu; selected=navigation.selection(); toast("Fine tune failed"); return; }
            tunedAt=millis();
        }
        return;
    }
    if(!observing || paused || (screen!=Screen::Inspector && screen!=Screen::Live && screen!=Screen::RfDiagnostics)) return;
    if(!rf::elapsed(now,tunedAt,20) || !rf::elapsed(now,monitorAt,2)) return;
    monitorAt=now; float measured=rx.rssi(); if(!isfinite(measured)) {haveSample=false;envelope.interrupt();activityWindow.interrupt();samplingQuality.interrupt();rfTrace.add(now,frequency,NAN,NAN);return;}
    rssi=measured; sampledFrequency=frequency; haveSample=true;
    samplingQuality.sample(now);
    quietReference.sample(now,rssi,envelope.active||activityWindow.active);
    noiseReady=quietReference.ready;noiseEstimate=quietReference.level;
    bool activityReady=activityWindow.sample(now,rssi,noiseEstimate,quietReference.fresh(now));
    if(activityReady && autoLog && !rfTrace.active) saveActivityWindow(now);
    rfTrace.add(now,frequency,rssi,noiseReady?noiseEstimate:NAN);
    if(quietReference.fresh(now) || envelope.active) {
        uint32_t before=envelope.count;
        envelope.sample(now,rssi,noiseEstimate);
        if(envelope.count!=before) {
            lastBurstInterval=envelope.recurrence.count>=2?uint16_t(envelope.recurrence.mean()):0;
            // Freeze the comparison BEFORE saving. The event cannot match itself.
            lastComparison=signalMemory.bestMatch(currentFingerprint(),lastComparisonScore);
            uint32_t age=now-envelope.last.end;
            lastBurstUptime=uint64_t(esp_timer_get_time()/1000)-age;
            timeval tv{};gettimeofday(&tv,nullptr);lastBurstEpoch=0;lastBurstUTC="UNSET";
            if(tv.tv_sec>=1704067200) {
                int64_t ms=int64_t(tv.tv_sec)*1000+tv.tv_usec/1000-age;
                lastBurstEpoch=uint32_t(ms/1000);time_t t=lastBurstEpoch;tm utc{};gmtime_r(&t,&utc);
                char date[24];strftime(date,sizeof(date),"%Y-%m-%dT%H:%M:%SZ",&utc);lastBurstUTC=date;
            }
            lastBurstLocated=gps.freshFix();lastBurstGeo=gps.csv();lastBurstContext=encounterContext(now,100);
            if(autoLog && !rfTrace.active) saveObservation("BURST");
        }
    }
    if(rf::elapsed(now,sampleAt,25)) {
        sampleAt=now; history[historyHead]=rssi; historyHead=(historyHead+1)%112;
        if(historyCount<112) ++historyCount;
    }
}

void sampleSweep() {
    if(!sweepActive || paused || (screen!=Screen::Glass && screen!=Screen::Finder)) return;
    uint32_t now=millis();
    if(!rf::elapsed(now,tunedAt,18) || !rf::elapsed(now,sweepAt,18)) return;
    sweepAt=now;
    const float value=rx.rssi(); if(!isfinite(value)) return; const unsigned sampled=sweep.index;
    const bool finished=sweep.add(value);
    if(finderMode && value>=threshold) ++finderHits;
    if(sampled==markerIndex && markerBeep && value>=threshold && sound) M5.Speaker.tone(1750,18);
    if(finished) {
        float sorted[rf::Sweep::maxBins]{};
        for(unsigned i=0;i<sweep.count;++i) sorted[i]=sweep.live[i];
        rf::sortValues(sorted,sweep.count); sweep.floor=sorted[sweep.count/5];
        for(unsigned i=0;i<sweep.count;++i) waterfall[waterfallHead][i]=rf::waterfallColour(sweep.live[i],sweep.floor);
        waterfallHead=(waterfallHead+1)%72;
    }
    frequency=sweep.current();
    if(!rx.tune(band,frequency,58.0f)) { sweepActive=false; screen=Screen::Menu; selected=navigation.selection(); toast("Sweep tune failed"); }
    tunedAt=millis();
}
uint16_t waterfallPalette(uint8_t level) { return ui::waterfall(level); }
void refreshMemoryOrder() {
    unsigned n=0;
    for(unsigned i=0;i<signalMemory.count();++i)
        if(!reviewOnly || field::reviewReason(signalMemory.entry(i))) memoryOrder[n++]=i;
    memoryCount=n;
    if(selected>=n) selected=n?n-1:0;
    std::stable_sort(memoryOrder,memoryOrder+n,[](uint8_t a,uint8_t b) {
        const auto& x=signalMemory.entry(a); const auto& y=signalMemory.entry(b);
        if(memorySort==1) return x.sightings>y.sightings;
        if(memorySort==2) return x.strongest>y.strongest;
        if(memorySort==3 && field::qualified(x.fingerprint)!=field::qualified(y.fingerprint)) return !field::qualified(x.fingerprint);
        return x.fingerprint.frequency_khz<y.fingerprint.frequency_khz;
    });
}
void monitorMemory(unsigned index) {
    if(index>=signalMemory.count()) return;
    uint32_t f=signalMemory.entry(index).fingerprint.frequency_khz;
    int target=field::bandFor(f);
    if(target<0) { toast("Frequency outside CC1101 bands"); return; }
    if(unsigned(target)!=band) defaultsForBand(target);
    fixed=f; scan=false; saveSettings(); startRadio();
}
void updateLiveMatch() {
    if(screen!=Screen::Live && screen!=Screen::Inspector && screen!=Screen::RfDiagnostics) return;
    if(!rf::elapsed(millis(),liveMatchAt,250)) return;
    liveMatchAt=millis();liveMatch=-1;liveScore=0;
    auto fp=currentFingerprint();liveMatchSample=field::qualified(fp);
    if(liveMatchSample) {liveMatch=lastComparison;liveScore=lastComparisonScore;}
}

void draw() {
    switch(screen) {
        case Screen::Fine:
            header("FINE SCAN / 5 kHz STEPS");
            text("Coarse "+mhz(coarseFrequency),6,27,CYAN);
            text("Tuning "+mhz(frequency),6,49);
            tiny("Sweep "+String(fine.pass+1)+"/3   Bin "+String(fine.index+1)+"/"+String(fine.count),6,74);
            tiny("Sampled peak, not exact TX frequency",6,94,AMBER);
            footer("58 kHz RX filter       Esc Cancel"); break;
        case Screen::Inspector: {
            header(inspectorPage?"INSPECT / MATCH":"INSPECT / SIGNAL");
            card(4,24,232,28,true);
            text(mhz(refinedValid?refinedFrequency:coarseFrequency)+" MHz",10,28,CYAN,151);
            tiny(refinedValid?"SAMPLED ~":"COARSE ?",164,35,refinedValid?GREEN:AMBER,66);
            if(!inspectorPage) {
                tiny("LIVE "+(haveSample?String(rssi,1):String("--"))+" dBm  Quiet "+(noiseReady?String(noiseEstimate,1):String("?")),7,59,WHITE,226);
                if(envelope.last.qualified) {
                    tiny("LAST peak "+String(envelope.last.peak,1)+" | span "+String(envelope.last.duration)+"ms",7,74,CYAN,226);
                    tiny("Event quiet "+String(envelope.last.quiet,1)+" | "+String(envelope.last.samples)+" samples",7,87,MUTED,226);
                } else tiny("No qualified completed burst",7,77,AMBER,226);
                tiny("Qualified "+String(envelope.count)+" | unresolved "+String(envelope.unresolved),7,100,WHITE,226);
                tiny(!noiseReady?"Quiet reference: learning":!quietReference.fresh(millis())?"Quiet reference STALE / N in Timing":envelope.active?"Activity above quiet reference":"Relative RSSI, not calibrated SNR",7,111,AMBER,226);
                footer("</> Match  D Timing  S Save last");
            } else {
                tiny(matchCaption(),7,58,liveMatchSample&&liveScore>=75?GREEN:AMBER,226);
                tiny("Evidence: "+String(liveMatchSample?"qualified sampled burst":"insufficient / wait"),7,74,WHITE,226);
                if(liveMatchSample) {
                tiny("Last-event age "+String(uint32_t(millis()-envelope.last.end)/1000)+"s",7,88,MUTED,226);
                    tiny("Margin "+String(envelope.last.peak-envelope.last.quiet,1)+"dB; max gap "+String(envelope.last.maxGap)+"ms",7,100,MUTED,226);
                }
                tiny("Similarity is not probability or ID",7,111,AMBER,226);
                footer("</> Signal  D Timing  S Save last");
            }
            break;
        }
        case Screen::RfDiagnostics: {
            header("RF / SAMPLING EVIDENCE");
            if(diagnosticPage==0) {
            tiny("Sample gap now "+String(envelope.recentGap)+" ms",7,27,WHITE,226);
            tiny("Max sample gap "+String(envelope.maxGap)+" ms",7,41,MUTED,226);
            tiny("Gaps >20ms "+String(envelope.gapCount)+" | rejected "+String(envelope.unresolved),7,55,AMBER,226);
            const auto& r=envelope.recurrence;
            tiny(r.count>=2?"Start interval mean "+String(r.mean())+"ms":"Recurrence: insufficient intervals",7,69,WHITE,226);
            tiny(r.count>=2?"Range "+String(r.span())+"ms over "+String(r.count)+" intervals":"No cadence or transmitter claim",7,83,MUTED,226);
            tiny(String(rfTrace.active?"CAPTURING ":"Trace buffer ")+String(rfTrace.count)+"/1024",7,98,CYAN,226);
            tiny(paused?"PAUSED | Enter resumes":envelope.gapCount?"Sampling gaps reduce evidence":"Timing regularity alone proves nothing",7,110,MUTED,226);
            } else {
                tiny("Gap mean "+String(samplingQuality.mean())+" max "+String(samplingQuality.maxGap)+"ms",7,27,WHITE,226);
                tiny("Intervals "+String(samplingQuality.intervals)+" late "+String(samplingQuality.late),7,41,MUTED,226);
                tiny("Draw max "+String(samplingQuality.drawUs/1000)+"ms Save "+String(samplingQuality.sdUs/1000)+"ms",7,55,MUTED,226);
                tiny(!noiseReady?"Quiet reference: learning":quietReference.fresh(millis())?"Quiet reference: recent":"Quiet reference: STALE",7,69,AMBER,226);
                tiny(noiseReady?"Quiet age "+String(quietReference.age(millis())/1000)+"s / N relearn":"N: relearn reference when quiet",7,83,WHITE,226);
                tiny("Activity windows: 5s, >=80% coverage",7,98,CYAN,226);
                tiny("Coverage measures sampled time only",7,110,MUTED,226);
            }
            footer("</> Page C Trace X Save Ent Pause Esc");break;
        }
        case Screen::Glass:
        case Screen::Finder: {
            const bool finder=screen==Screen::Finder;
            header(finder?"FINDER / RSSI":"WATERFALL / RSSI");
            tiny(mhz(low)+" - "+mhz(high)+"  step "+String(sweep.count>1?(sweep.frequencies[1]-sweep.frequencies[0]):0)+" kHz",4,22,MUTED);
            for(unsigned y=0;y<60;++y) {
                const unsigned source=(waterfallHead+72-60+y)%72;
                for(unsigned i=0;i<sweep.count;++i) {
                    const int x=4+int(i)*232/int(sweep.count);
                    const int width=std::max(1,int(i+1)*232/int(sweep.count)-int(i)*232/int(sweep.count));
                    canvas.fillRect(x,35+y,width,1,waterfallPalette(waterfall[source][i]));
                }
            }
            if(sweep.count) canvas.drawFastVLine(4+int(markerIndex)*232/int(sweep.count),34,60,AMBER);
            if(peakHold) {
                for(unsigned i=0;i<sweep.count;++i) {
                    const int x=4+int(i)*232/int(sweep.count);
                    const int y=93-constrain(int((sweep.held[i]-sweep.floor)*58.0f/55.0f),0,58);
                    canvas.drawPixel(x,y,WHITE);
                }
            }
            for(int x=0;x<96;++x) canvas.drawFastVLine(138+x,99,6,waterfallPalette(x*31/95));
            tiny(mhz(markerFrequency)+" "+String(sweep.live[markerIndex],0)+"dBm",4,99,CYAN,130);
            if(finder) tiny("Samples "+String(finderHits)+"  gate "+String(threshold)+" dBm",4,110,finderHits?AMBER:MUTED);
            else tiny("Floor "+String(sweep.floor,0)+"dBm | colour +0..60dB",4,110,MUTED);
            footer("</> Move F Listen B Mark P Hold M Mode"); break;
        }
        case Screen::Menu: {
            header(navigation.home?"2.8 / OBSERVE":ui::groups[navigation.group].name);
            String labels[]={"Scan "+String(rf::bands[band].name)+" MHz band", "Fixed monitor",
                "Band / antenna", "Frequency  "+mhz(fixed), "Scan start  "+mhz(low),
                "Scan end    "+mhz(high), "Step  "+String(step)+" kHz", "Looking Glass / waterfall",
                "Signal Finder / activity", "Frequency bookmarks", "GNSS Explorer", "Expedition / journeys", "Raw NMEA monitor", "RF catalogue", "NFC tag reader", "Saved logs", "Settings", "Signal Hunt / RSSI", "Signal Watch / timeline", "GPS RF Survey", "Discovery Memory", "Field journal", "Hardware limits"};
            const unsigned group=navigation.home?selected:navigation.group;
            const ui::Art arts[]={ui::Art::Radio,ui::Art::Archive,ui::Art::Sky,ui::Art::Sky,ui::Art::NFC,ui::Art::Journal,ui::Art::Instrument};
            ui::banner(canvas,arts[group],20,25);
            tiny(ui::groups[group].caption,8,29,MUTED,157);
            unsigned start=(selected/4)*4;
            for(unsigned i=start;i<navigation.count()&&i<start+4;++i) {
                int y=47+(i-start)*18;
                if(i==selected) {canvas.fillRoundRect(4,y,232,17,3,ui::theme.raised);canvas.fillRect(4,y+3,2,11,CYAN);}
                unsigned action=navigation.home?0:ui::groups[group].actions[i];
                String label=navigation.home?String(ui::groups[i].name):action==24?String("Investigate / saved signals"):action==23?String("GNSS diagnostics"):action>=25?satelliteMenuLabel(action):labels[action];
                tiny(label,11,y+5,i==selected?CYAN:WHITE,205);
            }
            footer(";/. Move  Enter Open  Esc Back"); break;
        }
        case Screen::Bands:
            header("SELECT RF BAND");
            for(unsigned i=0;i<4;++i) row(String(rf::bands[i].name)+" MHz antenna",i,0,i==selected);
            footer(";/. Move  Enter Select  Esc Back"); break;
        case Screen::Antenna:
            header("ANTENNA CHECK");
            text("Attach "+String(rf::bands[band].name)+" MHz antenna",7,28,CYAN);
            tiny("Use the Cap's RP-SMA connector.",7,53);
            tiny(scan?mhz(low)+" - "+mhz(high)+" MHz":mhz(fixed)+" MHz fixed",7,70);
            tiny("CC1101 receive only. No replay.",7,91,MUTED);
            footer("Enter Ready / Start    Esc Back"); break;
        case Screen::Live: {
            header(paused?"PAUSED / RX":observing?"UNDERSTAND / RX":scan?"SCAN / RX":"MONITOR / RX");
            card(4,24,150,29,true); card(158,24,78,29);
            text(haveSample?mhz(sampledFrequency):"Settling...",10,27,CYAN,139);
            tiny("MHz",122,43,MUTED,27);
            text(haveSample?String(rssi,0):"--",164,26,haveSample&&rssi>=threshold?AMBER:WHITE,43);
            tiny("dBm",208,39,MUTED,24);
            card(4,57,232,42);
            ui::trace(canvas,history,historyCount,historyHead,112,9,61,222,33,threshold);
            tiny(matchCaption(),7,103,liveMatchSample&&liveScore>=75?GREEN:MUTED,226);
            monitorFooter(observation?"I Info  S Save  W Pin  A Retry":"S Save  W Pin  M Mode  P Fine", "Enter Pause/resume  Esc Back"); break;
        }
        case Screen::Edit:
            header(editTitle());
            tiny((field==Field::Label||field==Field::MemoryLabel||field==Field::MemoryNote||field==Field::JourneyName||field==Field::SessionName||field==Field::ExpeditionBookmark)?"Text (24 characters maximum)":field==Field::Clock?"YYYY-MM-DD HH:MM:SS (UTC)":
                "Band: "+mhz(rf::bands[band].low)+" - "+mhz(rf::bands[band].high),6,28,MUTED);
            canvas.fillRoundRect(5,46,230,28,4,PANEL); text(edit+"_",10,50,CYAN);
            tiny(editError.length()?editError:"Typing replaces the current value",6,85,editError.length()?AMBER:MUTED);
            if(field==Field::Clock) tiny("Clock resets when power is removed.",6,102,MUTED);
            footer("Enter Save  Del Erase  Esc Cancel"); break;
        case Screen::Settings: {
            header("SETTINGS");
            String labels[]={"Threshold  "+String(threshold)+" dBm", "Dwell  "+String(dwell)+" ms",
                "RX width  "+String(bandwidths[bwIndex],1)+" kHz",String("Sound  ")+(sound?"On":"Off"),
                String("Event logging  ")+(autoLog?"On":"Off"),"Brightness  "+String(brightness),
                "Volume  "+String(volume),"Set UTC clock"};
            unsigned start=selected/5*5;
            for(unsigned i=start;i<8&&i<start+5;++i) row(labels[i],i,start,i==selected);
            footer(";/. Move  Enter Change  Esc Back"); break;
        }
        case Screen::NFC:
            header("NFC / READ ONLY");
            tiny("UID / NFC-A",6,27,MUTED); text(nfcUID.substring(0,28),6,39,CYAN);
            if(nfcUID.length()>28) tiny(nfcUID.substring(28),6,57,CYAN);
            tiny(nfcType.substring(0,38),6,75); tiny(nfcDetail.substring(0,38),6,91,MUTED);
            tiny("13.56 MHz field only during a read",6,108,MUTED);
            footer("Enter / Tab Read tag    Esc Back"); break;
        case Screen::Files: {
            header("LOGS / NEWEST 128");
            if(fileNames.empty()) text(logs.ready?"No saved logs":"SD unavailable",6,45,AMBER);
            unsigned start=fileIndex/5*5;
            for(unsigned i=start;i<fileNames.size()&&i<start+5;++i) row(fileNames[i],i,start,i==fileIndex);
            footer(";/. Move  Enter Read  Esc Back"); break;
        }
        case Screen::Bookmarks: {
            header("FREQUENCY BOOKMARKS");
            unsigned start=selected/5*5;
            for(unsigned i=start;i<8&&i<start+5;++i) {
                String entry=bookmarks[i].frequency ? mhz(bookmarks[i].frequency)+"  "+bookmarks[i].label : "(empty)";
                row(entry,i,start,i==selected);
            }
            footer(";/. Move Enter Monitor Esc Back"); break;
        }
        case Screen::Catalogue: {
            header("RF CATALOGUE / RECEIVE ONLY");
            unsigned start=selected/5*5;
            for(unsigned i=start;i<sizeof(profiles)/sizeof(profiles[0])&&i<start+5;++i) row(profiles[i].name,i,start,i==selected);
            tiny(profiles[selected].detail,5,108,MUTED);
            footer(";/. Select  Enter Waterfall  Esc Back"); break;
        }
        case Screen::Expedition: {
            header("EXPEDITION / JOURNEY");

            ui::banner(canvas,ui::Art::Sky,20,25);

            tiny(journeyRecorder.active()?"RECORDING":"READY",7,28,
                 journeyRecorder.active()?GREEN:CYAN,220);

            tiny("J Journey  "+expeditionJourney,7,47,WHITE,226);
            tiny("S Session  "+expeditionSession,7,61,WHITE,226);
            uint32_t elapsedSec=journeyRecorder.elapsedMs()/1000;
            uint32_t elapsedMin=elapsedSec/60;
            uint32_t elapsedRem=elapsedSec%60;

            String duration=
                String(elapsedMin)+":"+
                (elapsedRem<10?"0":"")+
                String(elapsedRem);

            double distance=journeyRecorder.distanceM();

            String distanceText=
                distance>=1000.0
                ? String(distance/1000.0,2)+" km"
                : String(distance,0)+" m";

            tiny("Time "+duration+
                 "   Dist "+distanceText,7,79,CYAN,226);

            tiny("Fix "+String(journeyRecorder.coveragePercent(),0)+"%"+
                 "  P "+String(journeyRecorder.points())+
                 "  G "+String(journeyRecorder.gaps()),7,93,MUTED,226);

            if(!journeyRecorder.ready)
                tiny("SD / recorder unavailable",7,107,AMBER,226);
            else if(journeyRecorder.active())
                tiny(String(gnssOnly?"GNSS only | ":"RF ready | ")+String(routeInterval/1000)+"s | I interval G mode",7,107,MUTED,226);
            else if(expeditionInterrupted)
                tiny("Previous session ended unexpectedly",7,107,AMBER,226);
            else
                tiny(String(gnssOnly?"GNSS only | ":"RF ready | ")+String(routeInterval/1000)+"s | I interval G mode",7,107,MUTED,226);

            footer(journeyRecorder.active()?
                "Enter Finish B Mark T Map V Data":
                "Enter Start  T Map  V Data");
            break;
        }

        case Screen::TravelStatus: {
            header("TRAVEL / LIVE DATA");const auto& f=gps.fix();
            tiny(gps.freshTime()?f.utc:String("UTC awaiting GNSS"),7,27,CYAN,226);
            tiny(gps.freshFix()?String(f.latitude,5)+", "+String(f.longitude,5):String("Position unavailable / stale"),7,44,gps.freshFix()?WHITE:AMBER,226);
            tiny("Altitude "+(gps.freshFix()&&gps.freshAltitude()?String(f.altitude,0)+" m GNSS":String("unknown")),7,61,WHITE,226);
            tiny("Ground speed "+(gps.freshFix()&&gps.freshMotion()&&isfinite(f.speedKmh)?String(f.speedKmh,1)+" km/h":String("unknown")),7,78,WHITE,226);
            tiny("Course "+(gps.freshFix()&&gps.freshMotion()&&isfinite(f.course)?String(f.course,0)+" deg":String("unknown")),7,95,MUTED,226);
            footer("Course = motion, not heading. Esc");break;
        }
        case Screen::Trail: {
            header("TRAIL / LAST 64 POINTS");
            unsigned count=journeyRecorder.trailCount;
            if(!count) tiny("Start a session and obtain a fix",8,52,AMBER,224);
            else {
                const auto& origin=journeyRecorder.trailPoint(0);
                double xs[64],ys[64],xmin=0,xmax=0,ymin=0,ymax=0;
                for(unsigned i=0;i<count;++i) {const auto& p=journeyRecorder.trailPoint(i);
                    double lon=p.longitude-origin.longitude;while(lon>180)lon-=360;while(lon<-180)lon+=360;
                    xs[i]=lon*cos(origin.latitude*M_PI/180);ys[i]=p.latitude-origin.latitude;
                    xmin=fmin(xmin,xs[i]);xmax=fmax(xmax,xs[i]);ymin=fmin(ymin,ys[i]);ymax=fmax(ymax,ys[i]);}
                double scale=fmin(210.0/fmax(xmax-xmin,0.00001),72.0/fmax(ymax-ymin,0.00001));
                int px=0,py=0;
                for(unsigned i=0;i<count;++i) {int x=120+int((xs[i]-(xmin+xmax)/2)*scale),y=68-int((ys[i]-(ymin+ymax)/2)*scale);
                    if(i && !journeyRecorder.trailPoint(i).start)canvas.drawLine(px,py,x,y,CYAN);
                    canvas.fillCircle(x,y,i==count-1?3:1,i==count-1?AMBER:MUTED);px=x;py=y;}
                tiny("N ^  auto scale",7,23,MUTED,140);
                tiny(String(count)+" points | gaps stay separate",7,108,MUTED,226);
            }
            footer("Schematic trail / no basemap  Esc Back");break;
        }
        case Screen::GPS: {
            const auto& f=gps.fix();
            header("GNSS EXPLORER");
            canvas.fillRoundRect(4,23,105,78,4,PANEL);
            const bool freshFix=gps.freshFix();
            const bool hadFix=f.lastFix!=0;
            String gnssState=freshFix ? "FRESH FIX" :
                f.connected ? (hadFix ? "STALE FIX" : "SATELLITES / NO FIX") : "NO UART";
            tiny(gnssState,9,28,freshFix?GREEN:AMBER,94);
            tiny("USED "+(gps.freshUsed()?String(f.used):String("?"))+"  LIST "+String(f.visible),9,41,CYAN,94);
            tiny("GPS "+String(f.gps)+" GLO "+String(f.glonass),9,54,WHITE,94);
            tiny("GAL "+String(f.galileo)+" BDS "+String(f.beidou),9,67,WHITE,94);
            tiny("QZS "+String(f.qzss)+" SBA "+String(f.sbas),9,80,MUTED,94);
            String fixType=gps.freshGsa()?(f.fixType==3?"3D":f.fixType==2?"2D":"NO FIX"):String("?");
            tiny("HDOP "+(gps.freshHdop()&&isfinite(f.hdop)?String(f.hdop,1):String("?"))+"  "+fixType,9,93,MUTED,94);

            const int cx=174,cy=59,r=31;
            canvas.drawCircle(cx,cy,r,PANEL); canvas.drawCircle(cx,cy,r/2,PANEL);
            canvas.drawFastHLine(cx-r,cy,r*2,PANEL); canvas.drawFastVLine(cx,cy-r,r*2,PANEL);
            const auto* sats=gps.satellites();
            for(uint8_t i=0;i<gps.count();++i) {
                if(sats[i].elevation<0||sats[i].azimuth<0) continue;
                float a=(sats[i].azimuth-90)*M_PI/180.0f, d=(90-sats[i].elevation)*r/90.0f;
                int x=cx+int(cosf(a)*d), y=cy+int(sinf(a)*d);
                uint16_t c=constellationColor(sats[i].system);
                if(sats[i].used) canvas.fillCircle(x,y,3,c); else canvas.drawCircle(x,y,2,c);
            }
            tiny("N",171,21,CYAN);
            String utcState=gps.freshTime()?"UTC GNSS":clockSync.synced?"UTC HOLD":"UTC WAIT";
            tiny(utcState,126,94,gps.freshTime()?GREEN:AMBER,107);
            String loc;
            if(freshFix) loc=String(f.latitude,4)+","+String(f.longitude,4);
            else if(f.connected && hadFix) loc="Last position stale - awaiting fix";
            else if(f.connected) loc="Satellite data; position unavailable";
            else loc="No GNSS UART data";
            tiny(loc,5,106,freshFix?WHITE:MUTED,230);
            footer("Ent Sat H Plot D Info L Log N Raw B HW"); break;
        }
        case Screen::Satellites: {
            header("SATELLITES / LIVE"); const auto* s=gps.satellites();
            if(selected>=gps.count() && gps.count()) selected=gps.count()-1;
            unsigned start=(selected/5)*5;
            for(unsigned i=start;i<gps.count()&&i<start+5;++i) {
                int y=23+(i-start)*18;
                if(i==selected) canvas.fillRoundRect(3,y,234,17,3,0x194c);
                String line=String(gnss::name(s[i].system))+" "+String(s[i].prn)+"  EL"+String(s[i].elevation)+" AZ"+String(s[i].azimuth)+"  "+String(s[i].snr)+"dBHz"+(s[i].used?" *":"");
                tiny(line,7,y+4,i==selected?CYAN:constellationColor(s[i].system),225);
            }
            tiny(gps.count()?String(gps.count())+" listed  * used in fix":"Waiting for GSV",5,112,MUTED,230);
            footer(";/. Scroll  Enter Detail  Esc Sky"); break;
        }
        case Screen::SatDetail: {
            header("SATELLITE DETAIL");
            const gnss::Satellite* current=nullptr;
            for(unsigned i=0;i<gps.count();++i){const auto& e=gps.satellites()[i];if(e.system==satelliteSystem&&e.prn==satelliteId){current=&e;break;}}
            text(String(gnss::name(satelliteSystem))+" "+String(satelliteId),8,25,CYAN,220);
            if(!current||!gnss::detected(*current,millis())){tiny("No fresh detected signal",8,53,AMBER);tiny("Previous identity retained",8,73,MUTED);footer("Esc Satellite list");break;}
            const auto& sat=*current;
            tiny("DETECTED / "+String(sat.snr)+" dB-Hz",8,46,GREEN,168);
            tiny(sat.elevation<0?String("Elevation unknown"):"Elevation "+String(sat.elevation)+" deg",8,62,WHITE,155);
            tiny(sat.azimuth<0?String("Azimuth unknown"):"Azimuth "+String(sat.azimuth)+" deg",8,76,WHITE,155);
            tiny(String("Fix use: ")+(sat.usageKnown?(sat.used?"used":"not used"):"unknown"),8,91,MUTED,155);
            int cx=198,cy=75,r=26;canvas.drawCircle(cx,cy,r,PANEL);canvas.drawFastHLine(cx-r,cy,r*2,PANEL);canvas.drawFastVLine(cx,cy-r,r*2,PANEL);tiny("N",195,39,MUTED,10);
            if(gnss::skyPosition(sat)){float a=(sat.azimuth-90)*M_PI/180.0f,d=(90-sat.elevation)*r/90.0f;canvas.fillCircle(cx+int(cosf(a)*d),cy+int(sinf(a)*d),3,GREEN);}
            tiny("Receiver sky angles; not orbit",8,109,MUTED,224);
            footer("Esc Satellite list"); break;
        }
        case Screen::SatCategory:
        case Screen::SatArchive: {
            bool archive=screen==Screen::SatArchive;unsigned count=satelliteRows(archive);
            header(archive?"SATELLITES / HISTORY":satelliteFilter<0?"SATELLITES / LIVE":gnss::systemTitle(gnss::System(satelliteFilter)));
            tiny(archive?"Saved detections / "+String(count):satelliteFilter<0?String("Fresh receiver signal reports"):String(gnss::systemRole(gnss::System(satelliteFilter))),6,25,MUTED,228);
            if(count&&satelliteRow>=count)satelliteRow=count-1;
            unsigned start=(satelliteRow/3)*3;
            for(unsigned i=start;i<count&&i<start+3;++i){
                int y=40+(i-start)*23;if(i==satelliteRow)canvas.fillRoundRect(4,y,232,22,3,ui::theme.raised);
                String line;
                if(archive){const auto* e=satelliteSaved(i);line=String(gnss::name(e->system))+" "+String(e->prn)+"  "+String(e->confirmedReports)+" reports";}
                else {const auto* e=satelliteLive(i);line=String(gnss::name(e->system))+" "+String(e->prn)+"  "+String(e->snr)+" dB-Hz";}
                tiny(line,10,y+7,i==satelliteRow?CYAN:WHITE,216);
            }
            if(!count){tiny(archive?"No saved detections":"No fresh signal reports",8,51,AMBER,224);tiny(archive?"Requires a writable SD card":"Availability depends on sky / mode",8,72,MUTED,224);}
            tiny(archive?(satLog.writeFailed()?"SD WRITE FAILED / retry pending":satLog.full()?"Logbook full / existing IDs update":satLog.ready()?"Historical; not currently detected":"SD logbook unavailable"):"Detected = fresh ID + positive C/N0",6,111,MUTED,228);
            footer(archive?";/. Move Enter Detail Esc Back":";/. Move Enter Detail L Log Esc Back");break;
        }
        case Screen::SatRecord: {
            header("SATELLITE / SAVED");const gnss::LogEntry* e=nullptr;
            for(unsigned i=0;i<satLog.count();++i){const auto* entry=satLog.get(i);if(entry->system==satelliteSystem&&entry->prn==satelliteId){e=entry;break;}}
            if(e){tiny(String(gnss::name(e->system))+" "+String(e->prn)+" / historical record",7,26,CYAN,226);
                if(!satellitePage){
                    tiny("Qualified reports: "+String(e->confirmedReports),7,44,WHITE,226);
                    tiny("Last C/N0: "+String(e->lastCn0)+" dB-Hz",7,59,WHITE,226);
                    tiny("Sky AZ "+(e->lastAzimuth<0?String("?"):String(e->lastAzimuth))+" / EL "+(e->lastElevation<0?String("?"):String(e->lastElevation))+" deg",7,74,WHITE,226);
                    tiny("LAST "+epochLabel(e->lastConfirmedUTC),7,90,MUTED,226);
                    tiny("Receiver-reported sky position",7,108,MUTED,226);
                }else{
                    tiny("RECEIVER LOCATION / last valid fix",7,45,MUTED,226);
                    tiny(e->confirmedGeotag?String(e->lastLatitude/1e6,5)+", "+String(e->lastLongitude/1e6,5):String("No valid detection geotag"),7,64,WHITE,226);
                    tiny(e->confirmedGeotag?epochLabel(e->lastLocationUTC):String("Unknown; never inferred"),7,84,MUTED,226);
                    tiny("This is not the satellite's location",7,108,MUTED,226);
                }
            }footer(",// Page  Esc History");break;
        }
        case Screen::SatCapabilities:
            header("SATELLITES / ACCURACY");
            tiny("GPS / GLONASS / Galileo / BeiDou",6,27,CYAN,228);
            tiny("QZSS: regional  SBAS: augmentation",6,43,CYAN,228);
            tiny("Only fresh supported GNSS reports",6,61,WHITE,228);
            tiny("C/N0 > 0; ID and signal validated",6,77,WHITE,228);
            tiny("CC1101 RSSI cannot identify a sat",6,94,AMBER,228);
            tiny("No orbital coordinates calculated",6,110,MUTED,228);
            footer("Esc Satellites menu");break;
        case Screen::SatRFMenu: {
            header("OBSERVE / EXPERIMENTAL");ui::banner(canvas,ui::Art::Sky,20,25);tiny("RF evidence / identity unknown",7,29,AMBER,225);
            const char* labels[]={"ISS UHF / 437.800 MHz","Saved target profiles","Recent RF windows / SD","Named observation sessions","Bookmarked evidence","Limits / interpretation"};
            unsigned start=satelliteRFRow/4*4;for(unsigned i=start;i<6&&i<start+4;++i){int y=47+(i-start)*18;if(i==satelliteRFRow)canvas.fillRoundRect(4,y,232,17,3,ui::theme.raised);tiny(labels[i],10,y+5,i==satelliteRFRow?CYAN:WHITE,218);}
            footer(";/. Move Enter Open Esc Satellites");break;
        }
        case Screen::RFProfiles:
            header("OBSERVE / TARGETS");tiny("User profiles; not satellite IDs",7,26,AMBER,226);
            for(unsigned i=0;i<4;++i){auto p=observations.profile(i);int y=43+i*18;if(i==satelliteRFRow)canvas.fillRoundRect(4,y,232,17,3,ui::theme.raised);tiny(String(i+1)+" "+p.name+" "+mhz(p.frequency),9,y+5,i==satelliteRFRow?CYAN:WHITE,222);}footer(";/. Move Enter Setup Esc Back");break;
        case Screen::SatRFSetup:
            header(rfSetupPage?"OBSERVE / SESSION":"OBSERVE / TARGET SETUP");
            if(!rfSetupPage){tiny(rfProfile.name+" / "+mhz(satelliteRFNominal)+" MHz",7,26,CYAN,226);tiny("Source: "+rfProfile.source,7,44,WHITE,226);tiny("Checked: "+rfProfile.checked,7,61,MUTED,226);tiny("UHF antenna / 58 kHz filter",7,80,WHITE,226);tiny("Experimental: no satellite ID",7,98,AMBER,226);monitorFooter("F MHz N Name U Source D Date P Save","</> Page Enter Start Esc Back");}
            else {tiny("Session: "+rfSessionName,7,26,CYAN,226);tiny("Antenna: "+rfAntenna,7,44,WHITE,226);tiny("Note: "+rfNote,7,61,WHITE,226);tiny("C cycles Before / During / After",7,80,MUTED,226);tiny("Enter starts a new SD session",7,98,AMBER,226);monitorFooter("J Session A Antenna T Note","</> Page Enter Start Esc Back");}break;
        case Screen::SatRFLive: {
            header("OBSERVE / SOURCE UNKNOWN");
            tiny(mhz(satelliteRFTuned)+" MHz  "+rfPhaseName(rfPhase),6,24,CYAN,228);
            if(!rfLivePage){
                tiny(satelliteRFPaused?String("PAUSED"):satelliteRFHave?"RSSI "+String(satelliteRFValue,1)+" dBm":String("Waiting for valid sample"),6,37,WHITE,228);
                canvas.drawRect(6,50,228,45,PANEL);uint32_t now=millis();bool previous=false;int px=0,py=0;uint32_t previousAt=0;
                for(unsigned i=0;i<rfTimeline.count;++i){const auto& point=rfTimeline.get(i);uint32_t age=now-point.at;if(age>30000)continue;int x=232-int(uint64_t(age)*224/30000);
                    if(point.event){canvas.drawFastVLine(x,51,42,point.event==2?CYAN:AMBER);previous=false;continue;}
                    int y=93-constrain((int(point.rssi10)+1200)*40/900,0,40);if(previous&&uint32_t(point.at-previousAt)<=500)canvas.drawLine(px,py,x,y,GREEN);else canvas.fillCircle(x,y,1,GREEN);px=x;py=y;previousAt=point.at;previous=true;
                }
                tiny("30s / -120..-30dBm | marks=changes",6,99,MUTED,228);
            }else{
                tiny("Gate "+String(satelliteRFGate)+" / above "+String(satelliteRFWindow.above),6,40,WHITE,228);
                tiny("N "+String(satelliteRFWindow.count)+" gap "+String(satelliteRFWindow.gap)+"ms",6,55,WHITE,228);
                tiny("Excess intervals "+String(satelliteRFWindow.excessMs)+"ms",6,70,MUTED,228);
                tiny("Prior save "+String(rfLastSaveUs/1000)+"ms auto "+String(satelliteRFAuto?"ON":"OFF"),6,85,MUTED,228);
                tiny(satelliteRFStatus,6,100,AMBER,228);
            }
            monitorFooter("</> Tune ;/. Gate C Phase B Keep","D Data A Auto L Log Enter Pause Esc");break;
        }
        case Screen::SatRFHistory: {
            header(rfPinnedOnly?"OBSERVE / BOOKMARKS":rfHistorySession?"OBSERVE / SESSION WINDOWS":"OBSERVE / RECENT WINDOWS");
            tiny(String(rfHistoryCount())+" RF windows / not detections",6,26,AMBER,228);
            for(unsigned i=0;i<rfCacheCount;++i){const auto* r=&rfCache[i];int y=43+i*17;bool chosen=rfCacheStart+i==satelliteRFRow;if(chosen)canvas.fillRoundRect(4,y,232,17,3,ui::theme.raised);tiny(mhz(r->frequency)+" "+String(r->mean10/10.0f,1)+"dBm #"+String(r->sequence),9,y+4,chosen?CYAN:WHITE,222);}
            if(!rfCacheCount)tiny("No saved RF observations",8,55,MUTED,224);
            tiny(observations.writeFailed()||satelliteRFArchive.writeFailed()?"SD FAILURE / persistence uncertain":!observations.ready()?"SD unavailable":rfPinnedOnly?"Bookmarks stay beyond rolling history":"Newest first; source unknown",6,112,MUTED,228);
            footer(";/. Move Enter Detail Esc Back");break;
        }
        case Screen::SatRFRecord: {
            header("OBSERVE / SAVED WINDOW");const auto* r=rfHistoryRecord(satelliteRFRow);
            if(r){tiny(mhz(r->frequency)+" MHz / source unknown",6,26,CYAN,228);
                if(satelliteRFPage==0){tiny("Mean "+String(r->mean10/10.0f,1)+" peak "+String(r->peak10/10.0f,1)+" dBm",6,44,WHITE,228);tiny("Gate "+String(r->gate)+" / above "+String(r->above)+" samples",6,62,WHITE,228);tiny("N "+String(r->samples)+" gap "+String(r->gap)+"ms",6,80,MUTED,228);tiny("Excess "+(r->excessMs==UINT32_MAX?String("unknown"):String(r->excessMs)+"ms")+" / BW 58 kHz",6,98,MUTED,228);}
                else if(satelliteRFPage==1){tiny(epochLabel(r->utc),6,45,WHITE,228);tiny("RECEIVER location at window end",6,65,MUTED,228);tiny(r->located?String(r->latitude/1e6,5)+", "+String(r->longitude/1e6,5):String("No fresh receiver fix"),6,83,WHITE,228);tiny("Satellite position unknown",6,103,AMBER,228);}
                else {auto* session=observations.find(r->session);tiny(session?session->name:String("Legacy / session unknown"),6,44,WHITE,228);tiny(session?"Antenna: "+session->antenna:String("Antenna unknown"),6,62,MUTED,228);tiny(session?String(rfPhaseName(r->phase))+" / #"+String(r->sequence):String("Phase unknown"),6,80,CYAN,228);tiny("Prior save "+String(r->priorSaveUs/1000)+"ms",6,98,MUTED,228);}
            }footer("</> Page B Bookmark Esc History");break;
        }
        case Screen::RFSessions: {
            header("OBSERVE / SESSIONS");tiny(String(observations.count())+" / 12 saved sessions",6,26,MUTED,228);unsigned start=satelliteRFRow/4*4;
            for(unsigned i=start;i<observations.count()&&i<start+4;++i){auto* session=observations.session(observations.count()-1-i);int y=43+(i-start)*18;if(i==satelliteRFRow)canvas.fillRoundRect(4,y,232,17,3,ui::theme.raised);tiny("#"+String(session->id)+" "+session->name+" / "+String(session->total),9,y+5,i==satelliteRFRow?CYAN:WHITE,222);}if(!observations.count())tiny("Start a session from target setup",6,61,AMBER,228);footer(";/. Move Enter Summary Esc Back");break;
        }
        case Screen::RFSession: {
            header(rfSessionPage?"OBSERVE / COMPARE":"OBSERVE / SESSION");auto* session=observations.find(rfSelectedSession);
            if(session){tiny("#"+String(session->id)+" "+session->name,6,26,CYAN,228);
                if(!rfSessionPage){tiny("Antenna: "+session->antenna,6,44,WHITE,228);tiny("Note: "+session->notes,6,61,WHITE,228);tiny(String(session->total)+" windows / "+String(session->closed?"closed":"interrupted/open"),6,79,MUTED,228);tiny(epochLabel(session->utc),6,97,MUTED,228);}
                else {for(unsigned i=0;i<3;++i)tiny(String(rfPhaseName(i))+": "+(session->phases[i].windows?String(session->phases[i].mean(),1)+" dBm":String("no windows")),6,41+i*14,WHITE,228);
                    const char* issue=observations.comparison(*session,rfComparePhase);tiny(issue?String(issue):String(rfPhaseName(rfComparePhase))+" - Before: "+String(session->phases[rfComparePhase].mean()-session->phases[0].mean(),1)+" dB",6,86,issue?AMBER:CYAN,228);tiny("Measured difference; no satellite ID",6,101,MUTED,228);
                }
            }monitorFooter("</> Page C During/After H Windows","T Note Esc Sessions");break;
        }
        case Screen::SatRFLimits:
            header("SAT RF / LIMITS");tiny("ISS + other UHF: experimental",6,27,CYAN,228);tiny("RF energy does not identify a sat",6,45,AMBER,228);tiny("Gate counts can include local noise",6,63,WHITE,228);tiny("No decoding, audio or pass forecast",6,81,WHITE,228);tiny("Manual tuning is not Doppler proof",6,99,MUTED,228);footer("Esc Experimental RF menu");break;
        case Screen::GnssHistory: {
            header("GNSS QUALITY / 2 MIN");
            canvas.drawRect(5,27,230,70,PANEL);
            if(gnssHistoryCount>1) {
                unsigned oldest=(gnssHistoryHead+gnssHistoryCapacity-gnssHistoryCount)%gnssHistoryCapacity;
                for(unsigned j=1;j<gnssHistoryCount;++j) {
                    unsigned a=(oldest+j-1)%gnssHistoryCapacity,b=(oldest+j)%gnssHistoryCapacity;
                    int x0=6+(j-1)*228/(gnssHistoryCapacity-1),x1=6+j*228/(gnssHistoryCapacity-1);
                    int y0=96-constrain(int(gnssVisibleHistory[a])*68/64,0,68), y1=96-constrain(int(gnssVisibleHistory[b])*68/64,0,68);
                    canvas.drawLine(x0,y0,x1,y1,CYAN);
                    int s0=96-constrain(int(gnssSnrHistory[a])*68/60,0,68), s1=96-constrain(int(gnssSnrHistory[b])*68/60,0,68);
                    if(gnssSnrHistory[a]!=255&&gnssSnrHistory[b]!=255)canvas.drawLine(x0,s0,x1,s1,GREEN);
                }
            }
            uint8_t last=gnssHistoryCount?(gnssHistoryHead+gnssHistoryCapacity-1)%gnssHistoryCapacity:0;
            tiny("Listed "+String(gnssVisibleHistory[last])+"  C/N0 "+(gnssHistoryCount&&gnssSnrHistory[last]!=255?String(gnssSnrHistory[last]):String("?"))+" dB-Hz",6,100,MUTED,228);
            tiny("HDOP "+(gnssHistoryCount&&gnssHdopHistory[last]!=65535?String(gnssHdopHistory[last]/10.0f,1):String("unknown")),6,110,MUTED,228);
            footer("Cyan listed Green mean C/N0 Esc Sky"); break;
        }
        case Screen::GnssDiag: {
            const auto& d=gps.diagnostics();
            header("GNSS DIAGNOSTICS");
            tiny("Lines "+String(d.lines)+"  checksum bad "+String(d.checksumBad),6,26,d.checksumBad?AMBER:GREEN,228);
            tiny("Truncated "+String(d.truncated)+"   NMEA age "+String(gps.age())+"ms",6,40,d.truncated?AMBER:MUTED,228);
            tiny("GGA "+String(d.gga)+"  RMC "+String(d.rmc)+"  GSA "+String(d.gsa),6,55,WHITE,228);
            tiny("GSV "+String(d.gsv)+"  ZDA "+String(d.zda)+"  TXT "+String(d.txt),6,69,WHITE,228);
            tiny("Age GGA "+String(gps.sentenceAge(d.lastGGA))+"ms",6,84,MUTED,228);
            tiny("Age GSA "+String(gps.sentenceAge(d.lastGSA))+"ms",6,98,MUTED,228);
            tiny("Age GSV "+String(gps.sentenceAge(d.lastGSV))+"ms",6,112,MUTED,228);
            footer("Esc GNSS Explorer"); break;
        }
        case Screen::GnssLogbook: {
            header("SATELLITE LOGBOOK");
            if(selected>=satLog.count()&&satLog.count()) selected=satLog.count()-1;
            unsigned start=(selected/5)*5;
            for(unsigned i=start;i<satLog.count()&&i<start+5;++i) {
                const auto* e=satLog.get(i); int y=23+(i-start)*18;
                if(i==selected) canvas.fillRoundRect(3,y,234,17,3,0x194c);
                String line=String(gnss::name(e->system))+" "+String(e->prn)+(e->confirmedReports?" last "+String(e->lastCn0)+"dBHz Q"+String(e->confirmedReports):" best "+String(e->bestSnr)+"dBHz legacy");
                tiny(line,7,y+4,i==selected?CYAN:constellationColor(e->system),225);
            }
            if(!satLog.count()) tiny(satLog.ready()?"No satellites recorded yet":"SD logbook unavailable",7,43,AMBER,225);
            else { tiny(satLog.writeFailed()?"SD WRITE FAILED / retry pending":"Enter: first / last receiver geotags",5,112,satLog.writeFailed()?AMBER:MUTED,230); }
            footer(";/. Scroll Enter Location Esc Sky"); break;
        }
        case Screen::SatLocation: {
            header("SATELLITE / LOCATION"); const auto* e=satLog.get(selected);
            if(e) {
                tiny(String(gnss::name(e->system))+" "+String(e->prn)+" / RECEIVER position",7,26,CYAN,226);
                tiny("FIRST GEOTAG "+epochLabel(e->firstLocationUTC),7,43,MUTED,226);
                tiny(e->located?String(e->firstLatitude/1e6,5)+", "+String(e->firstLongitude/1e6,5):String("No valid fix at a sighting yet"),7,57,WHITE,226);
                tiny("LAST GEOTAG "+epochLabel(e->lastLocationUTC),7,76,MUTED,226);
                tiny(e->located?String(e->lastLatitude/1e6,5)+", "+String(e->lastLongitude/1e6,5):String("Unknown; no location invented"),7,90,WHITE,226);
                tiny(satLog.writeFailed()?"SD WRITE FAILED / retry pending":"GNSS fix at observation; not orbit",7,108,satLog.writeFailed()?AMBER:MUTED,226);
            }
            footer("Esc Satellite logbook"); break;
        }
        case Screen::BuildInfo:
            header("ABOUT / BUILD");
            text("RFExplorer "+String(firmwareVersion),7,27,CYAN,225);
            tiny("M5Stack Cardputer ADV",7,50);
            tiny("CC1101 + GNSS field instrument",7,64,MUTED,225);
            tiny("Built "+String(buildLabel),7,80,WHITE,225);
            tiny("GNSS parser: SatelliteStore",7,95,GREEN,225);
            tiny("Logbook: "+String(satLog.ready()?"SD ready":"SD unavailable"),7,109,satLog.ready()?GREEN:AMBER,225);
            footer("Esc GNSS Explorer"); break;
        case Screen::NMEA: {
            header(String("RAW NMEA / ")+gps.filterName()); uint8_t total=gps.rawCountLines(); unsigned start=(selected/6)*6;
            for(unsigned i=start;i<total&&i<start+6;++i) tiny(gps.raw(i),2,24+(i-start)*15,i==selected?CYAN:WHITE,236);
            tiny(total?String(total)+" lines buffered":"Waiting for NMEA",4,111,MUTED,232); footer(";/. Scroll  F Filter  Esc Sky"); break;
        }
        case Screen::Hunt:
            header("SIGNAL HUNT / RSSI ONLY");
            text(mhz(fixed)+" MHz",8,28,CYAN); tiny("Walk and compare received strength",8,50,MUTED);
            tiny("This is signal-strength hunting,",8,67); tiny("not direction finding.",8,80,AMBER);
            tiny("Open Fixed monitor for live bar",8,101,CYAN); footer("Enter Fixed monitor  Esc Back"); break;
        case Screen::Watch:
            header("SIGNAL WATCH / FIXED CHANNEL"); text(mhz(fixed)+" MHz",8,28,CYAN);
            tiny("Persistent RSSI activity watch",8,49,MUTED); tiny("Above-threshold RSSI samples are",8,66); tiny("logged; not decoded transmissions.",8,80);
            footer("Enter Start watch  Esc Back"); break;
        case Screen::Survey:
            header("GPS RF SURVEY"); text(mhz(fixed)+" MHz",8,28,CYAN);
            tiny("Sample frequency + RSSI + GNSS",8,50); tiny("position along your route.",8,65,MUTED);
            tiny("CSV fields: UTC lat lon alt HDOP",8,88,CYAN); footer("Enter Start fixed monitor  Esc Back"); break;
        case Screen::Library: {
            header(reviewOnly?"INVESTIGATE / SAVED":"DISCOVERY MEMORY");
            unsigned count=memoryCount;
            tiny(String(count)+" / 48  |  "+(memorySort==0?"Frequency":memorySort==1?"Most seen":memorySort==2?"Strongest":"Review first")+
                (signalMemory.writeFailed()?"  SD ERROR":signalMemory.pending()?"  PENDING":""),7,24,signalMemory.writeFailed()?AMBER:MUTED,226);
            if(!count) {
                card(4,39,232,73);
                text(reviewOnly?"No candidates yet":"Your field notebook",12,45,CYAN,215);
                tiny(reviewOnly?"Save unknowns, or flag a dossier":"Inspect and save a signal to begin.",12,70,WHITE,215);
                tiny(reviewOnly?"with Q on its identity page.":"Completed logged bursts also count.",12,86,MUTED,215);
            } else {
                if(selected>=count) selected=count-1;
                unsigned start=(selected/3)*3;
                for(unsigned i=start;i<count&&i<start+3;++i) {
                    const auto& e=signalMemory.entry(memoryOrder[i]); int y=37+(i-start)*27;
                    card(4,y,232,25,i==selected);
                    tiny(mhz(e.fingerprint.frequency_khz)+" MHz",10,y+4,i==selected?CYAN:WHITE,110);
                    tiny("x"+String(e.sightings)+"  "+String(e.strongest)+"dBm",130,y+4,MUTED,100);
                    tiny(reviewOnly?String(field::reviewReason(e)):(field::observed(e.fingerprint)?String(""):String("UNVERIFIED / "))+(e.label.length()?e.label:field::categoryName(e.category)),10,y+14,MUTED,215);
                }
            }
            footer(";/. Move I Details O Sort Enter RX"); break;
        }
        case Screen::Dossier: {
            const auto& e=signalMemory.entry(dossierIndex);
            const char* titles[]={"DOSSIER / IDENTITY","DOSSIER / HISTORY","RF ANALYST","FAMILY / BASELINE","ENCOUNTER / CONTEXT"};
            header(titles[dossierPage]);
            if(dossierPage==0) {
                card(4,24,232,32,true);
                text(mhz(e.fingerprint.frequency_khz)+" MHz",10,26,CYAN,145);
                tiny(field::categoryName(e.category),160,31,AMBER,70);
                tiny(e.label.length()?e.label:"Unnamed discovery",10,46,WHITE,220);
                tiny(field::activityWindow(e.fingerprint)?"ACTIVITY windows saved "+String(e.sightings):field::qualified(e.fingerprint)?"EVIDENCE qualified | saved "+String(e.sightings):"REVIEW: legacy / evidence unverified",7,63,field::qualified(e.fingerprint)?GREEN:AMBER,226);
                tiny("First "+(e.firstUTC?epochLabel(e.firstUTC):String("UTC unknown / legacy")),7,77,MUTED,226);
                tiny("Last  "+(e.lastUTC?epochLabel(e.lastUTC):String("UTC unknown")),7,89,MUTED,226);
                tiny(String(e.flagged?"FLAGGED ":"")+String(e.reviewed?"REVIEWED ":"")+(e.notes.length()?e.notes:"Q flag / R reviewed"),7,100,CYAN,226);
                monitorFooter("</> Page N Name T Note C Tag", "Q Flag R Reviewed Ent RX Esc List");
            } else if(dossierPage==1) {
                tiny(field::observed(e.fingerprint)?"MEASURED / last 12 saved samples":"LEGACY / unverified saved samples",7,25,CYAN,226);
                if(e.encounterCount) {
                    float values[12]{}; for(unsigned i=0;i<e.encounterCount;++i) values[i]=e.encounterRSSI[i];
                    ui::trace(canvas,values,e.encounterCount,e.encounterCount%12,12,10,40,218,30,-200,false);
                    if(encounterSelected>=e.encounterCount) encounterSelected=e.encounterCount-1;
                    unsigned j=encounterSelected;
                    tiny("#"+String(j+1)+"  "+String(e.encounterRSSI[j])+" dBm  "+(e.encounterDuration[j]?String(e.encounterDuration[j])+" ms":"duration ?"),7,78,WHITE,226);
                    tiny(e.encounterUTC[j]?epochLabel(e.encounterUTC[j]):String("UTC unknown / imported"),7,91,MUTED,226);
                } else tiny("No saved encounter history yet",7,53,MUTED,226);
                tiny("Sample order, not a time axis",7,108,AMBER,226);
                footer(";/. Sample  </> Page  Enter RX");
            } else if(dossierPage==2) {
                tiny(field::observed(e.fingerprint)?"MEASURED / stored observations":"LEGACY / evidence unverified",7,26,CYAN,226);
                tiny("Tuned "+mhz(e.fingerprint.frequency_khz)+" MHz; best "+String(e.strongest)+"dBm",7,40,WHITE,226);
                tiny("Envelope "+(e.fingerprint.duration_ms?String(e.fingerprint.duration_ms)+" ms":"unknown"),7,54,WHITE,226);
                tiny("INFERRED / limited evidence",7,73,AMBER,226);
                tiny(field::activityWindow(e.fingerprint)?"Windows are not bursts or packets":!field::qualified(e.fingerprint)?"Historical counts need review":e.sightings>1?"Repeated qualified observations":"Single qualified observation",7,87,MUTED,226);
                tiny("Identity / motion / period: unknown",7,105,MUTED,226);
                footer("</> Page  Enter RX  Esc List");
            } else if(dossierPage==3) {
                field::Families families; families.build(signalMemory);
                unsigned anchor=families.anchor[dossierIndex];
                tiny(!field::qualified(e.fingerprint)?String("Family evidence unverified"):"Candidate family F"+String(anchor+1)+" / "+String(families.members(anchor))+" dossiers",7,26,CYAN,226);
                tiny("<=250 kHz + duration ratio <=2",7,40,MUTED,226);
                tiny("Heuristic; does not prove identity",7,54,AMBER,226);
                tiny(e.baselineCount?"Baseline "+String(e.baselineRSSI)+"dBm / n="+String(e.baselineCount):String("B: baseline needs 6 saved samples"),7,73,WHITE,226);
                tiny(!field::qualified(e.fingerprint)?String("Legacy baseline: review required"):e.baselineCount&&e.encounterCount?(field::deviation(e,e.encounterRSSI[e.encounterCount-1])?"RSSI deviation >=12 dB":"Latest within 12 dB baseline"):"No baseline comparison yet",7,87,AMBER,226);
                tiny("Compare same setup; no place model",7,105,MUTED,226);
                footer("B Baseline X Export </> Page Esc List");
            } else {
                if(e.encounterCount && encounterSelected>=e.encounterCount)encounterSelected=e.encounterCount-1;
                const auto& c=e.context[encounterSelected];
                tiny("Encounter "+String(encounterSelected+1)+" / "+String(e.encounterCount),7,25,CYAN,226);
                tiny(e.encounterUTC[encounterSelected]?epochLabel(e.encounterUTC[encounterSelected]):"UTC unknown",7,39,WHITE,226);
                tiny(c.located?String(c.latitudeE6/1000000.0,6)+", "+String(c.longitudeE6/1000000.0,6):"Receiver location not recorded",7,53,WHITE,226);
                tiny(c.located?"Fix age "+String(c.fixAgeMs)+"ms  HDOP "+(c.hdop100?String(c.hdop100/100.0,2):String("?")):"Geotag unavailable / legacy",7,67,MUTED,226);
                tiny(c.rxBandwidth10?"RX filter "+String(c.rxBandwidth10/10.0,1)+" kHz":"RX filter unknown / legacy",7,81,MUTED,226);
                tiny(c.coverage?"Sample coverage "+String(c.coverage)+"%":"Sampling coverage unknown",7,95,MUTED,226);
                tiny(c.coverage?"Quiet reference age "+String(c.quietAgeMs/1000)+"s":"Quiet reference age unknown",7,109,AMBER,226);
                footer(";/. Sample </> Page Enter RX Esc List");
            }
            break;
        }
        case Screen::Summary: {
            header("FIELD JOURNAL");
            ui::banner(canvas,ui::Art::Journal,20,25);
            tiny("BOOT / activity rows "+String(journal.activityRows),7,29,CYAN,218);
            tiny("Saved "+String(journal.observations)+"  New "+String(journal.discoveries)+"  Repeat "+String(journal.repeats),7,51,WHITE,226);
            tiny("Family candidates "+String(journal.familyCandidates)+"  Odd "+String(journal.anomalies),7,65,MUTED,226);
            tiny((journal.observations||journal.activityRows)?"Best "+mhz(journal.strongestFrequency)+" MHz / "+String(journal.strongest)+"dBm":String("No saved RF observations yet"),7,79,CYAN,226);
            tiny("GNSS-linked "+String(journal.geotagged)+"  Full "+String(journal.unstored),7,93,MUTED,226);
            tiny(!logs.ready?"SD unavailable":journalDirty?"Snapshot pending":"Snapshots stored in session CSV",7,108,logs.ready?MUTED:AMBER,226);
            footer("Enter Save snapshot  Esc Back"); break;
        }
        case Screen::Log:
            header("CSV PAGE "+String(logPage+1)+" COL "+String(logColumn+1));
            tiny(fileNames[fileIndex],6,25,CYAN);
            for(unsigned i=0;i<4;++i) tiny(logColumn<logLines[i].length()?logLines[i].substring(logColumn,logColumn+38):"",6,43+i*18);
            footer(";/. Page ,/ Pan columns Esc Back"); break;
        case Screen::About:
            header("HARDWARE / LIMITS");
            tiny("CC1101: 300-348 / 387-464 MHz",6,28);
            tiny("         779-928 MHz only",6,41);
            tiny("No VHF airband. No 1090 ADS-B.",6,59,AMBER);
            tiny("No I/Q, LoRa or wideband decoding.",6,72,AMBER);
            tiny("Ground sensors: only if in range.",6,91);
            tiny("RSSI shows energy, not identity.",6,104,MUTED);
            footer("Esc Back  Build info: GNSS > B"); break;
    }
    if(message.length() && !rf::elapsed(millis(),messageAt,2500)) {
        canvas.fillRect(0,105,240,16,PANEL); tiny(message.substring(0,39),3,109,AMBER);
    }
    canvas.pushSprite(0,0);
}
}
void setup() {
    auto cfg=M5.config();
    cfg.fallback_board=m5::board_t::board_M5CardputerADV;
    M5Cardputer.begin(cfg,true); M5.Display.setRotation(1);
    setenv("TZ","UTC0",1); tzset();
    batteryGauge.update(M5.Power.getBatteryLevel(),millis());
    loadSettings(); M5.Display.setBrightness(brightness); M5.Speaker.setVolume(volume); gps.begin();
    canvas.setColorDepth(16);
    if(!canvas.createSprite(240,135)) {
        M5.Display.fillScreen(ERROR_COLOR); M5.Display.drawString("Display memory error",4,20);
        while(true) delay(100);
    }
    header("RF EXPLORER / 2.8.0");
    card(8,29,224,77,true); text("OBSERVE",20,38,ui::theme.accent,200);
    for(int i=0;i<9;++i) canvas.fillRect(20+i*9,83-(i%5)*5,5,8+(i%5)*5,i>5?ui::theme.good:ui::theme.accent);
    tiny("Receive. Observe. Remember.",20,62,ui::theme.ink,201);
    tiny("Starting RF / GNSS / memory",20,94,MUTED,201); canvas.pushSprite(0,0);
    // Deselect all three devices before clocks or library initialisation.
    for(int cs:{pins::radioCS,pins::nfcCS,pins::sdCS}) { pinMode(cs,OUTPUT); digitalWrite(cs,HIGH); }
    SPI.begin(pins::sck,pins::miso,pins::mosi,-1);
    if(prefsReady) {
        expeditionJourney=prefs.getString("expJourney","New Journey");
        expeditionSession=prefs.getString("expSession","Session 1");
        expeditionInterrupted=prefs.getBool("expActive",false);
    }

    uint32_t boot=prefsReady?prefs.getUInt("boot",0)+1:esp_random();
    if(prefsReady) prefs.putUInt("boot",boot);
    logs.begin(boot);
    journeyRecorder.begin(boot);

    if(expeditionInterrupted && prefsReady) {
        String recoveryPath=prefs.getString("expPath","");

        if(journeyRecorder.recoverInterrupted(
                recoveryPath,
                expeditionJourney,
                expeditionSession)) {

            prefs.putBool("expActive",false);
            prefs.remove("expPath");
        } else {
            // Preserve the pending path; do not overwrite it with a new session.
            journeyRecorder.ready=false;
        }
    }
    satLog.begin();satelliteRFArchive.begin();observations.begin();
    signalMemory.begin();
    if(!rx.begin()) toast("CC1101 error "+String(rx.lastError()));
    else if(!logs.ready) toast(logs.error);
    else toast("Ready. Waiting for GNSS UTC.");
    // NFC is deferred to an explicit read; it cannot emit a field at boot.
}
void loop() {
    gps.poll();
    const auto& fix=gps.fix();uint32_t now=millis();
    if(clockSync.due(now,fix.timeValid,fix.lastTime)) {
        uint32_t ms=fix.utcFraction+uint32_t(now-fix.lastTime);
        timeval tv{time_t(fix.utcEpoch+ms/1000),suseconds_t((ms%1000)*1000)};
        if(settimeofday(&tv,nullptr)==0) {bool first=!clockSync.synced;clockSync.applied(now);if(first) toast("UTC synchronised from GNSS");}
    }
    sampleGnssFeatures();
    M5Cardputer.update();
    if(batteryGauge.due(now)) {batteryGauge.update(M5.Power.getBatteryLevel(),now);charging=M5.Power.isCharging()==m5::Power_Class::is_charging;}
    handleKeys(); sampleSatelliteRF(); sampleRadio(); analyseRadio(); sampleSweep();
    uint32_t storageAt=micros();
    signalMemory.flushIfDue(millis(),30000);
    journeyRecorder.poll(gps);
    if(journalDirty && rf::elapsed(millis(),journalAt,30000)) saveJournal();
    samplingQuality.sdUs=std::max(samplingQuality.sdUs,uint32_t(micros()-storageAt));
    updateLiveMatch();
    const uint32_t frameInterval=screen==Screen::SatRFLive?250:observing?(envelope.active||activityWindow.active?250:140):70;
    if(rf::elapsed(millis(),drawAt,frameInterval)) { drawAt=millis();uint32_t began=micros();draw();samplingQuality.drawUs=std::max(samplingQuality.drawUs,uint32_t(micros()-began)); }
    delay(1);
}















