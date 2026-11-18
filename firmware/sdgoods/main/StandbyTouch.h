// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>

namespace Sdgoods {
// Standby uses raw sensor coordinates, independent of page mapping/calibration.
// A complete release is required after every reset; I2C failures are not releases.
class StandbyTouch {
public:
    void reset() {
        blocked_ = true;
        contact_ = firstTap_ = validTap_ = false;
    }
    bool sample(const uint8_t* data, uint32_t now) {
        const int count = data ? data[0] & 15 : -1;
        const int x = data ? ((data[1] & 15) << 8) | data[2] : -1;
        const int y = data ? ((data[3] & 15) << 8) | data[4] : -1;
        if (!data || count > 1 || (count == 1 && (x >= 360 || y >= 360))) {
            reset();
            return false;
        }
        if (blocked_) {
            if (count == 0) blocked_ = false;
            return false;
        }
        if (count == 1) {
            if (!contact_) {
                contact_ = validTap_ = true;
                downAt_ = now; x_ = x; y_ = y;
            }
            const int dx = x - x_, dy = y - y_;
            if (dx * dx + dy * dy > 24 * 24 || now - downAt_ > 250) {
                validTap_ = firstTap_ = false;
            }
            return false;
        }
        if (!contact_) return false;
        contact_ = false;
        if (!validTap_ || now - downAt_ > 250) {
            firstTap_ = false;
            return false;
        }
        if (firstTap_ && downAt_ - releasedAt_ <= 350) {
            reset();
            return true;
        }
        firstTap_ = true;
        releasedAt_ = now;
        return false;
    }
private:
    uint32_t downAt_ = 0, releasedAt_ = 0;
    int x_ = 0, y_ = 0;
    bool blocked_ = true, contact_ = false, firstTap_ = false, validTap_ = false;
};
}  // namespace Sdgoods
