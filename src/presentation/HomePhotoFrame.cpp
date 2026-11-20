// SPDX-License-Identifier: Apache-2.0
#include "presentation/HomePhotoFrame.h"

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
constexpr Rect IMAGE{124, HomePhotoFrame::PHOTO_TOP, 112, HomePhotoFrame::PHOTO_HEIGHT};
constexpr size_t PICKER_IMAGE_PIXELS = IMAGE.width * IMAGE.height;
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
    if (source == rect.width && source == rect.height) {
        canvas.pushImage(rect.x, rect.y, rect.width, rect.height, pixels);
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
    drawPhoto(canvas, pixels, IMAGE.width, image);
    if (!pixels) label(canvas, "?", image, BitmapText::Font::PX16, PixelRenderer::rgb(67, 72, 66));
}

// Card centers travel from the frame center to the neighboring thumbnail.
constexpr int CARD_STEP = RIGHT.x + RIGHT.width/2 - PANEL/2;
constexpr int SETTLE_MS = 144;

Rect cardBounds(int distance) {
    const int weight = std::min(std::abs(distance), CARD_STEP);
    const int size = IMAGE.width - (IMAGE.width - RIGHT.width) * weight / CARD_STEP;
    return {PANEL/2 + distance - size/2, IMAGE.y + IMAGE.height/2 - size/2, size, size};
}

void drawCard(Canvas565& canvas, const uint16_t* pixels, Rect rect, int distance) {
    const int weight = std::min(std::abs(distance), CARD_STEP);
    const int border = (2 * weight + CARD_STEP/2) / CARD_STEP;
    if (border) {
        canvas.fillRect(rect.x, rect.y, rect.width, rect.height, PixelRenderer::rgb(148,128,104));
        rect = {rect.x + border, rect.y + border, rect.width - 2*border, rect.height - 2*border};
    }
    drawCandidate(canvas, pixels, rect);
    if (weight) {
        const int gain = 256 - 80 * weight / CARD_STEP;
        canvas.multiplyRectRgb(rect.x, rect.y, rect.width, rect.height, gain, gain, gain);
    }
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
    if (roundPanel_ && !background_.allocate(PANEL * PANEL)) return false;
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
    pickerImages_.reset();
}

bool HomePhotoFrame::back() {
    if (!active_) return false;
    cancelTouch();
    if (!picking_) {
        close();
    } else {
        picking_ = false;
        pickerImages_.reset();
        notice_ = nullptr;
        layoutDirty_ = true;
        loadPreview();
    }
    return true;
}

bool HomePhotoFrame::cancelTouch() {
    tap_.cancel();
    swipeDown_ = dragging_ = false;
    const bool changed = slide_ != 0;
    slide_ = settleFrom_ = 0;
    photosDirty_ |= changed;
    return changed;
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
    Photos::PixelBuffer original;
    if (!original.allocate(Photos::PIXELS) ||
        !pickerImages_.allocate(library_.count() * PICKER_IMAGE_PIXELS)) {
        setNotice(Photos::Result::NO_MEMORY);
        return;
    }
    const auto index = findPhoto(library_, binding_);
    selection_ = index < library_.count() ? index : 0;
    // Sample each original at the frame's display resolution once. All
    // subsequent swipes use this cache, and neighbors shrink the same pixels.
    for (unsigned i = 0; i < library_.count(); ++i) {
        const auto& entry = library_.entry(i);
        const bool cachedOriginal = entry.valid && entry.id == imageId_;
        validPickerImage_[i] = entry.valid && (cachedOriginal ||
            library_.loadImage(entry, original.data()) == Photos::Result::OK);
        if (!validPickerImage_[i]) continue;
        const uint16_t* pixels = cachedOriginal ? image_.data() : original.data();
        uint16_t* preview = pickerImages_.data() + i * PICKER_IMAGE_PIXELS;
        for (int y = 0; y < IMAGE.height; ++y) {
            const auto* sourceRow = pixels + (y * Photos::WIDTH / IMAGE.height) * Photos::WIDTH;
            for (int x = 0; x < IMAGE.width; ++x)
                preview[y * IMAGE.width + x] = sourceRow[x * Photos::WIDTH / IMAGE.width];
        }
    }
    picking_ = true;
}

const uint16_t* HomePhotoFrame::pickerImage(unsigned index) const {
    if (index >= library_.count() || !validPickerImage_[index]) return nullptr;
    return pickerImages_.data() + index * PICKER_IMAGE_PIXELS;
}

