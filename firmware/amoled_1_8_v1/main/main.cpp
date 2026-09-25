#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>

#include "AmoledApp.h"
#include "AmoledPlatform.h"
#include "AmoledGeometry.h"
#include "HomeScreen.h"
#include "TouchInput.h"
#include "core/AudioManager.h"
#if STICKMON_HAS_CLAW
#include "brain/BrainBridge.h"
#include "brain/StickmonClawRuntime.h"
#endif
#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_io_expander.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "presentation/Canvas565.h"
#include "presentation/PixelRenderer.h"

namespace {

constexpr char TAG[] = "StickMon";
constexpr uint16_t LOGICAL_WIDTH = AmoledUi::WIDTH;
constexpr uint16_t LOGICAL_HEIGHT = AmoledUi::HEIGHT;
constexpr uint16_t PHYSICAL_WIDTH = AmoledUi::WIDTH;
constexpr uint16_t PHYSICAL_HEIGHT = AmoledUi::HEIGHT;
constexpr uint16_t TRANSFER_LOGICAL_ROWS = 32;
constexpr uint16_t TRANSFER_PHYSICAL_ROWS = TRANSFER_LOGICAL_ROWS;
constexpr size_t TRANSFER_BUFFER_COUNT = 2;
constexpr uint32_t LOCK_ANIMATION_MS = 1000;
constexpr uint32_t LOCK_ANIMATION_FRAME_MS = 33;
constexpr uint32_t LOCK_WAKE_GRACE_MS = 1200;
constexpr uint32_t LOCK_FINAL_CLEANUP_DELAY_MS = 180;
constexpr int LOCK_FINAL_RADIUS = 66;
constexpr int LOCK_FEATHER_PIXELS = 8;
constexpr int LOCK_COVER_MARGIN = LOCK_FEATHER_PIXELS + 2;
constexpr int LOCK_UPDATE_ALIGNMENT = 2;
constexpr int LOCK_SLEEP_BREATH_AMPLITUDE = 8;
constexpr uint32_t LOCK_SLEEP_BREATH_PERIOD_MS = 2000;
enum class LockPhase : uint8_t { OPEN, CLOSING, LOCKED, OPENING };
constexpr size_t PHYSICAL_PIXELS =
    static_cast<size_t>(PHYSICAL_WIDTH) * PHYSICAL_HEIGHT;
constexpr size_t TRANSFER_PIXELS =
    static_cast<size_t>(PHYSICAL_WIDTH) * TRANSFER_PHYSICAL_ROWS;
using TransferBuffers = std::array<uint16_t*, TRANSFER_BUFFER_COUNT>;

#if STICKMON_ENABLE_DEBUG_FEATURES
struct ExploreFramePerf {
    int64_t windowStartedUs = 0;
    int64_t lastFrameStartedUs = 0;
    uint32_t frames = 0;
    uint32_t rows = 0;
    uint32_t maxRows = 0;
    uint32_t overBudgetFrames = 0;
    uint64_t updateTotalUs = 0;
    uint32_t updateMaxUs = 0;
    uint64_t clawTotalUs = 0;
    uint32_t clawMaxUs = 0;
    uint64_t renderTotalUs = 0;
    uint32_t renderMaxUs = 0;
    uint64_t transferTotalUs = 0;
    uint32_t transferMaxUs = 0;
    uint64_t frameTotalUs = 0;
    uint32_t frameMaxUs = 0;
    uint32_t frameGapMaxUs = 0;

    void flush(int64_t nowUs, bool force) {
        if (frames == 0) return;
        const int64_t elapsedUs = nowUs - windowStartedUs;
        if (!force && elapsedUs < 1000000) return;
        const uint32_t fps10 = elapsedUs > 0
            ? static_cast<uint32_t>(frames * 10000000ULL / elapsedUs) : 0;
        ESP_LOGI(
            TAG,
            "[ExplorePerf] fps=%lu.%lu frames=%lu rows(avg/max)=%lu/%lu "
            "update(avg/max)=%lu/%lu us claw(avg/max)=%lu/%lu us "
            "draw(avg/max)=%lu/%lu us lcd(avg/max)=%lu/%lu us "
            "total(avg/max)=%lu/%lu us gapMax=%lu us over20ms=%lu",
            static_cast<unsigned long>(fps10 / 10),
            static_cast<unsigned long>(fps10 % 10),
            static_cast<unsigned long>(frames),
            static_cast<unsigned long>(rows / frames),
            static_cast<unsigned long>(maxRows),
            static_cast<unsigned long>(updateTotalUs / frames),
            static_cast<unsigned long>(updateMaxUs),
            static_cast<unsigned long>(clawTotalUs / frames),
            static_cast<unsigned long>(clawMaxUs),
            static_cast<unsigned long>(renderTotalUs / frames),
            static_cast<unsigned long>(renderMaxUs),
            static_cast<unsigned long>(transferTotalUs / frames),
            static_cast<unsigned long>(transferMaxUs),
            static_cast<unsigned long>(frameTotalUs / frames),
            static_cast<unsigned long>(frameMaxUs),
            static_cast<unsigned long>(frameGapMaxUs),
            static_cast<unsigned long>(overBudgetFrames));
        const int64_t previousFrameStartedUs = lastFrameStartedUs;
        *this = ExploreFramePerf{};
        if (!force) {
            windowStartedUs = nowUs;
            lastFrameStartedUs = previousFrameStartedUs;
        }
    }

