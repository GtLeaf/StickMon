// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <set>
#include <vector>

using esp_err_t = int;
constexpr int ESP_OK=0, ESP_FAIL=-1, ESP_ERR_INVALID_STATE=1, ESP_ERR_WIFI_NOT_CONNECT=2;
inline const char* esp_err_to_name(int) { return "fake"; }
constexpr int pdTRUE=1, pdPASS=1, portMAX_DELAY=-1;
#define pdMS_TO_TICKS(value) (value)
namespace Fake {
inline int failAt=0, step=0;
inline bool shouldFail() { return failAt && ++step==failAt; }
struct StopTask {};
inline bool runningTask=false;
inline int taskIterations=0;
inline void (*task)(void*)=nullptr;
inline void* taskContext=nullptr;
inline bool pa=false, wifi=false, now=false;
inline int i2sWrites=0, stereoRate=0, storage=-1, channel=-1, microphoneReads=0;
inline bool nvsEnabled=true, failWrite=false, failRead=false;
inline std::vector<int16_t> lastWrite;
inline std::set<std::vector<uint8_t>> peers;
inline void runAudio(int iterations=1) {
    taskIterations=iterations; runningTask=true;
    try { task(taskContext); } catch (StopTask&) {}
    runningTask=false;
}
}
using SemaphoreHandle_t=int*;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { static int mutex; return &mutex; }
inline void xSemaphoreTake(SemaphoreHandle_t, int) {
    if (Fake::runningTask && Fake::taskIterations--<=0) throw Fake::StopTask{};
}
inline void xSemaphoreGive(SemaphoreHandle_t) {}
inline int xTaskCreatePinnedToCore(void (*task)(void*),const char*,int,void* context,int,void*,int) {
    Fake::task=task; Fake::taskContext=context; return pdPASS;
}
inline void vTaskDelay(int) {}
struct FakeQueue { size_t size, capacity; std::deque<std::vector<uint8_t>> data; };
using QueueHandle_t=FakeQueue*;
inline QueueHandle_t xQueueCreate(int capacity,size_t size) {
    static FakeQueue queue; queue={size,size_t(capacity),{}}; return &queue;
}
inline void xQueueReset(QueueHandle_t q) { q->data.clear(); }
inline int xQueueSend(QueueHandle_t q,const void* data,int) {
    if(q->data.size()==q->capacity)return 0;
    const auto* bytes=static_cast<const uint8_t*>(data);q->data.emplace_back(bytes,bytes+q->size);return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t q,void* data,int) {
    if(q->data.empty())return 0;
    std::memcpy(data,q->data.front().data(),q->size);q->data.pop_front();return pdTRUE;
}
constexpr int GPIO_MODE_OUTPUT=1, GPIO_NUM_NC=-1;
constexpr int BOARD_AUDIO_PA_EN_GPIO=3, BOARD_AUDIO_PA_EN_ACTIVE_LEVEL=1;
constexpr int BOARD_SPK_I2S_GPIO_MCLK=15, BOARD_SPK_I2S_GPIO_BCLK=48;
constexpr int BOARD_SPK_I2S_GPIO_WS=38, BOARD_SPK_I2S_GPIO_DOUT=47;
constexpr int BOARD_MIC_I2S_GPIO_BCLK=16, BOARD_MIC_I2S_GPIO_WS=2, BOARD_MIC_I2S_GPIO_DIN=17;
struct gpio_config_t { uint64_t pin_bit_mask; int mode; };
inline int gpio_config(const gpio_config_t*) { return ESP_OK; }
inline int gpio_set_level(int pin,int level) { assert(pin==3);Fake::pa=level;return ESP_OK; }
inline int gpio_hold_dis(int) { return ESP_OK; }
constexpr int I2S_NUM_0=0,I2S_NUM_1=1,I2S_ROLE_MASTER=1;
constexpr int I2S_DATA_BIT_WIDTH_16BIT=16,I2S_DATA_BIT_WIDTH_32BIT=32;
constexpr int I2S_SLOT_MODE_STEREO=2,I2S_SLOT_MODE_MONO=1,I2S_STD_SLOT_LEFT=1,I2S_MCLK_MULTIPLE_384=384;
struct i2s_chan_config_t { int id,role; bool auto_clear=false; int dma_desc_num=0,dma_frame_num=0; };
#define I2S_CHANNEL_DEFAULT_CONFIG(id,role) {id,role}
struct i2s_std_clk_config_t { uint32_t sample_rate_hz; int mclk_multiple=256; };
#define I2S_STD_CLK_DEFAULT_CONFIG(rate) {rate}
struct i2s_std_slot_config_t { int width,mode,slot_mask=0; };
#define I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(width,mode) {width,mode}
struct i2s_std_config_t {
    i2s_std_clk_config_t clk_cfg; i2s_std_slot_config_t slot_cfg;
    struct { int mclk,bclk,ws,dout,din; } gpio_cfg;
};
struct FakeI2s { bool enabled=false; int id=0; };
using i2s_chan_handle_t=FakeI2s*;
inline FakeI2s fakeChannels[2];
inline int i2s_new_channel(const i2s_chan_config_t* cfg,i2s_chan_handle_t* tx,i2s_chan_handle_t* rx) {
    auto* ch=&fakeChannels[cfg->id];ch->id=cfg->id;
    if(tx)*tx=ch;if(rx)*rx=ch;return ESP_OK;
}
inline int i2s_channel_init_std_mode(i2s_chan_handle_t ch,const i2s_std_config_t* config) {
    if(ch->id==0) {
        assert(config->slot_cfg.width==16 && config->slot_cfg.mode==2);
        assert(config->gpio_cfg.bclk==48 && config->gpio_cfg.ws==38 && config->gpio_cfg.dout==47);
        Fake::stereoRate=config->clk_cfg.sample_rate_hz;
    } else assert(config->slot_cfg.width==32 && config->slot_cfg.mode==1 && config->gpio_cfg.din==17);
    return ESP_OK;
}
inline int i2s_del_channel(i2s_chan_handle_t) { return ESP_OK; }
inline int i2s_channel_enable(i2s_chan_handle_t ch) { ch->enabled=true;return ESP_OK; }
inline int i2s_channel_disable(i2s_chan_handle_t ch) { ch->enabled=false;return ESP_OK; }
inline int i2s_channel_reconfig_std_clock(i2s_chan_handle_t ch,const i2s_std_clk_config_t*) {
    assert(!ch->enabled);return ESP_OK;
}
inline int i2s_channel_write(i2s_chan_handle_t ch,const void* data,size_t bytes,size_t* written,uint32_t) {
    assert(ch->enabled);++Fake::i2sWrites;
    const auto* samples=static_cast<const int16_t*>(data);
    Fake::lastWrite.assign(samples,samples+bytes/2);
    *written=Fake::failWrite?0:bytes;return Fake::failWrite?ESP_FAIL:ESP_OK;
}
inline int i2s_channel_read(i2s_chan_handle_t ch,void* data,size_t bytes,size_t* read,uint32_t) {
    assert(ch->enabled);++Fake::microphoneReads;
    auto* samples=static_cast<int32_t*>(data);
    for(size_t i=0;i<bytes/4;++i)samples[i]=(i%2?std::numeric_limits<int32_t>::min():std::numeric_limits<int32_t>::max());
    *read=Fake::failRead?0:bytes;return Fake::failRead?ESP_FAIL:ESP_OK;
}
struct esp_now_recv_info_t { uint8_t* src_addr; };
using FakeReceive=void(*)(const esp_now_recv_info_t*,const uint8_t*,int);
inline FakeReceive fakeReceive=nullptr;
struct esp_now_peer_info_t { uint8_t peer_addr[6]{};int channel=0,ifidx=0;bool encrypt=false; };
inline int esp_netif_init() { return Fake::shouldFail()?ESP_FAIL:ESP_OK; }
inline int esp_event_loop_create_default() { return Fake::shouldFail()?ESP_FAIL:ESP_OK; }
struct wifi_init_config_t { bool nvs_enable=true; };
#define WIFI_INIT_CONFIG_DEFAULT() {}
constexpr int WIFI_STORAGE_RAM=1,WIFI_MODE_STA=1,WIFI_PS_NONE=0,WIFI_SECOND_CHAN_NONE=0,WIFI_IF_STA=0;
inline int esp_wifi_init(const wifi_init_config_t* config) {
    Fake::nvsEnabled=config->nvs_enable;return Fake::shouldFail()?ESP_FAIL:ESP_OK;
}
inline int esp_wifi_set_storage(int value) { Fake::storage=value;return Fake::shouldFail()?ESP_FAIL:ESP_OK; }
inline int esp_wifi_set_mode(int) { return Fake::shouldFail()?ESP_FAIL:ESP_OK; }
inline int esp_wifi_start() { if(Fake::shouldFail())return ESP_FAIL;Fake::wifi=true;return ESP_OK; }
inline int esp_wifi_disconnect() { return Fake::shouldFail()?ESP_FAIL:ESP_ERR_WIFI_NOT_CONNECT; }
inline int esp_wifi_set_ps(int) { return Fake::shouldFail()?ESP_FAIL:ESP_OK; }
inline int esp_wifi_set_channel(int channel,int) { Fake::channel=channel;return Fake::shouldFail()?ESP_FAIL:ESP_OK; }
inline int esp_wifi_stop() { Fake::wifi=false;return ESP_OK; }
inline int esp_now_init() { if(Fake::shouldFail())return ESP_FAIL;Fake::now=true;return ESP_OK; }
inline int esp_now_deinit() { Fake::now=false;Fake::peers.clear();return ESP_OK; }
inline int esp_now_register_recv_cb(FakeReceive cb) {
    if(Fake::shouldFail())return ESP_FAIL;fakeReceive=cb;return ESP_OK;
}
inline int esp_now_unregister_recv_cb() { fakeReceive=nullptr;return ESP_OK; }
inline bool esp_now_is_peer_exist(const uint8_t* dest) { return Fake::peers.count({dest,dest+6}); }
inline int esp_now_add_peer(const esp_now_peer_info_t* peer) {
    assert(!peer->encrypt && peer->channel==0);Fake::peers.insert({peer->peer_addr,peer->peer_addr+6});return ESP_OK;
}
inline int esp_now_send(const uint8_t*,const uint8_t*,size_t) { return Fake::now?ESP_OK:ESP_FAIL; }

