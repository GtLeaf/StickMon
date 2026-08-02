#pragma once

namespace TutorialOverlay {

enum class Button {
    A,
    B,
};

void draw(Button button, const char* text, bool longPress = false,
          int y = 104);

} // namespace TutorialOverlay