    void record(int64_t frameStartedUs, int64_t finishedUs,
                uint32_t updateUs, uint32_t clawUs, uint32_t renderUs,
                uint32_t transferUs, uint16_t rowCount) {
        if (windowStartedUs == 0) windowStartedUs = frameStartedUs;
        if (lastFrameStartedUs != 0) {
            frameGapMaxUs = std::max(
                frameGapMaxUs,
                static_cast<uint32_t>(frameStartedUs - lastFrameStartedUs));
        }
        lastFrameStartedUs = frameStartedUs;
        const uint32_t totalUs = static_cast<uint32_t>(
            finishedUs - frameStartedUs);
        ++frames;
        rows += rowCount;
        maxRows = std::max<uint32_t>(maxRows, rowCount);
        if (totalUs > 20000) ++overBudgetFrames;
        updateTotalUs += updateUs;
        updateMaxUs = std::max(updateMaxUs, updateUs);
        clawTotalUs += clawUs;
        clawMaxUs = std::max(clawMaxUs, clawUs);
        renderTotalUs += renderUs;
        renderMaxUs = std::max(renderMaxUs, renderUs);
        transferTotalUs += transferUs;
        transferMaxUs = std::max(transferMaxUs, transferUs);
        frameTotalUs += totalUs;
        frameMaxUs = std::max(frameMaxUs, totalUs);
        flush(finishedUs, false);
    }
};

ExploreFramePerf exploreFramePerf;
#endif

uint16_t* allocatePhysicalPixels() {
    return static_cast<uint16_t*>(heap_caps_calloc(
        PHYSICAL_PIXELS, sizeof(uint16_t),
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

    ESP_LOGI(TAG, "Display: creating SH8601 panel");
    esp_err_t result = bsp_display_new(&config, panel, io);
    ESP_LOGI(TAG, "Display: bsp_display_new returned %s",
             esp_err_to_name(result));
    ESP_RETURN_ON_ERROR(result, TAG, "SH8601 initialization failed");

    ESP_LOGI(TAG, "Display: initializing brightness");
    result = bsp_display_brightness_init();
    ESP_LOGI(TAG, "Display: brightness init returned %s",
             esp_err_to_name(result));
    ESP_RETURN_ON_ERROR(result, TAG, "brightness initialization failed");

    result = bsp_display_brightness_set(72);
    ESP_LOGI(TAG, "Display: brightness set returned %s",
             esp_err_to_name(result));
    return result;
}

esp_err_t submitFrame(esp_lcd_panel_handle_t panel,
                      const uint16_t* physicalPixels,
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
        uint16_t physicalY = sourceY;
        uint16_t physicalRows = logicalRows;

        for (uint16_t row = 0; row < physicalRows; ++row) {
            const uint16_t* source = physicalPixels +
                static_cast<size_t>(physicalY + row) * PHYSICAL_WIDTH;
            std::memcpy(transferBuffers[nextBuffer] +
                            static_cast<size_t>(row) * PHYSICAL_WIDTH,
                        source, PHYSICAL_WIDTH * sizeof(uint16_t));
        }
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

esp_err_t submitFrameRegion(esp_lcd_panel_handle_t panel,
                            const uint16_t* physicalPixels,
                            const TransferBuffers& transferBuffers,
                            SemaphoreHandle_t transferDone,
                            uint16_t xBegin, uint16_t xEnd,
                            uint16_t sourceBegin, uint16_t sourceEnd) {
    if (!transferDone) return ESP_ERR_INVALID_ARG;
    xBegin = std::min<uint16_t>(xBegin, LOGICAL_WIDTH);
    xEnd = std::min<uint16_t>(xEnd, LOGICAL_WIDTH);
    sourceBegin = std::min<uint16_t>(sourceBegin, LOGICAL_HEIGHT);
    sourceEnd = std::min<uint16_t>(sourceEnd, LOGICAL_HEIGHT);
    if (xBegin >= xEnd || sourceBegin >= sourceEnd) return ESP_OK;

    uint16_t physicalXBegin = xBegin;
    uint16_t physicalXEnd = xEnd;
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
        uint16_t logicalRows = static_cast<uint16_t>(std::min<uint16_t>(
            TRANSFER_LOGICAL_ROWS, sourceEnd - sourceY));
        uint16_t physicalY = sourceY;
        uint16_t physicalRows = logicalRows;
        const uint16_t physicalRegionWidth = physicalXEnd - physicalXBegin;
        for (uint16_t row = 0; row < physicalRows; ++row) {
            const uint16_t* source = physicalPixels +
                static_cast<size_t>(physicalY + row) * PHYSICAL_WIDTH +
                physicalXBegin;
            std::memcpy(transferBuffers[nextBuffer] +
                            static_cast<size_t>(row) * physicalRegionWidth,
                        source, physicalRegionWidth * sizeof(uint16_t));
        }
        result = esp_lcd_panel_draw_bitmap(
            panel, physicalXBegin, physicalY, physicalXEnd,
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

void drawLockMask(Canvas565& canvas, int centerX, int centerY, int radius,
                  int xBegin, int xEnd, int yBegin, int yEnd) {
    canvas.clearClipRect();
    xBegin = std::clamp(xBegin, 0, canvas.width());
    xEnd = std::clamp(xEnd, xBegin, canvas.width());
    yBegin = std::clamp(yBegin, 0, canvas.height());
    yEnd = std::clamp(yEnd, yBegin, canvas.height());
    radius = std::max(0, radius);
    if (radius == 0) {
        canvas.fillRect(xBegin, yBegin, xEnd - xBegin, yEnd - yBegin, 0);
        return;
    }
    int innerRadius = std::max(0, radius - LOCK_FEATHER_PIXELS);
    int64_t outerRadiusSquared = static_cast<int64_t>(radius) * radius;
    int64_t innerRadiusSquared = static_cast<int64_t>(innerRadius) * innerRadius;
    auto shadeRange = [&](int y, int dy, int left, int right) {
        left = std::max(left, xBegin);
        right = std::min(right, xEnd - 1);
        for (int x = left; x <= right; ++x) {
            const int dx = x - centerX;
            const int64_t distanceSquared = static_cast<int64_t>(dx) * dx +
                                            static_cast<int64_t>(dy) * dy;
            if (distanceSquared <= innerRadiusSquared) continue;
            if (distanceSquared >= outerRadiusSquared) {
                canvas.drawPixel(x, y, 0);
                continue;
            }
            const uint16_t color = canvas.readPixel(x, y);
            const int64_t distanceInside =
                outerRadiusSquared - distanceSquared;
            const int factor = static_cast<int>(
                distanceInside * 255 /
                std::max<int64_t>(1,
                    outerRadiusSquared - innerRadiusSquared));
            const int red = ((color >> 11) & 0x1F) * factor / 255;
            const int green = ((color >> 5) & 0x3F) * factor / 255;
            const int blue = (color & 0x1F) * factor / 255;
            canvas.drawPixel(x, y, static_cast<uint16_t>(
                (red << 11) | (green << 5) | blue));
        }
    };
    for (int y = yBegin; y < yEnd; ++y) {
        int dy = y - centerY;
        if (std::abs(dy) > radius) {
            canvas.fillRect(xBegin, y, xEnd - xBegin, 1, 0);
            continue;
        }
        int span = static_cast<int>(std::sqrt(static_cast<float>(
            radius * radius - dy * dy)));
        int circleLeft = std::max(0, centerX - span);
        int circleRight = std::min(canvas.width() - 1, centerX + span);
        int left = std::max(xBegin, circleLeft);
        int right = std::min(xEnd - 1, circleRight);
        if (circleLeft > xBegin) {
            canvas.fillRect(xBegin, y, circleLeft - xBegin, 1, 0);
        }
        if (circleRight + 1 < xEnd) {
            canvas.fillRect(circleRight + 1, y,
                            xEnd - circleRight - 1, 1, 0);
        }
        if (left > right) continue;
        if (std::abs(dy) > innerRadius) {
            shadeRange(y, dy, left, right);
            continue;
        }
        const int innerSpan = static_cast<int>(std::sqrt(static_cast<float>(
            innerRadius * innerRadius - dy * dy)));
        shadeRange(y, dy, left, centerX - innerSpan - 1);
        shadeRange(y, dy, centerX + innerSpan + 1, right);
    }
}

void restoreFrameRegion(uint16_t* destination, const uint16_t* snapshot,
                        uint16_t xBegin, uint16_t xEnd,
                        uint16_t yBegin, uint16_t yEnd) {
    if (!destination || !snapshot || xBegin >= xEnd || yBegin >= yEnd) return;
    const size_t rowBytes = static_cast<size_t>(xEnd - xBegin) *
                            sizeof(uint16_t);
    for (uint16_t y = yBegin; y < yEnd; ++y) {
        const size_t offset = static_cast<size_t>(y) * PHYSICAL_WIDTH + xBegin;
        std::memcpy(destination + offset, snapshot + offset, rowBytes);
    }
}

void alignLockUpdateRegion(uint16_t& xBegin, uint16_t& xEnd,
                           uint16_t& yBegin, uint16_t& yEnd) {
    xBegin = static_cast<uint16_t>(
        xBegin / LOCK_UPDATE_ALIGNMENT * LOCK_UPDATE_ALIGNMENT);
    yBegin = static_cast<uint16_t>(
        yBegin / LOCK_UPDATE_ALIGNMENT * LOCK_UPDATE_ALIGNMENT);
    xEnd = static_cast<uint16_t>(std::min<int>(
        LOGICAL_WIDTH,
        ((xEnd + LOCK_UPDATE_ALIGNMENT - 1) / LOCK_UPDATE_ALIGNMENT) *
            LOCK_UPDATE_ALIGNMENT));
    yEnd = static_cast<uint16_t>(std::min<int>(
        LOGICAL_HEIGHT,
        ((yEnd + LOCK_UPDATE_ALIGNMENT - 1) / LOCK_UPDATE_ALIGNMENT) *
            LOCK_UPDATE_ALIGNMENT));
}

int lockCoverRadius(int centerX, int centerY) {
    const int farX = std::max(centerX, static_cast<int>(LOGICAL_WIDTH) - 1 -
                                          centerX);
    const int farY = std::max(centerY, static_cast<int>(LOGICAL_HEIGHT) - 1 -
                                          centerY);
    return static_cast<int>(std::ceil(std::sqrt(static_cast<float>(
        farX * farX + farY * farY)))) + LOCK_COVER_MARGIN;
}

int lockRadius(LockPhase phase, uint32_t nowMs,
               uint32_t animationStartedMs, int startRadius,
               bool preserveFocus, bool sleeping) {
    int finalRadius = preserveFocus ? LOCK_FINAL_RADIUS : 0;
    if (phase == LockPhase::LOCKED) {
        if (!preserveFocus || !sleeping) return finalRadius;
        float cycle = static_cast<float>(nowMs %
            LOCK_SLEEP_BREATH_PERIOD_MS) /
            static_cast<float>(LOCK_SLEEP_BREATH_PERIOD_MS);
        float wave = 0.5f - 0.5f * std::cos(cycle * 6.2831853f);
        return finalRadius - LOCK_SLEEP_BREATH_AMPLITUDE +
            static_cast<int>(std::lround(
                wave * LOCK_SLEEP_BREATH_AMPLITUDE * 2.0f));
    }
    uint32_t elapsed = nowMs - animationStartedMs;
    float progress = std::min(
        1.0f, static_cast<float>(elapsed) / LOCK_ANIMATION_MS);
    // Smoothstep gives both lock and wake animations a symmetric
    // ease-in/ease-out profile without adding a floating-point dependency.
    float eased = progress * progress * (3.0f - 2.0f * progress);
    if (phase == LockPhase::OPENING) {
        return static_cast<int>(std::lround(
            finalRadius +
            (startRadius - finalRadius) * eased));
    }
    return static_cast<int>(std::lround(
        startRadius - (startRadius - finalRadius) * eased));
}

#if STICKMON_HAS_CLAW
bool brainSnapshot(Stickmon::BrainBridge::Snapshot& out, void* userCtx) {
    auto* app = static_cast<AmoledV1::AmoledApp*>(userCtx);
    return app && app->brainSnapshot(out);
}

bool brainStartExpedition(uint8_t area, void* userCtx) {
    auto* app = static_cast<AmoledV1::AmoledApp*>(userCtx);
    return app && app->brainStartExpedition(area);
}

bool brainReturnHome(void* userCtx) {
    auto* app = static_cast<AmoledV1::AmoledApp*>(userCtx);
    return app && app->brainReturnHome();
}

bool brainInviteFriend(void* userCtx) {
    auto* app = static_cast<AmoledV1::AmoledApp*>(userCtx);
    return app && app->brainInviteFriend();
}

bool brainEat(void* userCtx) {
    auto* app = static_cast<AmoledV1::AmoledApp*>(userCtx);
    return app && app->brainEat();
}

bool brainBuyFood(uint8_t foodIndex, void* userCtx) {
    auto* app = static_cast<AmoledV1::AmoledApp*>(userCtx);
    return app && app->brainBuyFood(foodIndex);
}

bool brainSay(const char* text, void* userCtx) {
    auto* app = static_cast<AmoledV1::AmoledApp*>(userCtx);
    return app && app->brainSay(text);
}
#endif

}  // namespace

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Starting AMOLED V1 display milestone");
    ESP_LOGI(TAG, "Reset reason: %d", static_cast<int>(esp_reset_reason()));
    ESP_LOGI(TAG, "PSRAM size: %u bytes",
             static_cast<unsigned>(esp_psram_get_size()));

    uint16_t* physicalPixels = allocatePhysicalPixels();
    uint16_t* lockSnapshot = allocatePhysicalPixels();
    TransferBuffers transferBuffers{};
    bool transferBuffersReady = true;
    for (uint16_t*& buffer : transferBuffers) {
        buffer = allocateTransferPixels();
        transferBuffersReady = transferBuffersReady && buffer != nullptr;
    }
    if (!physicalPixels || !transferBuffersReady) {
        ESP_LOGE(TAG, "Unable to allocate RGB565 framebuffers");
        heap_caps_free(physicalPixels);
        heap_caps_free(lockSnapshot);
        for (uint16_t* buffer : transferBuffers) heap_caps_free(buffer);
        return;
    }
    if (!lockSnapshot) {
        ESP_LOGW(TAG, "Lock snapshot unavailable; using live scene rendering");
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
    ESP_LOGI(TAG, "Display: transfer semaphore ready");
    esp_lcd_panel_io_callbacks_t callbacks{};
    callbacks.on_color_trans_done = onColorTransferDone;
    result = esp_lcd_panel_io_register_event_callbacks(
        io, &callbacks, transferDone);
    ESP_LOGI(TAG, "Display: transfer callback returned %s",
             esp_err_to_name(result));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Display callback setup failed: %s",
                 esp_err_to_name(result));
        return;
    }

    const Platform::FrameBuffer565 frameBuffer{
        physicalPixels, PHYSICAL_WIDTH, PHYSICAL_HEIGHT, true};
    Canvas565 canvas;
    canvas.attach(frameBuffer);
    canvas.setCoordinateScale(1);
    canvas.setLayoutScale(1);
    canvas.setAssetScale(1);
    canvas.setNativeText(true);
    ESP_LOGI(TAG, "Platform: binding services");
    AmoledV1::bindAmoledPlatform();
    if (!AmoledV1::AmoledPlatform::instance().begin()) {
        ESP_LOGW(TAG, "AMOLED platform peripheral init incomplete");
    }
    ESP_LOGI(TAG, "Platform: ready");
    PixelRenderer::bind(frameBuffer);
    PixelRenderer::setCoordinateScale(1);
    PixelRenderer::canvas().setLayoutScale(1);
    PixelRenderer::canvas().setAssetScale(1);
    PixelRenderer::canvas().setNativeText(true);
    ESP_LOGI(TAG, "App: creating home application (object=%u bytes, stack-free=%u)",
             static_cast<unsigned>(sizeof(AmoledV1::AmoledApp)),
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    // AmoledApp contains the complete UI/game state. Keep it in static
    // storage so deep begin/render calls cannot consume the app_main stack.
    static AmoledV1::AmoledApp app;
    ESP_LOGI(TAG, "App: loading state and resources (stack-free=%u)",
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    app.begin(millisNow());
    // Scene transitions use the AMOLED brightness register. Intermediate
    // shades therefore avoid a full redraw, per-pixel blend and GRAM upload.
    app.setExternalSceneFade(true);
#if STICKMON_HAS_CLAW
    Stickmon::BrainBridge::HostAdapter brainHost{};
    brainHost.snapshot = &brainSnapshot;
    brainHost.startExpedition = &brainStartExpedition;
    brainHost.returnHome = &brainReturnHome;
    brainHost.inviteFriend = &brainInviteFriend;
    brainHost.eat = &brainEat;
    brainHost.buyFood = &brainBuyFood;
    brainHost.say = &brainSay;
    brainHost.userCtx = &app;
    Stickmon::BrainBridge::instance().setHost(brainHost);
#endif
    ESP_LOGI(TAG, "App: state and resources ready (stack-free=%u)",
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    ESP_LOGI(TAG, "App: rendering initial frame (stack-free=%u)",
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    app.render(canvas);
#if STICKMON_ENABLE_DEBUG_FEATURES
    app.renderDebugTouchOverlay(canvas);
#endif
    app.markRendered();
    ESP_LOGI(TAG, "App: initial frame rendered (stack-free=%u)",
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));

    ESP_LOGI(TAG, "Display pipeline: %u physical rows, %u DMA buffers",
             static_cast<unsigned>(TRANSFER_PHYSICAL_ROWS),
             static_cast<unsigned>(TRANSFER_BUFFER_COUNT));
    int64_t firstFrameStartedUs = esp_timer_get_time();
    result = submitFrame(panel, physicalPixels, transferBuffers, transferDone);
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

    LockPhase lockPhase = LockPhase::OPEN;
    uint32_t lockAnimationStartedMs = 0;
    int lockAnimationStartRadius = lockCoverRadius(
        AmoledUi::WIDTH / 2, AmoledUi::HEIGHT / 2);
    uint32_t lockWakeGraceUntilMs = 0;
    uint32_t lockFinalCleanupAtMs = 0;
    int16_t lastLockFocusX = 92;
    int16_t lastLockFocusY = 112;
    int lastLockRadius = LOCK_FINAL_RADIUS;
    bool lockVisualValid = false;
    bool lockHasFocus = false;
    bool lastLockSleeping = false;
    bool lockFinalCleanupPending = false;
    bool lockSnapshotValid = false;
    uint8_t lockedBrightness =
        AmoledV1::AmoledPlatform::instance().brightness();
    bool sceneFadeWasActive = false;
    bool sceneFadeWasInward = false;
    uint8_t sceneFadeBaseBrightness = lockedBrightness;
    uint8_t sceneFadeBrightness = lockedBrightness;
    ESP_LOGI(TAG, "Interactive home screen presented at 368x448");
#if STICKMON_HAS_CLAW
    Stickmon::ClawRuntime::instance().beginAsync();
#endif
    while (true) {
#if STICKMON_ENABLE_DEBUG_FEATURES
        const int64_t loopStartedUs = esp_timer_get_time();
#endif
        uint32_t nowMs = millisNow();
        AmoledV1::TouchEvent event;
        if (touchReady && touch.poll(nowMs, event)) {
#if STICKMON_HAS_CLAW
            Stickmon::ClawRuntime::instance().notePlayerActivity(nowMs);
#endif
            if (event.type == AmoledV1::TouchEventType::DOWN) {
                ESP_LOGI(TAG, "Touch down display=(%d,%d)", event.x, event.y);
            }
            if (lockPhase != LockPhase::OPEN) {
                if (event.type == AmoledV1::TouchEventType::DOWN) {
                    AmoledV1::AmoledPlatform::instance().setBrightness(
                        lockedBrightness);
                    if (lockPhase != LockPhase::OPENING) {
                        int16_t focusX = AmoledUi::WIDTH / 2;
                        int16_t focusY = AmoledUi::HEIGHT / 2;
                        if (lockHasFocus) app.lockFocusPoint(focusX, focusY);
                        lockAnimationStartRadius =
                            lockCoverRadius(focusX, focusY);
                        lockPhase = LockPhase::OPENING;
                        lockAnimationStartedMs = nowMs;
                        lockFinalCleanupPending = false;
                        lockSnapshotValid = false;
                    }
                    app.onWake(nowMs);
                    lockWakeGraceUntilMs = nowMs + LOCK_WAKE_GRACE_MS;
                    ESP_LOGI(TAG, "Display unlocked by touch");
                }
            } else {
                app.handleTouch(event);
            }
        }

#if STICKMON_ENABLE_DEBUG_FEATURES
        const int64_t updateStartedUs = esp_timer_get_time();
#endif
        app.update(nowMs);
#if STICKMON_ENABLE_DEBUG_FEATURES
        const uint32_t updateUs = static_cast<uint32_t>(
            esp_timer_get_time() - updateStartedUs);
        uint32_t clawUs = 0;
#endif
#if STICKMON_HAS_CLAW
#if STICKMON_ENABLE_DEBUG_FEATURES
        const int64_t clawStartedUs = esp_timer_get_time();
#endif
        Stickmon::ClawRuntime::instance().update(nowMs);
#if STICKMON_ENABLE_DEBUG_FEATURES
        clawUs = static_cast<uint32_t>(esp_timer_get_time() - clawStartedUs);
#endif
#endif
        const bool sceneFadeActive = app.sceneFadeActive();
        const bool sceneFadePhaseChanged = sceneFadeActive &&
            (!sceneFadeWasActive ||
             app.sceneFadeInward() != sceneFadeWasInward);
        if (lockPhase == LockPhase::OPEN && sceneFadeActive) {
            if (!sceneFadeWasActive) {
                sceneFadeBaseBrightness =
                    AmoledV1::AmoledPlatform::instance().brightness();
                sceneFadeBrightness = sceneFadeBaseBrightness;
            }
            const uint8_t brightness = static_cast<uint8_t>(
                (static_cast<uint32_t>(sceneFadeBaseBrightness) *
                 (255U - app.sceneFadeAlpha()) + 127U) / 255U);
            if (brightness != sceneFadeBrightness) {
                AmoledV1::AmoledPlatform::instance().setBrightness(brightness);
                sceneFadeBrightness = brightness;
            }
        } else if (sceneFadeWasActive && !sceneFadeActive) {
            AmoledV1::AmoledPlatform::instance().setBrightness(
                sceneFadeBaseBrightness);
            sceneFadeBrightness = sceneFadeBaseBrightness;
        }
        sceneFadeWasActive = sceneFadeActive;
        if (sceneFadeActive) sceneFadeWasInward = app.sceneFadeInward();

        bool lockRequest = app.consumeLockRequest();
        if (lockPhase == LockPhase::OPEN && lockRequest &&
            app.displayLockAllowed() &&
            static_cast<int32_t>(nowMs - lockWakeGraceUntilMs) >= 0) {
            lockedBrightness =
                AmoledV1::AmoledPlatform::instance().brightness();
            lockAnimationStartedMs = nowMs;
            lockPhase = LockPhase::CLOSING;
            lockVisualValid = false;
            lockFinalCleanupPending = false;
            int16_t focusX = AmoledUi::WIDTH / 2;
            int16_t focusY = AmoledUi::HEIGHT / 2;
            lockHasFocus = app.lockFocusPoint(focusX, focusY);
            lockAnimationStartRadius = lockCoverRadius(focusX, focusY);
            AudioManager::ins().setMusicSuspended(true);
            if (lockSnapshot) {
                std::memcpy(lockSnapshot, physicalPixels,
                            PHYSICAL_PIXELS * sizeof(uint16_t));
                lockSnapshotValid = true;
            } else {
                lockSnapshotValid = false;
                app.forceFullRender();
            }
            ESP_LOGI(TAG, "Display lock animation started");
        }
        if (lockPhase == LockPhase::CLOSING &&
            nowMs - lockAnimationStartedMs >= LOCK_ANIMATION_MS) {
            lockPhase = LockPhase::LOCKED;
            // Keep the animation-completion frame partial. The full cleanup
            // runs after motion has stopped so it cannot stall the transition.
            lockFinalCleanupAtMs = nowMs + LOCK_FINAL_CLEANUP_DELAY_MS;
            lockFinalCleanupPending = true;
            ESP_LOGI(TAG, "Display locked; touch to wake");
        }
        if (lockPhase == LockPhase::OPENING &&
            nowMs - lockAnimationStartedMs >= LOCK_ANIMATION_MS) {
            lockPhase = LockPhase::OPEN;
            lockVisualValid = false;
            lockHasFocus = false;
            lockFinalCleanupPending = false;
            lockSnapshotValid = false;
            app.forceFullRender();
            AudioManager::ins().setMusicSuspended(false);
            ESP_LOGI(TAG, "Display fully unlocked");
        }

        bool lockedWithoutFocus =
            lockPhase == LockPhase::LOCKED && !lockHasFocus;
        bool lockSleeping = lockHasFocus && app.petIsSleeping();
        const bool lockFinalCleanupDue =
            lockFinalCleanupPending && lockPhase == LockPhase::LOCKED &&
            static_cast<int32_t>(nowMs - lockFinalCleanupAtMs) >= 0;
        if (lockFinalCleanupDue) {
            lockVisualValid = false;
            app.forceFullRender();
        }
        const bool lockAnimating = lockPhase == LockPhase::CLOSING ||
                                   lockPhase == LockPhase::OPENING;
        if (lockPhase == LockPhase::OPENING && lockSnapshot &&
            !lockSnapshotValid) {
            // Build one clean wake snapshot. Expansion frames restore from it
            // instead of rerendering the complete room every 33 ms.
            app.forceFullRender();
            app.render(canvas);
#if STICKMON_ENABLE_DEBUG_FEATURES
            app.renderDebugTouchOverlay(canvas);
#endif
            app.markRendered();
            std::memcpy(lockSnapshot, physicalPixels,
                        PHYSICAL_PIXELS * sizeof(uint16_t));
            lockSnapshotValid = true;
        }
        const bool sceneFadeBrightnessOnly =
            lockPhase == LockPhase::OPEN && sceneFadeActive &&
            !sceneFadePhaseChanged;
        if (sceneFadeBrightnessOnly && app.needsRender()) {
            // A brightness update is the presented fade frame. Clearing the
            // dirty request lets SceneFade advance on the next loop without
            // redrawing the unchanged scene underneath it.
            app.markRendered();
        }
        bool renderNeeded =
                            (!lockedWithoutFocus && app.needsRender()) ||
                            lockPhase == LockPhase::CLOSING ||
                            lockPhase == LockPhase::OPENING;
        if (lockPhase != LockPhase::OPEN) {
            int16_t focusX = AmoledUi::WIDTH / 2;
            int16_t focusY = AmoledUi::HEIGHT / 2;
            if (lockHasFocus) app.lockFocusPoint(focusX, focusY);
            int radius = lockRadius(lockPhase, nowMs,
                                    lockAnimationStartedMs,
                                    lockAnimationStartRadius,
                                    lockHasFocus, lockSleeping);
            renderNeeded = renderNeeded || !lockVisualValid ||
                           focusX != lastLockFocusX ||
                           focusY != lastLockFocusY || radius != lastLockRadius ||
                           lockSleeping != lastLockSleeping;
        }
        if (renderNeeded) {
            const bool appRenderRequested = app.needsRender();
            bool lockFrame = lockPhase != LockPhase::OPEN;
            uint16_t renderBegin = app.renderRowBegin();
            uint16_t renderEnd = app.renderRowEnd();
            uint16_t renderXBegin = 0;
            uint16_t renderXEnd = LOGICAL_WIDTH;
            uint16_t nativeRenderBegin = static_cast<uint16_t>(renderBegin);
            uint16_t nativeRenderEnd = renderEnd;
            uint16_t nativeRenderXBegin = renderXBegin;
            uint16_t nativeRenderXEnd = renderXEnd;
            int16_t lockFrameFocusX = AmoledUi::WIDTH / 2;
            int16_t lockFrameFocusY = AmoledUi::HEIGHT / 2;
            int lockFrameRadius = 0;
            if (lockFrame) {
                if (lockHasFocus) {
                    app.lockFocusPoint(lockFrameFocusX, lockFrameFocusY);
                }
                lockFrameRadius = lockRadius(lockPhase, nowMs,
                                             lockAnimationStartedMs,
                                             lockAnimationStartRadius,
                                             lockHasFocus, lockSleeping);
                const int currentLeft = lockFrameFocusX - lockFrameRadius -
                    LOCK_FEATHER_PIXELS;
                const int currentRight = lockFrameFocusX + lockFrameRadius +
                    LOCK_FEATHER_PIXELS + 1;
                const int currentTop = lockFrameFocusY - lockFrameRadius -
                    LOCK_FEATHER_PIXELS;
                const int currentBottom = lockFrameFocusY + lockFrameRadius +
                    LOCK_FEATHER_PIXELS + 1;
                if (!lockVisualValid) {
                    // The first mask replaces the previously visible scene, so
                    // it must cover the whole panel once.
                    renderXBegin = nativeRenderXBegin = 0;
                    renderXEnd = nativeRenderXEnd = LOGICAL_WIDTH;
                    renderBegin = nativeRenderBegin = 0;
                    renderEnd = nativeRenderEnd = LOGICAL_HEIGHT;
                } else {
                    // The panel is already black outside the previous circle.
                    // Redraw only the union of old and new circles, including
                    // the feather band, so movement and breathing leave no
                    // stale pixels behind.
                    const int oldLeft = lastLockFocusX - lastLockRadius -
                        LOCK_FEATHER_PIXELS;
                    const int oldRight = lastLockFocusX + lastLockRadius +
                        LOCK_FEATHER_PIXELS + 1;
                    const int oldTop = lastLockFocusY - lastLockRadius -
                        LOCK_FEATHER_PIXELS;
                    const int oldBottom = lastLockFocusY + lastLockRadius +
                        LOCK_FEATHER_PIXELS + 1;
                    renderXBegin = static_cast<uint16_t>(std::clamp(
                        std::min(oldLeft, currentLeft),
                        0, static_cast<int>(LOGICAL_WIDTH)));
                    renderXEnd = static_cast<uint16_t>(std::clamp(
                        std::max(oldRight, currentRight),
                        0, static_cast<int>(LOGICAL_WIDTH)));
                    renderBegin = static_cast<uint16_t>(std::clamp(
                        std::min(oldTop, currentTop),
                        0, static_cast<int>(LOGICAL_HEIGHT)));
                    renderEnd = static_cast<uint16_t>(std::clamp(
                        std::max(oldBottom, currentBottom),
                        0, static_cast<int>(LOGICAL_HEIGHT)));
                    nativeRenderXBegin = renderXBegin;
                    nativeRenderXEnd = renderXEnd;
                    nativeRenderBegin = renderBegin;
                    nativeRenderEnd = renderEnd;
                }
                alignLockUpdateRegion(nativeRenderXBegin, nativeRenderXEnd,
                                      nativeRenderBegin, nativeRenderEnd);
                renderXBegin = nativeRenderXBegin;
                renderXEnd = nativeRenderXEnd;
                renderBegin = nativeRenderBegin;
                renderEnd = nativeRenderEnd;
                app.forceRenderRows(nativeRenderBegin, nativeRenderEnd);
            }
            const bool useLockSnapshot = lockFrame && lockSnapshotValid &&
                (lockAnimating ||
                 (lockPhase == LockPhase::LOCKED && !appRenderRequested));
#if STICKMON_ENABLE_DEBUG_FEATURES
            const bool profileExploreFrame =
                !lockFrame && app.exploreRouteMovingForDiagnostics();
            const int64_t renderStartedUs = esp_timer_get_time();
#endif
            if (useLockSnapshot) {
                restoreFrameRegion(physicalPixels, lockSnapshot,
                                   nativeRenderXBegin, nativeRenderXEnd,
                                   nativeRenderBegin, nativeRenderEnd);
            } else {
                app.render(canvas);
#if STICKMON_ENABLE_DEBUG_FEATURES
                app.renderDebugTouchOverlay(canvas);
#endif
                if (lockFrame && lockSnapshot &&
                    lockPhase == LockPhase::LOCKED) {
                    // Preserve the newly rendered visible content before the
                    // mask modifies the framebuffer for the panel.
                    restoreFrameRegion(lockSnapshot, physicalPixels,
                                       nativeRenderXBegin, nativeRenderXEnd,
                                       nativeRenderBegin, nativeRenderEnd);
                    lockSnapshotValid = true;
                }
            }
#if STICKMON_ENABLE_DEBUG_FEATURES
            const int64_t renderFinishedUs = esp_timer_get_time();
            const int64_t transferStartedUs = renderFinishedUs;
#endif
            if (lockFrame) {
                drawLockMask(canvas,
                             lockFrameFocusX,
                             lockFrameFocusY,
                             lockFrameRadius,
                             nativeRenderXBegin, nativeRenderXEnd,
                             nativeRenderBegin, nativeRenderEnd);
                result = submitFrameRegion(
                    panel, physicalPixels, transferBuffers, transferDone,
                    nativeRenderXBegin, nativeRenderXEnd,
                    nativeRenderBegin, nativeRenderEnd);
            } else {
                result = submitFrame(
                    panel, physicalPixels, transferBuffers, transferDone,
                    renderBegin, renderEnd);
            }
#if STICKMON_ENABLE_DEBUG_FEATURES
            const int64_t transferFinishedUs = esp_timer_get_time();
            if (profileExploreFrame) {
                exploreFramePerf.record(
                    loopStartedUs, transferFinishedUs, updateUs, clawUs,
                    static_cast<uint32_t>(renderFinishedUs - renderStartedUs),
                    static_cast<uint32_t>(transferFinishedUs -
                                          transferStartedUs),
                    static_cast<uint16_t>(renderEnd - renderBegin));
            }
#endif
            if (result != ESP_OK) {
                ESP_LOGE(TAG, "Frame update failed: %s",
                         esp_err_to_name(result));
            } else {
                app.markRendered();
#if STICKMON_ENABLE_DEBUG_FEATURES
                if (!lockFrame &&
                    app.encounterFirstFramePendingForDiagnostics()) {
                    app.markEncounterFirstFramePresented(
                        static_cast<uint32_t>(renderFinishedUs - renderStartedUs),
                        static_cast<uint32_t>(transferFinishedUs - transferStartedUs));
                }
#endif
                if (lockFrame) {
                    // Keep the exact center used to draw this frame. The
                    // next dirty region is the union of the old and new
                    // circles, so a fixed fallback center would overdraw
                    // needlessly and could leave a stale edge when the pet
                    // moves during the lock animation.
                    lastLockFocusX = lockFrameFocusX;
                    lastLockFocusY = lockFrameFocusY;
                    lastLockRadius = lockFrameRadius;
                    lastLockSleeping = lockSleeping;
                    lockVisualValid = true;
                    if (lockFinalCleanupDue) {
                        lockFinalCleanupPending = false;
                    }
                }
            }
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (!app.exploreRouteMovingForDiagnostics()) {
            exploreFramePerf.flush(esp_timer_get_time(), true);
        }
#endif
        const uint32_t targetLoopMs = lockAnimating
            ? LOCK_ANIMATION_FRAME_MS : 20U;
        const uint32_t loopElapsedMs = millisNow() - nowMs;
        if (loopElapsedMs < targetLoopMs) {
            vTaskDelay(pdMS_TO_TICKS(targetLoopMs - loopElapsedMs));
        } else {
            taskYIELD();
        }
    }
}
