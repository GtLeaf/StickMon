#include "OstbPlatform.h"

#include <cstdio>
#include <cstring>
#include <new>

#include "Rgb565Transfer.h"
#include "Nv3023Init.h"
#include "OstbBattery.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev_defaults.h"
#include "esp_heap_caps.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_psram.h"
#include "esp_random.h"
#include "esp_sleep.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "hardware/EspNowRadioConfig.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace Ostb {
namespace {

constexpr char RESOURCE_BASE_PATH[] = "/assets";
constexpr char RESOURCE_PARTITION[] = "resources";

constexpr uint8_t PEER_QUEUE_CAPACITY = 8;
// Recovered from the factory 2.2.2 board constructor. The middle key is
// externally biased; the factory Button constructor disables pulls on GPIO1.
constexpr gpio_num_t POWER_BUTTON_GPIO = GPIO_NUM_1;
constexpr int POWER_BUTTON_ACTIVE_LEVEL = 1;

Platform::PeerPacket peerQueue[PEER_QUEUE_CAPACITY];
volatile uint8_t peerQueueHead = 0;
volatile uint8_t peerQueueTail = 0;
portMUX_TYPE peerQueueMux = portMUX_INITIALIZER_UNLOCKED;

void receivePeerPacket(const esp_now_recv_info_t* info,
                       const uint8_t* data,
                       int length) {
    if (!info || !info->src_addr || !data || length <= 0 ||
        static_cast<size_t>(length) > Platform::PeerPacket::MAX_PAYLOAD_BYTES) {
        return;
    }
    portENTER_CRITICAL(&peerQueueMux);
    uint8_t next = static_cast<uint8_t>(
        (peerQueueHead + 1U) % PEER_QUEUE_CAPACITY);
    if (next != peerQueueTail) {
        Platform::PeerPacket& packet = peerQueue[peerQueueHead];
        std::memcpy(packet.source, info->src_addr, sizeof(packet.source));
        std::memcpy(packet.payload, data, static_cast<size_t>(length));
        packet.length = static_cast<size_t>(length);
        peerQueueHead = next;
    }
    portEXIT_CRITICAL(&peerQueueMux);
}

void resetPeerQueue() {
    portENTER_CRITICAL(&peerQueueMux);
    peerQueueHead = 0;
    peerQueueTail = 0;
    portEXIT_CRITICAL(&peerQueueMux);
}

class SpiffsResourceFile final : public Platform::IResourceFile {
public:
    explicit SpiffsResourceFile(FILE* file) : file_(file) {
        if (!file_) return;
        long original = std::ftell(file_);
        if (original < 0 || std::fseek(file_, 0, SEEK_END) != 0) return;
        long length = std::ftell(file_);
        if (length >= 0) size_ = static_cast<size_t>(length);
        std::fseek(file_, original, SEEK_SET);
    }

    bool valid() const override { return file_ != nullptr; }
    size_t size() const override { return valid() ? size_ : 0; }
    size_t position() const override {
        if (!valid()) return 0;
        long value = std::ftell(file_);
        return value >= 0 ? static_cast<size_t>(value) : 0;
    }
    size_t read(void* output, size_t length) override {
        return valid() && output ? std::fread(output, 1, length, file_) : 0;
    }
    bool seek(size_t position) override {
        return valid() && position <= size_ &&
               std::fseek(file_, static_cast<long>(position), SEEK_SET) == 0;
    }
    void close() override {
        if (!file_) return;
        std::fclose(file_);
        file_ = nullptr;
    }

private:
    FILE* file_ = nullptr;
    size_t size_ = 0;
};

}  // namespace


OstbPlatform::OstbPlatform()
    : services_{*this, *this, *this, *this, *this, *this, *this, *this,
                *this, *this, *this, *this}, mixer_(*this) {}
OstbPlatform& OstbPlatform::instance() { static OstbPlatform p; return p; }
Platform::Services& OstbPlatform::serviceBundle() { return services_; }
void bindOstbPlatform() { Platform::bind(OstbPlatform::instance().serviceBundle()); }

