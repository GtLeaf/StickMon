#pragma once

#include "FrameBufferDamage.h"
#include "HangDiag.h"
#include "Rgb565Transfer.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace AmoledV2 {

// Owns every LCD panel operation on one core. The display is started through
// run(), so the LCD SPI interrupt is installed on PANEL_CORE as well. IDF's
// spi_bus_lock hand-off is not safe when the ISR finishes a transfer on one core
// while a task starts the next bus acquire on the other: the ISR can exit
// without resuming the waiter, leaving the bus lock ownerless and the display
// blocked forever. Keeping the ISR and every acquirer on one core avoids that,
// while rendering stays on the UI core.
//
// Explore frames: at most one submitted frame. The UI owns the other
// framebuffer; this task owns the submitted source and both DMA strips until
// takeResult succeeds. No game state, renderer, assets or Canvas565 are
// accessed by this task.
class ExploreFramePipeline {
public:
    using Call = esp_err_t (*)(void* context);
    static constexpr BaseType_t PANEL_CORE = 1;

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

    // Must run before the display is started; on failure run() executes
    // inline and the caller's core owns the panel instead.
    bool start() {
        requests_ = xQueueCreate(2, sizeof(Request));
        results_ = xQueueCreate(1, sizeof(Result));
        callDone_ = xSemaphoreCreateBinary();
        callLock_ = xSemaphoreCreateMutex();
        if (requests_ && results_ && callDone_ && callLock_ &&
            xTaskCreatePinnedToCore(runTask, "explore_lcd", 4096, this, 3,
                                    &task_, PANEL_CORE) == pdPASS) {
            HangDiag::setWorker(task_);
            return true;
        }
        if (requests_) vQueueDelete(requests_);
        if (results_) vQueueDelete(results_);
        if (callDone_) vSemaphoreDelete(callDone_);
        if (callLock_) vSemaphoreDelete(callLock_);
        requests_ = results_ = nullptr;
        callDone_ = callLock_ = nullptr;
        task_ = nullptr;
        return false;
    }

    bool running() const { return task_ != nullptr; }

    // Enables explore frames once the DMA strips and completion exist.
    bool configureFrames(size_t stride, uint16_t* const* buffers, size_t bufferCount,
                         size_t stripRows, SemaphoreHandle_t completion,
                         Stickmon::SubmitPixels submit, void* context) {
        if (!running()) return false;
        stride_ = stride;
        buffers_ = buffers;
        bufferCount_ = bufferCount;
        stripRows_ = stripRows;
        completion_ = completion;
        submit_ = submit;
        context_ = context;
        return true;
    }

    // Executes a panel operation on the panel task and waits for its result.
    // Requests run in order, so a call queued behind an explore frame waits
    // for that frame to finish first.
    esp_err_t run(Call call, void* context) {
        if (!running() || xTaskGetCurrentTaskHandle() == task_) return call(context);
        xSemaphoreTake(callLock_, portMAX_DELAY);
        Request request;
        request.call = call;
        request.context = context;
        esp_err_t result = ESP_FAIL;
        if (xQueueSend(requests_, &request, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(callDone_, portMAX_DELAY);
            result = callResult_;
        }
        xSemaphoreGive(callLock_);
        return result;
    }

    bool submitFrame(const Frame& frame) {
        if (!requests_ || !submit_ || pending_) return false;
        Request request;
        request.frame = frame;
        if (xQueueSend(requests_, &request, 0) != pdTRUE) return false;
        pending_ = true;
        return true;
    }

    bool pending() const { return pending_; }

    bool takeResult(Result& result, TickType_t wait) {
        if (!pending_ || xQueueReceive(results_, &result, wait) != pdTRUE) return false;
        pending_ = false;
        return true;
    }

private:
    struct Request {
        Call call = nullptr;  // Null: transfer `frame` and post a Result.
        void* context = nullptr;
        Frame frame;
    };

    static void runTask(void* value) {
        auto& self = *static_cast<ExploreFramePipeline*>(value);
        Request request;
        for (;;) {
            HangDiag::markWorker(HangDiag::WORKER_WAIT);
            if (xQueueReceive(self.requests_, &request, portMAX_DELAY) != pdTRUE) continue;
            HangDiag::markWorker(HangDiag::WORKER_TRANSFER);
            if (request.call) {
                self.callResult_ = request.call(request.context);
                xSemaphoreGive(self.callDone_);
                continue;
            }
            const Frame& frame = request.frame;
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
            HangDiag::markWorker(HangDiag::WORKER_RESULT);
            xQueueSend(self.results_, &result, portMAX_DELAY);
        }
    }

    QueueHandle_t requests_ = nullptr, results_ = nullptr;
    SemaphoreHandle_t callDone_ = nullptr, callLock_ = nullptr;
    esp_err_t callResult_ = ESP_OK;
    TaskHandle_t task_ = nullptr;
    bool pending_ = false;
    size_t stride_ = 0, bufferCount_ = 0, stripRows_ = 0;
    uint16_t* const* buffers_ = nullptr;
    SemaphoreHandle_t completion_ = nullptr;
    Stickmon::SubmitPixels submit_ = nullptr;
    void* context_ = nullptr;
};

}  // namespace AmoledV2
