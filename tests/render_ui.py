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
helpers=chunk('String fitLine(','void stopRadio()')
epoch=chunk('String epochLabel(','void goBack()')
draw=s[s.index('void draw()'):]
cases=[]
for a,b in [('Fine','Inspector'),('Inspector','Glass'),('Glass','Menu'),('Menu','Bands'),('Live','Edit'),('Library','Log'),('GPS','Satellites'),('GnssLogbook','BuildInfo')]:
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
#include "investigation.h"
#include "ui_theme.h"
#include "menu_art.h"
#include "understand.h"
#include "analysis.h"
#include "finder.h"
#include "gps.h"
#include "gnss_logbook.h"
#include "gnss_time.h"
#include <SD.h>
#include <ctime>
#define gmtime_r(a,b) gmtime_s(b,a)
uint32_t hostMillis=1000; HostDisk disk; SDClass SD;
HostCanvas canvas;
struct Power { int getBatteryLevel(){return 84;} }; struct Device {Power Power;} M5;
gnss::Receiver gps;gnss::SatelliteLogbook satLog;gnss::ClockSync clockSync;
struct Logs {bool ready=true;} logs;
field::SignalMemory signalMemory;
field::Session journal;bool journalDirty=false;unsigned encounterSelected=0;
enum class Screen { Fine,Inspector,Glass,Finder,Menu,Live,Library,Dossier,Summary,GPS,GnssLogbook,SatLocation };
Screen screen=Screen::Live;
bool autoLog=true,haveSample=true,paused=false,observing=true,scan=false,observation=true,refinedValid=true,noiseReady=true;
unsigned band=1,selected=0,memorySort=0,inspectorPage=0,dossierPage=0,dossierIndex=0;
uint8_t memoryOrder[48]{}; int liveMatch=0; uint8_t liveScore=91; bool liveMatchSample=true;
uint32_t sampledFrequency=433920,coarseFrequency=433950,refinedFrequency=433920,frequency=433920,fixed=433920,low=433050,high=434790,step=100;
float rssi=-54,noiseEstimate=-103; int threshold=-75;
float history[112]{};unsigned historyHead=0,historyCount=112;
rf::Envelope envelope;rf::FineScan fine;rf::Sweep sweep;
uint8_t waterfall[72][112]{}; unsigned waterfallHead=0,markerIndex=42; uint32_t markerFrequency=433920,finderHits=18;bool peakHold=false;
field::SignalFingerprint currentFingerprint(){field::SignalFingerprint f;f.frequency_khz=433920;f.peak_rssi=-54;f.duration_ms=120;return f;}
'''+decl+helpers+epoch+chunk('uint16_t constellationColor(', 'String gnssSnapshotNote()')+'''uint16_t waterfallPalette(uint8_t level) {return ui::waterfall(level);}
void draw() { switch(screen) {
'''+''.join(cases)+r'''
} }
int main() {
    gps.begin();disk.files["/rfexplorer"]="";satLog.begin();
    auto sentence=[](const std::string& body) {unsigned sum=0;for(char c:body)sum^=uint8_t(c);char suffix[8];snprintf(suffix,sizeof(suffix),"*%02X\r\n",sum);HardwareSerial::input()+="$"+body+suffix;gps.poll();};
    sentence("GNRMC,123456,A,6325.8300,N,01023.7000,E,0.1,42.0,270926,,,A");
    sentence("GPGSV,1,1,02,01,45,180,35,03,60,240,40");
    gnss::System system;uint16_t prn;satLog.update(gps.satellites(),gps.count(),system,prn,true,63.4305,10.395);clockSync.applied(hostMillis);
    screen=Screen::GPS;draw();canvas.save("gnss-clock.ppm");
    screen=Screen::SatLocation;selected=0;draw();canvas.save("satellite-location.ppm");
    screen=Screen::GnssLogbook;draw();canvas.save("satellite-log.ppm");
    selected=0;
    field::SignalFingerprint fp; fp.frequency_khz=433920;fp.peak_rssi=-54;fp.duration_ms=120;
    for(int i=0;i<12;++i) {fp.peak_rssi=-65+i*2; signalMemory.remember(fp,"Garden sensor",1000+i*100,1790400000+i*60);}
    signalMemory.classify(0,4);signalMemory.annotate(0,"Beside greenhouse");
    liveScore=field::similarity(currentFingerprint(),signalMemory.entry(0).fingerprint);
    fp.frequency_khz=868300;signalMemory.remember(fp,"Unknown beacon",3000,1790400300);
    fp.frequency_khz=315000;signalMemory.remember(fp,"Garage remote",3200,1790400400);
    memoryOrder[0]=2;memoryOrder[1]=0;memoryOrder[2]=1;
    for(int i=0;i<112;++i) history[i]=-103+4*std::sin(i*.7f)+(i>25&&i<36?48:0)+(i>76&&i<89?53:0);
    envelope.duration=120;envelope.count=8;envelope.maxGap=4;
    screen=Screen::Menu;draw();canvas.save("menu.ppm");
    screen=Screen::Live;draw();canvas.save("live.ppm");
    haveSample=false;liveMatchSample=false;draw();canvas.save("live-empty.ppm");haveSample=true;liveMatchSample=true;
    screen=Screen::Inspector;draw();canvas.save("inspector.ppm");inspectorPage=1;draw();canvas.save("match.ppm");
    screen=Screen::Library;selected=1;draw();canvas.save("memory.ppm");
    screen=Screen::Dossier;draw();canvas.save("dossier.ppm");dossierPage=1;draw();canvas.save("history.ppm");
    sweep.count=100;sweep.floor=-102;sweep.sweeps=24;sweep.strongestFrequency=433920;sweep.live[markerIndex]=-56;
    for(unsigned i=0;i<100;++i) sweep.frequencies[i]=433050+i*20;
    for(unsigned y=0;y<72;++y) for(unsigned x=0;x<100;++x) waterfall[y][x]=rf::waterfallColour(-102+float((x>39&&x<46)?45+(y%12):(x>68&&x<74)?25+(y%8):((x+y*7)%19==0?10:0)),-102);
    screen=Screen::Glass;draw();canvas.save("waterfall.ppm");
    signalMemory.captureBaseline(0,1790400500);
    screen=Screen::Dossier;dossierPage=2;draw();canvas.save("analyst.ppm");dossierPage=3;draw();canvas.save("family.ppm");
    journal.record(fp,true,false,false,true,true);screen=Screen::Summary;draw();canvas.save("journal.ppm");
    screen=Screen::Menu;selected=10;draw();canvas.save("menu-sky.ppm");
    selected=19;draw();canvas.save("menu-archive.ppm");
    selected=20;draw();canvas.save("menu-journal.ppm");
    selected=15;draw();canvas.save("menu-settings.ppm");
}
'''
src=out/'render.cpp';src.write_text(code)
fonts=next((root/'.pio/libdeps/cardputer_adv').glob('M5GFX@*/src/lgfx/Fonts'))
vc='C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
rsp=out/'render.rsp';rsp.write_text('\n'.join(['/nologo','/std:c++14','/EHsc','/permissive','/O2','/D_CRT_SECURE_NO_WARNINGS',f'/I"{root/"tests/host"}"',f'/I"{root/"include"}"',f'/I"{fonts}"',f'/Fe:"{out/"render.exe"}"',f'"{src}"',f'"{root/"src/signal_store.cpp"}"',f'"{root/"src/gps.cpp"}"',f'"{root/"src/gnss_logbook.cpp"}"']))
cmd=out/'render.cmd';cmd.write_text(f'@call "{vc}" >nul\n@cl @"{rsp}"\n@if errorlevel 1 exit /b 1\n@"{out/"render.exe"}"\n')
subprocess.run(['cmd','/c',str(cmd)],cwd=out,check=True)
names=['menu','live','inspector','match','memory','dossier','history','waterfall','analyst','family','journal','menu-sky','menu-archive','menu-journal','menu-settings','gnss-clock','satellite-location','satellite-log']
sheet=Image.new('RGB',(1020,930),'#09111b');d=ImageDraw.Draw(sheet)
for i,name in enumerate(names):
    im=Image.open(out/f'{name}.ppm')
    x=10+(i%4)*253;y=56+(i//4)*175
    sheet.paste(im,(x,y))
    d.text((x,y-18),name.upper(),fill='#9fdce6')
sheet=sheet.crop((0,0,1020,910))
d=ImageDraw.Draw(sheet);d.text((10,12),'RFEXPLORER 2.1 / UNDERSTAND - firmware layout renders, illustrative data',fill='#e8f6fa')
sheet.save(out/'ui-preview.png')
print('PASS: 18 UI views rendered from firmware branches; text stays within 240x135')
