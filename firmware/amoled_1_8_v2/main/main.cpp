#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>

#include "AmoledApp.h"
#include "AmoledPlatform.h"
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
constexpr uint32_t LOCK_ANIMATION_MS = 1000;
constexpr uint32_t LOCK_WAKE_GRACE_MS = 1200;
constexpr int LOCK_START_RADIUS = 260;
constexpr int LOCK_FINAL_RADIUS = 33;
enum class LockPhase : uint8_t { OPEN, CLOSING, LOCKED, OPENING };
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

void scaleStripe2xRegion(const uint16_t* source, uint16_t sourceY,
                         uint16_t logicalRows, uint16_t xBegin,
                         uint16_t xEnd, uint16_t* destination) {
    uint16_t regionWidth = xEnd - xBegin;
    uint16_t physicalRegionWidth = regionWidth * 2U;
    for (uint16_t row = 0; row < logicalRows; ++row) {
        const uint16_t* sourceRow =
            source + static_cast<size_t>(sourceY + row) * LOGICAL_WIDTH + xBegin;
        uint16_t* top = destination +
            static_cast<size_t>(row * 2U) * physicalRegionWidth;
        uint16_t* bottom = top + physicalRegionWidth;
        for (uint16_t x = 0; x < regionWidth; ++x) {
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
                        "CO5300 initialization failed");
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

esp_err_t submitFrameRegion(esp_lcd_panel_handle_t panel,
                            const uint16_t* logicalPixels,
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

    uint16_t physicalXBegin = xBegin * 2U;
    uint16_t physicalXEnd = xEnd * 2U;
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
        uint16_t physicalY = sourceY * 2U;
        uint16_t physicalRows = logicalRows * 2U;
        scaleStripe2xRegion(logicalPixels, sourceY, logicalRows,
                            xBegin, xEnd, transferBuffers[nextBuffer]);
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
    constexpr int FEATHER_PIXELS = 4;
    int innerRadius = std::max(0, radius - FEATHER_PIXELS);
    int64_t outerRadiusSquared = static_cast<int64_t>(radius) * radius;
    int64_t innerRadiusSquared = static_cast<int64_t>(innerRadius) * innerRadius;
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
        for (int x = left; x <= right; ++x) {
            int dx = x - centerX;
            int64_t distanceSquared = static_cast<int64_t>(dx) * dx +
                                      static_cast<int64_t>(dy) * dy;
            if (distanceSquared <= innerRadiusSquared) {
                continue;
            }
            if (distanceSquared >= outerRadiusSquared) {
                canvas.drawPixel(x, y, 0);
                continue;
            }
            uint16_t color = canvas.readPixel(x, y);
            int64_t distanceInside = outerRadiusSquared - distanceSquared;
            int factor = static_cast<int>(
                distanceInside * 255 /
                std::max<int64_t>(1, outerRadiusSquared - innerRadiusSquared));
            int red = ((color >> 11) & 0x1F) * factor / 255;
            int green = ((color >> 5) & 0x3F) * factor / 255;
            int blue = (color & 0x1F) * factor / 255;
            canvas.drawPixel(x, y, static_cast<uint16_t>(
                (red << 11) | (green << 5) | blue));
        }
    }
}

int lockRadius(LockPhase phase, uint32_t nowMs,
               uint32_t animationStartedMs, bool preserveFocus) {
    int finalRadius = preserveFocus ? LOCK_FINAL_RADIUS : 0;
    if (phase == LockPhase::LOCKED) return finalRadius;
    uint32_t elapsed = nowMs - animationStartedMs;
    float progress = std::min(
        1.0f, static_cast<float>(elapsed) / LOCK_ANIMATION_MS);
    // Smoothstep gives both lock and wake animations a symmetric
    // ease-in/ease-out profile without adding a floating-point dependency.
    float eased = progress * progress * (3.0f - 2.0f * progress);
    if (phase == LockPhase::OPENING) {
        return static_cast<int>(std::lround(
            finalRadius +
            (LOCK_START_RADIUS - finalRadius) * eased));
    }
    return static_cast<int>(std::lround(
        LOCK_START_RADIUS -
        (LOCK_START_RADIUS - finalRadius) * eased));
}

AmoledV1::TouchEvent toAppTouchEvent(const AmoledV2::TouchEvent& event) {
    AmoledV1::TouchEvent appEvent{};
    appEvent.x = event.x;
    appEvent.y = event.y;
    appEvent.timestampMs = event.timestampMs;
    switch (event.type) {
    case AmoledV2::TouchEventType::DOWN:
        appEvent.type = AmoledV1::TouchEventType::DOWN;
        break;
    case AmoledV2::TouchEventType::MOVE:
        appEvent.type = AmoledV1::TouchEventType::MOVE;
        break;
    case AmoledV2::TouchEventType::UP:
        appEvent.type = AmoledV1::TouchEventType::UP;
        break;
    }
    return appEvent;
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
    AmoledV2::bindAmoledPlatform();
    if (!AmoledV2::AmoledPlatform::instance().begin()) {
        ESP_LOGW(TAG, "AMOLED platform peripheral init incomplete");
    }
    PixelRenderer::bind(frameBuffer);
    AmoledV1::AmoledApp app;
    app.begin(millisNow());
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

    AmoledV2::TouchInput touch;
    bool touchReady = touch.begin() == ESP_OK;
    if (!touchReady) {
        ESP_LOGE(TAG, "CST820 touch initialization failed");
    }

    LockPhase lockPhase = LockPhase::OPEN;
    uint32_t lockAnimationStartedMs = 0;
    uint32_t lockWakeGraceUntilMs = 0;
    int16_t lastLockFocusX = 92;
    int16_t lastLockFocusY = 112;
    int lastLockRadius = LOCK_FINAL_RADIUS;
    bool lockVisualValid = false;
    bool lockHasFocus = false;
    uint8_t lockedBrightness =
        AmoledV2::AmoledPlatform::instance().brightness();
    ESP_LOGI(TAG, "Interactive home screen presented at 368x448");
#if STICKMON_HAS_CLAW
    Stickmon::ClawRuntime::instance().beginAsync();
#endif
    while (true) {
        uint32_t nowMs = millisNow();
        AmoledV2::TouchEvent event;
        if (touchReady && touch.poll(nowMs, event)) {
#if STICKMON_HAS_CLAW
            Stickmon::ClawRuntime::instance().notePlayerActivity(nowMs);
#endif
            if (event.type == AmoledV2::TouchEventType::DOWN) {
                ESP_LOGI(TAG, "Touch down logical=(%d,%d)", event.x, event.y);
            }
            if (lockPhase != LockPhase::OPEN) {
                if (event.type == AmoledV2::TouchEventType::DOWN) {
                    AmoledV2::AmoledPlatform::instance().setBrightness(
                        lockedBrightness);
                    if (lockPhase != LockPhase::OPENING) {
                        lockPhase = LockPhase::OPENING;
                        lockAnimationStartedMs = nowMs;
                        AudioManager::ins().setMusicSuspended(false);
                    }
                    app.onWake(nowMs);
                    lockWakeGraceUntilMs = nowMs + LOCK_WAKE_GRACE_MS;
                    ESP_LOGI(TAG, "Display unlocked by touch");
                }
            } else {
                app.handleTouch(toAppTouchEvent(event));
            }
        }

        app.update(nowMs);
#if STICKMON_HAS_CLAW
        Stickmon::ClawRuntime::instance().update(nowMs);
#endif
        bool lockRequest = app.consumeLockRequest();
        if (lockPhase == LockPhase::OPEN && lockRequest &&
            static_cast<int32_t>(nowMs - lockWakeGraceUntilMs) >= 0) {
            lockedBrightness =
                AmoledV2::AmoledPlatform::instance().brightness();
            lockAnimationStartedMs = nowMs;
            lockPhase = LockPhase::CLOSING;
            lockVisualValid = false;
            int16_t focusX = 92;
            int16_t focusY = 112;
            lockHasFocus = app.lockFocusPoint(focusX, focusY);
            AudioManager::ins().setMusicSuspended(true);
            app.forceFullRender();
            ESP_LOGI(TAG, "Display lock animation started");
        }
        if (lockPhase == LockPhase::CLOSING &&
            nowMs - lockAnimationStartedMs >= LOCK_ANIMATION_MS) {
            lockPhase = LockPhase::LOCKED;
            ESP_LOGI(TAG, "Display locked; touch to wake");
        }
        if (lockPhase == LockPhase::OPENING &&
            nowMs - lockAnimationStartedMs >= LOCK_ANIMATION_MS) {
            lockPhase = LockPhase::OPEN;
            lockVisualValid = false;
            lockHasFocus = false;
            app.forceFullRender();
            ESP_LOGI(TAG, "Display fully unlocked");
        }

        bool lockedWithoutFocus =
            lockPhase == LockPhase::LOCKED && !lockHasFocus;
        bool renderNeeded = (!lockedWithoutFocus && app.needsRender()) ||
                            lockPhase == LockPhase::CLOSING ||
                            lockPhase == LockPhase::OPENING;
        if (lockPhase != LockPhase::OPEN) {
            int16_t focusX = 92;
            int16_t focusY = 112;
            if (lockHasFocus) app.lockFocusPoint(focusX, focusY);
            int radius = lockRadius(lockPhase, nowMs,
                                    lockAnimationStartedMs, lockHasFocus);
            renderNeeded = renderNeeded || !lockVisualValid ||
                           focusX != lastLockFocusX ||
                           focusY != lastLockFocusY || radius != lastLockRadius;
        }
        if (renderNeeded) {
            bool lockFrame = lockPhase != LockPhase::OPEN;
            uint16_t renderBegin = app.renderRowBegin();
            uint16_t renderEnd = app.renderRowEnd();
            uint16_t renderXBegin = 0;
            uint16_t renderXEnd = LOGICAL_WIDTH;
            if (lockFrame) {
                int16_t focusX = 92;
                int16_t focusY = 112;
                if (lockHasFocus) app.lockFocusPoint(focusX, focusY);
                int radius = lockRadius(lockPhase, nowMs,
                                        lockAnimationStartedMs, lockHasFocus);
                int oldLeft = lockVisualValid
                    ? lastLockFocusX - lastLockRadius : focusX - radius;
                int oldRight = lockVisualValid
                    ? lastLockFocusX + lastLockRadius : focusX + radius;
                int oldTop = lockVisualValid
                    ? lastLockFocusY - lastLockRadius : focusY - radius;
                int oldBottom = lockVisualValid
                    ? lastLockFocusY + lastLockRadius : focusY + radius;
                renderXBegin = static_cast<uint16_t>(std::clamp(
                    std::min(oldLeft, static_cast<int>(focusX - radius)),
                    0, static_cast<int>(LOGICAL_WIDTH)));
                renderXEnd = static_cast<uint16_t>(std::clamp(
                    std::max(oldRight, static_cast<int>(focusX + radius)) + 1,
                    0, static_cast<int>(LOGICAL_WIDTH)));
                renderBegin = static_cast<uint16_t>(std::clamp(
                    std::min(oldTop, static_cast<int>(focusY - radius)),
                    0, static_cast<int>(LOGICAL_HEIGHT)));
                renderEnd = static_cast<uint16_t>(std::clamp(
                    std::max(oldBottom, static_cast<int>(focusY + radius)) + 1,
                    0, static_cast<int>(LOGICAL_HEIGHT)));
                app.forceRenderRows(renderBegin, renderEnd);
            }
            app.render(canvas);
            if (lockFrame) {
                int16_t focusX = 92;
                int16_t focusY = 112;
                if (lockHasFocus) app.lockFocusPoint(focusX, focusY);
                int radius = lockRadius(lockPhase, nowMs,
                                        lockAnimationStartedMs, lockHasFocus);
                drawLockMask(canvas, focusX, focusY, radius,
                             renderXBegin, renderXEnd,
                             renderBegin, renderEnd);
                result = submitFrameRegion(
                    panel, logicalPixels, transferBuffers, transferDone,
                    renderXBegin, renderXEnd, renderBegin, renderEnd);
            } else {
                result = submitFrame(
                    panel, logicalPixels, transferBuffers, transferDone,
                    renderBegin, renderEnd);
            }
            if (result != ESP_OK) {
                ESP_LOGE(TAG, "Frame update failed: %s",
                         esp_err_to_name(result));
            } else {
                app.markRendered();
                if (lockFrame) {
                    lastLockFocusX = 92;
                    lastLockFocusY = 112;
                    if (lockHasFocus) {
                        app.lockFocusPoint(lastLockFocusX, lastLockFocusY);
                    }
                    lastLockRadius = lockRadius(lockPhase, nowMs,
                                                lockAnimationStartedMs,
                                                lockHasFocus);
                    lockVisualValid = true;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(lockPhase == LockPhase::OPEN ? 20 : 16));
    }
}