bool OstbPlatform::begin() {
    if (initialized_) return true;
    // Hide panel RAM before storage, display and game initialization.
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_13, 0));
    gpio_config_t backlight{};
    backlight.pin_bit_mask = 1ULL << 13;
    backlight.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&backlight));
    if (!initialize() || !beginDisplay()) return false;
    gpio_config_t keys{};
    keys.pin_bit_mask = (1ULL << 40) | (1ULL << 39);
    keys.mode = GPIO_MODE_INPUT;
    keys.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&keys));
    gpio_config_t power{};
    power.pin_bit_mask = 1ULL << POWER_BUTTON_GPIO;
    power.mode = GPIO_MODE_INPUT;
    ESP_ERROR_CHECK(gpio_config(&power));
    powerButton_.begin(gpio_get_level(POWER_BUTTON_GPIO) == POWER_BUTTON_ACTIVE_LEVEL, millis());
    if (beginI2c()) {
        if (!beginAudio()) ESP_LOGW("OSTB", "Speaker unavailable; continuing silently");
        const bool forceSetup = gpio_get_level(GPIO_NUM_40) == 0 && gpio_get_level(GPIO_NUM_39) == 0;
        if (!touch_.begin(i2cBus_, forceSetup)) ESP_LOGW("OSTB", "Touch unavailable; physical buttons remain enabled");
    } else ESP_LOGW("OSTB", "I2C unavailable; continuing with physical buttons");
    if (!beginBattery()) ESP_LOGW("OSTB", "Battery sampling unavailable");
    initialized_ = true;
    ESP_LOGI("OSTB", "Native 296x240; PSRAM=%u free=%u; A=40 B=39 POWER=1 active-high",
             unsigned(externalMemorySize()), unsigned(externalMemoryFree()));
    return true;
}