namespace Fake {
inline bool adcFailUnit=false, adcFailChannel=false, adcFailCalibration=false, adcFailRead=false;
inline bool adcFailConvert=false;
inline int adcCreated=0, adcDeleted=0, calCreated=0, calDeleted=0, adcReads=0;
inline int adcMv=1200;
inline std::deque<int> adcSamples;
}
using adc_oneshot_unit_handle_t=int*;
using adc_cali_handle_t=int*;
constexpr int ADC_UNIT_1=1, ADC_CHANNEL_7=7, ADC_ATTEN_DB_12=12, ADC_BITWIDTH_12=12;
struct adc_oneshot_unit_init_cfg_t { int unit_id; };
struct adc_oneshot_chan_cfg_t { int atten, bitwidth; };
struct adc_cali_curve_fitting_config_t { int unit_id,chan,atten,bitwidth; };
inline int adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t* cfg,adc_oneshot_unit_handle_t* out) {
    assert(cfg->unit_id==1);
    if(Fake::adcFailUnit)return ESP_FAIL;
    static int unit;*out=&unit;++Fake::adcCreated;return ESP_OK;
}
inline int adc_oneshot_del_unit(adc_oneshot_unit_handle_t) { ++Fake::adcDeleted;return ESP_OK; }
inline int adc_oneshot_config_channel(adc_oneshot_unit_handle_t,int ch,const adc_oneshot_chan_cfg_t* cfg) {
    assert(ch==7 && cfg->atten==12 && cfg->bitwidth==12);return Fake::adcFailChannel?ESP_FAIL:ESP_OK;
}
inline int adc_cali_create_scheme_curve_fitting(const adc_cali_curve_fitting_config_t* cfg,adc_cali_handle_t* out) {
    assert(cfg->unit_id==1 && cfg->chan==7 && cfg->atten==12 && cfg->bitwidth==12);
    if(Fake::adcFailCalibration)return ESP_FAIL;
    static int calibration;*out=&calibration;++Fake::calCreated;return ESP_OK;
}
inline int adc_cali_delete_scheme_curve_fitting(adc_cali_handle_t) { ++Fake::calDeleted;return ESP_OK; }
inline int adc_oneshot_read(adc_oneshot_unit_handle_t,int ch,int* raw) {
    assert(ch==7);++Fake::adcReads;
    if(Fake::adcFailRead)return ESP_FAIL;
    *raw=Fake::adcMv;
    if(!Fake::adcSamples.empty()) { *raw=Fake::adcSamples.front();Fake::adcSamples.pop_front(); }
    return ESP_OK;
}
inline int adc_cali_raw_to_voltage(adc_cali_handle_t,int raw,int* mv) {
    if(Fake::adcFailConvert)return ESP_FAIL;
    *mv=raw;return ESP_OK;
}
