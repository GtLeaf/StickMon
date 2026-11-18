#pragma once
#include "presentation/PhotoFlow.h"
class Canvas565;
namespace Ui::RoundPhotoUi {
void render(const PhotoFlow& flow);
void notice(const char* text);
}
