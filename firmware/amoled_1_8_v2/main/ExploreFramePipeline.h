#pragma once

#include "FrameBufferDamage.h"
#include "Rgb565Transfer.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace AmoledV2 {

// At most one submitted frame. The UI owns the other framebuffer; this task
// owns the submitted source and both DMA strips until takeResult succeeds.
// No game state, renderer, assets or Canvas565 are accessed by this task.
class ExploreFramePipeline {
public:
    struct Frame {
        const uint16_t* pixels = nullptr;
        Stickmon::FrameRect bounds;
        int64_t startedUs = 0;
        uint32_t updateUs = 0, clawUs = 0, drawUs = 0;
        bool profile = false;
    };
    struct Result {
        Frame frame;
        esp_err_t error = ESP_OK;
        int64_t finishedUs = 0;
        uint32_t transferUs = 0;
    };

    bool begin(size_t stride, uint16_t* const* buffers, size_t bufferCount,
               size_t stripRows, SemaphoreHandle_t completion,
               Stickmon::SubmitPixels submit, void* context) {
        stride_ = stride;
        buffers_ = buffers;
        bufferCount_ = bufferCount;
        stripRows_ = stripRows;
        completion_ = completion;
        submit_ = submit;
        context_ = context;
        requests_ = xQueueCreate(1, sizeof(Frame));
        results_ = xQueueCreate(1, sizeof(Result));
        if (requests_ && results_ &&
            xTaskCreatePinnedToCore(run, "explore_lcd", 3072, this, 3,
                                    nullptr, 1) == pdPASS) return true;
        if (requests_) vQueueDelete(requests_);
        if (results_) vQueueDelete(results_);
        requests_ = results_ = nullptr;
        return false;
    }

    bool submitFrame(const Frame& frame) {
        if (!requests_ || pending_) return false;
        if (xQueueSend(requests_, &frame, 0) != pdTRUE) return false;
        pending_ = true;
        return true;
    }

    bool takeResult(Result& result, bool wait) {
        if (!pending_ || xQueueReceive(results_, &result,
                                      wait ? portMAX_DELAY : 0) != pdTRUE) return false;
        pending_ = false;
        return true;
    }

private:
    static void run(void* value) {
        auto& self = *static_cast<ExploreFramePipeline*>(value);
        Frame frame;
        for (;;) {
            if (xQueueReceive(self.requests_, &frame, portMAX_DELAY) != pdTRUE) continue;
            Result result;
            result.frame = frame;
            const int64_t started = esp_timer_get_time();
            result.error = Stickmon::submitRgb565Region(
                frame.pixels, self.stride_, frame.bounds.left, frame.bounds.top,
                frame.bounds.right, frame.bounds.bottom, self.buffers_,
                self.bufferCount_, self.stripRows_, self.completion_,
                self.submit_, self.context_);
            result.finishedUs = esp_timer_get_time();
            result.transferUs = static_cast<uint32_t>(result.finishedUs - started);
            xQueueSend(self.results_, &result, portMAX_DELAY);
        }
    }

    QueueHandle_t requests_ = nullptr, results_ = nullptr;
    bool pending_ = false;
    size_t stride_ = 0, bufferCount_ = 0, stripRows_ = 0;
    uint16_t* const* buffers_ = nullptr;
    SemaphoreHandle_t completion_ = nullptr;
    Stickmon::SubmitPixels submit_ = nullptr;
    void* context_ = nullptr;
};

}  // namespace AmoledV2