bool OstbPlatform::beginDisplay() {
    frame_ = static_cast<uint16_t*>(heap_caps_calloc(296 * 240, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    for (auto& strip : strips_)
        strip = static_cast<uint16_t*>(heap_caps_malloc(296 * 32 * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    completion_ = xSemaphoreCreateCounting(2, 0);
    if (!frame_ || !strips_[0] || !strips_[1] || !completion_) return false;
    gpio_config_t reset{};
    reset.pin_bit_mask = 1ULL << 18;
    reset.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&reset));
    ledc_timer_config_t timer{};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_10_BIT;
    timer.timer_num = LEDC_TIMER_0;
    timer.freq_hz = 25000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    ledc_channel_config_t light{};
    light.gpio_num = 13;
    light.speed_mode = LEDC_LOW_SPEED_MODE;
    light.channel = LEDC_CHANNEL_0;
    light.timer_sel = LEDC_TIMER_0;
    light.duty = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&light));
    spi_bus_config_t bus{};
    bus.mosi_io_num = 10; bus.miso_io_num = -1; bus.sclk_io_num = 9;
    bus.quadwp_io_num = -1; bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 296 * 32 * 2;
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_spi_config_t config{};
    config.cs_gpio_num = 14; config.dc_gpio_num = 8;
    config.pclk_hz = 40000000; config.trans_queue_depth = 2;
    config.lcd_cmd_bits = 8; config.lcd_param_bits = 8;
    config.user_ctx = completion_;
    config.on_color_trans_done = [](esp_lcd_panel_io_handle_t,
        esp_lcd_panel_io_event_data_t*, void* context) -> bool {
        BaseType_t wake = pdFALSE;
        xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(context), &wake);
        return wake == pdTRUE;
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(SPI3_HOST), &config, &io_));
    gpio_set_level(GPIO_NUM_18, 0); sleepMs(20);
    gpio_set_level(GPIO_NUM_18, 1); sleepMs(120);
    for (const auto& c : init_commands) {
        // Defer display-on and RAM-write until a complete game frame exists.
        if (c.cmd == 0x29 || c.cmd == 0x2c) continue;
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_, c.cmd, c.data, c.size));
        if (c.delay) sleepMs(c.delay);
    }
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_, 0x20, nullptr, 0));
    const uint8_t madctl = 0xe8;
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_, 0x36, &madctl, 1));
    return true;
}
uint32_t OstbPlatform::millis() const { return uint32_t(esp_timer_get_time() / 1000); }
uint32_t OstbPlatform::micros() const { return uint32_t(esp_timer_get_time()); }
void OstbPlatform::sleepMs(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1)); }
void OstbPlatform::write(const char* text, size_t n) { if (text) std::fwrite(text, 1, n, stdout); }
void OstbPlatform::flush() { std::fflush(stdout); }
void OstbPlatform::update() {
    static int previousPower = -1;
    const int rawPower = gpio_get_level(POWER_BUTTON_GPIO);
    if (rawPower != previousPower) {
        ESP_LOGI("OSTB", "Power GPIO1 level=%d", rawPower);
        previousPower = rawPower;
    }
    const auto action = powerButton_.update(rawPower == POWER_BUTTON_ACTIVE_LEVEL, millis());
    if (action == Stickmon::PowerButton::Action::PRESSED) saveRequested_ = true;
    if (action == Stickmon::PowerButton::Action::SHUTDOWN) shutdown_ = true;
    if (action == Stickmon::PowerButton::Action::TOGGLE_LOCK) {
        locked_ = !locked_;
        blocked_[0] = blocked_[1] = true;
        applyBacklight();
        unlocked_ = !locked_;
        ESP_LOGI("OSTB", "Screen %s", locked_ ? "locked" : "unlocked");
    }
    const gpio_num_t pins[] = {GPIO_NUM_40, GPIO_NUM_39};
    for (int i = 0; i < 2; ++i) {
        const bool down = gpio_get_level(pins[i]) == 0;
        if (!down) blocked_[i] = false;
        buttons_[i] = down && !locked_ && !blocked_[i];
    }
    touch_.update(millis(), locked_ || powerButton_.down() || buttons_[0] || buttons_[1]);
    if (touch_.setupActive() && buttons_[1]) touch_.skipSetup();
}
bool OstbPlatform::pressed(Platform::InputButton key) const {
    size_t i = static_cast<size_t>(key);
    return i < 2 && buttons_[i];
}
Platform::FrameBuffer565 OstbPlatform::frameBuffer() { return {frame_, 296, 240, true}; }
void OstbPlatform::present() {
    if (locked_) return;
    const int64_t start = esp_timer_get_time();
    auto submit = [](void* context, int x0, int y0, int x1, int y1, const uint16_t* data) -> esp_err_t {
        auto io = static_cast<esp_lcd_panel_io_handle_t>(context);
        x0 += 24; x1 += 23; --y1;
        const uint8_t xs[] = {uint8_t(x0 >> 8), uint8_t(x0), uint8_t(x1 >> 8), uint8_t(x1)};
        const uint8_t ys[] = {uint8_t(y0 >> 8), uint8_t(y0), uint8_t(y1 >> 8), uint8_t(y1)};
        esp_err_t err = esp_lcd_panel_io_tx_param(io, 0x2a, xs, 4);
        if (err == ESP_OK) err = esp_lcd_panel_io_tx_param(io, 0x2b, ys, 4);
        if (err == ESP_OK) err = esp_lcd_panel_io_tx_color(io, 0x2c, data, (x1 - x0 + 1) * (y1 - y0 + 1) * 2);
        return err;
    };
    const auto err = Stickmon::submitRgb565Region(frame_, 296, 0, 0, 296, 240,
        strips_, 2, 32, completion_, submit, io_);
    // A timed-out DMA transfer may still own a strip. Never reuse it.
    if (err != ESP_OK) { ESP_LOGE("OSTB", "Frame transfer: %s", esp_err_to_name(err)); esp_restart(); return; }
    if (!firstFramePresented_) {
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_, 0x29, nullptr, 0));
        sleepMs(10);
        firstFramePresented_ = true;
        ESP_LOGI("OSTB", "First complete frame ready; enabling display backlight");
    }
    static unsigned frames = 0;
    if (++frames == 1 || frames % 300 == 0)
        ESP_LOGI("OSTB", "Frame %u transfer=%lld us; heap=%u PSRAM-free=%u", frames,
            esp_timer_get_time() - start, unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)), unsigned(externalMemoryFree()));
    applyBacklight();
}
void OstbPlatform::applyBacklight() {
    const uint32_t duty = (!firstFramePresented_ || locked_) ? 0 :
        (unsigned(brightness_) * 1023U + 127U) / 255U;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
void OstbPlatform::setBrightness(uint8_t v) { brightness_ = v; if (initialized_) applyBacklight(); }
uint8_t OstbPlatform::brightness() const { return brightness_; }
void OstbPlatform::sleep() { setBrightness(0); }
bool OstbPlatform::setAccelerationEnabled(bool) { return false; }
bool OstbPlatform::readAcceleration(float& x, float& y, float& z) { x = y = z = 0; return false; }
bool OstbPlatform::beginBattery() {
    // Factory PowerManager uses ADC_UNIT_2, CH6 (GPIO17), 12dB, 12 bits.
    adc_oneshot_unit_init_cfg_t unit{};
    unit.unit_id = ADC_UNIT_2;
    auto result = adc_oneshot_new_unit(&unit, &batteryAdc_);
    if (result != ESP_OK) {
        ESP_LOGW("OSTB", "Battery ADC create: %s", esp_err_to_name(result));
        return false;
    }
    adc_oneshot_chan_cfg_t channel{};
    channel.atten = ADC_ATTEN_DB_12;
    channel.bitwidth = ADC_BITWIDTH_12;
    result = adc_oneshot_config_channel(batteryAdc_, ADC_CHANNEL_6, &channel);
    if (result != ESP_OK) {
        adc_oneshot_del_unit(batteryAdc_);
        batteryAdc_ = nullptr;
        ESP_LOGW("OSTB", "Battery ADC channel: %s", esp_err_to_name(result));
        return false;
    }
    batteryLevel();
    return true;
}
int OstbPlatform::batteryLevel() {
    if (!batteryAdc_) return -1;
    const uint32_t now = millis();
    if (batterySampled_ && now - batterySampleMs_ < 10000) return batteryPercent_;
    batterySampled_ = true;
    batterySampleMs_ = now;
    int sum = 0;
    for (int i = 0; i < 8; ++i) {
        int raw = 0;
        const auto result = adc_oneshot_read(batteryAdc_, ADC_CHANNEL_6, &raw);
        if (result != ESP_OK || batteryPercentFromRaw(raw) < 0) {
            batteryPercent_ = -1;
            batteryRaw_ = 0;
            ESP_LOGW("OSTB", "Battery ADC sample: %s raw=%d", esp_err_to_name(result), raw);
            return -1;
        }
        sum += raw;
    }
    const int mean = sum / 8;
    batteryRaw_ = batteryRaw_ ? (batteryRaw_ * 4 + mean) / 5 : mean;
    batteryPercent_ = batteryPercentFromRaw(batteryRaw_);
    ESP_LOGI("OSTB", "Battery ADC2 CH6 GPIO17 raw=%d filtered=%d estimated=%d%%",
        mean, batteryRaw_, batteryPercent_);
    return batteryPercent_;
}
Platform::PowerCapabilities OstbPlatform::capabilities() const { return {batteryAdc_ != nullptr, false, true, true}; }
Platform::WakeReason OstbPlatform::wakeReason() const {
    auto cause = esp_sleep_get_wakeup_cause();
    return cause == ESP_SLEEP_WAKEUP_TIMER ? Platform::WakeReason::TIMER :
        cause == ESP_SLEEP_WAKEUP_EXT1 ? Platform::WakeReason::EXTERNAL_SIGNAL : Platform::WakeReason::NORMAL;
}
uint32_t OstbPlatform::hardwareRandom() { return esp_random(); }
size_t OstbPlatform::externalMemorySize() const { return esp_psram_get_size(); }
size_t OstbPlatform::externalMemoryFree() const { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }
void OstbPlatform::restart() { esp_restart(); }
void OstbPlatform::enterDeepSleep(uint64_t timerUs, bool externalWake) {
    stop(); end(); setBrightness(0);
    gpio_set_level(GPIO_NUM_4, 0);
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_, 0x28, nullptr, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_, 0x10, nullptr, 0));
    sleepMs(120);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (timerUs) ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(timerUs));
    if (externalWake) ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << POWER_BUTTON_GPIO, ESP_EXT1_WAKEUP_ANY_HIGH));
    ESP_LOGI("OSTB", "Saved; deep sleep, wake on GPIO1 high"); flush();
    esp_deep_sleep_start();
}

