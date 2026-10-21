#include "OstbTouch.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "esp_log.h"
#include "presentation/PixelRenderer.h"

namespace Ostb {
bool OstbTouch::begin(i2c_master_bus_handle_t bus, bool forceSetup) {
    if (!bus || i2c_master_probe(bus, 0x15, 30) != ESP_OK) return false;
    i2c_device_config_t config{};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = 0x15; config.scl_speed_hz = 100000;
    if (i2c_master_bus_add_device(bus, &config, &device_) != ESP_OK) return false;
    uint8_t reg = 0xa7, id = 0;
    const auto result = i2c_master_transmit_receive(device_, &reg, 1, &id, 1, 20);
    ESP_LOGI("OstbTouch", "0x15 ACK; chip ID read=%s value=0x%02x; SDA12 SCL11 polling", esp_err_to_name(result), id);
    uint8_t saved[17] = {};
    if (!forceSetup && Platform::blobs().blobSize("ostb_touch", "cal_v1") == sizeof(saved) &&
        Platform::blobs().readBlob("ostb_touch", "cal_v1", saved, sizeof(saved))) calibrated_ = calibration_.decode(saved);
    ESP_LOGI("OstbTouch", "Calibration %s", calibrated_ ? "loaded" : "required; tap four corners then center");
    return true;
}

void OstbTouch::emit(Type type, uint32_t now) {
    const int x = int(std::lround(calibration_.coordinate(rawX_,rawY_,0)));
    const int y = int(std::lround(calibration_.coordinate(rawX_,rawY_,1)));
    event_ = {type, int16_t(std::clamp(x,0,295)), int16_t(std::clamp(y,0,239)), now};
    pending_ = true;
    if (type != Type::MOVE) ESP_LOGI("OstbTouch", "%s raw=(%d,%d) screen=(%d,%d)",
        type == Type::DOWN ? "DOWN" : type == Type::UP ? "UP" : "CANCEL", rawX_,rawY_,event_.x,event_.y);
}

void OstbTouch::update(uint32_t now, bool blocked) {
    if (!device_ || skipped_) return;
    if (blocked) {
        if (down_) emit(Type::CANCEL, now);
        down_ = false; blockedUntilRelease_ = true; setupDown_ = false;
    }
    if (now - lastPoll_ < 20) return;
    lastPoll_ = now;
    uint8_t reg = 0x02, data[6] = {};
    if (i2c_master_transmit_receive(device_, &reg, 1, data, sizeof(data), 10) != ESP_OK) {
        if (down_) emit(Type::CANCEL, now);
        down_ = false; blockedUntilRelease_ = true; setupDown_ = false;
        return;
    }
    const int count = data[0] & 0x0f;
    if (count > 1) {
        if (down_) emit(Type::CANCEL, now);
        down_ = false; blockedUntilRelease_ = true; setupDown_ = false;
        return;
    }
    if (blockedUntilRelease_) {
        if (count == 0 && !blocked) blockedUntilRelease_ = false;
        return;
    }
    const int x = ((data[1] & 0x0f) << 8) | data[2];
    const int y = ((data[3] & 0x0f) << 8) | data[4];
    if (setupActive()) {
        if (count && !setupDown_) {
            setupX_ = x; setupY_ = y; setupDown_ = true; setupMoved_ = false; setupDownAt_ = now;
            setup_.beginContact();
        } else if (count && (std::abs(x-setupX_) > 15 || std::abs(y-setupY_) > 15)) setupMoved_ = true;
        if (count) setup_.sample(x, y);
        if (!count && setupDown_) {
            setupDown_ = false;
            if (setupMoved_ || now - setupDownAt_ > 1500) {
                setupHint_ = "\xe8\xaf\xb7\xe6\x8c\x89\xe4\xbd\x8f\xe5\x86\x8d\xe6\x94\xbe\xe5\xbc\x80";
                dirty_ = true;
                return;
            }
            const int sampleX = setup_.coordinate(0), sampleY = setup_.coordinate(1);
            const auto target = setup_.target();
            ESP_LOGI("OstbTouch", "Calibration target=%u screen=(%d,%d) raw=(%d,%d)", target,
                Stickmon::TouchCalibration::TARGETS[target][0], Stickmon::TouchCalibration::TARGETS[target][1], sampleX,sampleY);
            using Result = Stickmon::TouchCalibrationSetup::Result;
            const auto result = setup_.accept(calibration_, sampleX, sampleY);
            setupHint_ = nullptr;
            if (result == Result::COMPLETE) {
                uint8_t saved[17]; calibration_.encode(saved);
                calibrated_ = true; blockedUntilRelease_ = true; pending_ = false;
                const bool stored = Platform::blobs().writeBlob("ostb_touch", "cal_v1", saved, sizeof(saved));
                ESP_LOGI("OstbTouch", "Calibration passed center_error=%.2f saved=%u", calibration_.error(sampleX,sampleY,4),stored);
            } else if (result == Result::RETRY_CENTER) {
                setupHint_ = "\xe4\xb8\xad\xe5\xbf\x83\xe6\x9c\xaa\xe9\x80\x9a\xe8\xbf\x87\xe8\xaf\xb7\xe9\x87\x8d\xe8\xaf\x95";
                ESP_LOGW("OstbTouch", "Calibration center_error=%.2f; tap center again", calibration_.error(sampleX,sampleY,4));
            } else if (result == Result::RESTART) {
                setupHint_ = "\xe8\xaf\xb7\xe9\x87\x8d\xe6\x96\xb0\xe5\xbc\x80\xe5\xa7\x8b";
                ESP_LOGW("OstbTouch", "Calibration inconsistent; restarting corners");
            }
            dirty_ = true;
        }
        return;
    }
    if (count) {
        const bool changed = rawX_ != x || rawY_ != y;
        rawX_ = x; rawY_ = y;
        if (!down_) { down_ = true; emit(Type::DOWN, now); }
        else if (changed) emit(Type::MOVE, now);
    } else if (down_) { down_ = false; emit(Type::UP, now); }
}

bool OstbTouch::poll(Event& event) {
    if (!pending_) return false;
    event = event_; pending_ = false;
    return true;
}

void OstbTouch::drawSetup() {
    if (!setupActive() || !dirty_) return;
    dirty_ = false;
    PixelRenderer::clear(0);
    auto& c = PixelRenderer::canvas();
    PixelRenderer::text(100, 61, "\xe8\xaf\xb7\xe7\x82\xb9\xe5\x87\xbb\xe5\x8d\x81\xe5\xad\x97", 0xffff);
    char label[24]; std::snprintf(label,sizeof(label),"%u / 5",setup_.target()+1);
    PixelRenderer::text(128, 82, label, 0xffe0);
    PixelRenderer::text(92, 151, "\xe8\xaf\xb7\xe6\x8c\x89\xe4\xbd\x8f\xe5\x86\x8d\xe6\x94\xbe\xe5\xbc\x80", 0x7bef);
    PixelRenderer::text(100, 216, "\xe5\x8f\xb3\xe9\x94\xae\xe8\xb7\xb3\xe8\xbf\x87\xe8\xa7\xa6\xe6\x91\xb8", 0x7bef);
    if (setupHint_) PixelRenderer::text(76, 174, setupHint_, 0xfd20);
    const int x = Stickmon::TouchCalibration::TARGETS[setup_.target()][0];
    const int y = Stickmon::TouchCalibration::TARGETS[setup_.target()][1];
    c.drawCircle(x,y,10,0xffe0);
    c.drawFastHLine(x-15,y,31,0xffff); c.drawFastVLine(x,y-15,31,0xffff);
    Platform::display().present();
}
}  // namespace Ostb