void HomePhotoFrame::settle(int delta, uint32_t nowMs) {
    if (delta) {
        selection_ = static_cast<unsigned>(static_cast<int>(selection_) + delta);
        slide_ += delta * CARD_STEP;  // Rebase without shifting any visible card.
        layoutDirty_ |= notice_ != nullptr;
        notice_ = nullptr;
    }
    settleFrom_ = slide_;
    settleStart_ = nowMs;
    photosDirty_ = true;
}

bool HomePhotoFrame::update(uint32_t nowMs) {
    if (!settling()) return photosDirty_;
    const int elapsed = std::clamp<int32_t>(static_cast<int32_t>(nowMs - settleStart_), 0, SETTLE_MS);
    const int remaining = SETTLE_MS - elapsed;
    const int offset = settleFrom_ * remaining * remaining / (SETTLE_MS * SETTLE_MS);
    const bool changed = slide_ != offset;
    slide_ = offset;
    if (!slide_) settleFrom_ = 0;
    photosDirty_ |= changed;
    return changed;
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
    cancelTouch();
    binding_ = entry.id;
    cacheWallPhoto(image_.data(), Photos::WIDTH);
    pickerImages_.reset();
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
    const bool inside = roundPanel_ ? onPanel(event.x, event.y) :
        event.x >= 0 && event.x < 368 && event.y >= 0 && event.y < 448;
    if (event.type == T::CANCEL || !inside) {
        return cancelTouch();
    }
    const int x = event.x - (roundPanel_ ? 0 : 4);
    const int y = event.y - (roundPanel_ ? 0 : RECT_PANEL_Y);
    int target = -1;
    if (BUTTON.contains(x, y)) target = ACTION;
    else if (picking_ && selection_ && cardBounds(slide_ - CARD_STEP).contains(x, y)) target = PREVIOUS;
    else if (picking_ && selection_ + 1 < library_.count() && cardBounds(slide_ + CARD_STEP).contains(x, y)) target = NEXT;
    else if (!FRAME.contains(x, y)) target = OUTSIDE;

    if (event.type == T::DOWN) {
        swipeDown_ = picking_ && library_.count() > 1 && y >= FRAME.y && y < FRAME.y + FRAME.height;
        if (swipeDown_) {
            settleFrom_ = 0; // Hold the current on-screen position if grabbed mid-animation.
            dragOrigin_ = slide_;
        }
        dragging_ = false;
        swipeX_ = x;
        swipeY_ = y;
    } else if (swipeDown_) {
        const int dx = x - swipeX_, dy = y - swipeY_;
        if (std::abs(dx) > 6 && std::abs(dx) > std::abs(dy)) dragging_ = true;
        if (dragging_) {
            tap_.cancel();
            int offset = std::clamp(dragOrigin_ + dx, -CARD_STEP, CARD_STEP);
            if ((!selection_ && offset > 0) || (selection_ + 1 == library_.count() && offset < 0))
                offset = std::clamp(offset / 4, -10, 10);
            const bool changed = slide_ != offset;
            slide_ = offset;
            photosDirty_ |= changed;
            if (event.type == T::UP) {
                dragging_ = swipeDown_ = false;
                const int delta = slide_ <= -32 ? 1 : slide_ >= 32 ? -1 : 0;
                settle(delta, event.timestampMs);
                return true;
            }
            return changed;
        }
        if (event.type == T::UP) {
            swipeDown_ = false;
            if (slide_) settle(0, event.timestampMs);
        }
    }
    switch (tap_.update(event, target)) {
    case OUTSIDE: return back();
    case PREVIOUS: settle(-1, event.timestampMs); return true;
    case NEXT: settle(1, event.timestampMs); return true;
    case ACTION:
        if (picking_) confirm();
        else openPicker();
        return true;
    default: return settling();
    }
}

void HomePhotoFrame::render(Canvas565& canvas) {
    if (!roundPanel_) {
        // AMOLED redraws the room in the requested region before the overlay.
        canvas.multiplyRectRgb(0, 0, canvas.width(), canvas.height(), 122, 122, 122);
        drawOverlay(canvas, true, true);
        layoutDirty_ = photosDirty_ = false;
        return;
    }
    const bool first = !backgroundReady_;
    const bool full = first || !FrameDamage::allowed ||
        lastOutput_ != canvas.rawPixels() || lastOutputSwapped_ != canvas.byteSwapped();
    lastOutput_ = canvas.rawPixels();
    lastOutputSwapped_ = canvas.byteSwapped();
    if (first) {
        // The native canvas persists across presents. Keep one immutable copy
        // for clearing the side previews and recovering after a forced repaint.
        canvas.multiplyRectRgb(0, 0, PANEL, PANEL, 122, 122, 122);
        FurnitureArt::drawFrame(canvas, PANEL/2, FRAME.y + FRAME.height/2, FRAME.width);
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
    } else {
        restore({0, IMAGE.y, PANEL, IMAGE.height});
        if (layoutDirty_) {
            restore(BUTTON);
            restore(NOTICE);
        }
        const int top = layoutDirty_ ? NOTICE.y : IMAGE.y;
        const int bottom = layoutDirty_ ? BUTTON.y + BUTTON.height : IMAGE.y + IMAGE.height;
        FrameDamage::restrictTo(0, top, PANEL, bottom - top);
    }
    drawOverlay(canvas, false, full || layoutDirty_);
    layoutDirty_ = photosDirty_ = false;
}