bool OstbPlatform::beginI2c() {
    i2c_master_bus_config_t bus{};
    bus.i2c_port = I2C_NUM_0; bus.sda_io_num = GPIO_NUM_12; bus.scl_io_num = GPIO_NUM_11;
    bus.clk_source = I2C_CLK_SRC_DEFAULT; bus.glitch_ignore_cnt = 7; bus.flags.enable_internal_pullup = true;
    return i2c_new_master_bus(&bus, &i2cBus_) == ESP_OK;
}

bool OstbPlatform::beginAudio() {
    if (!i2cBus_ || i2c_master_probe(i2cBus_, ES8311_CODEC_DEFAULT_ADDR >> 1, 100) != ESP_OK) return false;
    i2s_chan_handle_t tx = nullptr;
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    if (i2s_new_channel(&channel, &tx, nullptr) != ESP_OK) return false;
    i2s_std_config_t standard{};
    standard.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(PcmMixer::OUTPUT_SAMPLE_RATE);
    standard.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
    standard.gpio_cfg.mclk = GPIO_NUM_5; standard.gpio_cfg.bclk = GPIO_NUM_15;
    standard.gpio_cfg.ws = GPIO_NUM_16; standard.gpio_cfg.dout = GPIO_NUM_6;
    standard.gpio_cfg.din = I2S_GPIO_UNUSED;
    if (i2s_channel_init_std_mode(tx, &standard) != ESP_OK) return false;
    if (i2s_channel_enable(tx) != ESP_OK) return false;
    audio_codec_i2s_cfg_t dataCfg{}; dataCfg.tx_handle = tx;
    const auto* data = audio_codec_new_i2s_data(&dataCfg);
    audio_codec_i2c_cfg_t ctrlCfg{}; ctrlCfg.addr = ES8311_CODEC_DEFAULT_ADDR; ctrlCfg.bus_handle = i2cBus_;
    const auto* control = audio_codec_new_i2c_ctrl(&ctrlCfg);
    es8311_codec_cfg_t codecCfg{};
    codecCfg.ctrl_if = control; codecCfg.gpio_if = audio_codec_new_gpio();
    codecCfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    codecCfg.pa_pin = 4; codecCfg.use_mclk = true;
    const auto* codec = es8311_codec_new(&codecCfg);
    esp_codec_dev_cfg_t cfg{}; cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT;
    cfg.codec_if = codec; cfg.data_if = data;
    speakerCodec_ = esp_codec_dev_new(&cfg);
    if (!speakerCodec_) return false;
    esp_codec_dev_sample_info_t info{};
    info.bits_per_sample = 16; info.channel = 1; info.sample_rate = PcmMixer::OUTPUT_SAMPLE_RATE;
    if (esp_codec_dev_open(speakerCodec_, &info) != ESP_CODEC_DEV_OK) { speakerCodec_ = nullptr; return false; }
    esp_codec_dev_set_out_vol(speakerCodec_, volume_);
    audioMutex_ = xSemaphoreCreateMutex();
    if (!audioMutex_) return false;
    ESP_LOGI("OSTB", "ES8311 speaker ready, 16kHz mono; microphone not enabled");
    return xTaskCreatePinnedToCore(audioTaskEntry, "ostb_audio", 4096, this, 2, nullptr, 1) == pdPASS;
}
void OstbPlatform::audioTaskEntry(void* ctx) { static_cast<OstbPlatform*>(ctx)->audioTask(); }
void OstbPlatform::audioTask() {
    int16_t pcm[PcmMixer::MIX_BLOCK_SAMPLES];
    bool wasActive = false;
    while (true) {
        xSemaphoreTake(audioMutex_, portMAX_DELAY);
        const bool active = mixer_.anyActive();
        if (active || wasActive) mixer_.mix(pcm, PcmMixer::MIX_BLOCK_SAMPLES);
        xSemaphoreGive(audioMutex_);
        if (active || wasActive) esp_codec_dev_write(speakerCodec_, pcm, sizeof(pcm));
        else sleepMs(8);
        wasActive = active;
    }
}
void OstbPlatform::setVolume(uint8_t v) {
    volume_ = v > 100 ? 100 : v;
    if (speakerCodec_) esp_codec_dev_set_out_vol(speakerCodec_, volume_);
}
uint8_t OstbPlatform::volume() const { return volume_; }
bool OstbPlatform::playPcmU8(const uint8_t* p, size_t n, uint32_t rate) { return playPcmU8Channel(p,n,rate,0,true); }
bool OstbPlatform::playPcmU8Channel(const uint8_t* p, size_t n, uint32_t rate, uint8_t ch, bool replace) {
    if (!audioMutex_ || !speakerCodec_ || ch >= CHANNEL_COUNT) return false;
    PcmMixer::Voice voice{p, n, rate, 100, false};
    xSemaphoreTake(audioMutex_, portMAX_DELAY);
    const bool ok = mixer_.start(static_cast<PcmMixer::Channel>(ch), voice,
        replace ? PcmMixer::StartMode::REPLACE : PcmMixer::StartMode::APPEND);
    xSemaphoreGive(audioMutex_);
    return ok;
}
void OstbPlatform::setChannelVolume(uint8_t ch, uint8_t v) {
    if (!audioMutex_ || ch >= CHANNEL_COUNT) return;
    xSemaphoreTake(audioMutex_, portMAX_DELAY);
    mixer_.setVolume(static_cast<PcmMixer::Channel>(ch), v > 100 ? 100 : v);
    xSemaphoreGive(audioMutex_);
}
bool OstbPlatform::playing() const {
    if (!audioMutex_) return false;
    xSemaphoreTake(audioMutex_, portMAX_DELAY);
    const bool value = mixer_.anyActive();
    xSemaphoreGive(audioMutex_); return value;
}
uint8_t OstbPlatform::queuedPcm(uint8_t ch) const {
    if (!audioMutex_ || ch >= CHANNEL_COUNT) return 0;
    xSemaphoreTake(audioMutex_, portMAX_DELAY);
    const auto n = mixer_.queuedBlocks(static_cast<PcmMixer::Channel>(ch));
    xSemaphoreGive(audioMutex_); return n;
}
void OstbPlatform::stop() {
    if (!audioMutex_) return;
    xSemaphoreTake(audioMutex_, portMAX_DELAY); mixer_.stopAll(); xSemaphoreGive(audioMutex_);
}
void OstbPlatform::stopChannel(uint8_t ch) {
    if (!audioMutex_ || ch >= CHANNEL_COUNT) return;
    xSemaphoreTake(audioMutex_, portMAX_DELAY);
    mixer_.stop(static_cast<PcmMixer::Channel>(ch)); xSemaphoreGive(audioMutex_);
}
bool OstbPlatform::beginMicrophone() { return false; }
void OstbPlatform::endMicrophone() {}
bool OstbPlatform::recordMicrophone(int16_t*, size_t, uint32_t) { return false; }
bool OstbPlatform::microphoneRecording() const { return false; }
bool OstbPlatform::microphoneActive() const { return false; }

