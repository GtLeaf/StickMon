#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "platform/api/TouchTransform.h"
#include "ui/common/TouchTest.h"

namespace Ui {
// Eight perimeter points fit the model. The center is never part of that fit.
class TouchCalibrationSession {
public:
    enum class Stage { INTRO, COLLECT, READY };
    enum class Result { NONE, NEXT, HOLD, FIT_FAILED, CENTER_FAILED, READY };
    Stage stage = Stage::INTRO;
    int count = 0;
    float rms = 0, maximum = 0, centerError = 0;
    Platform::TouchTransform transform;
    TouchTest::Point last{};
    Result result = Result::NONE;
    void start() { *this = {}; stage = Stage::COLLECT; }
    void cancelContact() { pressed_ = false; }
    void down(int x, int y, uint32_t now) {
        pressed_ = true; since_ = now; size_ = next_ = 0;
        sample(x, y);
    }
    void move(int x, int y) { if (pressed_) sample(x, y); }
    Result up(uint32_t now) {
        if (!pressed_ || stage != Stage::COLLECT) return Result::NONE;
        pressed_ = false;
        if (now - since_ < 100 || size_ < 4) return result = Result::HOLD;
        int sorted[2][5]{};
        for (int i = 0; i < size_; ++i) {
            sorted[0][i] = recent_[i].x; sorted[1][i] = recent_[i].y;
        }
        for (auto& axis : sorted) {
            std::sort(axis, axis + size_);
            if (axis[size_-1] - axis[0] > 10) return result = Result::HOLD;
        }
        const TouchTest::Point point{sorted[0][size_/2], sorted[1][size_/2]};
        last = point;
        if (count < 8) {
            raw_[count++] = point;
            if (count == 8 && !fit()) {
                count = 0;
                return result = Result::FIT_FAILED;
            }
            return result = Result::NEXT;
        }
        centerError = error(point, TouchTest::TARGETS[8]);
        if (centerError > 12) return result = Result::CENTER_FAILED;
        stage = Stage::READY;
        return result = Result::READY;
    }
private:
    TouchTest::Point raw_[8]{}, recent_[5]{};
    uint32_t since_ = 0;
    int size_ = 0, next_ = 0;
    bool pressed_ = false;
    void sample(int x, int y) {
        recent_[next_] = {x, y};
        next_ = (next_ + 1) % 5;
        size_ = std::min(size_ + 1, 5);
    }
    float error(TouchTest::Point raw, TouchTest::Point target) const {
        return std::hypot(transform.x(raw.x, raw.y) - target.x,
                          transform.y(raw.x, raw.y) - target.y);
    }
    bool fit() {
        Platform::TouchTransform candidate;
        for (int axis = 0; axis < 2; ++axis) {
            double a[3][4]{};
            for (int i = 0; i < 8; ++i) {
                // Normalize before solving to keep the small system well conditioned.
                const double v[]{(raw_[i].x-180)/180.0, (raw_[i].y-180)/180.0, 1};
                const double target = axis ? TouchTest::TARGETS[i].y : TouchTest::TARGETS[i].x;
                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 3; ++c) a[r][c] += v[r]*v[c];
                    a[r][3] += v[r]*target;
                }
            }
            for (int c = 0; c < 3; ++c) {
                int pivot = c;
                for (int r = c+1; r < 3; ++r)
                    if (std::abs(a[r][c]) > std::abs(a[pivot][c])) pivot = r;
                if (std::abs(a[pivot][c]) < 1e-8) return false;
                for (int j = c; j < 4; ++j) std::swap(a[c][j], a[pivot][j]);
                const double divisor = a[c][c];
                for (int j = c; j < 4; ++j) a[c][j] /= divisor;
                for (int r = 0; r < 3; ++r) if (r != c) {
                    const double multiple = a[r][c];
                    for (int j = c; j < 4; ++j) a[r][j] -= multiple*a[c][j];
                }
            }
            candidate.values[axis*3] = a[0][3]/180;
            candidate.values[axis*3+1] = a[1][3]/180;
            candidate.values[axis*3+2] = a[2][3] - a[0][3] - a[1][3];
        }
        if (!candidate.valid(360, 360)) return false;
        transform = candidate;
        rms = maximum = 0;
        for (int i = 0; i < 8; ++i) {
            const float e = error(raw_[i], TouchTest::TARGETS[i]);
            rms += e*e;
            maximum = std::max(maximum, e);
        }
        rms = std::sqrt(rms/8);
        return maximum <= 12 && rms <= 8;
    }
};
}  // namespace Ui
