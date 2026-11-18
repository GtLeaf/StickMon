// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/PhotoLibrary.h"
#include "core/TouchTap.h"

class Canvas565;

namespace Ui {

class HomePhotoFrame {
public:
    void enter(bool placed);
    bool open();
    void close();
    bool active() const { return active_; }
    bool needsBackground() const { return !backgroundReady_; }
    bool back();
    bool touch(const TouchTap::Event& event);
    void cancelTouch();
    void render(Canvas565& canvas);
    void drawWallPhoto(Canvas565& canvas, int x, int y, bool night) const;

private:
    static constexpr int WALL_PHOTO_SIZE = 13;
    Photos::Library library_;
    Photos::PixelBuffer background_, image_, pickerImages_;
    uint16_t wallPhoto_[2][WALL_PHOTO_SIZE * WALL_PHOTO_SIZE] = {};
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
    bool swipeDown_ = false;
    int swipeX_ = 0, swipeY_ = 0;

    void openPicker();
    void loadPreview();
    bool moveSelection(int delta);
    void confirm();
    void setNotice(Photos::Result result);
    void cacheWallPhoto(const uint16_t* pixels, int width);
    const uint16_t* pickerImage(unsigned index) const;
};

}  // namespace Ui
