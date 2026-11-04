// SPDX-License-Identifier: Apache-2.0
#include "SdgoodsPlatform.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "Rgb565Transfer.h"
#include "TouchCalibrationStore.h"
extern "C" {
#include "board_pins.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_random.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
}

extern const uint8_t archiveStart[] asm("_binary_stickmon_resources_bin_start");
extern const uint8_t archiveEnd[] asm("_binary_stickmon_resources_bin_end");

namespace Sdgoods {
namespace {
esp_err_t openStore(const char* nameSpace, nvs_open_mode_t mode, nvs_handle_t& handle) {
    if (!nameSpace || !*nameSpace || std::strlen(nameSpace) > 12) return ESP_ERR_INVALID_ARG;
    char isolated[16];
    std::snprintf(isolated, sizeof(isolated), "sg_%s", nameSpace);
    return nvs_open(isolated, mode, &handle);
}
}

SdgoodsPlatform::SdgoodsPlatform()
    : resources_(archiveStart, archiveEnd - archiveStart),
      audio_(*this),
      services_{*this, *this, *this, *this, *this, audio_, *this, *this,
                *this, resources_, *this, radio_} {}

bool SdgoodsPlatform::initialize() {
    if (storageReady_) return true;
    const auto error = nvs_flash_init();
    if (error != ESP_OK) {
        Platform::logf("[SDGOODS] NVS init failed; storage retained: %s\n", esp_err_to_name(error));
        return false;
    }
    storageReady_ = true;
    return true;
}

bool SdgoodsPlatform::begin() {
    if (initialized_) return true;
    // Restore the latch before releasing retained holds, including USB fallback wake.
    rtc_gpio_deinit(BOARD_BAT_CONTROL_GPIO);
    rtc_gpio_deinit(BOARD_KEY_GPIO);
    gpio_config_t latch{};
    latch.pin_bit_mask = 1ULL << BOARD_BAT_CONTROL_GPIO;
    latch.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&latch));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_BAT_CONTROL_GPIO, BOARD_BAT_CONTROL_LATCH_LEVEL));
    ESP_ERROR_CHECK(gpio_hold_dis(BOARD_BAT_CONTROL_GPIO));
    gpio_deep_sleep_hold_dis();
    if (!initialize() || !resources_.mount()) return false;
    if (!audio_.begin()) Platform::logLine("[SDGOODS] speaker unavailable; continuing silently");
    if (!beginDisplay() || !beginTouch()) return false;
    Platform::logf("[SDGOODS] IMU=%s; standby, shower uses touch drag\n",
        imu_.begin(i2cBus_) ? "ready" : "unavailable");
    Platform::logf("[SDGOODS] battery ADC=%s\n", battery_.begin() ? "ready" : "unavailable");
    batteryLevel();
    gpio_config_t key{};
    key.pin_bit_mask = 1ULL << BOARD_KEY_GPIO;
    key.mode = GPIO_MODE_INPUT;
    key.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&key));
    powerButton_.begin(gpio_get_level(BOARD_KEY_GPIO) == BOARD_KEY_ACTIVE_LEVEL, millis());
    initialized_ = true;
    Platform::logLine("[SDGOODS] native 360x360 RGB565; PSRAM frame + two 8-row DMA strips; no LVGL");
    return true;
}

