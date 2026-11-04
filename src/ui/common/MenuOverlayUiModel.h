#pragma once

namespace Ui {
struct MenuOverlayLayout { int width, height, contentHeight; };
struct MenuOverlayFrame {
    const char* toast = nullptr;
    bool bodyNotice = false;
    bool directTouchPage = false;
    bool touchAvailable = false;
    bool exploreContext = false;
    bool hatched = false;
    bool menuPage = false;
    bool navigationComplete = false;
    bool backComplete = false;
};
}  // namespace Ui
