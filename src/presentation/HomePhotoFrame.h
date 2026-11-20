// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/PhotoLibrary.h"
#include "core/TouchTap.h"

class Canvas565;

namespace Ui {

class HomePhotoFrame {
public:
    explicit HomePhotoFrame(bool roundPanel = true) : roundPanel_(roundPanel) {}
    void enter(bool placed);
    bool open();
    void close();
    bool active() const { return active_; }
    bool needsBackground() const { return !backgroundReady_; }
    bool back();
    bool touch(const TouchTap::Event& event);
    bool cancelTouch();
    bool update(uint32_t nowMs);
    bool sliding() const { return dragging_ || settleFrom_ != 0; }
    bool settling() const { return settleFrom_ != 0; }
    bool photosOnly() const { return active_ && picking_ && !layoutDirty_; }
    static constexpr int PHOTO_TOP = 116, PHOTO_HEIGHT = 112, RECT_PANEL_Y = 44;
    void render(Canvas565& canvas);
    void drawWallPhoto(Canvas565& canvas, int x, int y, int scale = 1) const;

private:
    static constexpr int WALL_PHOTO_SIZE = 13;
    bool roundPanel_;
    Photos::Library library_;
    Photos::PixelBuffer background_, image_, pickerImages_;
    uint16_t wallPhoto_[WALL_PHOTO_SIZE * WALL_PHOTO_SIZE] = {};
    uint64_t binding_ = 0, imageId_ = 0;
    unsigned selection_ = 0;
    bool validPickerImage_[Photos::CAPACITY] = {};
    bool active_ = false, picking_ = false;
    bool wallPhotoValid_ = false;
    bool backgroundReady_ = false, backgroundSwapped_ = false;
    const uint16_t* lastOutput_ = nullptr;
    bool lastOutputSwapped_ = false;
    bool layoutDirty_ = false, photosDirty_ = false;
    const char* notice_ = nullptr;
    TouchTap tap_;
    bool swipeDown_ = false, dragging_ = false;
    int slide_ = 0, dragOrigin_ = 0, settleFrom_ = 0;
    uint32_t settleStart_ = 0;
    int swipeX_ = 0, swipeY_ = 0;

    void openPicker();
    void loadPreview();
    void settle(int delta, uint32_t nowMs);
    void confirm();
    void setNotice(Photos::Result result);
    void cacheWallPhoto(const uint16_t* pixels, int width);
    const uint16_t* pickerImage(unsigned index) const;
    void drawOverlay(Canvas565& canvas, bool drawFrame, bool drawControls) const;
};

}  // namespace Ui