void HomePhotoFrame::drawOverlay(Canvas565& canvas, bool drawFrame, bool drawControls) const {
    const int dx = roundPanel_ ? 0 : 4;
    const int dy = roundPanel_ ? 0 : RECT_PANEL_Y;
    const auto offset = [dx, dy](Rect r) {
        r.x += dx;
        r.y += dy;
        return r;
    };
    if (drawFrame)
        FurnitureArt::drawFrame(canvas, PANEL/2 + dx, FRAME.y + FRAME.height/2 + dy, FRAME.width);
    if (picking_) {
        for (unsigned i = 0; i < library_.count(); ++i) {
            const int distance = (static_cast<int>(i) - static_cast<int>(selection_)) * CARD_STEP + slide_;
            const Rect rect = offset(cardBounds(distance));
            if (rect.x + rect.width <= canvas.clipLeft() || rect.x >= canvas.clipRight()) continue;
            drawCard(canvas, pickerImage(i), rect, distance);
        }
    } else {
        drawPhoto(canvas, image_.data(), Photos::WIDTH, offset(IMAGE));
    }
    if (drawControls) {
        const auto button = offset(BUTTON);
        canvas.fillRoundRect(button.x, button.y, button.width, button.height,
            10, PixelRenderer::rgb(36, 64, 60));
        canvas.drawRoundRect(button.x, button.y, button.width, button.height,
            10, PixelRenderer::rgb(98, 133, 124));
        const uint16_t ink = PixelRenderer::rgb(240, 248, 242);
        label(canvas, picking_ ? Photo::CONFIRM_FRAME :
              image_.data() ? Photo::CHANGE_FRAME : Photo::CHOOSE_FRAME, button, BitmapText::Font::PX32, ink);
        if (notice_) {
            const auto notice = offset(NOTICE);
            canvas.fillRoundRect(notice.x, notice.y, notice.width, notice.height,
                7, PixelRenderer::rgb(17, 27, 34));
            label(canvas, notice_, notice, BitmapText::Font::PX16, ink);
        }
    }
}

void HomePhotoFrame::cacheWallPhoto(const uint16_t* pixels, int width) {
    const int scale = width / Photos::THUMB_WIDTH;
    auto sample = [scale](int value) {
        return (value * Photos::THUMB_WIDTH / WALL_PHOTO_SIZE) * scale + scale / 2;
    };
    auto nightChannel = [](int value) { return (4 * value + 2) / 5; };
    for (int row = 0; row < WALL_PHOTO_SIZE; ++row) {
        const uint16_t* source = pixels + sample(row) * width;
        for (int col = 0; col < WALL_PHOTO_SIZE; ++col) {
            const int index = row * WALL_PHOTO_SIZE + col;
            const uint16_t pixel = source[sample(col)];
            wallPhoto_[0][index] = pixel;
            wallPhoto_[1][index] = (nightChannel((pixel >> 11) & 31) << 11) |
                                  (nightChannel((pixel >> 5) & 63) << 5) |
                                  nightChannel(pixel & 31);
        }
    }
    wallPhotoValid_ = true;
}

void HomePhotoFrame::drawWallPhoto(Canvas565& canvas, int x, int y, bool night, int scale) const {
    if (!wallPhotoValid_) return;
    constexpr int inset = (FurnitureArt::FRAME_WORLD_SIZE - WALL_PHOTO_SIZE) / 2;
    const auto* pixels = wallPhoto_[night];
    for (int row = 0; row < WALL_PHOTO_SIZE; ++row) {
        for (int col = 0; col < WALL_PHOTO_SIZE; ++col) {
            const int frameX = (inset + col) * scale;
            const uint16_t pixel = pixels[row * WALL_PHOTO_SIZE + col];
            for (int dx = 0; dx < scale; ++dx) {
                const int column = frameX + dx;
                canvas.drawFastVLine(x + column, y + (inset + row) * scale + column/3, scale, pixel);
            }
        }
    }
}

}  // namespace Ui