bool OstbPlatform::initialize() {
    esp_err_t result = nvs_flash_init();
    // Never erase an existing NVS partition as an initialization fallback.
    if (result != ESP_OK) ESP_LOGW("OSTB", "Factory NVS unavailable: %s", esp_err_to_name(result));
    const auto gameResult = nvs_flash_init_partition("game_nvs");
    if (gameResult != ESP_OK) ESP_LOGE("OSTB", "Game NVS init: %s", esp_err_to_name(gameResult));
    if (gameResult != ESP_OK) return false;
    // Import any successful writes from the initial bring-up once. The
    // original factory partition remains intact and is never erased here.
    nvs_handle_t meta = 0;
    if (nvs_open_from_partition("game_nvs", "ostb_meta", NVS_READWRITE, &meta) != ESP_OK) return false;
    uint8_t imported = 0;
    if (nvs_get_u8(meta, "imported", &imported) != ESP_OK || !imported) {
        nvs_handle_t old = 0, dest = 0;
        bool ok = nvs_open_from_partition("game_nvs", "stickmon", NVS_READWRITE, &dest) == ESP_OK;
        if (ok && nvs_open("stickmon", NVS_READONLY, &old) == ESP_OK) {
            for (const char* key : {"state", "state_a", "state_b", "hatch", "encounters"}) {
                size_t existing = 0, bytes = 0;
                if (nvs_get_blob(dest, key, nullptr, &existing) == ESP_OK) continue;
                if (nvs_get_blob(old, key, nullptr, &bytes) != ESP_OK) continue;
                void* data = allocate(bytes, true);
                if (!data) { ok = false; break; }
                ok = nvs_get_blob(old, key, data, &bytes) == ESP_OK && nvs_set_blob(dest, key, data, bytes) == ESP_OK;
                release(data);
                if (!ok) break;
            }
            nvs_close(old);
        }
        if (dest) { if (ok) ok = nvs_commit(dest) == ESP_OK; nvs_close(dest); }
        if (ok) ok = nvs_set_u8(meta, "imported", 1) == ESP_OK && nvs_commit(meta) == ESP_OK;
        nvs_close(meta);
        return ok;
    }
    nvs_close(meta);
    return true;

}

