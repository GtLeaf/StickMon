#pragma once
#include "presentation/PhotoFlow.h"
class Canvas565;
namespace Ui::RoundPhotoUi {
void cameraIcon(Canvas565& canvas,int x,int y);
void render(const PhotoFlow& flow);
void notice(const char* text);
}
