#pragma once
#include <algorithm>
#include "presentation/PixelRenderer.h"

namespace FurnitureArt {
inline uint16_t framePixel(int x,int y,bool night=false){
    if(x<0||x>=112||y<0||y>=112)return 0;
    int r=88,g=56,b=40;
    const auto set=[&](int nr,int ng,int nb){r=nr;g=ng;b=nb;};
    if(x>=3&&x<109&&y>=3&&y<109)set(184,128,72);
    if(x>=6&&x<106&&y>=6&&y<12)set(232,188,120);
    if(x>=6&&x<12&&y>=6&&y<106)set(224,172,104);
    if(x>=9&&x<103&&y>=97&&y<103)set(136,88,56);
    if(x>=97&&x<103&&y>=9&&y<103)set(136,88,56);
    if(x>=12&&x<100&&y>=12&&y<100&&(x==12||x==99||y==12||y==99))set(104,72,48);
    if(x>=15&&x<97&&y>=15&&y<97)set(248,240,212);
    if(x>=20&&x<92&&y>=20&&y<92)set(228,232,208);
    if(x>=25&&y>=25&&x+y<=71)set(244,244,224);
    if(x>=24&&x<88&&y>=24&&y<26)set(248,248,232);
    if(night){r=r*3/5;g=g*3/5;b=b*3/5;}
    return PixelRenderer::rgb(uint8_t(r),uint8_t(g),uint8_t(b));
}
inline void drawFrame(Canvas565& c,int cx,int cy,int size=112){
    if(size<=0)return;
    const int x=cx-size/2,y=cy-size/2;
    if(size==112){
        const auto rgb=[](int r,int g,int b){return PixelRenderer::rgb(uint8_t(r),uint8_t(g),uint8_t(b));};
        c.fillRect(x+6,y+7,112,112,rgb(56,40,24));c.fillRect(x,y,112,112,rgb(88,56,40));
        c.fillRect(x+3,y+3,106,106,rgb(184,128,72));c.fillRect(x+6,y+6,100,6,rgb(232,188,120));
        c.fillRect(x+6,y+6,6,100,rgb(224,172,104));c.fillRect(x+9,y+97,94,6,rgb(136,88,56));
        c.fillRect(x+97,y+9,6,94,rgb(136,88,56));c.drawRect(x+12,y+12,88,88,rgb(104,72,48));
        c.fillRect(x+15,y+15,82,82,rgb(248,240,212));c.fillRect(x+20,y+20,72,72,rgb(228,232,208));
        for(int i=0;i<22;++i)c.drawLine(x+25+i,y+25,x+25,y+25+i,rgb(244,244,224));
        c.fillRect(x+24,y+24,64,2,rgb(248,248,232));return;
    }
    c.fillRect(x+std::max(1,size*6/112),y+std::max(1,size*7/112),size,size,PixelRenderer::rgb(56,40,24));
    for(int row=0;row<size;++row)for(int col=0;col<size;++col)c.drawPixel(x+col,y+row,framePixel(col*112/size,row*112/size));
}
}
