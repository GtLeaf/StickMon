#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "AmoledApp.h"
#include "AmoledPlatform.h"
#include "HomeScreen.h"
#include "TouchInput.h"
#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_io_expander.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "presentation/Canvas565.h"
#include "presentation/PixelRenderer.h"

namespace {

constexpr char TAG[] = "StickMon";
constexpr uint16_t LOGICAL_WIDTH = 184;
constexpr uint16_t LOGICAL_HEIGHT = 224;
constexpr uint16_t PHYSICAL_WIDTH = 368;
constexpr uint16_t PHYSICAL_HEIGHT = 448;
constexpr uint16_t TRANSFER_LOGICAL_ROWS = 32;
constexpr uint16_t TRANSFER_PHYSICAL_ROWS = TRANSFER_LOGICAL_ROWS * 2;
constexpr size_t TRANSFER_BUFFER_COUNT = 2;
constexpr size_t LOGICAL_PIXELS =
    static_cast<size_t>(LOGICAL_WIDTH) * LOGICAL_HEIGHT;
constexpr size_t TRANSFER_PIXELS =
    static_cast<size_t>(PHYSICAL_WIDTH) * TRANSFER_PHYSICAL_ROWS;
using TransferBuffers = std::array<uint16_t*, TRANSFER_BUFFER_COUNT>;

uint16_t* allocateLogicalPixels() {
    return static_cast<uint16_t*>(heap_caps_calloc(
        LOGICAL_PIXELS, sizeof(uint16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

uint16_t* allocateTransferPixels() {
    return static_cast<uint16_t*>(heap_caps_malloc(
        TRANSFER_PIXELS * sizeof(uint16_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
}

esp_err_t resetBoardPeripherals() {
    esp_io_expander_handle_t expander = bsp_io_expander_init();
    if (!expander) return ESP_FAIL;

    constexpr uint32_t RESET_MASK =
        IO_EXPANDER_PIN_NUM_0 |
        IO_EXPANDER_PIN_NUM_1 |
        IO_EXPANDER_PIN_NUM_2;
    ESP_RETURN_ON_ERROR(
        esp_io_expander_set_dir(expander, RESET_MASK, IO_EXPANDER_OUTPUT),
        TAG, "TCA9554 reset pin configuration failed");
    ESP_RETURN_ON_ERROR(
        esp_io_expander_set_level(expander, RESET_MASK, 0),
        TAG, "TCA9554 reset assertion failed");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(
        esp_io_expander_set_level(expander, RESET_MASK, 1),
        TAG, "TCA9554 reset release failed");
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

void scaleStripe2x(const uint16_t* source, uint16_t sourceY,
                   uint16_t logicalRows, uint16_t* destination) {
    for (uint16_t row = 0; row < logicalRows; ++row) {
        const uint16_t* sourceRow =
            source + static_cast<size_t>(sourceY + row) * LOGICAL_WIDTH;
        uint16_t* top =
            destination + static_cast<size_t>(row * 2U) * PHYSICAL_WIDTH;
        uint16_t* bottom = top + PHYSICAL_WIDTH;
        for (uint16_t x = 0; x < LOGICAL_WIDTH; ++x) {
            uint16_t pixel = sourceRow[x];
            size_t outputX = static_cast<size_t>(x) * 2U;
            top[outputX] = pixel;
            top[outputX + 1U] = pixel;
            bottom[outputX] = pixel;
            bottom[outputX + 1U] = pixel;
        }
    }
}

bool onColorTransferDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*,
                         void* userContext) {
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(userContext),
                         &higherPriorityTaskWoken);
    return higherPriorityTaskWoken == pdTRUE;
}

esp_err_t startDisplay(esp_lcd_panel_handle_t* panel,
                       esp_lcd_panel_io_handle_t* io) {
    if (!panel || !io) return ESP_ERR_INVALID_ARG;

    bsp_display_config_t config{};
    config.max_transfer_sz = TRANSFER_PIXELS * sizeof(uint16_t);

    ESP_RETURN_ON_ERROR(bsp_display_new(&config, panel, io), TAG,
                        "SH8601 initialization failed");
    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG,
                        "brightness initialization failed");
    return bsp_display_brightness_set(72);
}

esp_err_t submitFrame(esp_lcd_panel_handle_t panel,
                      const uint16_t* logicalPixels,
                      const TransferBuffers& transferBuffers,
                      SemaphoreHandle_t transferDone,
                      uint16_t sourceBegin = 0,
                      uint16_t sourceEnd = LOGICAL_HEIGHT) {
    if (!transferDone) return ESP_ERR_INVALID_ARG;
    sourceBegin = std::min<uint16_t>(sourceBegin, LOGICAL_HEIGHT);
    sourceEnd = std::min<uint16_t>(sourceEnd, LOGICAL_HEIGHT);
    if (sourceBegin >= sourceEnd) return ESP_OK;
    esp_err_t result = ESP_OK;
    size_t pendingTransfers = 0;
    size_t nextBuffer = 0;

    for (uint16_t sourceY = sourceBegin;
         result == ESP_OK && sourceY < sourceEnd;
         sourceY += TRANSFER_LOGICAL_ROWS) {
        if (pendingTransfers == TRANSFER_BUFFER_COUNT) {
            if (xSemaphoreTake(transferDone, pdMS_TO_TICKS(1000)) != pdTRUE) {
                result = ESP_ERR_TIMEOUT;
                break;
            }
            --pendingTransfers;
        }

        uint16_t logicalRows = static_cast<uint16_t>(
            std::min<uint16_t>(TRANSFER_LOGICAL_ROWS, sourceEnd - sourceY));
        uint16_t physicalY = sourceY * 2U;
        uint16_t physicalRows = logicalRows * 2U;

        scaleStripe2x(logicalPixels, sourceY, logicalRows,
                      transferBuffers[nextBuffer]);
        result = esp_lcd_panel_draw_bitmap(
            panel, 0, physicalY, PHYSICAL_WIDTH,
            physicalY + physicalRows, transferBuffers[nextBuffer]);
        if (result == ESP_OK) {
            ++pendingTransfers;
            nextBuffer = (nextBuffer + 1U) % TRANSFER_BUFFER_COUNT;
        }
    }

    while (pendingTransfers > 0) {
        if (xSemaphoreTake(transferDone, pdMS_TO_TICKS(1000)) != pdTRUE) {
            if (result == ESP_OK) result = ESP_ERR_TIMEOUT;
            break;
        }
        --pendingTransfers;
    }

    return result;
}

uint32_t millisNow() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

}  // namespace

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Starting AMOLED V1 display milestone");
    ESP_LOGI(TAG, "PSRAM size: %u bytes",
             static_cast<unsigned>(esp_psram_get_size()));

    uint16_t* logicalPixels = allocateLogicalPixels();
    TransferBuffers transferBuffers{};
    bool transferBuffersReady = true;
    for (uint16_t*& buffer : transferBuffers) {
        buffer = allocateTransferPixels();
        transferBuffersReady = transferBuffersReady && buffer != nullptr;
    }
    if (!logicalPixels || !transferBuffersReady) {
        ESP_LOGE(TAG, "Unable to allocate RGB565 framebuffers");
        heap_caps_free(logicalPixels);
        for (uint16_t* buffer : transferBuffers) heap_caps_free(buffer);
        return;
    }
    ESP_LOGI(TAG, "DMA buffers: %u bytes, free DMA memory: %u bytes",
             static_cast<unsigned>(TRANSFER_PIXELS * sizeof(uint16_t) *
                                   TRANSFER_BUFFER_COUNT),
             static_cast<unsigned>(heap_caps_get_free_size(
                 MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)));

    esp_err_t result = resetBoardPeripherals();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Board peripheral reset failed: %s",
                 esp_err_to_name(result));
        return;
    }

    esp_lcd_panel_handle_t panel = nullptr;
    esp_lcd_panel_io_handle_t io = nullptr;
    result = startDisplay(&panel, &io);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Display start failed: %s", esp_err_to_name(result));
        return;
    }

    SemaphoreHandle_t transferDone =
        xSemaphoreCreateCounting(TRANSFER_BUFFER_COUNT, 0);
    if (!transferDone) {
        ESP_LOGE(TAG, "Unable to allocate display transfer semaphore");
        return;
    }
    esp_lcd_panel_io_callbacks_t callbacks{};
    callbacks.on_color_trans_done = onColorTransferDone;
    result = esp_lcd_panel_io_register_event_callbacks(
        io, &callbacks, transferDone);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Display callback setup failed: %s",
                 esp_err_to_name(result));
        return;
    }

    const Platform::FrameBuffer565 frameBuffer{
        logicalPixels, LOGICAL_WIDTH, LOGICAL_HEIGHT, true};
    Canvas565 canvas;
    canvas.attach(frameBuffer);
    AmoledV1::bindAmoledPlatform();
    PixelRenderer::bind(frameBuffer);
    AmoledV1::AmoledApp app;
    app.begin(millisNow());
    app.render(canvas);
    app.markRendered();

    ESP_LOGI(TAG, "Display pipeline: %u physical rows, %u DMA buffers",
             static_cast<unsigned>(TRANSFER_PHYSICAL_ROWS),
             static_cast<unsigned>(TRANSFER_BUFFER_COUNT));
    int64_t firstFrameStartedUs = esp_timer_get_time();
    result = submitFrame(panel, logicalPixels, transferBuffers, transferDone);
    ESP_LOGI(TAG, "Initial full-frame transfer: %lld us",
             static_cast<long long>(esp_timer_get_time() - firstFrameStartedUs));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Frame transfer failed: %s", esp_err_to_name(result));
        return;
    }

    AmoledV1::TouchInput touch;
    bool touchReady = touch.begin() == ESP_OK;
    if (!touchReady) {
        ESP_LOGE(TAG, "FT3168 touch initialization failed");
    }

    bool locked = false;
    ESP_LOGI(TAG, "Interactive home screen presented at 368x448");
    while (true) {
        uint32_t nowMs = millisNow();
        AmoledV1::TouchEvent event;
        if (touchReady && touch.poll(nowMs, event)) {
            if (event.type == AmoledV1::TouchEventType::DOWN) {
                ESP_LOGI(TAG, "Touch down logical=(%d,%d)", event.x, event.y);
            }
            if (locked) {
                if (event.type == AmoledV1::TouchEventType::DOWN) {
                    ESP_ERROR_CHECK_WITHOUT_ABORT(
                        esp_lcd_panel_disp_on_off(panel, true));
                    ESP_ERROR_CHECK_WITHOUT_ABORT(
                        bsp_display_brightness_set(72));
                    locked = false;
                    app.onWake();
                    ESP_LOGI(TAG, "Display unlocked by touch");
                }
            } else {
                app.handleTouch(event);
            }
        }

        app.update(nowMs);
        if (!locked && app.consumeLockRequest()) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_display_brightness_set(0));
            ESP_ERROR_CHECK_WITHOUT_ABORT(
                esp_lcd_panel_disp_on_off(panel, false));
            locked = true;
            ESP_LOGI(TAG, "Display locked; touch to wake");
        }

        if (!locked && app.needsRender()) {
            uint16_t renderBegin = app.renderRowBegin();
            uint16_t renderEnd = app.renderRowEnd();
            app.render(canvas);
            result = submitFrame(
                panel, logicalPixels, transferBuffers, transferDone,
                renderBegin, renderEnd);
            if (result != ESP_OK) {
                ESP_LOGE(TAG, "Frame update failed: %s",
                         esp_err_to_name(result));
            } else {
                app.markRendered();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(locked ? 60 : 20));
    }
}