size_t OstbPlatform::blobSize(const char* nameSpace, const char* key) {
    if (!nameSpace || !key) return 0;
    nvs_handle_t handle = 0;
    if (nvs_open_from_partition("game_nvs", nameSpace, NVS_READONLY, &handle) != ESP_OK) return 0;
    size_t length = 0;
    esp_err_t result = nvs_get_blob(handle, key, nullptr, &length);
    nvs_close(handle);
    return result == ESP_OK ? length : 0;
}

bool OstbPlatform::readBlob(const char* nameSpace, const char* key,
                              void* output, size_t length) {
    if (!nameSpace || !key || !output || length == 0) return false;
    nvs_handle_t handle = 0;
    if (nvs_open_from_partition("game_nvs", nameSpace, NVS_READONLY, &handle) != ESP_OK) return false;
    size_t storedLength = length;
    esp_err_t result = nvs_get_blob(handle, key, output, &storedLength);
    nvs_close(handle);
    return result == ESP_OK && storedLength == length;
}

bool OstbPlatform::writeBlob(const char* nameSpace, const char* key,
                               const void* data, size_t length) {
    if (!nameSpace || !key || !data || length == 0) return false;
    nvs_handle_t handle = 0;
    if (nvs_open_from_partition("game_nvs", nameSpace, NVS_READWRITE, &handle) != ESP_OK) return false;
    esp_err_t result = nvs_set_blob(handle, key, data, length);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    if (result != ESP_OK) ESP_LOGE("OSTB", "Game blob write: %s", esp_err_to_name(result));
    return result == ESP_OK;
}

