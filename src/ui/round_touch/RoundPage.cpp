// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/RoundPage.h"

#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"

namespace Ui { namespace RoundPage {
bool safePoint(int x, int y) {
    return x >= 0 && x < 240 && y >= 0 && y < 240;
}

bool backHit(int x, int y) {
    return BACK.contains(x, y);
}

void background() { PixelRenderer::clear(PixelRenderer::rgb(7, 12, 17)); }

void header(const char* title, bool pressed) {
    TouchUi::safePageHeader(title, pressed);
}

void nativeHeader(const char* title,bool pressed) {
    nativeHeader(title,pressed,{PixelRenderer::rgb(7,12,17),0xffff,
        PixelRenderer::rgb(pressed?53:27,pressed?76:43,pressed?83:51),PixelRenderer::rgb(67,97,101)});
}
void nativeHeader(const char* title,bool,const NativeHeaderColors& colors) {
    auto& c=PixelRenderer::canvas();const int cx=NATIVE_BACK.x+20,cy=NATIVE_BACK.y+20;
    c.fillRect(0,0,360,52,colors.background);c.fillCircle(cx,cy,16,colors.fill);c.drawCircle(cx,cy,16,colors.border);
    for(int offset=-1;offset<=1;++offset){c.drawLine(cx+2+offset,cy-7,cx-5+offset,cy,colors.ink);c.drawLine(cx-5+offset,cy,cx+2+offset,cy+7,colors.ink);}
    PixelRenderer::textSmooth(NATIVE_BACK.x+NATIVE_BACK.width+4,NATIVE_BACK.y+10,title,colors.ink,20);
}

void notice(const char* text) {
    if (!text) return;
    auto& c = PixelRenderer::canvas();
    c.fillRoundRect(NOTICE.x, NOTICE.y, NOTICE.width, NOTICE.height, 8, PixelRenderer::rgb(34, 45, 52));
    MenuGrid::label(c, 120, NOTICE.y + 5, text, NOTICE.width - 16, 0xffff);
}
} }  // namespace Ui::RoundPage
