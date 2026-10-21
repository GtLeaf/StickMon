#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Stickmon {
// Four corners fit orientation, axis exchange and scale. A fifth point checks
// the fit independently. LCD address-window offsets are not touch offsets.
class TouchCalibration {
public:
    static constexpr int TARGETS[5][2] = {{32,32},{263,32},{263,207},{32,207},{148,120}};
    uint16_t raw[4][2] = {};
    bool fit() {
        valid_ = false;
        for (int axis = 0; axis < 2; ++axis) {
            double a[3][4] = {};
            for (int i = 0; i < 4; ++i) {
                if (raw[i][0] > 4095 || raw[i][1] > 4095) return false;
                const double v[] = {double(raw[i][0]), double(raw[i][1]), 1};
                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 3; ++c) a[r][c] += v[r] * v[c];
                    a[r][3] += v[r] * TARGETS[i][axis];
                }
            }
            for (int c = 0; c < 3; ++c) {
                int pivot = c;
                for (int r = c + 1; r < 3; ++r) if (std::abs(a[r][c]) > std::abs(a[pivot][c])) pivot = r;
                if (std::abs(a[pivot][c]) < 1e-6) return false;
                for (int j = c; j < 4; ++j) std::swap(a[c][j], a[pivot][j]);
                const double divisor = a[c][c];
                for (int j = c; j < 4; ++j) a[c][j] /= divisor;
                for (int r = 0; r < 3; ++r) if (r != c) {
                    const double multiple = a[r][c];
                    for (int j = c; j < 4; ++j) a[r][j] -= multiple * a[c][j];
                }
            }
            for (int i = 0; i < 3; ++i) matrix_[axis][i] = a[i][3];
        }
        for (int i = 0; i < 4; ++i) if (error(raw[i][0], raw[i][1], i) > 12) return false;
        return valid_ = true;
    }
    bool valid() const { return valid_; }
    double coordinate(int x, int y, int axis) const {
        return matrix_[axis][0] * x + matrix_[axis][1] * y + matrix_[axis][2];
    }
    double error(int x, int y, int target) const {
        return std::hypot(coordinate(x,y,0) - TARGETS[target][0], coordinate(x,y,1) - TARGETS[target][1]);
    }
    void encode(uint8_t (&data)[17]) const {
        data[0] = 1;
        for (int i = 0; i < 8; ++i) {
            const uint16_t value = raw[i / 2][i % 2];
            data[1+i*2] = value; data[2+i*2] = value >> 8;
        }
    }
    bool decode(const uint8_t (&data)[17]) {
        if (data[0] != 1) return false;
        for (int i = 0; i < 8; ++i) raw[i / 2][i % 2] = data[1+i*2] | uint16_t(data[2+i*2]) << 8;
        return fit();
    }
private:
    bool valid_ = false;
    double matrix_[2][3] = {};
};

class TouchCalibrationSetup {
public:
    enum class Result { NEXT, RETRY_CENTER, RESTART, COMPLETE };
    uint8_t target() const { return target_; }
    void beginContact() { sampleCount_ = nextSample_ = 0; }
    void sample(uint16_t x, uint16_t y) {
        samples_[nextSample_][0] = x;
        samples_[nextSample_][1] = y;
        nextSample_ = (nextSample_ + 1) % 5;
        if (sampleCount_ < 5) ++sampleCount_;
    }
    uint16_t coordinate(int axis) const {
        uint16_t sorted[5] = {};
        for (int i = 0; i < sampleCount_; ++i) sorted[i] = samples_[i][axis];
        std::sort(sorted, sorted + sampleCount_);
        return sampleCount_ ? sorted[sampleCount_ / 2] : 0;
    }
    Result accept(TouchCalibration& calibration, uint16_t x, uint16_t y) {
        if (target_ < 4) {
            calibration.raw[target_][0] = x;
            calibration.raw[target_][1] = y;
            ++target_;
            if (target_ == 4 && !calibration.fit()) {
                target_ = 0;
                return Result::RESTART;
            }
            return Result::NEXT;
        }
        if (calibration.error(x, y, 4) <= 12) return Result::COMPLETE;
        // A missed center tap should not discard four usable corners.
        if (++centerRetries_ < 3) return Result::RETRY_CENTER;
        centerRetries_ = target_ = 0;
        return Result::RESTART;
    }
private:
    uint8_t target_ = 0, centerRetries_ = 0, sampleCount_ = 0, nextSample_ = 0;
    uint16_t samples_[5][2] = {};
};
}  // namespace Stickmon
