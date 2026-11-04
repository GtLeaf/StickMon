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
    std::vector<unsigned char> bytes;
    bool full = false;
    explicit Queue(size_t size): bytes(size) {}
};
QueueHandle_t xQueueCreate(unsigned count, size_t size) {
    assert(count == 1);
    return new Queue(size);
}
void vQueueDelete(QueueHandle_t handle) { delete static_cast<Queue*>(handle); }
int xQueueSend(QueueHandle_t handle, const void* data, unsigned wait) {
    auto& q = *static_cast<Queue*>(handle);
    std::unique_lock<std::mutex> lock(q.mutex);
    if (!wait && q.full) return 0;
    q.changed.wait(lock, [&] { return !q.full; });
    std::memcpy(q.bytes.data(), data, q.bytes.size());
    q.full = true;
    q.changed.notify_all();
    return pdTRUE;
}
int xQueueReceive(QueueHandle_t handle, void* data, unsigned wait) {
    auto& q = *static_cast<Queue*>(handle);
    std::unique_lock<std::mutex> lock(q.mutex);
    if (!wait && !q.full) return 0;
    q.changed.wait(lock, [&] { return q.full; });
    std::memcpy(data, q.bytes.data(), q.bytes.size());
    q.full = false;
    q.changed.notify_all();
    return pdTRUE;
}
int xTaskCreatePinnedToCore(void (*run)(void*), const char*, unsigned,
                           void* context, unsigned, void*, int) {
    std::thread(run, context).detach();
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
    std::mutex mutex;
    std::condition_variable changed;
    bool hold = false, blocked = false, timeout = false;
    int calls = 0, failAt = -1;
    std::deque<Transfer> pending;
    std::array<uint16_t, W*H> pixels{};
};
int xSemaphoreTake(SemaphoreHandle_t handle, unsigned) {
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
    assert(pipeline->begin(W, buffers, 2, 8, display, submit, display));
    auto* a = new uint16_t[W*H]{};
    auto* b = new uint16_t[W*H]{};
    auto* front = a;
    auto* back = b;
    std::array<uint16_t, W*H> expected{};
    Stickmon::FrameRect previous{0,0,W,H};
    AmoledV2::ExploreFramePipeline::Result result;
    const auto finish = [&] {
        assert(pipeline->takeResult(result, true));
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
    assert(!pipeline->takeResult(result, false));
    assert(!pipeline->submitFrame({front,{0,0,W,H}}));
    std::fill_n(back, W*H, 0x55aa);
    assert(std::all_of(front, front+W*H, [](uint16_t p) { return p==0; }));
    {
        std::lock_guard<std::mutex> lock(display->mutex);
        display->hold = false;
        display->changed.notify_all();
    }
    finish();
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
    assert(pipeline->takeResult(result,true) && result.error==ESP_FAIL);
    assert(display->pending.empty());
    // Timeout explicitly leaves DMA-owned memory untouched; no retry follows.
    display->calls=0;display->failAt=-1;display->timeout=true;
    assert(pipeline->submitFrame({front,{0,0,W,H}}));
    assert(pipeline->takeResult(result,true) && result.error==ESP_ERR_TIMEOUT);
    assert(display->pending.size()==2);
}
