#include "brain/BrainBridge.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include "freertos/semphr.h"

namespace {

using Stickmon::BrainBridge;

struct HostState {
    std::mutex mutex;
    std::condition_variable cv;
    int buyCalls = 0;
    bool blockBuy = false;
    bool buyEntered = false;
    bool releaseBuy = false;
    bool accept = true;

    bool buy() {
        std::unique_lock<std::mutex> lock(mutex);
        ++buyCalls;
        if (blockBuy) {
            blockBuy = false;
            buyEntered = true;
            cv.notify_all();
            cv.wait(lock, [&] { return releaseBuy; });
        }
        return accept;
    }

    bool waitBuyEntered() {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, std::chrono::seconds(2), [&] {
            return buyEntered;
        });
    }

    void release() {
        std::lock_guard<std::mutex> lock(mutex);
        releaseBuy = true;
        cv.notify_all();
    }

    int calls() {
        std::lock_guard<std::mutex> lock(mutex);
        return buyCalls;
    }
};

bool snapshot(BrainBridge::Snapshot&, void*) {
    return true;
}

bool buyFood(uint8_t, void* userCtx) {
    return static_cast<HostState*>(userCtx)->buy();
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

BrainBridge::Request buyRequest(uint32_t requestId) {
    BrainBridge::Request request{};
    request.action = BrainBridge::Action::BUY_FOOD;
    request.foodIndex = 2;
    request.requestId = requestId;
    return request;
}

void expectAccepted(BrainBridge& bridge, HostState& host,
                    const BrainBridge::Request& request) {
    BrainBridge::Result result{};
    std::thread updater([&] { bridge.update(10); });
    const bool accepted = bridge.enqueue(request, result, 1000);
    updater.join();
    require(accepted && result.accepted, "ordinary purchase should be accepted");
    require(std::strcmp(result.text, "Food purchased") == 0,
            "ordinary purchase should return its own result");
    (void)host;
}

void testQueuedTimeout(BrainBridge& bridge, HostState& host) {
    BrainBridge::Result result{};
    require(!bridge.enqueue(buyRequest(1), result, 5),
            "queued timeout should return false");
    require(std::strcmp(result.text, "StickMon action timed out") == 0,
            "queued timeout should report a timeout");
    bridge.update(10);
    require(host.calls() == 0, "a cancelled queued request must not execute");
}

void testRunningTimeout(BrainBridge& bridge, HostState& host) {
    {
        std::lock_guard<std::mutex> lock(host.mutex);
        host.blockBuy = true;
        host.buyEntered = false;
        host.releaseBuy = false;
    }
    const int givesBefore = host_sem::gives();
    BrainBridge::Result result{};
    bool accepted = true;
    std::thread waiter([&] {
        accepted = bridge.enqueue(buyRequest(2), result, 20);
    });
    std::thread updater([&] { bridge.update(20); });
    require(host.waitBuyEntered(), "purchase should enter the host callback");
    waiter.join();
    require(!accepted, "running timeout should return false to its waiter");
    require(std::strcmp(result.text, "StickMon action timed out") == 0,
            "running timeout should report an unknown outcome as timeout");
    host.release();
    updater.join();
    require(host.calls() == 1, "running request should complete exactly once");
    require(host_sem::gives() == givesBefore,
            "a detached request must not leave a stale notification");
    bridge.update(30);
    require(host.calls() == 1, "a completed detached slot must not run again");
    expectAccepted(bridge, host, buyRequest(3));
    require(host.calls() == 2, "a later legal purchase must still execute");
}

void testCompletionRace(BrainBridge& bridge, HostState& host) {
    host_sem::blockNext();
    BrainBridge::Result firstResult{};
    bool firstAccepted = false;
    std::thread firstWaiter([&] {
        firstAccepted = bridge.enqueue(buyRequest(4), firstResult, 20);
    });
    std::thread updater([&] { bridge.update(40); });
    require(host_sem::waitGiveEntered(), "update should reach result notification");
    firstWaiter.join();
    require(firstAccepted && firstResult.accepted,
            "a result ready at the timeout boundary must be returned");
    require(std::strcmp(firstResult.text, "Food purchased") == 0,
            "completion race must preserve the original result");

    BrainBridge::Result secondResult{};
    bool secondAccepted = false;
    std::thread secondWaiter([&] {
        secondAccepted = bridge.enqueue(buyRequest(5), secondResult, 1000);
    });
    host_sem::release();
    updater.join();
    bridge.update(50);
    secondWaiter.join();
    require(secondAccepted && secondResult.accepted,
            "notification handoff must not corrupt the next request");
    require(std::strcmp(secondResult.text, "Food purchased") == 0,
            "next request must receive its own result");
    require(host.calls() == 4, "completion race must not duplicate purchases");
}

void testBusyAndRejectedResult(BrainBridge& bridge, HostState& host) {
    {
        std::lock_guard<std::mutex> lock(host.mutex);
        host.blockBuy = true;
        host.buyEntered = false;
        host.releaseBuy = false;
    }
    BrainBridge::Result results[4]{};
    bool accepted[4] = {true, true, true, true};
    std::thread waiters[4];
    for (int index = 0; index < 4; ++index) {
        waiters[index] = std::thread([&, index] {
            accepted[index] = bridge.enqueue(
                buyRequest(static_cast<uint32_t>(10 + index)),
                results[index], 1000);
        });
    }
    std::thread updater([&] { bridge.update(60); });
    require(host.waitBuyEntered(), "busy test should enter its first purchase");
    BrainBridge::Result busy{};
    require(!bridge.enqueue(buyRequest(20), busy, 0),
            "fifth queued request should be rejected while all slots are used");
    require(std::strcmp(busy.text, "StickMon brain is busy") == 0,
            "busy result should be explicit");
    host.release();
    updater.join();
    for (auto& waiter : waiters) waiter.join();
    for (bool value : accepted) require(value, "queued purchases should complete");
    require(host.calls() == 8, "busy handling must preserve four distinct calls");

    {
        std::lock_guard<std::mutex> lock(host.mutex);
        host.accept = false;
    }
    BrainBridge::Result rejected{};
    std::thread rejectUpdater([&] { bridge.update(70); });
    require(!bridge.enqueue(buyRequest(30), rejected, 1000),
            "host rejection should remain a rejected result");
    rejectUpdater.join();
    require(std::strcmp(rejected.text, "Food could not be purchased") == 0,
            "host rejection text should survive queue handoff");
    require(host.calls() == 9, "rejected host call should execute only once");
}

}  // namespace

int main() {
    BrainBridge& bridge = BrainBridge::instance();
    HostState host{};
    BrainBridge::HostAdapter adapter{};
    adapter.snapshot = snapshot;
    adapter.buyFood = buyFood;
    adapter.userCtx = &host;
    bridge.setHost(adapter);
    require(bridge.begin(), "bridge should begin");

    testQueuedTimeout(bridge, host);
    testRunningTimeout(bridge, host);
    testCompletionRace(bridge, host);
    testBusyAndRejectedResult(bridge, host);
    host_sem::reset();
    std::puts("BrainBridge queue checks passed");
    return 0;
}
