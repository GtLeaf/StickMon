#pragma once

#include <cstddef>
#include <cstdint>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace Stickmon {

using SubmitPixels = esp_err_t (*)(void*, int, int, int, int, const uint16_t*);

// Bounds are exclusive. Frame pixels already have the panel's byte order.
// The caller owns DMA buffers and a counting semaphore signalled once per
// completed transfer. Each buffer must hold stride * stripRows pixels; the
// source must contain at least yEnd rows. Callers clamp to their panel bounds.
// Successful return guarantees all buffers are idle. A timeout requires a
// restart or cancellation before either the source or buffers can be reused.
esp_err_t submitRgb565Region(const uint16_t* pixels, size_t stride,
                            int xBegin, int yBegin, int xEnd, int yEnd,
                            uint16_t* const* buffers, size_t bufferCount,
                            size_t stripRows, SemaphoreHandle_t completion,
                            SubmitPixels submit, void* context);

}  // namespace Stickmon