bool OstbPlatform::removeBlob(const char* nameSpace, const char* key) {
    if (!nameSpace || !key) return false;
    nvs_handle_t handle = 0;
    if (nvs_open_from_partition("game_nvs", nameSpace, NVS_READWRITE, &handle) != ESP_OK) return false;
    esp_err_t result = nvs_erase_key(handle, key);
    if (result == ESP_ERR_NVS_NOT_FOUND) result = ESP_OK;
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result == ESP_OK;
}

bool OstbPlatform::clearNamespace(const char* nameSpace) {
    if (!nameSpace) return false;
    nvs_handle_t handle = 0;
    if (nvs_open_from_partition("game_nvs", nameSpace, NVS_READWRITE, &handle) != ESP_OK) return false;
    esp_err_t result = nvs_erase_all(handle);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result == ESP_OK;
}

bool OstbPlatform::mount() {
    if (resourcesMounted_) return true;
    esp_vfs_spiffs_conf_t config{};
    config.base_path = RESOURCE_BASE_PATH;
    config.partition_label = RESOURCE_PARTITION;
    config.max_files = 8;
    config.format_if_mount_failed = false;
    resourcesMounted_ = esp_vfs_spiffs_register(&config) == ESP_OK;
    return resourcesMounted_;
}

size_t OstbPlatform::totalBytes() const {
    size_t total = 0;
    size_t used = 0;
    return resourcesMounted_ &&
                   esp_spiffs_info(RESOURCE_PARTITION, &total, &used) == ESP_OK
               ? total
               : 0;
}

size_t OstbPlatform::usedBytes() const {
    size_t total = 0;
    size_t used = 0;
    return resourcesMounted_ &&
                   esp_spiffs_info(RESOURCE_PARTITION, &total, &used) == ESP_OK
               ? used
               : 0;
}