bool SdgoodsPlatform::beginDisplay() {
    frame_ = static_cast<uint16_t*>(heap_caps_calloc(360 * 360, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    for (auto& strip : strips_)
        strip = static_cast<uint16_t*>(heap_caps_malloc(360 * 8 * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    completion_ = xSemaphoreCreateCounting(2, 0);
    if (!frame_ || !strips_[0] || !strips_[1] || !completion_) return false;
    // Keep backlight off until a full framebuffer has reached the panel.
    gpio_config_t output{};
    output.pin_bit_mask = (1ULL << BOARD_LCD_GPIO_BACKLIGHT) | (1ULL << BOARD_LCD_POW_EN_GPIO) |
                          (1ULL << BOARD_LCD_GPIO_RST);
    output.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&output));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(BOARD_LCD_GPIO_BACKLIGHT), BOARD_LCD_BACKLIGHT_LEDC_INVERT));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(BOARD_LCD_POW_EN_GPIO), 0));
    sleepMs(10);
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(BOARD_LCD_GPIO_RST), 0));
    sleepMs(10);
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(BOARD_LCD_GPIO_RST), 1));
    sleepMs(50);
    ledc_timer_config_t timer{};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_13_BIT;
    timer.timer_num = LEDC_TIMER_0;
    timer.freq_hz = 5000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    ledc_channel_config_t light{};
    light.gpio_num = BOARD_LCD_GPIO_BACKLIGHT;
    light.speed_mode = LEDC_LOW_SPEED_MODE;
    light.channel = LEDC_CHANNEL_0;
    light.timer_sel = LEDC_TIMER_0;
    light.flags.output_invert = BOARD_LCD_BACKLIGHT_LEDC_INVERT;
    ESP_ERROR_CHECK(ledc_channel_config(&light));
    spi_bus_config_t bus{};
    bus.data0_io_num = BOARD_LCD_SPI_IO_DATA0; bus.data1_io_num = BOARD_LCD_SPI_IO_DATA1;
    bus.sclk_io_num = BOARD_LCD_SPI_IO_SCK;
    bus.data2_io_num = BOARD_LCD_SPI_IO_DATA2; bus.data3_io_num = BOARD_LCD_SPI_IO_DATA3;
    bus.data4_io_num = bus.data5_io_num = bus.data6_io_num = bus.data7_io_num = -1;
    bus.max_transfer_sz = 360 * 8 * 2;
    bus.flags = SPICOMMON_BUSFLAG_MASTER;
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_spi_config_t config{};
    config.cs_gpio_num = BOARD_LCD_SPI_IO_CS; config.dc_gpio_num = -1;
    config.pclk_hz = BOARD_LCD_SPI_PCLK_HZ; config.trans_queue_depth = 2;
    config.lcd_cmd_bits = 32; config.lcd_param_bits = 8;
    config.flags.quad_mode = 1;
    config.user_ctx = completion_;
    config.on_color_trans_done = [](esp_lcd_panel_io_handle_t,
        esp_lcd_panel_io_event_data_t*, void* context) -> bool {
        BaseType_t wake = pdFALSE;
        xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(context), &wake);
        return wake == pdTRUE;
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(SPI2_HOST), &config, &io_));
    return stickmon_sdgoods_panel_create(io_, &panel_) == ESP_OK;
}

bool SdgoodsPlatform::beginTouch() {
    gpio_config_t reset{};
    reset.pin_bit_mask = 1ULL << BOARD_TOUCH_GPIO_RST;
    reset.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&reset));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_TOUCH_GPIO_RST, 0)); sleepMs(11);
    ESP_ERROR_CHECK(gpio_set_level(BOARD_TOUCH_GPIO_RST, 1)); sleepMs(50);
    i2c_master_bus_config_t bus{};
    bus.i2c_port = BOARD_I2C_PORT;
    bus.sda_io_num = static_cast<gpio_num_t>(BOARD_I2C_SDA);
    bus.scl_io_num = static_cast<gpio_num_t>(BOARD_I2C_SCL);
    bus.clk_source = I2C_CLK_SRC_DEFAULT;
    bus.glitch_ignore_cnt = 7;
    bus.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&bus, &i2cBus_) != ESP_OK) return false;
    i2c_device_config_t device{};
    device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device.device_address = 0x15;
    device.scl_speed_hz = BOARD_I2C_FREQ_HZ;
    if (i2c_master_bus_add_device(i2cBus_, &device, &touchDevice_) != ESP_OK) return false;
    if (i2c_master_probe(i2cBus_, 0x15, 100) != ESP_OK) return false;
    bool restored = false;
    const auto transform = TouchCalibrationStore::load(*this, &restored);
    touch_.setTransform(transform);
    const bool rightCompensation = STICKMON_SDGOODS_RIGHT_TOUCH_COMPENSATION && !restored;
    touch_.setRightCompensation(rightCompensation);
    Platform::logf("[TouchCorrection] right_progressive=%d saved_affine=%d\n",
        rightCompensation, restored);
    Platform::logf("[TouchCalibration] loaded %.5f %.5f %.3f / %.5f %.5f %.3f\n",
        transform.values[0], transform.values[1], transform.values[2],
        transform.values[3], transform.values[4], transform.values[5]);
    return true;
}

void SdgoodsPlatform::bypassTouchCalibration(bool value) {
    touchBlocked_ = true;
    cancelPending_ = true;
    touch_.sample(nullptr, millis());
    touch_.bypass(value);
}

