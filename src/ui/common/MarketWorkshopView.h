#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "core/RoomRenderer.h"
#include "core/UiStrings.h"
#include "game/HomeFurnitureService.h"
#include "game/MarketWorkshopService.h"
#include "presentation/WallpaperArt.h"
#include "presentation/FurnitureArt.h"
#include "ui/common/MarketCoinHud.h"
#include "ui/common/MarketStallMotion.h"

namespace Ui {
struct MarketWorkshopView {
    using Rect = MarketStallMotion::Rect;

    int width=368, height=448;
    bool round=false;

    Rect panel() const { return round ? Rect{48,132,264,236} : Rect{4,208,360,236}; }
    Rect row() const { return round ? Rect{58,146,244,80} : Rect{14,246,340,80}; }
    Rect furnitureRow() const {
        auto r=row();
        r.y+=r.h;
        return r;
    }
    Rect furnitureBack() const {
        const auto p=panel();
        return {p.x+8,p.y+6,48,44};
    }
    Rect room() const { return round ? Rect{42,70,276,144} : Rect{24,68,320,218}; }
    Rect back() const { return round ? Rect{88,48,44,40} : Rect{12,8,56,48}; }
    Rect selectionRow() const { return {0, round ? 216 : 294, width, round ? 52 : 60}; }
    static constexpr int swatchCount = int(Game::MarketWorkshop::WALLPAPER_COUNT);
    int swatchPitch() const { return round ? 52 : 60; }
    int dividerX() const { return round ? 211 : 271; }
    Rect swatchViewport() const {
        const auto row = selectionRow();
        const int left = round ? 26 : 6;
        const int available = dividerX() - 5 - left;
        const int visible = std::min(swatchCount*swatchPitch()-4, available);
        return {left + (available-visible)/2, row.y, visible, row.h};
    }
    Rect swatch(int index, int offset) const {
        const auto v = swatchViewport();
        const int size = swatchPitch()-4;
        return {v.x + index*swatchPitch()-offset, v.y+2, size, size};
    }
    int maxSwatchOffset() const { return swatchCount*swatchPitch()-4 - swatchViewport().w; }
    int swatchAt(int x, int y, int offset) const {
        const auto v = swatchViewport();
        if (!v.contains(x,y)) return -1;
        const int index = (x - v.x + offset)/swatchPitch();
        return index < swatchCount && swatch(index, offset).contains(x,y) ? index : -1;
    }
    // Snap to the nearest tile edge (the far end may be a fractional tile).
    // Keep the chosen swatch completely in view after a tap or a room swipe.
    int settledOffset(int offset, int selected) const {
        const int pitch = swatchPitch(), last = maxSwatchOffset();
        int snapped = std::min(last, ((offset + pitch/2)/pitch)*pitch);
        if (last-offset < std::abs(snapped-offset)) snapped = last;
        const int left = selected*pitch;
        const int right = left + pitch-4;
        const int firstVisible = std::max(0, (right - swatchViewport().w + pitch-1)/pitch)*pitch;
        return std::min(last, std::clamp(snapped, firstVisible, left));
    }
    Rect action() const { return round ? Rect{72,276,216,44} : Rect{24,362,320,52}; }

    static bool night(const Game::GameState& state) {
        const auto minute=state.gameMinutesTotal%(24*60);
        return minute<6*60 || minute>=18*60;
    }
    void menu(const Game::GameState& state,const MarketCoinMotion* motion,int offset,int hudOffset,bool furniture) const {
        auto& c=PixelRenderer::canvas();
        const auto p=panel(),r=row();
        c.fillRoundRect(p.x,p.y+offset,p.w,p.h,14,paper());
        c.drawRoundRect(p.x,p.y+offset,p.w,p.h,14,edge());
        if (furniture) {
            const auto b=furnitureBack();
            const int x=b.x+b.w/2, y=b.y+b.h/2+offset;
            for (int d=-1;d<=1;++d) {
                c.drawLine(x+4+d,y-8,x-4+d,y,ink());
                c.drawLine(x-4+d,y,x+4+d,y+8,ink());
            }
            centerText(width/2,p.y+96+offset,MarketWorkshopText::PREPARING,BitmapText::Font::PX32,ink());
        } else {
            const int iconX=r.x+10, labelX=iconX+64;
            const int mid=r.y+r.h/2+offset;
            const int furnitureMid=furnitureRow().y+r.h/2+offset;
            WallpaperArt::drawWorkshopIcon(iconX,mid-24,48);
            PixelRenderer::bitmapText(c,labelX,mid-16,MarketWorkshopText::WALLPAPER,ink(),BitmapText::Font::PX32);
            FurnitureArt::drawFrame(c,iconX+24,furnitureMid,44);
            PixelRenderer::bitmapText(c,labelX,furnitureMid-16,MarketWorkshopText::FURNITURE,ink(),BitmapText::Font::PX32);
        }
        wallet(state,motion,hudOffset);
    }

