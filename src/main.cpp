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
#include "gnss_time.h"
#include "gnss_logbook.h"
#include "signal_store.h"
#include "investigation.h"
#include "understand.h"
#include "menu_art.h"
#include "ui_theme.h"
#include <esp_timer.h>
// RFExplorer 2.1 release build: GNSS position/time and calibrated colour waterfall.

namespace {
constexpr uint16_t BG=ui::theme.background, PANEL=ui::theme.panel, CYAN=ui::theme.accent,
    MUTED=ui::theme.muted, WHITE=ui::theme.ink, AMBER=ui::theme.warning,
    GREEN=ui::theme.good, ERROR_COLOR=ui::theme.error;
M5Canvas canvas(&M5.Display);
Receiver rx;
EventStore logs;
gnss::Receiver gps;
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
enum class Screen { Menu, Bands, Live, Antenna, Edit, Settings, NFC, Files, Log, About, Fine, Inspector, Glass, Finder, Bookmarks, GPS, Satellites, SatDetail, NMEA, GnssHistory, GnssDiag, GnssLogbook, SatLocation, BuildInfo, Catalogue, Hunt, Watch, Survey, Library, Dossier, Summary };
enum class Field { Frequency, Low, High, Step, Threshold, Dwell, Clock, Label, Bookmark, MemoryLabel, MemoryNote };
Screen screen=Screen::Menu, returnScreen=Screen::Menu;
Field field=Field::Frequency;
unsigned selected=0, band=1, bwIndex=2;
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
bool observation=false, refinedValid=false, observing=false;
uint32_t coarseFrequency=0, refinedFrequency=0, observationAt=0;
float refinedRSSI=-120, noiseEstimate=-120;
String observationUTC, userLabel, refinementStatus;
int signalMemoryMatch=-1;
uint8_t signalMemoryScore=0;
bool signalMemoryNew=false;
float quietSamples[128]{}; unsigned quietCount=0, quietHead=0;
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
constexpr const char* firmwareVersion="2.1.0";
unsigned dossierIndex=0, dossierPage=0, inspectorPage=0;
uint8_t memoryOrder[48]{}; unsigned memorySort=0;
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
    int battery=M5.Power.getBatteryLevel();
    canvas.drawRect(193,5,15,9,battery>=0&&battery<20?AMBER:MUTED);
    canvas.fillRect(208,8,2,3,MUTED);
    if(battery>=0) canvas.fillRect(195,7,constrain(battery,0,100)*11/100,5,battery<20?AMBER:GREEN);
    tiny(battery<0?"?":String(battery),213,6,WHITE,25);
}
void footer(const String& help) {
    canvas.fillRect(0,121,240,14,PANEL); canvas.drawFastHLine(0,121,240,ui::theme.edge);
    tiny(help,4,125,MUTED,232);
}
void row(const String& label,unsigned i,unsigned start,bool chosen) {
    int y=24+(i-start)*18;
    if(chosen) { canvas.fillRoundRect(4,y,232,18,3,ui::theme.raised); canvas.fillRect(4,y+3,2,12,CYAN); }
    text(label,11,y,chosen?CYAN:WHITE,216);
}
void card(int x,int y,int w,int h,bool active=false) { ui::card(canvas,x,y,w,h,active); }
String matchCaption() {
    if(!liveMatchSample) return "Awaiting signal above threshold";
    if(liveMatch<0 || liveScore<75) return "No likely match / RSSI similarity";
    const auto& e=signalMemory.entry(liveMatch);
    return String(liveScore)+"/100 heuristic: "+(e.label.length()?e.label:String("Unknown"));
}
String mhz(uint32_t f) { return String(f/1000.0f,3); }
void stopRadio() { rx.stop(); haveSample=false; }
bool tune() {
    if(!rx.tune(band,frequency,observing?58.0f:bandwidths[bwIndex])) {
        toast("Radio error "+String(rx.lastError())); screen=Screen::Menu; haveSample=false; return false;
    }
    tunedAt=millis(); return true;
}
void startRadio() {
    if(!rx.available()) { toast("CC1101 unavailable: reboot/check Cap"); screen=Screen::Menu; return; }
    if(!rf::validRange(band,low,high,step) || !rf::inBand(band,fixed)) {
        toast("Invalid frequency settings"); screen=Screen::Menu; return;
    }
    observation=false; observing=false; coarseFrequency=0; refinedValid=false; envelope={}; liveMatch=-1; liveScore=0; liveMatchSample=false;
    frequency=scan?low:fixed; sampledFrequency=frequency;
    peak=-120; peakFrequency=frequency; haveSample=false;
    historyHead=historyCount=0; trigger.high=false; paused=false; lastEvent=millis()-1000;
    screen=Screen::Live; tune();
}
void promptRadio(bool scanning) { scan=scanning; screen=Screen::Antenna; }
void enterEdit(Field f,Screen back) {
    field=f; returnScreen=back; editError=""; editFresh=true;
    switch(f) {
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
        case Field::Bookmark: edit=bookmarks[bookmarkIndex].label; break;
    }
    screen=Screen::Edit;
}
const char* editTitle() {
    switch(field) {
        case Field::Frequency: return "FIXED FREQUENCY / MHz";
        case Field::Low: return "SCAN START / MHz";
        case Field::High: return "SCAN END / MHz";
        case Field::Step: return "SCAN STEP / kHz";
        case Field::Threshold: return "THRESHOLD / dBm";
        case Field::Dwell: return "TUNE DWELL / ms";
        case Field::Label: return "OBSERVATION LABEL";
        case Field::MemoryLabel: return "RENAME DISCOVERY";
        case Field::MemoryNote: return "DOSSIER NOTE";
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
    if(field==Field::MemoryLabel || field==Field::MemoryNote) {
        bool ok=field==Field::MemoryLabel?signalMemory.rename(dossierIndex,edit):signalMemory.annotate(dossierIndex,edit);
        screen=returnScreen; toast(ok?"Dossier saved":"SD failed: change pending"); return;
    }
    if(field==Field::Label) {
        userLabel=edit; screen=returnScreen; saveObservation("SAVE");
        if(!paused) { tune(); monitorAt=millis(); envelope.sampled=false; }
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
    saveSettings(); screen=returnScreen; toast("Saved");
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
void selectMenu() {
    switch(selected) {
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
        case 10: screen=Screen::GPS; break;
        case 11: screen=Screen::NMEA; break;
        case 12: screen=Screen::Catalogue; selected=0; break;
        case 13: screen=Screen::NFC; break;
        case 14: fileNames=logs.files(); fileIndex=0; screen=Screen::Files; break;
        case 15: screen=Screen::Settings; selected=0; break;
        case 16: screen=Screen::Hunt; break;
        case 17: screen=Screen::Watch; break;
        case 18: screen=Screen::Survey; break;
        case 19: screen=Screen::Library; selected=0; refreshMemoryOrder(); break;
        case 20: screen=Screen::Summary; break;
        case 21: screen=Screen::About; break;
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
    if(!rx.available()) { toast("CC1101 unavailable: reboot/check Cap"); return; }
    // Looking Glass needs a finite screen width. Increase the configured step if needed.
    uint32_t utilityStep=step;
    const uint32_t span=high-low;
    if(span/utilityStep+1>rf::Sweep::maxBins) utilityStep=(span+rf::Sweep::maxBins-2)/(rf::Sweep::maxBins-1);
    if(!sweep.begin(band,low,high,utilityStep)) { toast("Invalid / too wide range"); return; }
    finderMode=finder; sweepActive=true; paused=false; observation=false; observing=false;
    finderHits=0; waterfallHead=0; memset(waterfall,0,sizeof(waterfall)); markerIndex=sweep.count/2; markerFrequency=sweep.frequencies[markerIndex];
    frequency=sweep.current(); screen=finder?Screen::Finder:Screen::Glass;
    if(!rx.tune(band,frequency,58.0f)) { sweepActive=false; screen=Screen::Menu; toast("Radio tune failed"); return; }
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
    String note="GPS="+String(f.gps)+" GLO="+String(f.glonass)+" GAL="+String(f.galileo)+" BDS="+String(f.beidou)+" QZS="+String(f.qzss)+" SBA="+String(f.sbas)+" SATS=";
    const auto* sats=gps.satellites();
    for(uint8_t i=0;i<gps.count();++i) {
        if(note.length()>420) { note+="..."; break; }
        if(i) note+=';';
        note+=String(gnss::name(sats[i].system))+String(sats[i].prn)+":"+String(sats[i].snr);
    }
    return note;
}
void sampleGnssFeatures() {
    uint32_t now=millis();
    if(!rf::elapsed(now,gnssSampleAt,2000)) return;
    gnssSampleAt=now;
    const auto* sats=gps.satellites(); const uint8_t count=gps.count();
    int total=0, valid=0;
    for(uint8_t i=0;i<count;++i) if(sats[i].snr>=0) { total+=sats[i].snr; ++valid; }
    gnssVisibleHistory[gnssHistoryHead]=count;
    gnssSnrHistory[gnssHistoryHead]=valid?uint8_t(constrain(total/valid,0,99)):0;
    gnssHdopHistory[gnssHistoryHead]=isfinite(gps.fix().hdop)?uint16_t(constrain(int(gps.fix().hdop*10.0f),0,999)):0;
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
    if(journalDirty) saveJournal();
    if(screen==Screen::SatLocation) {screen=Screen::GnssLogbook;return;}
    if(screen==Screen::Dossier) { screen=Screen::Library; refreshMemoryOrder(); return; }
    if(screen==Screen::Edit) { screen=returnScreen; if(field==Field::Label && !paused) { tune(); monitorAt=millis(); envelope.sampled=false; } return; }
    if(screen==Screen::Log) { screen=Screen::Files; return; }
    if(screen==Screen::SatDetail) { screen=Screen::Satellites; return; }
    if(screen==Screen::Satellites || screen==Screen::NMEA || screen==Screen::GnssHistory || screen==Screen::GnssDiag || screen==Screen::GnssLogbook || screen==Screen::BuildInfo) { screen=Screen::GPS; return; }
    if(screen==Screen::Live || screen==Screen::Fine || screen==Screen::Inspector || screen==Screen::Glass || screen==Screen::Finder) { stopRadio(); observing=false; sweepActive=false; }
    if(screen==Screen::NFC && nfcReady) nfc.disableField();
    screen=Screen::Menu; selected=0;
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
        for(char c:k.word) if(((field==Field::Label || field==Field::Bookmark || field==Field::MemoryLabel || field==Field::MemoryNote) && c>=32 && c<=126) || (c>='0'&&c<='9') || c=='.' || c=='-' || c==':' || c==' ') {
            if(editFresh) { edit=""; editFresh=false; }
            if(edit.length()<24) edit+=c;
        }
        if(k.enter) commitEdit(); return;
    }
    if(screen==Screen::GPS) {
        if(k.enter) { screen=Screen::Satellites; selected=0; return; }
        for(char c:k.word) {
            if(c=='n') { screen=Screen::NMEA; selected=0; return; }
            if(c=='h') { screen=Screen::GnssHistory; return; }
            if(c=='d') { screen=Screen::GnssDiag; return; }
            if(c=='l') { screen=Screen::GnssLogbook; selected=0; return; }
            if(c=='x') {
                const auto& e=signalMemory.entry(dossierIndex); field::Families f;f.build(signalMemory);
                String note="dossier="+String(dossierIndex+1)+";label="+e.label+";tag="+field::categoryName(e.category)+
                    ";notes="+e.notes+";saved="+String(e.sightings)+";first_utc="+String(e.firstUTC)+";last_utc="+String(e.lastUTC)+
                    ";candidate_family="+String(f.anchor[dossierIndex]+1)+";family_rule=250kHz_duration_ratio2;identity=unknown;baseline_n="+String(e.baselineCount)+
                    ";baseline_dbm="+String(e.baselineRSSI)+";baseline_utc="+String(e.baselineUTC);
                toast(logs.save("DOSSIER",e.fingerprint.frequency_khz,e.strongest,note)?"Dossier exported to session CSV":logs.error);
            }
            if(c=='b') { screen=Screen::BuildInfo; return; }
        }
    }
    if(screen==Screen::NMEA) { if(up&&selected) --selected; if(down&&selected+1<gps.rawCountLines()) ++selected; for(char c:k.word) if(c=='f') { gps.cycleRawFilter(); selected=0; toast(String("NMEA filter ")+gps.filterName()); } return; }
    if(screen==Screen::Satellites) {
        if(up&&selected) --selected; if(down&&selected+1<gps.count()) ++selected;
        if(k.enter && gps.count()) { screen=Screen::SatDetail; return; }
        return;
    }
    if(screen==Screen::GnssLogbook) { if(up&&selected) --selected; if(down&&selected+1<satLog.count()) ++selected; if(k.enter&&satLog.count()) screen=Screen::SatLocation; return; }
    if(screen==Screen::SatDetail || screen==Screen::GnssHistory || screen==Screen::GnssDiag || screen==Screen::BuildInfo) return;
    if(screen==Screen::Menu || screen==Screen::Settings || screen==Screen::Bands || screen==Screen::Bookmarks || screen==Screen::Catalogue) {
        unsigned count=screen==Screen::Menu?22:screen==Screen::Settings?8:screen==Screen::Bookmarks?8:screen==Screen::Catalogue?sizeof(profiles)/sizeof(profiles[0]):4;
        if(up) selected=(selected+count-1)%count;
        if(down) selected=(selected+1)%count;
        if(k.enter) {
            if(screen==Screen::Menu) selectMenu();
            else if(screen==Screen::Settings) selectSetting();
            else if(screen==Screen::Bookmarks) {
                if(bookmarks[selected].frequency) { fixed=bookmarks[selected].frequency; saveSettings(); scan=false; startRadio(); }
                else toast("Save a marker first");
            } else if(screen==Screen::Catalogue) {
                const Profile& p=profiles[selected]; band=p.band; low=p.low; high=p.high; step=p.step; fixed=rf::bands[band].centre; saveSettings(); startSweep(false);
            } else { defaultsForBand(selected); saveSettings(); screen=Screen::Menu; selected=0; toast("Band selected"); }
        }
    } else if((screen==Screen::Hunt || screen==Screen::Watch || screen==Screen::Survey) && k.enter) {
        scan=false; startRadio();
    } else if(screen==Screen::Summary) {
        if(k.enter) saveJournal();
    } else if(screen==Screen::Dossier) {
        if(left||right) dossierPage=(dossierPage+(right?1:3))%4;
        if(up && encounterSelected) --encounterSelected;
        if(down && encounterSelected+1<signalMemory.entry(dossierIndex).encounterCount) ++encounterSelected;
        if(k.enter) { monitorMemory(dossierIndex); return; }
        for(char c:k.word) {
            if(c=='x') {
                const auto& e=signalMemory.entry(dossierIndex); field::Families f;f.build(signalMemory);
                String note="dossier="+String(dossierIndex+1)+";label="+e.label+";tag="+field::categoryName(e.category)+
                    ";notes="+e.notes+";saved="+String(e.sightings)+";first_utc="+String(e.firstUTC)+";last_utc="+String(e.lastUTC)+
                    ";candidate_family="+String(f.anchor[dossierIndex]+1)+";family_rule=250kHz_duration_ratio2;identity=unknown;baseline_n="+String(e.baselineCount)+
                    ";baseline_dbm="+String(e.baselineRSSI)+";baseline_utc="+String(e.baselineUTC);
                toast(logs.save("DOSSIER",e.fingerprint.frequency_khz,e.strongest,note)?"Dossier exported to session CSV":logs.error);
            }
            if(c=='b') { const auto& e=signalMemory.entry(dossierIndex); toast(e.encounterCount<6?"Need 6 saved encounters":signalMemory.captureBaseline(dossierIndex,time(nullptr)>=1704067200?uint32_t(time(nullptr)):0)?"Baseline saved":"SD failed: baseline pending"); }
            if(c=='n') enterEdit(Field::MemoryLabel,Screen::Dossier);
            if(c=='t') enterEdit(Field::MemoryNote,Screen::Dossier);
            if(c=='c') toast(signalMemory.classify(dossierIndex,(signalMemory.entry(dossierIndex).category+1)%6)?"Category saved":"SD failed: change pending");
        }
    } else if(screen==Screen::Library) {
        unsigned count=signalMemory.count();
        if(count) {
            if(up) selected=(selected+count-1)%count;
            if(down) selected=(selected+1)%count;
            if(k.enter) { monitorMemory(memoryOrder[selected]); return; }
            for(char c:k.word) {
                if(c=='i') { dossierIndex=memoryOrder[selected]; dossierPage=0; encounterSelected=0; screen=Screen::Dossier; }
                if(c=='o') { memorySort=(memorySort+1)%3; selected=0; refreshMemoryOrder(); }
            }
        } else if(k.enter) toast("Signal memory empty");
    } else if(screen==Screen::Antenna && k.enter) startRadio();
    else if(screen==Screen::Live || screen==Screen::Inspector) {
        if(screen==Screen::Inspector && (left||right)) inspectorPage=1-inspectorPage;
        if(k.enter && screen==Screen::Inspector) { screen=Screen::Live; return; }
        if(k.enter) {
            paused=!paused;
            if(paused) { rx.stop(); envelope.active=false; } else { haveSample=false; trigger.high=false; tune(); monitorAt=millis(); }
        }
        for(char c:k.word) {
            if(c=='s' && haveSample) {
                if(observation) { rx.stop(); envelope.active=false; enterEdit(Field::Label,screen); }
                else toast(logs.save("SAVE",sampledFrequency,rssi,"",gps.csv())?"Discovery saved":logs.error);
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
        if(k.enter) { paused=!paused; if(paused) rx.stop(); else { frequency=sweep.current(); rx.tune(band,frequency,58.0f); tunedAt=millis(); } }
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

    fp.frequency_khz = observation ? (refinedValid?refinedFrequency:coarseFrequency) : sampledFrequency;

    float strength = observation && envelope.count ? envelope.peak : rssi;

    fp.peak_rssi=int16_t(lround(strength));
    fp.noise_rssi=noiseReady ? int16_t(lround(noiseEstimate)) : -120;

    uint32_t duration=observation && !envelope.active?envelope.duration:0;
    fp.duration_ms=duration>65535 ? 65535 : uint16_t(duration);

    fp.repeat_ms=0;

    // We currently know the receiver filter width, not true occupied
    // signal bandwidth, so leave this unknown rather than inventing it.
    fp.bandwidth_khz=0;

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
void rememberCurrentSignal(float savedStrength) {
    field::SignalFingerprint fp=currentFingerprint();
    fp.peak_rssi=int16_t(lround(savedStrength));

    uint8_t score=0;
    int prior=signalMemory.bestMatch(fp,score);

    signalMemoryNew=!(prior>=0 && score>=75);
    signalMemoryScore=signalMemoryNew ? 0 : score;

    bool familyCandidate=false;
    if(signalMemoryNew) for(unsigned i=0;i<signalMemory.count();++i)
        if(field::familyCandidate(fp,signalMemory.entry(i).fingerprint)) { familyCandidate=true; break; }
    bool anomaly=prior>=0 && !signalMemoryNew && field::deviation(signalMemory.entry(prior),fp.peak_rssi);
    int stored=signalMemory.remember(
        fp,
        userLabel,
        millis(), time(nullptr)>=1704067200?uint32_t(time(nullptr)):0
    );

    signalMemoryMatch=stored;
    journal.record(fp,signalMemoryNew,familyCandidate,anomaly,gps.freshFix(),stored>=0);
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
        toast("LIKELY MATCH "+String(signalMemoryScore)+"/100");
    }
}

void saveObservation(const char* kind) {
    if(!observation) return;

    bool saved=logs.saveObservation(
        kind,
        utcTimestamp(),
        uint64_t(esp_timer_get_time()/1000),
        coarseFrequency,
        refinedFrequency,
        refinedValid,
        String(kind)=="BURST" ? envelope.peak : rssi,
        noiseReady ? noiseEstimate : NAN,
        envelope.duration,
        envelope.count,
        userLabel,
        refinementStatus,
        envelope.maxGap,
        gps.csv()
    );

    if(saved) {
        rememberCurrentSignal(String(kind)=="BURST"?envelope.peak:rssi);
    } else {
        toast(logs.error);
    }
}
void startObservationMonitor() {
    observing=true; paused=false; screen=Screen::Inspector;
    frequency=refinedValid?refinedFrequency:coarseFrequency;
    if(!rx.tune(band,frequency,58.0f)) {
        observing=false; screen=Screen::Menu; toast("Radio tune failed"); return;
    }
    tunedAt=monitorAt=noiseStarted=millis();
    quietCount=quietHead=0; noiseReady=false; envelope={};
    historyHead=historyCount=0; haveSample=false;
}
void beginFine(uint32_t centre) {
    observing=false; paused=false; observation=true; signalMemoryMatch=-1; liveMatch=-1; liveScore=0; liveMatchSample=false; inspectorPage=0;
    coarseFrequency=centre; observationAt=millis(); observationUTC=utcTimestamp();
    refinedValid=false; userLabel=""; fine.begin(band,centre);
    screen=Screen::Fine; frequency=fine.current();
    if(!rx.tune(band,frequency,58.0f)) { screen=Screen::Menu; toast("Fine tune failed"); return; }
    tunedAt=millis(); refinedRSSI=-120;
}
void analyseRadio() {
    uint32_t now=millis();
    if(screen==Screen::Fine) {
        if(!rf::elapsed(now,tunedAt,15)) return;
        float value=rx.rssi(); if(!isfinite(value)) return;
        if(fine.add(value)) {
            refinedFrequency=fine.result(refinedRSSI,noiseEstimate,refinedValid);
            refinementStatus=refinedValid?"repeatable candidate":"uncertain / retry A";
            startObservationMonitor();
        } else {
            frequency=fine.current();
            if(!rx.tune(band,frequency,58.0f)) { screen=Screen::Menu; toast("Fine tune failed"); return; }
            tunedAt=millis();
        }
        return;
    }
    if(!observing || paused || (screen!=Screen::Inspector && screen!=Screen::Live)) return;
    if(!rf::elapsed(now,tunedAt,20) || !rf::elapsed(now,monitorAt,2)) return;
    monitorAt=now; float measured=rx.rssi(); if(!isfinite(measured)) {haveSample=false;return;}
    rssi=measured; sampledFrequency=frequency; haveSample=true;
    // Bootstrap from the lower fifth of one second of fixed-frequency samples.
    // Then only update from quiet readings, avoiding learning the burst as noise.
    if(!noiseReady || rssi<noiseEstimate+6) {
        quietSamples[quietHead]=rssi; quietHead=(quietHead+1)%128;
        if(quietCount<128) ++quietCount;
    }
    if(quietCount>=32 && rf::elapsed(now,noiseStarted,1000)) {
        float sorted[128]; std::copy(quietSamples,quietSamples+quietCount,sorted);
        std::sort(sorted,sorted+quietCount); noiseEstimate=sorted[quietCount/5];
        noiseReady=true; noiseStarted=now;
    }
    if(noiseReady) {
        bool wasActive=envelope.active;
        envelope.sample(now,rssi,noiseEstimate);
        if(wasActive && !envelope.active && autoLog && rf::elapsed(now,lastEvent,1000)) { lastEvent=now; saveObservation("BURST"); }
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
    if(!rx.tune(band,frequency,58.0f)) { sweepActive=false; screen=Screen::Menu; toast("Sweep tune failed"); }
    tunedAt=millis();
}
uint16_t waterfallPalette(uint8_t level) { return ui::waterfall(level); }
void refreshMemoryOrder() {
    unsigned n=signalMemory.count();
    for(unsigned i=0;i<n;++i) memoryOrder[i]=i;
    std::stable_sort(memoryOrder,memoryOrder+n,[](uint8_t a,uint8_t b) {
        const auto& x=signalMemory.entry(a); const auto& y=signalMemory.entry(b);
        if(memorySort==1) return x.sightings>y.sightings;
        if(memorySort==2) return x.strongest>y.strongest;
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
    if(screen!=Screen::Live && screen!=Screen::Inspector) return;
    if(!rf::elapsed(millis(),liveMatchAt,250)) return;
    liveMatchAt=millis(); liveMatch=-1; liveScore=0;
    liveMatchSample=haveSample && rssi>=(observing&&noiseReady?noiseEstimate+10:float(threshold));
    if(!liveMatchSample) return;
    field::SignalFingerprint fp=currentFingerprint();
    fp.peak_rssi=int16_t(lround(rssi));
    // Only a completed observed burst supplies a measured duration.
    if(!observing || envelope.active) fp.duration_ms=0;
    liveMatch=signalMemory.bestMatch(fp,liveScore);
}

void draw() {
    switch(screen) {
        case Screen::Fine:
            header("FINE SCAN / 5 kHz STEPS");
            text("Coarse "+mhz(coarseFrequency),6,27,CYAN);
            text("Tuning "+mhz(frequency),6,49);
            tiny("Sweep "+String(fine.pass+1)+"/3   Bin "+String(fine.index+1)+"/"+String(fine.count),6,74);
            tiny("Repeat remote presses for ~3 sec",6,94,AMBER);
            footer("58 kHz RX filter       Esc Cancel"); break;
        case Screen::Inspector: {
            header(inspectorPage?"INSPECT / MATCH":"INSPECT / SIGNAL");
            card(4,24,232,28,true);
            text(mhz(refinedValid?refinedFrequency:coarseFrequency)+" MHz",10,28,CYAN,151);
            tiny(refinedValid?"REFINED ~":"COARSE ?",164,35,refinedValid?GREEN:AMBER,66);
            if(!inspectorPage) {
                card(4,56,113,37); card(121,56,115,37);
                tiny("RSSI / dBm",10,61,MUTED,100);
                text(haveSample?String(rssi,1):"--",10,72,WHITE,101);
                tiny("QUIET EST / dBm",127,61,MUTED,103);
                text(noiseReady?String(noiseEstimate,1):"Learning...",127,72,WHITE,102);
                tiny("Burst "+String(envelope.duration)+"ms  x"+String(envelope.count)+"  gap "+String(envelope.maxGap)+"ms",7,98,WHITE,226);
                tiny("Delta "+(noiseReady?String(rssi-noiseEstimate,1):String("--"))+"dB, not SNR | RX 58kHz",7,110,MUTED,226);
                footer("</> Match  Enter Graph S Save A Retry");
            } else {
                tiny(matchCaption(),7,58,liveMatchSample&&liveScore>=75?GREEN:AMBER,226);
                if(liveMatchSample && liveMatch>=0) {
                    auto fp=currentFingerprint(); fp.peak_rssi=int16_t(lround(rssi));
                    if(!observing||envelope.active) fp.duration_ms=0;
                    auto e=field::evidence(fp,signalMemory.entry(liveMatch).fingerprint);
                    tiny("Penalties from 100 (lower = closer)",7,73,MUTED,226);
                    tiny("Freq -"+String(e.frequencyPenalty)+"  RSSI -"+String(e.strengthPenalty),7,85,WHITE,226);
                    tiny("Duration "+(e.durationKnown?String("-")+String(e.durationPenalty):String("unknown"))+"  BW unknown",7,97,WHITE,226);
                } else tiny("Receive an active signal to compare",7,80,MUTED,226);
                tiny("Similarity is not transmitter ID",7,110,AMBER,226);
                footer("</> Signal  Enter Graph  S Save");
            }
            break;
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
            if(finder) tiny("Hits "+String(finderHits)+"  threshold "+String(threshold)+" dBm",4,110,finderHits?AMBER:MUTED);
            else tiny("Floor "+String(sweep.floor,0)+"dBm | colour +0..60dB",4,110,MUTED);
            footer("</> Move F Listen B Mark P Hold M Mode"); break;
        }
        case Screen::Menu: {
            header("2.1 / UNDERSTAND");
            String labels[]={"Scan "+String(rf::bands[band].name)+" MHz band", "Fixed monitor",
                "Band / antenna", "Frequency  "+mhz(fixed), "Scan start  "+mhz(low),
                "Scan end    "+mhz(high), "Step  "+String(step)+" kHz", "Looking Glass / waterfall",
                "Signal Finder / activity", "Frequency bookmarks", "GNSS Explorer", "Raw NMEA monitor", "RF catalogue", "NFC tag reader", "Saved logs", "Settings", "Signal Hunt / RSSI", "Signal Watch / timeline", "GPS RF Survey", "Discovery Memory", "Field journal", "Hardware limits"};
            ui::Art art=(selected==10||selected==11)?ui::Art::Sky:selected==19?ui::Art::Archive:
                (selected==14||selected==18||selected==20)?ui::Art::Journal:selected==15||selected==21?ui::Art::Instrument:ui::Art::Radio;
            ui::banner(canvas,art,20,25);
            tiny(art==ui::Art::Sky?"ORBIT / POSITION":art==ui::Art::Archive?"EVIDENCE / MEMORY":
                art==ui::Art::Journal?"SURVEY / JOURNAL":art==ui::Art::Instrument?"INSTRUMENT / SETUP":"SPECTRUM / RECEIVE",8,29,MUTED,157);
            unsigned start=(selected/4)*4;
            for(unsigned i=start;i<22&&i<start+4;++i) {
                int y=47+(i-start)*18;
                if(i==selected) {canvas.fillRoundRect(4,y,232,17,3,ui::theme.raised);canvas.fillRect(4,y+3,2,11,CYAN);}
                tiny(labels[i],11,y+5,i==selected?CYAN:WHITE,205);
            }
            footer(";/. Move   Enter Open"); break;
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
            tiny(paused?"LAST SAMPLE / frozen":scan?"RSSI over scan time / not spectrum":"RSSI over time / -120 to -20 dBm",7,113,MUTED,226);
            footer(observation?"I Info S Save W Pin A Retry Ent Pause":"S Save W Pin M Mode P Fine Ent Pause"); break;
        }
        case Screen::Edit:
            header(editTitle());
            tiny((field==Field::Label||field==Field::MemoryLabel||field==Field::MemoryNote)?"Text (24 characters maximum)":field==Field::Clock?"YYYY-MM-DD HH:MM:SS (UTC)":
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
            footer(";/. Select  Enter Looking Glass Esc Back"); break;
        }
        case Screen::GPS: {
            const auto& f=gps.fix();
            header("GNSS EXPLORER");
            canvas.fillRoundRect(4,23,105,78,4,PANEL);
            tiny(f.connected?(f.valid?"FIX VALID":"NO POSITION") : "WAITING NMEA",9,28,f.valid?GREEN:AMBER,94);
            tiny("USED "+String(f.used)+"  VIS "+String(f.visible),9,41,CYAN,94);
            tiny("GPS "+String(f.gps)+" GLO "+String(f.glonass),9,54,WHITE,94);
            tiny("GAL "+String(f.galileo)+" BDS "+String(f.beidou),9,67,WHITE,94);
            tiny("QZS "+String(f.qzss)+" SBA "+String(f.sbas),9,80,MUTED,94);
            tiny("HDOP "+(isfinite(f.hdop)?String(f.hdop,1):String("?"))+"  "+(f.fixType==3?"3D":f.fixType==2?"2D":"NO FIX"),9,93,MUTED,94);

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
            tiny(!clockSync.synced?"UTC WAIT":gps.fix().timeValid?"UTC GNSS":"UTC HOLD",126,94,clockSync.synced?GREEN:AMBER,107);
            String loc=f.valid?String(f.latitude,4)+","+String(f.longitude,4):"Place receiver with clear sky";
            tiny(loc,5,106,f.valid?WHITE:MUTED,230);
            footer("Enter Sats  H Hist D Diag L Log N NMEA B Build"); break;
        }
        case Screen::Satellites: {
            header("SATELLITES / LIVE"); const auto* s=gps.satellites();
            if(selected>=gps.count() && gps.count()) selected=gps.count()-1;
            unsigned start=(selected/5)*5;
            for(unsigned i=start;i<gps.count()&&i<start+5;++i) {
                int y=23+(i-start)*18;
                if(i==selected) canvas.fillRoundRect(3,y,234,17,3,0x194c);
                String line=String(gnss::name(s[i].system))+" "+String(s[i].prn)+"  EL"+String(s[i].elevation)+" AZ"+String(s[i].azimuth)+"  "+String(s[i].snr)+"dB"+(s[i].used?" *":"");
                tiny(line,7,y+4,i==selected?CYAN:constellationColor(s[i].system),225);
            }
            tiny(gps.count()?String(gps.count())+" visible  * used in fix":"Waiting for GSV",5,112,MUTED,230);
            footer(";/. Scroll  Enter Detail  Esc Sky"); break;
        }
        case Screen::SatDetail: {
            header("SATELLITE DETAIL");
            if(!gps.count()) { tiny("Satellite no longer visible",8,35,AMBER); footer("Esc List"); break; }
            if(selected>=gps.count()) selected=gps.count()-1;
            const auto& sat=gps.satellites()[selected];
            text(String(gnss::name(sat.system))+" "+String(sat.prn),8,27,constellationColor(sat.system),220);
            tiny("Elevation  "+String(sat.elevation)+" deg",8,51);
            tiny("Azimuth    "+String(sat.azimuth)+" deg",8,65);
            tiny("Signal     "+String(sat.snr)+" dB",8,79);
            tiny(String("Fix use    ")+(sat.usageKnown?(sat.used?"USED":"not used"):"unknown"),8,93,sat.used?GREEN:MUTED);
            int cx=190,cy=72,r=30; canvas.drawCircle(cx,cy,r,PANEL); canvas.drawFastHLine(cx-r,cy,r*2,PANEL); canvas.drawFastVLine(cx,cy-r,r*2,PANEL);
            if(sat.elevation>=0&&sat.azimuth>=0) { float a=(sat.azimuth-90)*M_PI/180.0f,d=(90-sat.elevation)*r/90.0f; canvas.fillCircle(cx+int(cosf(a)*d),cy+int(sinf(a)*d),3,constellationColor(sat.system)); }
            footer("Esc Satellite list"); break;
        }
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
                    canvas.drawLine(x0,s0,x1,s1,GREEN);
                }
            }
            uint8_t last=gnssHistoryCount?(gnssHistoryHead+gnssHistoryCapacity-1)%gnssHistoryCapacity:0;
            tiny("VIS "+String(gnssVisibleHistory[last])+"   AVG SNR "+String(gnssSnrHistory[last])+" dB   HDOP "+String(gnssHdopHistory[last]/10.0f,1),6,104,MUTED,228);
            footer("Cyan visible  Green avg SNR  Esc Sky"); break;
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
                String line=String(gnss::name(e->system))+" "+String(e->prn)+" best "+String(e->bestSnr)+"dB seen "+String(e->sightings);
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
            tiny("Persistent RSSI activity watch",8,49,MUTED); tiny("Detection count and timeline are",8,66); tiny("recorded in SD BURST rows.",8,80);
            footer("Enter Start watch  Esc Back"); break;
        case Screen::Survey:
            header("GPS RF SURVEY"); text(mhz(fixed)+" MHz",8,28,CYAN);
            tiny("Sample frequency + RSSI + GNSS",8,50); tiny("position along your route.",8,65,MUTED);
            tiny("CSV fields: UTC lat lon alt HDOP",8,88,CYAN); footer("Enter Start fixed monitor  Esc Back"); break;
        case Screen::Library: {
            header("DISCOVERY MEMORY");
            unsigned count=signalMemory.count();
            tiny(String(count)+" / 48  |  "+(memorySort==0?"Frequency":memorySort==1?"Most seen":"Strongest")+
                (signalMemory.writeFailed()?"  SD ERROR":signalMemory.pending()?"  PENDING":""),7,24,signalMemory.writeFailed()?AMBER:MUTED,226);
            if(!count) {
                card(4,39,232,73);
                text("Your field notebook",12,45,CYAN,215);
                tiny("Inspect and save a signal to begin.",12,70,WHITE,215);
                tiny("Completed logged bursts also count.",12,86,MUTED,215);
            } else {
                if(selected>=count) selected=count-1;
                unsigned start=(selected/3)*3;
                for(unsigned i=start;i<count&&i<start+3;++i) {
                    const auto& e=signalMemory.entry(memoryOrder[i]); int y=37+(i-start)*27;
                    card(4,y,232,25,i==selected);
                    tiny(mhz(e.fingerprint.frequency_khz)+" MHz",10,y+4,i==selected?CYAN:WHITE,110);
                    tiny("x"+String(e.sightings)+"  "+String(e.strongest)+"dBm",130,y+4,MUTED,100);
                    tiny(e.label.length()?e.label:field::categoryName(e.category),10,y+14,MUTED,215);
                }
            }
            footer(";/. Move I Dossier O Sort Enter Listen"); break;
        }
        case Screen::Dossier: {
            const auto& e=signalMemory.entry(dossierIndex);
            const char* titles[]={"DOSSIER / IDENTITY","DOSSIER / HISTORY","RF ANALYST","FAMILY / BASELINE"};
            header(titles[dossierPage]);
            if(dossierPage==0) {
                card(4,24,232,32,true);
                text(mhz(e.fingerprint.frequency_khz)+" MHz",10,26,CYAN,145);
                tiny(field::categoryName(e.category),160,31,AMBER,70);
                tiny(e.label.length()?e.label:"Unnamed discovery",10,46,WHITE,220);
                tiny("Saved "+String(e.sightings)+"  Best "+String(e.strongest)+" dBm",7,63,WHITE,226);
                tiny("First "+(e.firstUTC?epochLabel(e.firstUTC):String("UTC unknown / legacy")),7,77,MUTED,226);
                tiny("Last  "+(e.lastUTC?epochLabel(e.lastUTC):String("UTC unknown")),7,89,MUTED,226);
                tiny(e.notes.length()?e.notes:String("N: name  T: note  C: user tag"),7,104,CYAN,226);
                footer("</> Page  N Name T Note C Tag Esc List");
            } else if(dossierPage==1) {
                tiny("MEASURED / last 12 saved samples",7,25,CYAN,226);
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
                tiny("MEASURED / stored observations",7,26,CYAN,226);
                tiny("Tuned "+mhz(e.fingerprint.frequency_khz)+" MHz; best "+String(e.strongest)+"dBm",7,40,WHITE,226);
                tiny("Envelope "+(e.fingerprint.duration_ms?String(e.fingerprint.duration_ms)+" ms":"unknown"),7,54,WHITE,226);
                tiny("INFERRED / limited evidence",7,73,AMBER,226);
                tiny(e.sightings>1?"Recurring saved fingerprint":"Single saved observation",7,87,MUTED,226);
                tiny("Identity / motion / period: unknown",7,105,MUTED,226);
                footer("</> Page  Enter RX  Esc List");
            } else {
                field::Families families; families.build(signalMemory);
                unsigned anchor=families.anchor[dossierIndex];
                tiny("Candidate family F"+String(anchor+1)+" / "+String(families.members(anchor))+" dossiers",7,26,CYAN,226);
                tiny("<=250 kHz + duration ratio <=2",7,40,MUTED,226);
                tiny("Heuristic; does not prove identity",7,54,AMBER,226);
                tiny(e.baselineCount?"Baseline "+String(e.baselineRSSI)+"dBm / n="+String(e.baselineCount):String("B: baseline needs 6 saved samples"),7,73,WHITE,226);
                tiny(e.baselineCount&&e.encounterCount?(field::deviation(e,e.encounterRSSI[e.encounterCount-1])?"RSSI deviation >=12 dB":"Latest within 12 dB baseline"):"No baseline comparison yet",7,87,AMBER,226);
                tiny("Compare same setup; no place model",7,105,MUTED,226);
                footer("B Baseline X Export </> Page Esc List");
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
    loadSettings(); M5.Display.setBrightness(brightness); M5.Speaker.setVolume(volume); gps.begin();
    canvas.setColorDepth(16);
    if(!canvas.createSprite(240,135)) {
        M5.Display.fillScreen(ERROR_COLOR); M5.Display.drawString("Display memory error",4,20);
        while(true) delay(100);
    }
    header("RF EXPLORER / 2.1");
    card(8,29,224,77,true); text("UNDERSTAND",20,38,ui::theme.accent,200);
    for(int i=0;i<9;++i) canvas.fillRect(20+i*9,83-(i%5)*5,5,8+(i%5)*5,i>5?ui::theme.good:ui::theme.accent);
    tiny("Receive. Observe. Remember.",20,62,ui::theme.ink,201);
    tiny("Starting RF / GNSS / memory",20,94,MUTED,201); canvas.pushSprite(0,0);
    // Deselect all three devices before clocks or library initialisation.
    for(int cs:{pins::radioCS,pins::nfcCS,pins::sdCS}) { pinMode(cs,OUTPUT); digitalWrite(cs,HIGH); }
    SPI.begin(pins::sck,pins::miso,pins::mosi,-1);
    uint32_t boot=prefsReady?prefs.getUInt("boot",0)+1:esp_random();
    if(prefsReady) prefs.putUInt("boot",boot);
    logs.begin(boot);
    satLog.begin();
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
    M5Cardputer.update(); handleKeys(); sampleRadio(); analyseRadio(); sampleSweep();
    signalMemory.flushIfDue(millis(),30000);
    if(journalDirty && rf::elapsed(millis(),journalAt,30000)) saveJournal();
    updateLiveMatch();
    if(rf::elapsed(millis(),drawAt,70)) { drawAt=millis(); draw(); }
    delay(1);
}