bool SdgoodsPlatform::saveTouchCalibration(const Platform::TouchTransform& transform) {
    if (!TouchCalibrationStore::save(*this, transform)) {
        Platform::logLine("[TouchCalibration] save failed; previous correction retained");
        return false;
    }
    // Publish only after the independent NVS record has been committed.
    touch_.setTransform(transform);
    touch_.setRightCompensation(false);
    bypassTouchCalibration(false);
    Platform::logf("[TouchCalibration] saved %.5f %.5f %.3f / %.5f %.5f %.3f\n",
        transform.values[0], transform.values[1], transform.values[2],
        transform.values[3], transform.values[4], transform.values[5]);
    return true;
}

uint32_t SdgoodsPlatform::micros() const { return static_cast<uint32_t>(esp_timer_get_time()); }
uint32_t SdgoodsPlatform::millis() const { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
void SdgoodsPlatform::sleepMs(uint32_t ms) { vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(ms))); }
void SdgoodsPlatform::write(const char* text, size_t length) { if (text) std::fwrite(text, 1, length, stdout); }
void SdgoodsPlatform::flush() { std::fflush(stdout); }

void SdgoodsPlatform::pollInputs() {
    const uint32_t now = millis();
    const bool raw = gpio_get_level(BOARD_KEY_GPIO) == BOARD_KEY_ACTIVE_LEVEL;
    switch (powerButton_.update(raw, now)) {
    case Stickmon::PowerButton::Action::PRESSED:
        saveRequested_ = true; cancelPending_ = true; break;
    case Stickmon::PowerButton::Action::TOGGLE_LOCK:
        setLocked(!locked_); unlocked_ = !locked_; break;
    case Stickmon::PowerButton::Action::SHUTDOWN: shutdown_ = true; break;
    default: break;
    }
    if (!powerButton_.down()) battery_.poll(now);
    if (now - sampledAtMs_ < 20) return;
    sampledAtMs_ = now;
    if (powerButton_.down() || locked_) {
        touchBlocked_ = true;
        touch_.sample(nullptr, now); return;
    }
    const uint8_t reg = 0x02;
    uint8_t data[5]{};
    const auto err = i2c_master_transmit_receive(touchDevice_, &reg, 1, data, sizeof(data), 20);
    if (err == ESP_OK && (data[0] & 0x0f) == 0) touchBlocked_ = false;
    touch_.sample(err == ESP_OK && !touchBlocked_ ? data : nullptr, now);
}
void SdgoodsPlatform::present() { presentRegion(0, 0, 360, 360); }
void SdgoodsPlatform::presentRegion(int x, int y, int width, int height) {
    if (locked_) return;
    if (!firstFrame_ || x < 0 || y < 0 || width <= 0 || height <= 0 ||
        width > 360 || height > 360 || x > 360 - width || y > 360 - height) {
        x = y = 0; width = height = 360;
    }
    const auto submit = [](void* context, int x0, int y0, int x1, int y1, const uint16_t* data) -> esp_err_t {
        return esp_lcd_panel_draw_bitmap(static_cast<esp_lcd_panel_handle_t>(context), x0, y0, x1, y1, data);
    };
    const auto err = Stickmon::submitRgb565Region(frame_, 360, x, y, x + width, y + height,
        strips_, 2, 8, completion_, submit, panel_);
    // A timed-out DMA may still own a strip. Stop before any memory can be reused.
    if (err != ESP_OK) {
        Platform::logf("[SDGOODS] frame transfer failed: %s\n", esp_err_to_name(err));
        abort();
    }
    firstFrame_ = true;
    applyBacklight();
}
void SdgoodsPlatform::applyBacklight() {
    const uint32_t duty = (firstFrame_ && !locked_) ? (unsigned(brightness_) * 8191U + 127U) / 255U : 0;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}
