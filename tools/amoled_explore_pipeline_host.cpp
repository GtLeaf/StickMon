#include <array>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include "ExploreFramePipeline.h"

struct Queue {
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<std::vector<unsigned char>> items;
    size_t size, capacity;
    Queue(unsigned count, size_t itemSize): size(itemSize), capacity(count) {}
};
QueueHandle_t xQueueCreate(unsigned count, size_t size) {
    assert(count >= 1);
    return new Queue(count, size);
}
void vQueueDelete(QueueHandle_t handle) { delete static_cast<Queue*>(handle); }
int xQueueSend(QueueHandle_t handle, const void* data, unsigned wait) {
    auto& q = *static_cast<Queue*>(handle);
    std::unique_lock<std::mutex> lock(q.mutex);
    if (!wait && q.items.size() == q.capacity) return 0;
    q.changed.wait(lock, [&] { return q.items.size() < q.capacity; });
    const auto* bytes = static_cast<const unsigned char*>(data);
    q.items.emplace_back(bytes, bytes + q.size);
    q.changed.notify_all();
    return pdTRUE;
}
int xQueueReceive(QueueHandle_t handle, void* data, unsigned wait) {
    auto& q = *static_cast<Queue*>(handle);
    std::unique_lock<std::mutex> lock(q.mutex);
    if (!wait && q.items.empty()) return 0;
    q.changed.wait(lock, [&] { return !q.items.empty(); });
    std::memcpy(data, q.items.front().data(), q.size);
    q.items.pop_front();
    q.changed.notify_all();
    return pdTRUE;
}
// Counting semaphore for the pipeline's call handshake and call lock. The
// display completion handle is a Display; `kind` tells them apart.
struct Semaphore {
    int kind = 1;
    std::mutex mutex;
    std::condition_variable changed;
    int count = 0;
};
SemaphoreHandle_t xSemaphoreCreateBinary() { return new Semaphore; }
SemaphoreHandle_t xSemaphoreCreateMutex() { auto* s = new Semaphore; s->count = 1; return s; }
void vSemaphoreDelete(SemaphoreHandle_t handle) { delete static_cast<Semaphore*>(handle); }
int xSemaphoreGive(SemaphoreHandle_t handle) {
    auto& s = *static_cast<Semaphore*>(handle);
    std::lock_guard<std::mutex> lock(s.mutex);
    ++s.count;
    s.changed.notify_all();
    return pdTRUE;
}
thread_local TaskHandle_t currentTask = nullptr;
TaskHandle_t xTaskGetCurrentTaskHandle() { return currentTask; }
struct TaskStart { void (*run)(void*); void* context; TaskHandle_t handle; };
int xTaskCreatePinnedToCore(void (*run)(void*), const char*, unsigned,
                           void* context, unsigned, TaskHandle_t* handle, int) {
    auto* start = new TaskStart{run, context, new int(0)};
    if (handle) *handle = start->handle;
    std::thread([start] {
        currentTask = start->handle;
        start->run(start->context);
    }).detach();
    return pdPASS;
}
int64_t esp_timer_get_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
constexpr int W = 38, H = 46;
struct Transfer {
    const uint16_t* source;
    Stickmon::FrameRect bounds;
    std::vector<uint16_t> snapshot;
};
struct Display {
    int kind = 0;
    std::mutex mutex;
    std::condition_variable changed;
    bool hold = false, blocked = false, timeout = false;
    int calls = 0, failAt = -1;
    std::deque<Transfer> pending;
    std::array<uint16_t, W*H> pixels{};
};
int xSemaphoreTake(SemaphoreHandle_t handle, unsigned) {
    if (*static_cast<int*>(handle) == 1) {
        auto& s = *static_cast<Semaphore*>(handle);
        std::unique_lock<std::mutex> lock(s.mutex);
        s.changed.wait(lock, [&] { return s.count > 0; });
        --s.count;
        return pdTRUE;
    }
    auto& display = *static_cast<Display*>(handle);
    std::unique_lock<std::mutex> lock(display.mutex);
    if (display.timeout) return 0;
    display.blocked = true;
    display.changed.notify_all();
    display.changed.wait(lock, [&] { return !display.hold; });
    assert(!display.pending.empty());
    const auto work = display.pending.front();
    display.pending.pop_front();
    assert(std::equal(work.snapshot.begin(), work.snapshot.end(), work.source));
    const auto b = work.bounds;
    for (int y = b.top; y < b.bottom; ++y)
        std::copy_n(work.source + (y-b.top)*(b.right-b.left), b.right-b.left,
                    display.pixels.data() + y*W+b.left);
    return pdTRUE;
}
int submit(void* context, int x0, int y0, int x1, int y1, const uint16_t* data) {
    auto& display = *static_cast<Display*>(context);
    std::lock_guard<std::mutex> lock(display.mutex);
    assert(x0 % 2 == 0 && x1 % 2 == 0 && y0 % 2 == 0 && y1 % 2 == 0);
    if (display.calls++ == display.failAt) return ESP_FAIL;
    for (const auto& pending : display.pending) assert(pending.source != data);
    display.pending.push_back({data, {x0,y0,x1,y1},
        std::vector<uint16_t>(data, data + (x1-x0)*(y1-y0))});
    return ESP_OK;
}
int main() {
    // Deliberately leaked: the production worker lives until device reset.
    auto* display = new Display;
    auto* pipeline = new AmoledV2::ExploreFramePipeline;
    auto* buffers = new uint16_t*[2]{new uint16_t[W*8], new uint16_t[W*8]};
    // Panel calls before start() run inline on the caller.
    assert(pipeline->run([](void*) { return xTaskGetCurrentTaskHandle() ? ESP_FAIL : ESP_OK; },
                         nullptr) == ESP_OK);
    assert(pipeline->start());
    assert(pipeline->configureFrames(W, buffers, 2, 8, display, submit, display));
    // Panel calls run on the panel task, never on the caller.
    static TaskHandle_t panelThread = nullptr;
    assert(pipeline->run([](void*) {
        panelThread = xTaskGetCurrentTaskHandle();
        return 7;
    }, nullptr) == 7);
    assert(panelThread != nullptr && panelThread != xTaskGetCurrentTaskHandle());
    auto* a = new uint16_t[W*H]{};
    auto* b = new uint16_t[W*H]{};
    auto* front = a;
    auto* back = b;
    std::array<uint16_t, W*H> expected{};
    Stickmon::FrameRect previous{0,0,W,H};
    AmoledV2::ExploreFramePipeline::Result result;
    const auto finish = [&] {
        assert(pipeline->takeResult(result, portMAX_DELAY));
        assert(result.error == ESP_OK);
        assert(display->pixels == expected);
        assert(display->pending.empty());
    };
    // A full frame is in flight while the UI prepares another image.
    display->hold = true;
    assert(pipeline->submitFrame({front,{0,0,W,H}}));
    {
        std::unique_lock<std::mutex> lock(display->mutex);
        display->changed.wait(lock, [&] { return display->blocked; });
    }
    assert(!pipeline->takeResult(result, 0));
    assert(!pipeline->submitFrame({front,{0,0,W,H}}));
    // A panel call queued behind the in-flight frame runs only after it.
    static Display* callDisplay = display;
    static bool callSawPending = true;
    std::thread caller([&] {
        assert(pipeline->run([](void*) {
            std::lock_guard<std::mutex> lock(callDisplay->mutex);
            callSawPending = !callDisplay->pending.empty();
            return ESP_OK;
        }, nullptr) == ESP_OK);
    });
    std::fill_n(back, W*H, 0x55aa);
    assert(std::all_of(front, front+W*H, [](uint16_t p) { return p==0; }));
    {
        std::lock_guard<std::mutex> lock(display->mutex);
        display->hold = false;
        display->changed.notify_all();
    }
    finish();
    caller.join();
    assert(!callSawPending);
    // Random, disjoint, nested, shrinking, HUD-only and full updates.
    uint32_t random = 214;
    for (int frame = 1; frame <= 400; ++frame) {
        const auto next = [&] { random = random*1664525U+1013904223U; return random; };
        int x0 = 2*(next()%(W/2)), x1 = 2*(next()%(W/2)+1);
        int y0 = 2*(next()%(H/2)), y1 = 2*(next()%(H/2)+1);
        if (x0 >= x1) { x0=0; x1=W; }
        if (y0 >= y1) { y0=0; y1=H; }
        const Stickmon::FrameRect damage{x0,y0,x1,y1};
        Stickmon::syncFrameOutsideDamage(back,front,W,previous,damage);
        for (int y=y0;y<y1;++y) for (int x=x0;x<x1;++x)
            expected[y*W+x]=back[y*W+x]=uint16_t(frame*31+x+y);
        assert(std::equal(expected.begin(),expected.end(),back));
        std::swap(front,back);
        assert(pipeline->submitFrame({front,damage}));
        finish();
        previous=damage;
    }
    // A failed submission drains earlier strips before returning ownership.
    display->calls=0;display->failAt=1;
    assert(pipeline->submitFrame({front,{0,0,W,H}}));
    assert(pipeline->takeResult(result,portMAX_DELAY) && result.error==ESP_FAIL);
    assert(display->pending.empty());
    // Timeout explicitly leaves DMA-owned memory untouched; no retry follows.
    display->calls=0;display->failAt=-1;display->timeout=true;
    assert(pipeline->submitFrame({front,{0,0,W,H}}));
    assert(pipeline->takeResult(result,portMAX_DELAY) && result.error==ESP_ERR_TIMEOUT);
    assert(display->pending.size()==2);
}
