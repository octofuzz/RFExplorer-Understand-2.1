#pragma once
#include "ui_theme.h"
namespace ui {
enum class Art { Radio,Sky,Archive,Journal,Instrument };
template<class C> void headerTexture(C& c) {
    for(int x=112;x<151;x+=5) c.drawFastVLine(x,13-(x%7),5,theme.edge);
}
// Procedural line artwork stays crisp at the native 240x135 resolution.
template<class C> void banner(C& c,Art art,int y,int h) {
    c.fillRect(0,y,240,h,theme.background);
    for(int x=0;x<240;x+=12) c.drawPixel(x,y+h-3,theme.edge);
    const int x=178,b=y+h-4;
    if(art==Art::Radio) {
        c.drawLine(x,b,x+10,y+3,theme.muted);c.drawLine(x+10,y+3,x+20,b,theme.muted);
        c.drawLine(x+4,b-7,x+16,b-7,theme.edge);c.drawLine(x+10,y+3,x+10,b,theme.edge);
        c.drawCircle(x+10,y+5,3,theme.accent);
        for(int i=0;i<16;++i) c.drawLine(210+i,b-5+(i%4),211+i,b-5+((i+1)%4),theme.edge);
    } else if(art==Art::Sky) {
        c.drawCircle(x+24,y+13,9,theme.edge); c.drawLine(x+8,y+18,x+40,y+5,theme.muted);
        c.fillRect(x+24,y+8,3,3,theme.accent);
        for(int i=0;i<7;++i)c.drawPixel(x+(i*17)%49,y+2+(i*7)%18,theme.muted);
    } else if(art==Art::Archive) {
        for(int i=2;i>=0;--i)c.drawRect(x+i*3,y+3+i*3,30,14,theme.edge);
        c.drawFastHLine(x+7,y+12,17,theme.accent);c.drawFastHLine(x+7,y+16,10,theme.muted);
    } else if(art==Art::Journal) {
        int px=x,py=b;
        for(int i=0;i<5;++i) {int nx=x+i*10,ny=y+4+(i*7)%16;c.drawLine(px,py,nx,ny,theme.edge);c.drawCircle(nx,ny,2,i==4?theme.accent:theme.muted);px=nx;py=ny;}
    } else {
        for(int i=0;i<3;++i) {c.drawRect(x+i*15,y+5,10,14,theme.edge);c.drawFastHLine(x+2+i*15,y+10+i*3,6,theme.accent);}
    }
}
}
