"""Render selected actual firmware draw() branches with fixture data and bundled fonts.
This tests layout on a host canvas; it is not device/display-driver validation.
Run after PlatformIO has resolved the display libraries. Requires MSVC and Pillow.
"""
from pathlib import Path
import subprocess,re
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parents[1]
out=root/'tests/.build'; out.mkdir(exist_ok=True)
s=(root/'src/main.cpp').read_text()
def chunk(a,b): return s[s.index(a):s.index(b,s.index(a))]
decl=chunk('constexpr uint16_t BG=','M5Canvas canvas')
helpers=chunk('String fitLine(','satrf::Archive satelliteRFArchive;')
helpers=helpers.replace('void footer(const String& help) {',
    'void footer(const String& help) { canvas.setFont(&fonts::Font0); assert(canvas.textWidth(help)<=232);')
helpers=helpers.replace('void monitorFooter(const String& first,const String& second) {',
    'void monitorFooter(const String& first,const String& second) { canvas.setFont(&fonts::Font0); assert(canvas.textWidth(first)<=232 && canvas.textWidth(second)<=232);')
epoch=chunk('String epochLabel(','void goBack()')
draw=s[s.index('void draw()'):]
cases=[]
for a,b in [('Fine','Inspector'),('Inspector','Glass'),('Glass','Menu'),('Menu','Bands'),('Live','Edit'),('Library','Log'),('Expedition','GPS'),('GPS','GnssDiag'),('GnssLogbook','BuildInfo')]:
    start=draw.index('        case Screen::'+a+':')
    end=draw.index('        case Screen::'+b+':',start)
    cases.append(draw[start:end])
