// SPDX-License-Identifier: Apache-2.0
#include <cassert>
#include <cmath>
#include <cstdio>
#include "RightTouchCompensation.h"
#include "Cst816TouchState.h"

int main() {
    using Correction = Sdgoods::RightTouchCompensation;
    const auto upper = Correction::apply(344.5f, 113);
    const auto lower = Correction::apply(335.5f, 248);
    assert(std::hypot(upper.x - 309, upper.y - 126) < 0.01f);
    assert(std::hypot(lower.x - 309, lower.y - 234) < 0.01f);
    float minimumJacobian = 10;
    for (int y = 0; y < 360; ++y) for (int x = 0; x < 360; ++x) {
        const auto p = Correction::apply(x, y);
        assert(p.x >= 0 && p.x <= 359 && p.y >= 0 && p.y <= 359);
        if (x <= 180 || y <= 56 || y >= 309) assert(p.x == x && p.y == y);
        const auto right = Correction::apply(x + 0.01f, y);
        const auto below = Correction::apply(x, y + 0.01f);
        const float xx = (right.x - p.x) / 0.01f, xy = (below.x - p.x) / 0.01f;
        const float yx = (right.y - p.y) / 0.01f, yy = (below.y - p.y) / 0.01f;
        const float jacobian = xx * yy - xy * yx;
        assert(xx > 0 && yy > 0 && jacobian > 0.25f);
        minimumJacobian = std::fmin(minimumJacobian, jacobian);
        assert(std::hypot(right.x - p.x, right.y - p.y) < 0.025f);
        assert(std::hypot(below.x - p.x, below.y - p.y) < 0.025f);
    }
    // Four diagnostic rounds, upper/lower right in each round. These are
    // development samples, not an independent post-flash accuracy claim.
    const int recorded[8][2] = {{346,114},{343,246},{324,110},{326,243},
                               {343,112},{332,250},{347,121},{339,253}};
    float before = 0, after = 0, maximum = 0;
    for (int i = 0; i < 8; ++i) {
        const auto p = Correction::apply(recorded[i][0], recorded[i][1]);
        const float targetY = i % 2 ? 234 : 126;
        before += std::hypot(recorded[i][0] - 309.f, recorded[i][1] - targetY);
        const float error = std::hypot(p.x - 309, p.y - targetY);
        after += error; maximum = std::fmax(maximum, error);
    }
    assert(after < before * 0.4f && maximum < 22);
    std::printf("PASS right-side recorded-sample replay mean %.2f -> %.2f px, maximum %.2f px (offline only)\n",
        before / 8, after / 8, maximum);
    std::printf("PASS full-panel bounds, unchanged left/header/lower rim, smooth seams and non-folding map (minimum Jacobian %.3f)\n",
        minimumJacobian);

    Sdgoods::Cst816TouchState driver;
    Platform::IInputDevice::TouchEvent event;
    using Type = Platform::IInputDevice::TouchType;
    const auto sample = [&](int x, int y, uint32_t now) {
        const int rx = 359 - y;
        const uint8_t data[]{1, uint8_t(rx >> 8), uint8_t(rx), uint8_t(x >> 8), uint8_t(x)};
        driver.sample(data, now);
    };
    const uint8_t up[5]{};
    sample(347,121,10);
    assert(driver.poll(event) && event.type == Type::DOWN && event.x == 347 && event.y == 121);
    driver.sample(up,20); assert(driver.poll(event));
    driver.setRightCompensation(true);
    sample(347,121,30);
    const auto expected = Correction::apply(347,121);
    assert(driver.poll(event) && event.type == Type::DOWN);
    assert(event.x == std::lround(expected.x) && event.y == std::lround(expected.y));
    for (int x = 346; x >= 170; --x) {
        sample(x,180,40 + 347 - x);
        const auto p = Correction::apply(x,180);
        assert(driver.poll(event) && event.type == Type::MOVE);
        assert(event.x == std::lround(p.x) && event.y == std::lround(p.y));
    }
    driver.sample(up,300);
    assert(driver.poll(event) && event.type == Type::UP && event.x == 170 && event.y == 180);
    driver.bypass(true); sample(347,121,320);
    assert(driver.poll(event) && event.type == Type::DOWN && event.x == 347 && event.y == 121);
    driver.sample(up,340);assert(driver.poll(event));
    driver.bypass(false);sample(339,253,360);assert(driver.poll(event));
    const auto last = event;
    driver.sample(nullptr,380);
    assert(driver.poll(event) && event.type == Type::CANCEL && event.x == last.x && event.y == last.y);
    sample(339,253,400);assert(!driver.poll(event));
    driver.sample(up,420);assert(!driver.poll(event));
    // Production disables the fallback when a saved affine record is active.
    driver.setRightCompensation(false);
    Platform::TouchTransform affine; affine.values[2] = -10;
    driver.setTransform(affine);sample(339,253,440);
    assert(driver.poll(event) && event.type == Type::DOWN && event.x == 329 && event.y == 253);
    std::puts("PASS actual sensor rotation, corrected DOWN/MOVE/UP, raw bypass, cancel/release recovery, disabled fallback and saved affine path");
}
