// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/HomePhotoFrame.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "core/UiStrings.h"
#include "presentation/FrameDamage.h"
#include "presentation/FurnitureArt.h"
#include "presentation/MenuGrid.h"
#include "presentation/TextBlock.h"

namespace Ui {
namespace {

using Rect = MenuGrid::Rect;
constexpr int PANEL = 360;
constexpr Rect FRAME{99, 91, 162, 162};
constexpr Rect IMAGE{124, 116, 112, 112};
constexpr Rect LEFT{20, 141, 62, 62};
constexpr Rect RIGHT{278, 141, 62, 62};
constexpr Rect BUTTON{112, 277, 136, 46};
constexpr Rect NOTICE{52, 58, 256, 28};

enum Target { OUTSIDE, ACTION, PREVIOUS, NEXT };

bool onPanel(int x, int y) {
    if (x < 0 || y < 0 || x >= PANEL || y >= PANEL) return false;
    const int dx = 2*x - (PANEL-1), dy = 2*y - (PANEL-1);
    return dx*dx + dy*dy < PANEL*PANEL;
}

unsigned findPhoto(const Photos::Library& library, uint64_t id) {
    for (unsigned i = 0; id && i < library.count(); ++i) {
        const auto& entry = library.entry(i);
        if (entry.valid && entry.id == id) return i;
    }
    return library.count();
}

void drawPhoto(Canvas565& canvas, const uint16_t* pixels, int source, const Rect& rect) {
    if (!pixels) {
        canvas.fillRect(rect.x, rect.y, rect.width, rect.height, PixelRenderer::rgb(228, 232, 208));
        return;
    }
    uint16_t row[IMAGE.width];
    int column[IMAGE.width];
    for (int x = 0; x < rect.width; ++x) column[x] = x * source / rect.width;
    for (int y = 0; y < rect.height; ++y) {
        const uint16_t* from = pixels + (y * source / rect.height) * source;
        for (int x = 0; x < rect.width; ++x) row[x] = from[column[x]];
        canvas.pushImage(rect.x, rect.y + y, rect.width, 1, row);
    }
}

void label(Canvas565& canvas, const char* text, const Rect& rect, BitmapText::Font font,
           uint16_t color) {
    TextBlock block;
    TextBlock::Options options;
    options.width = rect.width;
    options.maxHeight = BitmapText::height(font);
    options.font = font;
    options.align = TextBlock::Align::CENTER;
    block.setText(text);
    block.setOptions(options);
    block.draw(canvas, rect.x, rect.y + (rect.height - options.maxHeight)/2, color);
}

void drawCandidate(Canvas565& canvas, const uint16_t* pixels, const Rect& image) {
    drawPhoto(canvas, pixels, Photos::THUMB_WIDTH, image);
    if (!pixels) label(canvas, "?", image, BitmapText::Font::PX16, PixelRenderer::rgb(67, 72, 66));
}

void drawNeighbor(Canvas565& canvas, const uint16_t* pixels, const Rect& rect) {
    canvas.fillRect(rect.x, rect.y, rect.width, rect.height, PixelRenderer::rgb(148, 128, 104));
    const Rect image{rect.x + 2, rect.y + 2, rect.width - 4, rect.height - 4};
    drawCandidate(canvas, pixels, image);
    canvas.multiplyRectRgb(image.x, image.y, image.width, image.height, 176, 176, 176);
}

}  // namespace

void HomePhotoFrame::enter(bool placed) {
    close();
    binding_ = 0;
    wallPhotoValid_ = false;
    if (!placed || library_.readFrameBinding(binding_) != Photos::Result::OK ||
        !binding_ || library_.refresh() != Photos::Result::OK) return;
    const auto index = findPhoto(library_, binding_);
    if (index == library_.count()) return;
    const auto& entry = library_.entry(index);
    Photos::PixelBuffer pixels;
    if (!pixels.allocate(Photos::THUMB_PIXELS)) return;
    if (library_.loadThumbnail(entry, pixels.data()) == Photos::Result::OK) {
        cacheWallPhoto(pixels.data(), Photos::THUMB_WIDTH);
    } else if (pixels.allocate(Photos::PIXELS) &&
               library_.loadImage(entry, pixels.data()) == Photos::Result::OK) {
        cacheWallPhoto(pixels.data(), Photos::WIDTH);
    }
}

bool HomePhotoFrame::open() {
    if (!background_.allocate(PANEL * PANEL)) return false;
    active_ = true;
    layoutDirty_ = true;
    const auto result = library_.readFrameBinding(binding_);
    if (result == Photos::Result::OK) loadPreview();
    else setNotice(result);
    return true;
}

void HomePhotoFrame::close() {
    cancelTouch();
    active_ = picking_ = backgroundReady_ = layoutDirty_ = photosDirty_ = false;
    notice_ = nullptr;
    lastOutput_ = nullptr;
    imageId_ = 0;
    background_.reset();
    image_.reset();
    thumbnails_.reset();
}

bool HomePhotoFrame::back() {
    if (!active_) return false;
    if (!picking_) {
        close();
    } else {
        picking_ = false;
        thumbnails_.reset();
        notice_ = nullptr;
        layoutDirty_ = true;
        loadPreview();
    }
    return true;
}

void HomePhotoFrame::cancelTouch() {
    tap_.cancel();
    swipeDown_ = false;
}

void HomePhotoFrame::loadPreview() {
    if (binding_ && binding_ == imageId_) return;
    imageId_ = 0;
    image_.reset();
    if (!binding_) return;
    const auto result = library_.refresh();
    if (result != Photos::Result::OK) {
        setNotice(result);
        return;
    }
    const auto index = findPhoto(library_, binding_);
    if (index == library_.count()) {
        wallPhotoValid_ = false;
        return;
    }
    if (!image_.allocate(Photos::PIXELS)) {
        setNotice(Photos::Result::NO_MEMORY);
        return;
    }
    const auto loaded = library_.loadImage(library_.entry(index), image_.data());
    if (loaded != Photos::Result::OK) {
        image_.reset();
        setNotice(loaded);
    } else {
        imageId_ = binding_;
        cacheWallPhoto(image_.data(), Photos::WIDTH);
    }
}

void HomePhotoFrame::openPicker() {
    notice_ = nullptr;
    layoutDirty_ = true;
    const auto result = library_.refresh();
    if (result != Photos::Result::OK) {
        setNotice(result);
        return;
    }
    if (!library_.count()) {
        notice_ = Photo::EMPTY;
        return;
    }
    if (!thumbnails_.allocate(library_.count() * Photos::THUMB_PIXELS)) {
        setNotice(Photos::Result::NO_MEMORY);
        return;
    }
    const auto index = findPhoto(library_, binding_);
    selection_ = index < library_.count() ? index : 0;
    // The album already preserves display order. Keep only its small images
    // for browsing so touch never waits for a full-resolution read.
    for (unsigned i = 0; i < library_.count(); ++i) {
        const auto& entry = library_.entry(i);
        validThumb_[i] = entry.valid && library_.loadThumbnail(entry,
            thumbnails_.data() + i * Photos::THUMB_PIXELS) == Photos::Result::OK;
    }
    picking_ = true;
}

const uint16_t* HomePhotoFrame::thumbnail(unsigned index) const {
    if (index >= library_.count() || !validThumb_[index]) return nullptr;
    return thumbnails_.data() + index * Photos::THUMB_PIXELS;
}

bool HomePhotoFrame::moveSelection(int delta) {
    const int next = static_cast<int>(selection_) + delta;
    if (next < 0 || next >= static_cast<int>(library_.count())) return false;
    selection_ = static_cast<unsigned>(next);
    layoutDirty_ |= notice_ != nullptr;
    notice_ = nullptr;
    photosDirty_ = true;
    return true;
}

void HomePhotoFrame::confirm() {
    layoutDirty_ = true;
    const auto& entry = library_.entry(selection_);
    if (!entry.valid) {
        setNotice(Photos::Result::CORRUPT);
        return;
    }
    if (!image_.allocate(Photos::PIXELS)) {
        setNotice(Photos::Result::NO_MEMORY);
        return;
    }
    auto result = Photos::Result::OK;
    if (entry.id != imageId_) {
        imageId_ = 0;
        result = library_.loadImage(entry, image_.data());
        if (result == Photos::Result::OK) imageId_ = entry.id;
    }
    if (result == Photos::Result::OK && entry.id != binding_)
        result = library_.writeFrameBinding(entry.id);
    if (result != Photos::Result::OK) {
        if (!imageId_) image_.reset();
        setNotice(result);
        return;
    }
    binding_ = entry.id;
    cacheWallPhoto(image_.data(), Photos::WIDTH);
    thumbnails_.reset();
    notice_ = nullptr;
    picking_ = false;
}

void HomePhotoFrame::setNotice(Photos::Result result) {
    switch (result) {
    case Photos::Result::UNAVAILABLE: notice_ = Photo::UNAVAILABLE; break;
    case Photos::Result::CORRUPT: notice_ = Photo::CORRUPT; break;
    case Photos::Result::NO_MEMORY: notice_ = Photo::NO_MEMORY; break;
    default: notice_ = Photo::FAILED; break;
    }
    layoutDirty_ = true;
}

bool HomePhotoFrame::touch(const TouchTap::Event& event) {
    using T = TouchTap::Type;
    if (event.type == T::CANCEL || !onPanel(event.x, event.y)) {
        cancelTouch();
        return false;
    }
    const int x = event.x, y = event.y;
    int target = -1;
    if (BUTTON.contains(x, y)) target = ACTION;
    else if (picking_ && selection_ && LEFT.contains(x, y)) target = PREVIOUS;
    else if (picking_ && selection_ + 1 < library_.count() && RIGHT.contains(x, y)) target = NEXT;
    else if (!FRAME.contains(x, y)) target = OUTSIDE;

    if (event.type == T::DOWN) {
        swipeDown_ = picking_ && y >= FRAME.y && y < FRAME.y + FRAME.height;
        swipeX_ = x;
        swipeY_ = y;
    } else if (event.type == T::UP && swipeDown_) {
        swipeDown_ = false;
        const int dx = x - swipeX_, dy = y - swipeY_;
        if (std::abs(dx) >= 32 && std::abs(dx) > std::abs(dy)) {
            tap_.cancel();
            return moveSelection(dx < 0 ? 1 : -1);
        }
    }
    switch (tap_.update(event, target)) {
    case OUTSIDE: return back();
    case PREVIOUS: return moveSelection(-1);
    case NEXT: return moveSelection(1);
    case ACTION:
        if (picking_) confirm();
        else openPicker();
        return true;
    default: return false;
    }
}

void HomePhotoFrame::render(Canvas565& canvas) {
    const bool first = !backgroundReady_;
    const bool full = first || !FrameDamage::allowed ||
        lastOutput_ != canvas.rawPixels() || lastOutputSwapped_ != canvas.byteSwapped();
    lastOutput_ = canvas.rawPixels();
    lastOutputSwapped_ = canvas.byteSwapped();
    if (first) {
        // The native canvas persists across presents. Keep one immutable copy
        // for clearing the side previews and recovering after a forced repaint.
        canvas.multiplyRectRgb(0, 0, PANEL, PANEL, 122, 122, 122);
        std::memcpy(background_.data(), canvas.rawPixels(), PANEL * PANEL * sizeof(uint16_t));
        backgroundSwapped_ = canvas.byteSwapped();
        backgroundReady_ = true;
    }
    auto restore = [&](const Rect& rect) {
        for (int y = rect.y; y < rect.y + rect.height; ++y) {
            const uint16_t* row = background_.data() + y * PANEL + rect.x;
            if (backgroundSwapped_ == canvas.byteSwapped()) {
                canvas.pushImageNative(rect.x, y, rect.width, 1, row);
            } else {
                uint16_t colors[PANEL];
                for (int x = 0; x < rect.width; ++x)
                    colors[x] = backgroundSwapped_ ? uint16_t(row[x]<<8 | row[x]>>8) : row[x];
                canvas.pushImage(rect.x, y, rect.width, 1, colors);
            }
        }
    };
    if (!full && !layoutDirty_ && !photosDirty_) return;
    if (full) {
        if (!first) restore({0, 0, PANEL, PANEL});
        FurnitureArt::drawFrame(canvas, PANEL/2, FRAME.y + FRAME.height/2, FRAME.width);
    } else {
        restore(LEFT);
        restore(RIGHT);
        if (layoutDirty_) {
            restore(BUTTON);
            restore(NOTICE);
        }
        const int top = layoutDirty_ ? NOTICE.y : IMAGE.y;
        const int bottom = layoutDirty_ ? BUTTON.y + BUTTON.height : IMAGE.y + IMAGE.height;
        FrameDamage::restrictTo(LEFT.x, top, RIGHT.x + RIGHT.width - LEFT.x, bottom - top);
    }
    if (picking_) {
        drawCandidate(canvas, thumbnail(selection_), IMAGE);
        if (selection_) drawNeighbor(canvas, thumbnail(selection_ - 1), LEFT);
        if (selection_ + 1 < library_.count()) drawNeighbor(canvas, thumbnail(selection_ + 1), RIGHT);
    } else {
        drawPhoto(canvas, image_.data(), Photos::WIDTH, IMAGE);
    }
    if (full || layoutDirty_) {
        canvas.fillRoundRect(BUTTON.x, BUTTON.y, BUTTON.width, BUTTON.height,
            10, PixelRenderer::rgb(36, 64, 60));
        canvas.drawRoundRect(BUTTON.x, BUTTON.y, BUTTON.width, BUTTON.height,
            10, PixelRenderer::rgb(98, 133, 124));
        const uint16_t ink = PixelRenderer::rgb(240, 248, 242);
        label(canvas, picking_ ? Photo::CONFIRM_FRAME :
              image_.data() ? Photo::CHANGE_FRAME : Photo::CHOOSE_FRAME, BUTTON, BitmapText::Font::PX32, ink);
        if (notice_) {
            canvas.fillRoundRect(NOTICE.x, NOTICE.y, NOTICE.width, NOTICE.height,
                7, PixelRenderer::rgb(17, 27, 34));
            label(canvas, notice_, NOTICE, BitmapText::Font::PX16, ink);
        }
    }
    layoutDirty_ = photosDirty_ = false;
}

void HomePhotoFrame::cacheWallPhoto(const uint16_t* pixels, int width) {
    const int scale = width / Photos::THUMB_WIDTH;
    auto sample = [scale](int value) {
        return (value * Photos::THUMB_WIDTH / WALL_PHOTO_SIZE) * scale + scale / 2;
    };
    for (int row = 0; row < WALL_PHOTO_SIZE; ++row) {
        const uint16_t* source = pixels + sample(row) * width;
        for (int col = 0; col < WALL_PHOTO_SIZE; ++col)
            wallPhoto_[row * WALL_PHOTO_SIZE + col] = source[sample(col)];
    }
    wallPhotoValid_ = true;
}

void HomePhotoFrame::drawWallPhoto(Canvas565& canvas, int x, int y, bool night) const {
    if (!wallPhotoValid_) return;
    constexpr int inset = (FurnitureArt::FRAME_WORLD_SIZE - WALL_PHOTO_SIZE) / 2;
    for (int row = 0; row < WALL_PHOTO_SIZE; ++row) {
        for (int col = 0; col < WALL_PHOTO_SIZE; ++col) {
            const int frameX = inset + col;
            auto pixel = wallPhoto_[row * WALL_PHOTO_SIZE + col];
            if (night) pixel = (pixel >> 1) & 0x7bef;
            canvas.drawPixel(x + frameX, y + inset + row + frameX/3, pixel);
        }
    }
}

}  // namespace Ui