void SdgoodsPlatform::setBrightness(uint8_t value) {
    if (brightness_ != value) battery_.resetChargingEstimate();
    brightness_ = value;
    if (firstFrame_) applyBacklight();
}
void SdgoodsPlatform::setLocked(bool value) {
    battery_.resetChargingEstimate();
    locked_ = value;
    touchBlocked_ = true; cancelPending_ = true;
    touch_.sample(nullptr, millis());
    audio_.suspend(value);
    if (value) { audio_.endMicrophone(); imu_.setAccelerationEnabled(false); }
    if (firstFrame_) applyBacklight();
    Platform::logf("[SDGOODS] screen %s; volume=%u\n", value ? "locked" : "unlocked", audio_.volume());
}
void SdgoodsPlatform::sleep() { setLocked(true); }
Platform::WakeReason SdgoodsPlatform::wakeReason() const {
    switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0: return Platform::WakeReason::EXTERNAL_SIGNAL;
    case ESP_SLEEP_WAKEUP_TIMER: return Platform::WakeReason::TIMER;
    default: return Platform::WakeReason::NORMAL;
    }
}
uint32_t SdgoodsPlatform::hardwareRandom() { return esp_random(); }
size_t SdgoodsPlatform::externalMemorySize() const { return esp_psram_get_size(); }
size_t SdgoodsPlatform::externalMemoryFree() const { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }
void SdgoodsPlatform::restart() { esp_restart(); }
void SdgoodsPlatform::enterDeepSleep(uint64_t durationUs, bool externalWake) {
    prepareSleep();
    ESP_ERROR_CHECK(gpio_set_level(BOARD_BAT_CONTROL_GPIO, BOARD_BAT_CONTROL_LATCH_LEVEL));
    ESP_ERROR_CHECK(gpio_hold_en(BOARD_BAT_CONTROL_GPIO));
    gpio_deep_sleep_hold_en();
    if (durationUs) ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(durationUs));
    if (externalWake) ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(BOARD_KEY_GPIO, BOARD_KEY_ACTIVE_LEVEL));
    esp_deep_sleep_start();
}
void SdgoodsPlatform::prepareSleep() {
    setLocked(true);
    radio_.end();
    audio_.stop();
    ESP_ERROR_CHECK(gpio_hold_en(BOARD_AUDIO_PA_EN_GPIO));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, false));
    ESP_ERROR_CHECK(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL));
    ESP_ERROR_CHECK(rtc_gpio_pullup_en(BOARD_KEY_GPIO));
    ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(BOARD_KEY_GPIO));
    flush();
}
void SdgoodsPlatform::powerOff() {
    prepareSleep();
    Platform::logLine("[SDGOODS] snapshot saved; releasing power latch (USB: deep sleep fallback)");
    flush();
    ESP_ERROR_CHECK(gpio_set_level(BOARD_BAT_CONTROL_GPIO, 1 - BOARD_BAT_CONTROL_LATCH_LEVEL));
    ESP_ERROR_CHECK(gpio_hold_en(BOARD_BAT_CONTROL_GPIO));
    gpio_deep_sleep_hold_en();
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(BOARD_KEY_GPIO, BOARD_KEY_ACTIVE_LEVEL));
    esp_deep_sleep_start();
}
void* SdgoodsPlatform::allocate(size_t bytes, bool external) {
    void* result = external ? heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : nullptr;
    return result ? result : heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
}
void SdgoodsPlatform::release(void* memory) { heap_caps_free(memory); }

size_t SdgoodsPlatform::blobSize(const char* ns, const char* key) {
    nvs_handle_t handle;
    if (openStore(ns, NVS_READONLY, handle) != ESP_OK) return 0;
    size_t length = 0;
    const auto result = nvs_get_blob(handle, key, nullptr, &length);
    nvs_close(handle);
    return result == ESP_OK ? length : 0;
}
bool SdgoodsPlatform::readBlob(const char* ns, const char* key, void* output, size_t length) {
    if (!output || blobSize(ns, key) != length) return false;
    nvs_handle_t handle;
    if (openStore(ns, NVS_READONLY, handle) != ESP_OK) return false;
    size_t actual = length;
    const auto result = nvs_get_blob(handle, key, output, &actual);
    nvs_close(handle);
    return result == ESP_OK && actual == length;
}
bool SdgoodsPlatform::writeBlob(const char* ns, const char* key, const void* data, size_t length) {
    nvs_handle_t handle;
    if (!data || openStore(ns, NVS_READWRITE, handle) != ESP_OK) return false;
    auto result = nvs_set_blob(handle, key, data, length);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    if (result != ESP_OK) Platform::logf("[SDGOODS] NVS write %s/%s failed: %s\n", ns, key, esp_err_to_name(result));
    return result == ESP_OK;
}
bool SdgoodsPlatform::removeBlob(const char* ns, const char* key) {
    nvs_handle_t handle;
    if (openStore(ns, NVS_READWRITE, handle) != ESP_OK) return false;
    auto result = nvs_erase_key(handle, key);
    if (result == ESP_ERR_NVS_NOT_FOUND) result = ESP_OK;
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result == ESP_OK;
}
bool SdgoodsPlatform::clearNamespace(const char* ns) {
    nvs_handle_t handle;
    if (openStore(ns, NVS_READWRITE, handle) != ESP_OK) return false;
    auto result = nvs_erase_all(handle);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result == ESP_OK;
}
}  // namespace Sdgoods