Platform::ResourceFile OstbPlatform::open(const char* path) {
    if (!resourcesMounted_ || !path || path[0] != '/' ||
        std::strstr(path, "..") != nullptr) {
        return {};
    }
    char fullPath[192];
    int written = std::snprintf(fullPath, sizeof(fullPath), "%s%s",
                                RESOURCE_BASE_PATH, path);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(fullPath)) return {};
    FILE* file = std::fopen(fullPath, "rb");
    if (!file) return {};
    auto* implementation = new (std::nothrow) SpiffsResourceFile(file);
    if (!implementation) {
        std::fclose(file);
        return {};
    }
    return Platform::ResourceFile(implementation);
}

void* OstbPlatform::allocate(size_t bytes, bool preferExternal) {
    if (bytes == 0) return nullptr;
    if (preferExternal) {
        void* memory = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (memory) return memory;
    }
    return heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
}

void OstbPlatform::release(void* memory) { heap_caps_free(memory); }
size_t OstbPlatform::externalFree() const { return externalMemoryFree(); }

bool OstbPlatform::enable() {
    if (peerTransportActive_) return true;
    if (!wifiInitialized_) {
        esp_err_t netif = esp_netif_init();
        if (netif != ESP_OK && netif != ESP_ERR_INVALID_STATE) return false;
        esp_err_t eventResult = esp_event_loop_create_default();
        if (eventResult != ESP_OK && eventResult != ESP_ERR_INVALID_STATE) return false;
        wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
        esp_err_t wifiInit = esp_wifi_init(&config);
        if (wifiInit != ESP_OK && wifiInit != ESP_ERR_WIFI_STATE) return false;
        wifiInitialized_ = true;
    }
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
        esp_wifi_start() != ESP_OK) {
        return false;
    }
    const esp_err_t disconnectResult = esp_wifi_disconnect();
    if ((disconnectResult != ESP_OK &&
         disconnectResult != ESP_ERR_WIFI_NOT_CONNECT) ||
        esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK ||
        esp_wifi_set_channel(EspNowRadioConfig::CHANNEL,
                             WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        return false;
    }
    if (esp_now_init() != ESP_OK ||
        esp_now_register_recv_cb(receivePeerPacket) != ESP_OK) {
        esp_now_deinit();
        return false;
    }
    resetPeerQueue();
    peerTransportActive_ = true;
    return true;
}
void OstbPlatform::end() {
    if (!peerTransportActive_) return;
    esp_now_unregister_recv_cb();
    esp_now_deinit();
    const esp_err_t stopped = esp_wifi_stop();
    ESP_LOGI("RadioPower", "Visit radio stopped: %s", esp_err_to_name(stopped));
    peerTransportActive_ = false;
    resetPeerQueue();
}
bool OstbPlatform::active() const { return peerTransportActive_; }
bool OstbPlatform::send(const uint8_t destination[6], const void* data,
                          size_t length) {
    if (!peerTransportActive_ || !destination || !data || length == 0 ||
        length > Platform::PeerPacket::MAX_PAYLOAD_BYTES) return false;
    if (!esp_now_is_peer_exist(destination)) {
        esp_now_peer_info_t peer{};
        std::memcpy(peer.peer_addr, destination, sizeof(peer.peer_addr));
        peer.channel = 0;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;
        if (esp_now_add_peer(&peer) != ESP_OK) return false;
    }
    return esp_now_send(destination, static_cast<const uint8_t*>(data), length) ==
           ESP_OK;
}
bool OstbPlatform::receive(Platform::PeerPacket& packet) {
    if (!peerTransportActive_) return false;
    portENTER_CRITICAL(&peerQueueMux);
    if (peerQueueTail == peerQueueHead) {
        portEXIT_CRITICAL(&peerQueueMux);
        return false;
    }
    packet = peerQueue[peerQueueTail];
    peerQueueTail = static_cast<uint8_t>(
        (peerQueueTail + 1U) % PEER_QUEUE_CAPACITY);
    portEXIT_CRITICAL(&peerQueueMux);
    return true;
}

}  // namespace Ostb
