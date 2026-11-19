#pragma once

#include "presentation/Canvas565.h"
#include "presentation/PhotoFlow.h"

namespace Ui {
inline void drawPhotoCropMarks(Canvas565& canvas,const PhotoFlow& flow,int length,uint16_t color){
    const int left=flow.frameX(),top=flow.frameY();
    const int right=left+flow.frameSize()-1,bottom=top+flow.frameSize()-1;
    for(int y:{top,bottom})for(int x:{left,right}){
        for(int n=0;n<2;++n){
            canvas.drawFastHLine(x==left?left:right-length+1,y==top?top+n:bottom-n,length,color);
            canvas.drawFastVLine(x==left?left+n:right-n,y==top?top:bottom-length+1,length,color);
        }
    }
    // Two diagonal grip lines distinguish the resize corner from the move area.
    for(int inset:{10,16}){
        canvas.drawLine(right-inset,bottom-4,right-4,bottom-inset,color);
        canvas.drawLine(right-inset+1,bottom-4,right-4,bottom-inset+1,color);
    }
}
}
