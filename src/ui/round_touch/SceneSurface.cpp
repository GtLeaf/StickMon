// SPDX-License-Identifier: Apache-2.0
#include "ui/SceneSurface.h"

#include <cstring>
#include <cstdio>
#include "core/Scene.h"
#include "presentation/PixelRenderer.h"
#include "ui/PageProfile.h"
#include "ui/round_touch/RoundPage.h"
#include "ui/round_touch/ExploreRouteUi.h"
#include "ui/round_touch/ExploreBattleUi.h"

namespace Ui::SceneSurface {
namespace {
bool page = false;
bool menuScene = false;
bool exploreScene = false;
bool nativeExplore = false;
bool nativeMenu = false;
bool nativeShop = false;
bool nativeDetail = false;
const char* headerTitle = nullptr;
bool headerPressed = false;
char footerText[16] = {};
uint16_t* pixels = nullptr;
Platform::FrameBuffer565 output;
constexpr int LEFT = (device.width - Page::WIDTH) / 2;
constexpr int TOP = (device.height - Page::HEIGHT) / 2;
static_assert(device.width == 360 && device.height == 360, "Round surface requires the native panel");
static_assert(2 * 120 * 120 < 180 * 180, "Every menu corner must fit the round panel");

bool ready() {
    if (!pixels) {
        pixels = static_cast<uint16_t*>(Platform::memory().allocate(
            Page::WIDTH * Page::HEIGHT * sizeof(uint16_t), true));
        if (!pixels) Platform::logLine("[RoundSurface] page allocation failed");
    }
    return pixels != nullptr;
}
}  // namespace

void select(SceneID scene) {
    headerTitle = nullptr;
    footerText[0] = 0;
    menuScene = scene == SceneID::MENU;
    exploreScene = scene == SceneID::EXPLORE;
    nativeExplore = false;
    nativeMenu = false;
    nativeDetail = false;
    nativeShop = scene == SceneID::SHOP;
    page = scene != SceneID::MAIN && scene != SceneID::HATCH && !nativeShop;
}
void mainMenu(bool native, bool detail) {
    headerTitle = nullptr;
    footerText[0] = 0;
    if (!menuScene && !exploreScene && !detail && !nativeDetail) return;
    nativeMenu = menuScene && native;
    nativeDetail = detail;
    page = !nativeMenu && !nativeDetail;
}
void exploreSelection(bool native) {
    if (!exploreScene) return;
    headerTitle = nullptr;
    footerText[0] = 0;
    nativeExplore = native;
    nativeDetail = false;
    nativeMenu = false;
    page = !native;
}
void explorePhase(ExplorePhase phase) {
    exploreSelection(phase == ExplorePhase::SELECT || RoundExplore::nativeRoute(phase) ||
                     RoundExplore::nativeBattle(phase));
}
void pageHeader(const char* title, bool pressed) {
    if (!page) return;
    headerTitle = title;
    headerPressed = pressed;
}
void pageFooter(const char* text) {
    if (!page) return;
    std::snprintf(footerText, sizeof(footerText), "%s", text ? text : "");
}
int width() { return page ? Page::WIDTH : device.width; }
int height() { return page ? Page::HEIGHT : device.height; }

InputScope::InputScope(const Platform::FrameBuffer565& target) : output_(target) {
    if (page && ready())
        PixelRenderer::bind({pixels, Page::WIDTH, Page::HEIGHT, target.byteSwapped});
    else if (!page) PixelRenderer::bind(target);
}
InputScope::~InputScope() { PixelRenderer::bind(output_); }

bool begin(const Platform::FrameBuffer565& target) {
    output = target;
    headerTitle = nullptr;
    footerText[0] = 0;
    if (!page) { PixelRenderer::bind(output); return true; }
    if (!ready()) {
        PixelRenderer::bind(output);
        PixelRenderer::clear(0);
        PixelRenderer::text(100, 166, "UI memory low", 0xf800);
        return false;
    }
    PixelRenderer::bind({pixels, Page::WIDTH, Page::HEIGHT, output.byteSwapped});
    PixelRenderer::clear(0);
    return true;
}

void finish() {
    if (!page || !pixels || !output.pixels) return;
    std::memset(output.pixels, 0, output.width * output.height * sizeof(uint16_t));
    for (int y = 0; y < Page::HEIGHT; ++y)
        std::memcpy(output.pixels + (TOP + y) * output.width + LEFT,
            pixels + y * Page::WIDTH, Page::WIDTH * sizeof(uint16_t));
    PixelRenderer::bind(output);
    if (headerTitle) RoundPage::nativeHeader(headerTitle, headerPressed);
    if (footerText[0]) MenuGrid::label(PixelRenderer::canvas(), device.width / 2, 324,
        footerText, 120, PixelRenderer::rgb(126, 145, 145));
}

bool mapTouch(Platform::IInputDevice::TouchEvent& event) {
    if (!page) {
        if (output.pixels) PixelRenderer::bind(output);
        if (nativeMenu || nativeShop || nativeDetail || nativeExplore) {
            const int dx = 2 * event.x - (device.width - 1);
            const int dy = 2 * event.y - (device.height - 1);
            if (dx * dx + dy * dy >= device.width * device.width)
                event.type = Platform::IInputDevice::TouchType::CANCEL;
        }
        return true;
    }
    if (!ready()) return false;
    // Status swipes capture native page frames while handling input.
    PixelRenderer::bind({pixels, Page::WIDTH, Page::HEIGHT, output.byteSwapped});
    const bool headerBack = headerTitle && RoundPage::NATIVE_BACK.contains(event.x, event.y);
    event.x -= LEFT;
    event.y -= TOP;
    if (!headerBack && (event.x < 0 || event.x >= Page::WIDTH || event.y < 0 || event.y >= Page::HEIGHT))
        event.type = Platform::IInputDevice::TouchType::CANCEL;
    return true;
}
}  // namespace Ui::SceneSurface