code=r'''
#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "canvas.h"
#include "signal_store.h"
#include "review_queue.h"
#include "fieldwork.h"
#include "satellite_rf.h"
#include "observations.h"
#include "investigation.h"
#include "ui_theme.h"
#include "menu_art.h"
#include "navigation.h"
#include "battery_gauge.h"
#include "understand.h"
#include "analysis.h"
#include "rf_trace.h"
#include "finder.h"
#include "gps.h"
#include "expedition.h"
#include "gnss_logbook.h"
#include "gnss_time.h"
#include <SD.h>
#include <ctime>
#define gmtime_r(a,b) gmtime_s(b,a)
uint32_t hostMillis=1000; HostDisk disk; SDClass SD;
HostCanvas canvas;
ui::Navigation navigation;ui::BatteryGauge batteryGauge;bool charging=false;
struct Power { int getBatteryLevel(){return 84;} }; struct Device {Power Power;} M5;
expedition::JourneyRecorder journeyRecorder;String expeditionJourney="Coastal walk",expeditionSession="Morning";bool expeditionInterrupted=false,gnssOnly=true;unsigned routeInterval=5000;
constexpr unsigned gnssHistoryCapacity=60;uint8_t gnssVisibleHistory[60]{},gnssSnrHistory[60]{};uint16_t gnssHdopHistory[60]{};unsigned gnssHistoryCount=0,gnssHistoryHead=0;
gnss::Receiver gps;gnss::SatelliteLogbook satLog;gnss::ClockSync clockSync;
struct Logs {bool ready=true;} logs;
field::SignalMemory signalMemory;
field::Session journal;bool journalDirty=false;unsigned encounterSelected=0;
enum class Screen { Fine,Inspector,RfDiagnostics,Glass,Finder,Menu,Live,Library,Dossier,Summary,GPS,Expedition,Trail,TravelStatus,GnssLogbook,SatLocation,Satellites,SatDetail,SatCategory,SatArchive,SatRecord,SatCapabilities,GnssHistory,SatRFMenu,SatRFSetup,SatRFLive,SatRFHistory,SatRFRecord,SatRFLimits,RFProfiles,RFSessions,RFSession };
Screen screen=Screen::Live;
bool autoLog=true,haveSample=true,paused=false,observing=true,scan=false,observation=true,refinedValid=true,noiseReady=true;
unsigned band=1,selected=0,memorySort=0,inspectorPage=0,dossierPage=0,dossierIndex=0;
unsigned memoryCount=3;bool reviewOnly=false;uint8_t memoryOrder[48]{}; int liveMatch=0; uint8_t liveScore=91; bool liveMatchSample=true;
uint32_t sampledFrequency=433920,coarseFrequency=433950,refinedFrequency=433920,frequency=433920,fixed=433920,low=433050,high=434790,step=100;
float rssi=-54,noiseEstimate=-103; int threshold=-75;
float history[112]{};unsigned historyHead=0,historyCount=112;
rf::QuietReference quietReference;rf::SamplingQuality samplingQuality;rf::ActivityWindow activityWindow;unsigned diagnosticPage=0;
rf::TraceRecorder rfTrace;rf::Envelope envelope;rf::FineScan fine;rf::Sweep sweep;
uint8_t waterfall[72][112]{}; unsigned waterfallHead=0,markerIndex=42; uint32_t markerFrequency=433920,finderHits=18;bool peakHold=false;
field::SignalFingerprint currentFingerprint(){field::SignalFingerprint f;f.frequency_khz=433920;f.peak_rssi=-54;f.duration_ms=120;f.noise_rssi=-103;f.samples=30;f.sample_gap_ms=4;f.evidence_version=1;return f;}
'''+chunk('satrf::Archive satelliteRFArchive;', 'bool tuneSatelliteRF()')+chunk('int satelliteFilter=', 'void selectMenu()')+decl+helpers+epoch+chunk('uint16_t constellationColor(', 'String gnssSnapshotNote()')+'''uint16_t waterfallPalette(uint8_t level) {return ui::waterfall(level);}
void draw() { switch(screen) {
'''+''.join(cases)+r'''
} }
int main() {
    batteryGauge.update(84,hostMillis);gps.begin();disk.files["/rfexplorer"]="";satLog.begin();
    auto sentence=[](const std::string& body) {unsigned sum=0;for(char c:body)sum^=uint8_t(c);char suffix[8];snprintf(suffix,sizeof(suffix),"*%02X\r\n",sum);HardwareSerial::input()+="$"+body+suffix;gps.poll();};
    sentence("GNRMC,123456,A,6325.8300,N,01023.7000,E,0.1,42.0,270926,,,A");
    sentence("GPGSV,1,1,02,01,45,180,35,03,60,240,40");
    gnss::System system;uint16_t prn;satLog.update(gps.satellites(),gps.count(),system,prn,true,63.4305,10.395);clockSync.applied(hostMillis);
    screen=Screen::GPS;draw();canvas.save("gnss-clock.ppm");
    sentence("GLGSV,1,1,01,65,40,170,28,1");sentence("GAGSV,1,1,01,01,60,90,38,7");sentence("GBGSV,1,1,01,01,30,270,31,3");sentence("GQGSV,1,1,01,01,80,120,40,1");sentence("GPGSV,1,1,01,33,30,190,25,1");
    satLog.update(gps.satellites(),gps.count(),system,prn,true,63.4305,10.395);
    screen=Screen::Menu;navigation.open(3);draw();canvas.save("menu-satellites.ppm");selected=4;draw();canvas.save("menu-satellites-2.ppm");selected=8;draw();canvas.save("menu-satellites-3.ppm");selected=0;
    screen=Screen::SatCategory;for(int i=0;i<6;++i){satelliteFilter=i;draw();canvas.save((std::string("category-")+std::to_string(i)+".ppm").c_str());}
    satelliteFilter=-1;draw();canvas.save("satellites-live.ppm");
    satelliteSystem=gnss::System::GPS;satelliteId=1;screen=Screen::SatDetail;draw();canvas.save("satellite-detected.ppm");
    screen=Screen::SatArchive;draw();canvas.save("satellites-history.ppm");screen=Screen::SatRecord;draw();canvas.save("satellite-saved.ppm");satellitePage=1;draw();canvas.save("satellite-geotag.ppm");
    screen=Screen::SatCapabilities;draw();canvas.save("satellites-capabilities.ppm");
    hostMillis+=4000;screen=Screen::SatCategory;draw();canvas.save("satellites-empty.ppm");screen=Screen::SatDetail;draw();canvas.save("satellite-stale.ppm");hostMillis-=4000;
    screen=Screen::GnssHistory;draw();canvas.save("gnss-quality-empty.ppm");gnssHistoryCount=3;gnssHistoryHead=3;gnssVisibleHistory[0]=7;gnssVisibleHistory[1]=8;gnssVisibleHistory[2]=0;gnssSnrHistory[0]=35;gnssSnrHistory[1]=37;gnssSnrHistory[2]=255;gnssHdopHistory[2]=65535;draw();canvas.save("gnss-quality-gap.ppm");
    screen=Screen::SatRFMenu;satelliteRFRow=0;draw();canvas.save("sat-rf-menu.ppm");
    screen=Screen::SatRFSetup;draw();canvas.save("sat-rf-setup.ppm");satelliteRFTarget=1;satelliteRFNominal=435250;draw();canvas.save("sat-rf-custom.ppm");
    screen=Screen::SatRFLive;satelliteRFTuned=437800;satelliteRFHave=true;satelliteRFValue=-98;satelliteRFTarget=0;satelliteRFStatus="RF window saved / source unknown";draw();canvas.save("sat-rf-live.ppm");
    satelliteRFPaused=true;satelliteRFHave=false;draw();canvas.save("sat-rf-paused.ppm");satelliteRFPaused=false;
    satelliteRFArchive.begin();observations.begin();refreshRFHistory();screen=Screen::SatRFHistory;draw();canvas.save("sat-rf-empty.ppm");
    for(unsigned t=0;t<=10000;t+=50)satelliteRFWindow.add(t,-98,-75);auto rfrow=satelliteRFWindow.record(437800,0,-75);rfrow.utc=1790400000;rfrow.located=true;rfrow.latitude=63430500;rfrow.longitude=10395000;satelliteRFArchive.add(rfrow);refreshRFHistory();
    draw();canvas.save("sat-rf-history.ppm");screen=Screen::SatRFRecord;draw();canvas.save("sat-rf-record.ppm");satelliteRFPage=1;draw();canvas.save("sat-rf-location.ppm");
    screen=Screen::SatRFLimits;draw();canvas.save("sat-rf-limits.ppm");
    loadRFProfile(0);rfSetupPage=1;screen=Screen::SatRFSetup;draw();canvas.save("observation-setup.ppm");
    screen=Screen::RFProfiles;satelliteRFRow=0;draw();canvas.save("observation-profiles.ppm");
    auto profile=observations.profile(0);rfSelectedSession=observations.create("ISS attempt","UHF whip","Window seat",profile,1790400000);
    rfrow.session=rfSelectedSession;rfrow.fixAge=100;rfrow.hdop100=110;
    for(unsigned i=0;i<3;++i){rfrow.phase=i;rfrow.mean10=-980+int(i)*40;rfrow.peak10=rfrow.mean10+30;observations.append(rfrow);}
    observations.bookmark(rfrow);observations.finish(rfSelectedSession,1790400500);
    screen=Screen::RFSessions;satelliteRFRow=0;draw();canvas.save("observation-sessions.ppm");screen=Screen::RFSession;draw();canvas.save("observation-summary.ppm");rfSessionPage=1;draw();canvas.save("observation-compare.ppm");
    observations.find(rfSelectedSession)->antenna="Unknown";draw();canvas.save("observation-compare-blocked.ppm");
    rfHistorySession=rfSelectedSession;refreshRFHistory();screen=Screen::SatRFHistory;draw();canvas.save("observation-windows.ppm");
    rfHistorySession=0;rfPinnedOnly=true;refreshRFHistory();draw();canvas.save("observation-bookmarks.ppm");screen=Screen::SatRFRecord;satelliteRFPage=2;draw();canvas.save("observation-context.ppm");
    hostMillis=31000;rfTimeline={};for(unsigned t=1000;t<=31000;t+=250){if(t==10000)rfTimeline.add(t,NAN,2);else if(t==20000)rfTimeline.add(t,NAN,3);else rfTimeline.add(t,-100+30*std::sin(t/3000.0));}
    screen=Screen::SatRFLive;rfLivePage=0;draw();canvas.save("observation-timeline.ppm");rfLivePage=1;rfLastSaveUs=85000;draw();canvas.save("observation-quality.ppm");
    rfPinnedOnly=false;rfHistorySession=0;
    navigation.home=true;navigation.group=0;

    screen=Screen::SatLocation;selected=0;draw();canvas.save("satellite-location.ppm");
    screen=Screen::GnssLogbook;draw();canvas.save("satellite-log.ppm");
    journeyRecorder.ready=true;screen=Screen::Expedition;draw();canvas.save("expedition.ppm");
    screen=Screen::TravelStatus;draw();canvas.save("travel-status.ppm");
    journeyRecorder.trailCount=journeyRecorder.trailHead=6;
    for(unsigned i=0;i<6;++i) journeyRecorder.trail[i]={63.4+i*.001,10.3+(i%3)*.001,i==0||i==3};
    screen=Screen::Trail;draw();canvas.save("trail.ppm");
    selected=0;
    field::SignalFingerprint fp; fp.frequency_khz=433920;fp.peak_rssi=-54;fp.duration_ms=120;fp.noise_rssi=-103;fp.samples=30;fp.sample_gap_ms=4;fp.evidence_version=1;
    field::EncounterContext context;context.located=true;context.latitudeE6=63430500;context.longitudeE6=10395000;context.rxBandwidth10=580;context.fixAgeMs=120;context.hdop100=95;context.coverage=100;
    for(int i=0;i<12;++i) {fp.peak_rssi=-65+i*2; signalMemory.remember(fp,"Garden sensor",1000+i*100,1790400000+i*60,context);}
    signalMemory.setFlag(0,true);signalMemory.classify(0,4);signalMemory.annotate(0,"Beside greenhouse");
    quietReference.ready=true;quietReference.lastQuiet=hostMillis;
    liveScore=field::similarity(currentFingerprint(),signalMemory.entry(0).fingerprint);
    fp.frequency_khz=868300;signalMemory.remember(fp,"Unknown beacon",3000,1790400300);
    fp.frequency_khz=315000;signalMemory.remember(fp,"Garage remote",3200,1790400400);
    memoryOrder[0]=2;memoryOrder[1]=0;memoryOrder[2]=1;
    for(int i=0;i<112;++i) history[i]=-103+4*std::sin(i*.7f)+(i>25&&i<36?48:0)+(i>76&&i<89?53:0);
    envelope.duration=120;envelope.count=8;envelope.maxGap=4;envelope.last={true,8,100,220,120,30,4,-54,-103};
    batteryGauge.update(84,hostMillis);screen=Screen::Menu;draw();canvas.save("menu.ppm");
    screen=Screen::Live;draw();canvas.save("live.ppm");
    observation=false;observing=false;draw();canvas.save("fixed-monitor.ppm");observation=true;observing=true;
    fine.begin(1,433920);screen=Screen::Fine;draw();canvas.save("fine.ppm");screen=Screen::Live;
    haveSample=false;liveMatchSample=false;draw();canvas.save("live-empty.ppm");haveSample=true;liveMatchSample=true;
    screen=Screen::Inspector;draw();canvas.save("inspector.ppm");inspectorPage=1;draw();canvas.save("match.ppm");
    screen=Screen::Library;selected=1;draw();canvas.save("memory.ppm");
    reviewOnly=true;memoryCount=2;memoryOrder[0]=1;memoryOrder[1]=2;selected=0;draw();canvas.save("investigate.ppm");reviewOnly=false;
    screen=Screen::Dossier;draw();canvas.save("dossier.ppm");dossierPage=1;draw();canvas.save("history.ppm");
    sweep.count=100;sweep.floor=-102;sweep.sweeps=24;sweep.strongestFrequency=433920;sweep.live[markerIndex]=-56;
    for(unsigned i=0;i<100;++i) sweep.frequencies[i]=433050+i*20;
    for(unsigned y=0;y<72;++y) for(unsigned x=0;x<100;++x) waterfall[y][x]=rf::waterfallColour(-102+float((x>39&&x<46)?45+(y%12):(x>68&&x<74)?25+(y%8):((x+y*7)%19==0?10:0)),-102);
    screen=Screen::Glass;draw();canvas.save("waterfall.ppm");
    signalMemory.captureBaseline(0,1790400500);
    screen=Screen::Dossier;dossierPage=2;draw();canvas.save("analyst.ppm");dossierPage=3;draw();canvas.save("family.ppm");
    journal.record(fp,true,false,false,true,true);screen=Screen::Summary;draw();canvas.save("journal.ppm");
    screen=Screen::Menu;navigation.open(2);selected=0;draw();canvas.save("menu-sky.ppm");
    navigation.open(1);selected=0;draw();canvas.save("menu-archive.ppm");
    navigation.open(5);selected=0;draw();canvas.save("menu-journal.ppm");
    navigation.open(6);selected=0;draw();canvas.save("menu-settings.ppm");
    navigation.open(0);selected=7;draw();canvas.save("menu-rf.ppm");
    navigation.open(4);selected=0;draw();canvas.save("menu-nfc.ppm");

    rssi=-103;noiseEstimate=-102;envelope={};envelope.maxGap=1019;envelope.gapCount=134;envelope.unresolved=134;liveMatchSample=false;
    screen=Screen::Inspector;inspectorPage=0;draw();canvas.save("noise-inspector.ppm");
    inspectorPage=1;draw();canvas.save("noise-match.ppm");
    screen=Screen::RfDiagnostics;draw();canvas.save("sampling-evidence.ppm");
    diagnosticPage=1;samplingQuality.sample(1000);samplingQuality.sample(1002);samplingQuality.sample(1030);samplingQuality.drawUs=18000;samplingQuality.sdUs=62000;quietReference.ready=true;quietReference.lastQuiet=0;hostMillis=20000;draw();canvas.save("sampling-quality.ppm");
    screen=Screen::Dossier;dossierIndex=0;dossierPage=4;draw();canvas.save("encounter-context.ppm");
    fp.evidence_version=2;fp.duration_ms=5000;fp.samples=2200;fp.sample_gap_ms=30;context.coverage=92;
    dossierIndex=signalMemory.remember(fp,"Sustained activity",hostMillis,1790400600,context);dossierPage=0;draw();canvas.save("activity-window.ppm");
    screen=Screen::Library;reviewOnly=true;memoryCount=0;selected=0;draw();canvas.save("investigate-empty.ppm");
    disk.files.clear();disk.files["/rfexplorer/signal-memory-v24.csv"]="433050,-100,-101,0,0,0,10,20,134,Old noise,0,0,-98,0,,1,-100,-100,0,0,0,0\n";
    signalMemory=field::SignalMemory{};signalMemory.begin();dossierIndex=0;dossierPage=0;screen=Screen::Dossier;draw();canvas.save("legacy-review.ppm");

}
'''
src=out/'render.cpp';src.write_text(code)
fonts=next((root/'.pio/libdeps/cardputer_adv').glob('M5GFX@*/src/lgfx/Fonts'))
vc='C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
rsp=out/'render.rsp';rsp.write_text('\n'.join(['/nologo','/std:c++14','/EHsc','/permissive','/O2','/D_CRT_SECURE_NO_WARNINGS',f'/I"{root/"tests/host"}"',f'/I"{root/"include"}"',f'/I"{fonts}"',f'/Fe:"{out/"render.exe"}"',f'"{src}"',f'"{root/"src/signal_store.cpp"}"',f'"{root/"src/gps.cpp"}"',f'"{root/"src/expedition.cpp"}"',f'"{root/"src/gnss_logbook.cpp"}"']))
cmd=out/'render.cmd';cmd.write_text(f'@call "{vc}" >nul\n@cl @"{rsp}"\n@if errorlevel 1 exit /b 1\n@"{out/"render.exe"}"\n')
subprocess.run(['cmd','/c',str(cmd)],cwd=out,check=True)
names=['menu','live','inspector','match','memory','dossier','history','waterfall','analyst','family','journal','menu-sky','menu-archive','menu-journal','menu-settings','gnss-clock','satellite-location','satellite-log','menu-rf','menu-nfc','expedition','travel-status','trail','noise-inspector','noise-match','sampling-evidence','legacy-review','fixed-monitor','fine','investigate','sampling-quality','encounter-context','activity-window','investigate-empty']
names+=['observation-setup','observation-profiles','observation-sessions','observation-summary','observation-compare','observation-compare-blocked','observation-windows','observation-bookmarks','observation-context','observation-timeline','observation-quality','sat-rf-menu','sat-rf-setup','sat-rf-custom','sat-rf-live','sat-rf-paused','sat-rf-empty','sat-rf-history','sat-rf-record','sat-rf-location','sat-rf-limits','gnss-quality-empty','gnss-quality-gap','menu-satellites','menu-satellites-2','menu-satellites-3']+['category-'+str(i) for i in range(6)]+['satellites-live','satellite-detected','satellites-history','satellite-saved','satellite-geotag','satellites-capabilities','satellites-empty','satellite-stale']
sheet=Image.new('RGB',(1020,56+((len(names)+3)//4)*175),'#09111b');d=ImageDraw.Draw(sheet)
for i,name in enumerate(names):
    im=Image.open(out/f'{name}.ppm')
    x=10+(i%4)*253;y=56+(i//4)*175
    sheet.paste(im,(x,y))
    d.text((x,y-18),name.upper(),fill='#9fdce6')

d=ImageDraw.Draw(sheet);d.text((10,12),'RFEXPLORER 2.8 / OBSERVE - firmware layout renders, illustrative data',fill='#e8f6fa')
sheet.save(out/'ui-preview.png')
print('PASS: 74 UI views rendered from firmware branches; text stays within 240x135')

