#include "ui/SettingsScreens.h"
#include "ui/ThemedUi.h"
#include "ui/models/ScreenModels.h"
#include "core/UiStrings.h"
#include <algorithm>
#include <cstdio>

namespace AmoledV1 {
namespace {
void drawSliderIcon(Canvas565& c,int item,int cy,uint16_t color){
    constexpr int cx=52;
    if(item==SETTINGS_BRIGHTNESS_ITEM){
        c.drawCircle(cx,cy,5,color);c.drawCircle(cx,cy,4,color);
        for(int sign : {-1,1}){
            c.drawLine(cx+sign*8,cy,cx+sign*11,cy,color);
            c.drawLine(cx,cy+sign*8,cx,cy+sign*11,color);
            c.drawLine(cx-8,cy+sign*8,cx-6,cy+sign*6,color);
            c.drawLine(cx+8,cy+sign*8,cx+6,cy+sign*6,color);
        }
    }else{
        c.drawRect(cx-10,cy-4,6,9,color);
        c.drawLine(cx-4,cy-4,cx+2,cy-9,color);
        c.drawLine(cx+2,cy-9,cx+2,cy+9,color);
        c.drawLine(cx+2,cy+9,cx-4,cy+4,color);
        c.drawLine(cx+6,cy-5,cx+9,cy-2,color);
        c.drawLine(cx+9,cy-2,cx+9,cy+2,color);
        c.drawLine(cx+9,cy+2,cx+6,cy+5,color);
    }
}
void drawSlider(Canvas565& c,int item,uint8_t value,bool pressed){
    const auto& colors=UiTheme::current();
    const int top=SettingsLayout::itemRect(item,false).y;
    const int minimum=item==SETTINGS_BRIGHTNESS_ITEM?32:0;
    const int maximum=item==SETTINGS_BRIGHTNESS_ITEM?255:100;
    const int clamped=std::clamp<int>(value,minimum,maximum);
    const int knobX=SETTINGS_SLIDER_LEFT+(clamped-minimum)*
        (SETTINGS_SLIDER_RIGHT-SETTINGS_SLIDER_LEFT)/(maximum-minimum);
    const int cy=SettingsLayout::sliderCenter(item);
    const char* label=item==SETTINGS_BRIGHTNESS_ITEM?Ui::BRIGHTNESS:Ui::Amoled::VOLUME;
    drawSliderIcon(c,item,top+28,colors.accent);
    ThemedUi::text(c,80,top+12,label,colors.text);
    char percentage[8];
    const int percent=item==SETTINGS_BRIGHTNESS_ITEM?(clamped*100+127)/255:clamped;
    std::snprintf(percentage,sizeof(percentage),"%d%%",percent);
    ThemedUi::text(c,328-ThemedUi::textWidth(percentage,16),top+20,
                   percentage,colors.secondaryText,16);
    c.fillRoundRect(SETTINGS_SLIDER_LEFT,cy-6,
                    SETTINGS_SLIDER_RIGHT-SETTINGS_SLIDER_LEFT,12,6,colors.track);
    if(knobX>SETTINGS_SLIDER_LEFT)
        c.fillRoundRect(SETTINGS_SLIDER_LEFT,cy-6,knobX-SETTINGS_SLIDER_LEFT,12,6,colors.accent);
    c.fillCircle(knobX,cy,pressed?14:12,colors.accent);
    c.fillCircle(knobX,cy,pressed?10:8,colors.accentText);
}
void drawFooter(Canvas565& c,int item,bool systemUpdate,bool pressed){
    const auto& colors=UiTheme::current();
    const auto rect=SettingsLayout::itemRect(item,systemUpdate);
    if(!rect.width)return;
    const bool reset=item==SETTINGS_RESET_GAME_ITEM;
    const auto color=reset?colors.danger:colors.accent;
    const char* label=reset?Ui::Settings::RESET_GAME:Ui::Amoled::SYSTEM_UPDATE;
    if(pressed)c.fillRoundRect(rect.x,rect.y,rect.width,rect.height,16,
                               reset?colors.dangerSurface:colors.pressed);
    const int textX=rect.x+(rect.width-ThemedUi::textWidth(label,32)+24)/2;
    const int cx=textX-18,cy=rect.y+rect.height/2;
    if(reset){
        c.drawCircle(cx,cy,7,color);
        c.drawLine(cx-7,cy-6,cx-7,cy,color);
        c.drawLine(cx-7,cy,cx-1,cy,color);
    }else{
        c.drawLine(cx,cy-8,cx,cy+5,color);
        c.drawLine(cx-4,cy+1,cx,cy+5,color);
        c.drawLine(cx+4,cy+1,cx,cy+5,color);
        c.drawLine(cx-7,cy+8,cx+7,cy+8,color);
    }
    ThemedUi::text(c,textX,rect.y+(rect.height-32)/2,label,color,32);
}
void drawResetConfirmation(Canvas565& c,const SettingsViewModel& model){
    const auto& colors=UiTheme::current();
    c.fillCircle(184,144,36,colors.dangerSurface);
    c.drawLine(184,125,164,158,colors.danger);
    c.drawLine(164,158,204,158,colors.danger);
    c.drawLine(204,158,184,125,colors.danger);
    c.fillRect(183,137,3,11,colors.danger);c.fillCircle(184,153,2,colors.danger);
    char title[48];std::snprintf(title,sizeof(title),"%s?",Ui::Settings::RESET_GAME);
    ThemedUi::text(c,(368-ThemedUi::textWidth(title))/2,204,title,colors.text);
    ThemedUi::text(c,(368-ThemedUi::textWidth(Ui::Settings::RESET_WARNING,16))/2,
                   252,Ui::Settings::RESET_WARNING,colors.secondaryText,16);
    const char* labels[]={Ui::Settings::RESET_NO,Ui::Settings::RESET_YES};
    for(int choice=0;choice<2;++choice){
        const auto rect=SettingsLayout::resetChoiceRect(choice);
        c.fillRoundRect(rect.x,rect.y,rect.width,rect.height,16,choice?colors.danger:colors.control);
        if(model.pressedItem==choice)
            c.blendRectAlphaNative(rect.x+16,rect.y+4,rect.width-32,rect.height-8,colors.text,24);
        ThemedUi::text(c,rect.x+(rect.width-ThemedUi::textWidth(labels[choice]))/2,
                       rect.y+8,labels[choice],choice?colors.accentText:colors.text);
    }
}
} // namespace

bool settingsBackAt(int x,int y){return UiCommon::pageHeaderBackAt(x,y);}
int settingsItemAt(int x,int y,bool systemUpdate){
    for(int item=0;item<=SETTINGS_SYSTEM_UPDATE_ITEM;++item)
        if(SettingsLayout::hitRect(item,systemUpdate).contains(x,y))return item;
    return -1;
}
int settingsResetChoiceAt(int x,int y){
    for(int choice=0;choice<2;++choice)
        if(SettingsLayout::resetChoiceRect(choice).contains(x,y))return choice;
    return -1;
}
void renderSettingsScreen(Canvas565& c,const SettingsViewModel& model,
                          uint16_t rowBegin,uint16_t rowEnd){
    rowBegin=std::min<uint16_t>(rowBegin,AmoledUi::HEIGHT);
    rowEnd=std::min<uint16_t>(rowEnd,AmoledUi::HEIGHT);
    UiCommon::PageClip clip(c,rowBegin,rowEnd);
    if(rowBegin>=rowEnd)return;
    const auto& colors=UiTheme::current();
    c.fillRect(0,0,368,448,colors.background);
    ThemedUi::drawHeader(c,Ui::SETTINGS);
    if(model.resetConfirm){
        drawResetConfirmation(c,model);ThemedUi::drawToast(c,model.toast);return;
    }
    const auto sliders=SettingsLayout::SLIDER_CARD,options=SettingsLayout::OPTIONS_CARD;
    c.fillRoundRect(sliders.x,sliders.y,sliders.width,sliders.height,SettingsLayout::RADIUS,colors.surface);
    drawSlider(c,0,model.brightness,model.pressedItem==0);
    drawSlider(c,1,model.volume,model.pressedItem==1);
    c.fillRoundRect(options.x,options.y,options.width,options.height,SettingsLayout::RADIUS,colors.surface);
    for(int item : {SETTINGS_GAME_SPEED_ITEM,SETTINGS_POWER_SAVE_ITEM}){
        const auto rect=SettingsLayout::itemRect(item,model.systemUpdate);
        if(model.pressedItem==item)
            c.fillRoundRect(rect.x+2,rect.y+2,rect.width-4,rect.height-4,18,colors.pressed);
        const char* label=item==SETTINGS_GAME_SPEED_ITEM?Ui::Amoled::GAME_SPEED:Ui::Amoled::POWER_SAVE;
        ThemedUi::text(c,40,rect.y+10,label,colors.text);
        char value[24]{};
        if(model.state){
            const auto& settings=model.state->settings;
            if(item==SETTINGS_GAME_SPEED_ITEM)
                std::snprintf(value,sizeof(value),"%ux",1U<<std::min<uint8_t>(settings.speedIndex,3));
            else{
                const char* power[]={Ui::Amoled::IDLE_30S,Ui::Amoled::IDLE_2MIN,
                    Ui::Amoled::IDLE_5MIN,Ui::Amoled::IDLE_10MIN,Ui::Settings::IDLE_NEVER};
                std::snprintf(value,sizeof(value),"%s",power[std::min<uint8_t>(settings.idleTimeoutIndex,4)]);
            }
        }
        ThemedUi::text(c,304-ThemedUi::textWidth(value,16),rect.y+18,value,colors.accent,16);
        for(int dy=-5;dy<=5;++dy)c.fillRect(320-std::abs(dy),rect.y+26+dy,2,1,colors.secondaryText);
    }
    c.fillRect(40,324,288,1,colors.track);
    drawFooter(c,SETTINGS_RESET_GAME_ITEM,model.systemUpdate,model.pressedItem==SETTINGS_RESET_GAME_ITEM);
    if(SettingsLayout::hasSystemUpdate(model.systemUpdate))
        drawFooter(c,SETTINGS_SYSTEM_UPDATE_ITEM,true,model.pressedItem==SETTINGS_SYSTEM_UPDATE_ITEM);
    ThemedUi::drawToast(c,model.toast);
}
} // namespace AmoledV1
