#pragma once
#include "ui/UiTheme.h"
#include "ui/UiCommon.h"
#include "presentation/PixelRenderer.h"
#include <algorithm>

namespace AmoledV1::ThemedUi {

inline int textWidth(const char* value,int size=32){
    return UiCommon::textWidth(value)/(size==16?2:1);
}
inline void text(Canvas565& canvas,int x,int y,const char* value,
                 uint16_t color,int size=32){
    const bool native=canvas.nativeText();
    canvas.setNativeText(size==32);
    PixelRenderer::text(canvas,x,y,value,color);
    canvas.setNativeText(native);
}
inline void drawHeader(Canvas565& canvas,const char* title){
    const auto& colors=UiTheme::current();
    canvas.fillRect(0,0,AmoledUi::WIDTH,UiMetrics::PAGE_HEADER_HEIGHT,colors.background);
    canvas.fillCircle(40,38,26,colors.control);
    UiCommon::drawBackChevron(canvas,29,38,16,colors.text);
    text(canvas,340-textWidth(title),22,title,colors.title);
}
inline void drawToast(Canvas565& canvas,const char* value){
    if(!value || !*value)return;
    const auto& colors=UiTheme::current();
    const int size=textWidth(value)>324?16:32;
    const int width=std::min(352,textWidth(value,size)+28);
    canvas.fillRoundRect((368-width)/2,UiCommon::TOAST_TOP,width,
                         UiCommon::TOAST_HEIGHT,16,colors.text);
    text(canvas,(368-textWidth(value,size))/2,
         UiCommon::TOAST_TOP+(UiCommon::TOAST_HEIGHT-size)/2,
         value,colors.accentText,size);
}

} // namespace AmoledV1::ThemedUi
