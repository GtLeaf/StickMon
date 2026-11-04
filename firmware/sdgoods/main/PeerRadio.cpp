// SPDX-License-Identifier: Apache-2.0
#include "PeerRadio.h"
#include <cstring>
#include "hardware/EspNowRadioConfig.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

namespace Sdgoods {
PeerRadio* PeerRadio::instance_ = nullptr;
void PeerRadio::received(const esp_now_recv_info_t* info, const uint8_t* data, int length) {
    auto* self = instance_;
    if (!self || !self->active_ || !info || !data || length <= 0 ||
        size_t(length) > Platform::PeerPacket::MAX_PAYLOAD_BYTES) return;
    Platform::PeerPacket packet{};
    std::memcpy(packet.source, info->src_addr, 6);
    std::memcpy(packet.payload, data, length);
    packet.length = length;
    // The Wi-Fi callback must never block; the shared link retries lost packets.
    xQueueSend(self->queue_, &packet, 0);
}
bool PeerRadio::enable() {
    if (active_) return true;
    if (!queue_) queue_ = xQueueCreate(8, sizeof(Platform::PeerPacket));
    if (!queue_) return false;
    if (!initialized_) {
        auto result = esp_netif_init();
        if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return false;
        result = esp_event_loop_create_default();
        if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return false;
        wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
        config.nvs_enable = false;
        if (esp_wifi_init(&config) != ESP_OK) return false;
        initialized_ = true;
    }
    auto fail = [this]() { end(); return false; };
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK ||
        esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK || esp_wifi_start() != ESP_OK) return fail();
    wifiStarted_ = true;
    auto result = esp_wifi_disconnect();
    if ((result != ESP_OK && result != ESP_ERR_WIFI_NOT_CONNECT) ||
        esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK ||
        esp_wifi_set_channel(EspNowRadioConfig::CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) return fail();
    if (esp_now_init() != ESP_OK) return fail();
    nowStarted_ = true;
    xQueueReset(queue_);
    instance_ = this;
    if (esp_now_register_recv_cb(received) != ESP_OK) return fail();
    active_ = true;
    Platform::logf("[SDGOODS] ESP-NOW ready channel=%u; RAM Wi-Fi settings\n", EspNowRadioConfig::CHANNEL);
    return true;
}
void PeerRadio::end() {
    active_ = false;
    if (nowStarted_) {
        esp_now_unregister_recv_cb();
        esp_now_deinit();
        nowStarted_ = false;
    }
    if (wifiStarted_) {
        const auto result = esp_wifi_stop();
        Platform::logf("[SDGOODS] visit radio stopped: %s\n", esp_err_to_name(result));
        wifiStarted_ = false;
    }
    if (queue_) xQueueReset(queue_);
}
bool PeerRadio::send(const uint8_t destination[6], const void* data, size_t length) {
    if (!active_ || !destination || !data || !length || length > Platform::PeerPacket::MAX_PAYLOAD_BYTES) return false;
    if (!esp_now_is_peer_exist(destination)) {
        esp_now_peer_info_t peer{};
        std::memcpy(peer.peer_addr, destination, 6);
        peer.channel = 0;
        peer.ifidx = WIFI_IF_STA;
        if (esp_now_add_peer(&peer) != ESP_OK) return false;
    }
    return esp_now_send(destination, static_cast<const uint8_t*>(data), length) == ESP_OK;
}
bool PeerRadio::receive(Platform::PeerPacket& packet) {
    return active_ && xQueueReceive(queue_, &packet, 0) == pdTRUE;
}
}  // namespace Sdgoods
