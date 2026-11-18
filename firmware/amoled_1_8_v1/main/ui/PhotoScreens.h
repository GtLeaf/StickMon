#pragma once
#include "PhotoSession.h"
class Canvas565;
namespace AmoledV1 {
void drawPhotoCameraIcon(Canvas565&,int cx,int cy);
void renderPhotoScreen(Canvas565&,const PhotoSession&,uint16_t top,uint16_t bottom,uint16_t left=0,uint16_t right=368);
}