    void wallpaper(const Game::GameState& state,int selected,int offset,bool failed,const uint16_t* photo,
                   const MarketCoinMotion* motion) const {
        const auto& entry = Game::MarketWorkshop::WALLPAPERS[selected];
        auto& c=PixelRenderer::canvas();
        c.fillRect(0,0,width,height,0);
        const auto row=selectionRow();
        const auto panelColor=PixelRenderer::rgb(10,28,32);
        c.fillRect(0,row.y-2,width,height-row.y+2,panelColor);
        c.drawLine(0,row.y-2,width-1,row.y-2,PixelRenderer::rgb(48,82,86));
        if (c.clipBottom()>row.y && c.clipTop()<row.y+row.h)
            drawSelection(selected,offset);
        if (c.clipTop()>=row.y && c.clipBottom()<=row.y+row.h) return;

        const auto r=room();
        const bool shown=RoomRenderer::drawPreview(r.x,r.y,r.w,r.h,night(state),Game::HomeFurniture::visibleMask(state),photo,entry.id);
        if(!shown)centerText(width/2,r.y+(r.h-20)/2,MarketWorkshopText::UNAVAILABLE,BitmapText::Font::PX20,paper());

        const auto price = entry.price;
        const bool equipped = Game::MarketWorkshop::equippedWallpaper(state) == entry.id;
        const bool enabled = shown && !equipped && state.coins >= price;
        const auto b = action();
        c.fillRoundRect(b.x,b.y,b.w,b.h,9,enabled ? PixelRenderer::rgb(14,118,83) : PixelRenderer::rgb(52,61,63));
        c.drawRoundRect(b.x,b.y,b.w,b.h,9,enabled ? PixelRenderer::rgb(118,225,175) : PixelRenderer::rgb(82,94,96));
        char purchase[64];
        const char* label = equipped ? MarketWorkshopText::EQUIPPED : MarketWorkshopText::EQUIP;
        if (!equipped && price) {
            std::snprintf(purchase,sizeof(purchase),MarketWorkshopText::BUY,unsigned(price));
            label = purchase;
        }
        auto font = round ? BitmapText::Font::PX20 : BitmapText::Font::PX32;
        if (textWidth(label,font) > b.w-8)
            font = round ? BitmapText::Font::PX16 : BitmapText::Font::PX20;
        centerText(width/2,b.y+(b.h-BitmapText::height(font))/2,label,font,enabled ? paper() : muted());
        if (failed) centerText(width/2,b.y+b.h+2,HomeLibraryText::FAILED,BitmapText::Font::PX16,paper());

        const auto header=back();
        const int bx=header.x+header.w/2,by=header.y+header.h/2;
        c.fillCircle(bx,by,round ? 15 : 20,dark());
        for(int d=-1;d<=1;++d){
            c.drawLine(bx+3+d,by-7,bx-4+d,by,paper());
            c.drawLine(bx-4+d,by,bx+3+d,by+7,paper());
        }
        wallet(state,motion,0);
    }
private:
    void drawSelection(int selected,int offset) const {
        const auto row=selectionRow();
        auto& c=PixelRenderer::canvas();
        const auto strip = swatchViewport();
        const int left = c.clipLeft(), top = c.clipTop(), right = c.clipRight(), bottom = c.clipBottom();
        const int clipX = std::max(left, strip.x), clipY = std::max(top, strip.y);
        c.setClipRect(clipX, clipY, std::max(0, std::min(right, strip.x+strip.w)-clipX),
                      std::max(0, std::min(bottom, strip.y+strip.h)-clipY));
        const int first = offset/swatchPitch();
        for (int i = first; i < swatchCount; ++i) {
            const auto hit = swatch(i, offset);
            if (hit.x >= strip.x + strip.w) break;
            const int size = round ? 36 : 48;
            const int x = hit.x + (hit.w - size)/2, y = hit.y + (hit.h - size)/2;
            WallpaperArt::drawSwatch(x, y, size, Game::MarketWorkshop::WALLPAPERS[i].id);
            if (i == selected) {
                const auto gold = PixelRenderer::rgb(250, 218, 116);
                c.drawRoundRect(x-3, y-3, size+6, size+6, 4, gold);
                c.drawRoundRect(x-2, y-2, size+4, size+4, 3, gold);
            }
        }
        c.setClipRect(left,top,right-left,bottom-top);
        const int divider = dividerX();
        c.fillRect(divider, row.y+10, 2, row.h-19, PixelRenderer::rgb(65, 99, 100));
        constexpr auto font = BitmapText::Font::PX20;
        PixelRenderer::bitmapText(c, divider+7, row.y+(row.h-BitmapText::height(font))/2,
                                  Game::MarketWorkshop::WALLPAPERS[selected].name, paper(), font);
    }
    static uint16_t paper() {return PixelRenderer::rgb(248,248,224);}
    static uint16_t ink() {return PixelRenderer::rgb(40,56,80);}
    static uint16_t edge() {return PixelRenderer::rgb(48,64,112);}
    static uint16_t dark() {return PixelRenderer::rgb(27,43,51);}
    static uint16_t muted() {return PixelRenderer::rgb(173,185,181);}
    static int textWidth(const char* text,BitmapText::Font font){
        const char* pos=text;
        const char* end=text+std::strlen(text);
        int width=0;
        while(pos<end)width+=BitmapText::advance(BitmapText::decode(pos,end),font);
        return width;
    }
    static void centerText(int x,int y,const char* text,BitmapText::Font font,uint16_t color){
        PixelRenderer::bitmapText(PixelRenderer::canvas(),x-textWidth(text,font)/2,y,text,color,font);
    }
    void wallet(const Game::GameState& state,const MarketCoinMotion* motion,int offset) const {
        if (round) MarketCoinHud::renderRound(state.coins,motion,offset);
        else MarketCoinHud::render(state.coins,motion,16+offset);
    }
};
} // namespace Ui
