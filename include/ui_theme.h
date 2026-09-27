#pragma once
#include <stdint.h>

// Semantic tokens: all new screens share these colours and the same 240x135 grid.
namespace ui {
struct Theme {
    uint16_t background, panel, raised, edge, accent, muted, ink, warning, good, error;
};
constexpr Theme fieldNight{0x0823,0x1085,0x1908,0x298b,0x3e9e,0x9d35,0xef9e,0xfdaa,0x7795,0xfaac};
constexpr Theme theme=fieldNight;
constexpr int width=240, height=135, contentTop=23, footerTop=121;
inline int strengthY(float value,int top,int height) {
    int v=int((value+120)*height/100);
    return top+height-(v<0?0:v>height?height:v);
}
template<class C> void card(C& c,int x,int y,int w,int h,bool active=false) {
    c.fillRoundRect(x,y,w,h,4,theme.panel);
    c.drawRoundRect(x,y,w,h,4,active?theme.accent:theme.edge);
}
template<class C> void trace(C& c,const float* values,unsigned count,unsigned head,unsigned capacity,
                            int x,int y,int w,int h,int threshold,bool grid=true) {
    if(grid) for(int level=-100;level<=-40;level+=20) {
        int gy=strengthY(level,y,h);
        for(int gx=x;gx<x+w;gx+=4) c.drawPixel(gx,gy,theme.edge);
    }
    if(threshold>=-120 && threshold<=-20) {
        int ty=strengthY(threshold,y,h);
        for(int gx=x;gx<x+w;gx+=6) c.drawFastHLine(gx,ty,3,theme.warning);
    }
    if(!count) return;
    unsigned oldest=(head+capacity-count)%capacity;
    for(unsigned j=0;j<count;++j) {
        int px=x+int(j)*(w-1)/int(capacity-1);
        float value=values[(oldest+j)%capacity];
        int py=strengthY(value,y,h);
        c.drawFastVLine(px,py,y+h-py+1,theme.raised);
        if(j) c.drawLine(x+int(j-1)*(w-1)/int(capacity-1),
            strengthY(values[(oldest+j-1)%capacity],y,h),px,py,theme.accent);
        if(j+1==count) c.fillCircle(px,py,2,theme.ink);
    }
}
inline uint16_t waterfall(uint8_t level) {
    // Indigo -> blue -> cyan -> green -> yellow -> orange -> red -> white.
    // Precomputed RGB565 table avoids interpolation in thousands of draw calls.
    constexpr uint16_t colours[]={0x0823,0x1006,0x1809,0x200d,0x2810,0x2014,0x1018,0x081c,
        0x009f,0x029f,0x049f,0x069f,0x07fc,0x07f4,0x07ec,0x07e4,
        0x27e0,0x67e0,0xa7e0,0xe7e0,0xff40,0xfe40,0xfd40,0xfc40,
        0xfb20,0xfa20,0xf920,0xf820,0xf9c7,0xfbcf,0xfdd7,0xffff};
    return colours[level>31?31:level];
}
}
