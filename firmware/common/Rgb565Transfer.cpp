#include "Rgb565Transfer.h"

#include <algorithm>
#include <cstring>

namespace Stickmon {

esp_err_t submitRgb565Region(const uint16_t* pixels, size_t stride,
                            int xBegin, int yBegin, int xEnd, int yEnd,
                            uint16_t* const* buffers, size_t bufferCount,
                            size_t stripRows, SemaphoreHandle_t completion,
                            SubmitPixels submit, void* context) {
    if (!pixels || !buffers || !bufferCount || !stripRows || !completion ||
        !submit || xBegin < 0 || yBegin < 0 || xEnd < xBegin || yEnd < yBegin ||
        static_cast<size_t>(xEnd) > stride) return ESP_ERR_INVALID_ARG;
    if (xBegin == xEnd || yBegin == yEnd) return ESP_OK;
    for (size_t i = 0; i < bufferCount; ++i)
        if (!buffers[i]) return ESP_ERR_INVALID_ARG;
    const size_t width = static_cast<size_t>(xEnd - xBegin);
    size_t pending = 0, next = 0;
    esp_err_t result = ESP_OK;
    for (int y = yBegin; result == ESP_OK && y < yEnd;) {
        if (pending == bufferCount) {
            if (xSemaphoreTake(completion, pdMS_TO_TICKS(1000)) != pdTRUE)
                return ESP_ERR_TIMEOUT;
            --pending;
        }
        const size_t rows = std::min(stripRows, static_cast<size_t>(yEnd - y));
        for (size_t row = 0; row < rows; ++row)
            std::memcpy(buffers[next] + row * width,
                        pixels + (static_cast<size_t>(y) + row) * stride + xBegin,
                        width * sizeof(uint16_t));
        result = submit(context, xBegin, y, xEnd, y + rows, buffers[next]);
        if (result == ESP_OK) {
            ++pending;
            next = (next + 1) % bufferCount;
        }
        y += rows;
    }
    while (pending) {
        if (xSemaphoreTake(completion, pdMS_TO_TICKS(1000)) != pdTRUE)
            return ESP_ERR_TIMEOUT;
        --pending;
    }
    return result;
}

}  // namespace Stickmon
