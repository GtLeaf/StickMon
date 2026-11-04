// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <atomic>
#include "platform/api/PlatformServices.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace Sdgoods {
class PeerRadio final : public Platform::IPeerTransport {
public:
    bool enable() override;
    void end() override;
    bool active() const override { return active_; }
    bool send(const uint8_t[6], const void*, size_t) override;
    bool receive(Platform::PeerPacket&) override;
private:
    static void received(const esp_now_recv_info_t*, const uint8_t*, int);
    static PeerRadio* instance_;
    QueueHandle_t queue_ = nullptr;
    std::atomic<bool> active_{false};
    bool initialized_ = false, wifiStarted_ = false, nowStarted_ = false;
};
}  // namespace Sdgoods
