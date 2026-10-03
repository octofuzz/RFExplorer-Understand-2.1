#pragma once
#include "Arduino.h"
#include <fstream>
#include <cmath>
#include <cassert>
#define PROGMEM
#include "Font16.h"
namespace font0data {
#include "glcdfont.h"
}
namespace fonts { static int Font0=0,Font2=2; }
struct HostCanvas {
    uint16_t pixels[135][240]{};
    int selectedFont=0; uint16_t ink=0xffff;
    void setFont(const int* f) { selectedFont=*f; }
    void setTextSize(int){}
    void setTextColor(uint16_t c) { ink=c; }
    int textWidth(const String& s) {
        int w=0; for(unsigned i=0;i<s.length();++i) { unsigned c=uint8_t(s[i]); w+=selectedFont==2&&c>=32&&c<128?widtbl_f16[c-32]:6; } return w;
    }
    void drawPixel(int x,int y,uint16_t c) { if(x>=0&&x<240&&y>=0&&y<135) pixels[y][x]=c; }
    void fillRect(int x,int y,int w,int h,uint16_t c) { for(int j=0;j<h;++j) for(int i=0;i<w;++i) drawPixel(x+i,y+j,c); }
    void fillScreen(uint16_t c) { fillRect(0,0,240,135,c); }
    void drawFastHLine(int x,int y,int w,uint16_t c) { fillRect(x,y,w,1,c); }
    void drawFastVLine(int x,int y,int h,uint16_t c) { fillRect(x,y,1,h,c); }
    void drawRect(int x,int y,int w,int h,uint16_t c) {
        drawFastHLine(x,y,w,c); drawFastHLine(x,y+h-1,w,c); drawFastVLine(x,y,h,c); drawFastVLine(x+w-1,y,h,c);
    }
    void fillRoundRect(int x,int y,int w,int h,int r,uint16_t c) {
        for(int j=0;j<h;++j) for(int i=0;i<w;++i) {
            int dx=i<r?r-i:i>=w-r?i-(w-r-1):0;
            int dy=j<r?r-j:j>=h-r?j-(h-r-1):0;
            if(dx*dx+dy*dy<=r*r) drawPixel(x+i,y+j,c);
        }
    }
    void drawRoundRect(int x,int y,int w,int h,int r,uint16_t c) {
        drawFastHLine(x+r,y,w-2*r,c); drawFastHLine(x+r,y+h-1,w-2*r,c);
        drawFastVLine(x,y+r,h-2*r,c); drawFastVLine(x+w-1,y+r,h-2*r,c);
        for(int i=0;i<=r;++i) { int j=int(std::round(std::sqrt(float(r*r-i*i))));
            drawPixel(x+r-i,y+r-j,c); drawPixel(x+w-r-1+i,y+r-j,c);
            drawPixel(x+r-i,y+h-r-1+j,c); drawPixel(x+w-r-1+i,y+h-r-1+j,c);
        }
    }
    void fillCircle(int x,int y,int r,uint16_t c) { for(int j=-r;j<=r;++j) for(int i=-r;i<=r;++i) if(i*i+j*j<=r*r) drawPixel(x+i,y+j,c); }
    void drawCircle(int x,int y,int r,uint16_t c) {for(int a=0;a<360;++a) drawPixel(x+int(std::round(r*std::cos(a*3.14159/180))),y+int(std::round(r*std::sin(a*3.14159/180))),c);}
    void drawLine(int x,int y,int x1,int y1,uint16_t c) {
        int dx=abs(x1-x),sx=x<x1?1:-1,dy=-abs(y1-y),sy=y<y1?1:-1,err=dx+dy;
        for(;;) { drawPixel(x,y,c); if(x==x1&&y==y1) break; int e=2*err; if(e>=dy) {err+=dy;x+=sx;} if(e<=dx) {err+=dx;y+=sy;} }
    }
    void drawString(const String& s,int x,int y) {
        // Exact bundled bitmap font glyphs; geometry comes from the firmware draw() cases.
        assert(x>=0&&y>=0&&x+textWidth(s)<=240&&y+(selectedFont==2?16:8)<=135);
        for(unsigned i=0;i<s.length();++i) {
            unsigned c=uint8_t(s[i]); if(c<32||c>127) c='?';
            if(selectedFont==2) {
                int w=widtbl_f16[c-32],bytes=(w+6)/8;
                const unsigned char* glyph=chrtbl_f16[c-32];
                for(int row=0;row<16;++row) for(int col=0;col<w-1;++col)
                    if(glyph[row*bytes+col/8]&(0x80>>(col%8))) drawPixel(x+col,y+row,ink);
                x+=w;
            } else {
                for(int col=0;col<5;++col) for(int row=0;row<8;++row)
                    if(font0data::font[c*5+col]&(1<<row)) drawPixel(x+col,y+row,ink);
                x+=6;
            }
        }
    }
    void save(const char* path) {
        std::ofstream f(path,std::ios::binary); f<<"P6\n240 135\n255\n";
        for(auto& row:pixels) for(uint16_t c:row) { char rgb[]={char(((c>>11)&31)*255/31),char(((c>>5)&63)*255/63),char((c&31)*255/31)}; f.write(rgb,3); }
    }
};
