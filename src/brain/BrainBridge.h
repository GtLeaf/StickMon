#pragma once

#include <cstddef>
#include <cstdint>

#if defined(STICKMON_CLAW_INTERNAL)
#include "claw_core.h"
#endif
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace Stickmon {

class BrainBridge {
public:
    enum class Action : uint8_t {
        SAY = 0,
        START_EXPEDITION,
        RETURN_HOME,
        INVITE_FRIEND,
        EAT,
        BUY_FOOD,
    };

    struct Request {
        Action action = Action::SAY;
        uint8_t area = 0;
        uint8_t foodIndex = 0;
        uint32_t requestId = 0;
        bool autonomous = false;
        char text[161] = {};
    };

    struct Result {
        bool accepted = false;
        char text[192] = {};
    };

    struct Snapshot {
        bool initialized = false;
        bool oobeDone = false;
        bool visitActive = false;
        uint8_t teamCount = 0;
        uint16_t speciesId = 0;
        char speciesName[32] = {};
        uint8_t nature = 0;
        char natureName[24] = {};
        uint8_t gender = 0;
        uint8_t level = 0;
        uint16_t hp = 0;
        uint16_t hpMax = 0;
        uint8_t satiety = 0;
        uint8_t mood = 0;
        int8_t battery = -1;
        uint8_t explorePhase = 0;
        uint8_t exploreArea = 0;
        uint8_t unlockedArea = 0;
        bool playerActive = false;
        bool agentAllowed = false;
        bool actionLocked = false;
        uint32_t idleSeconds = 0;
        uint32_t coins = 0;
        uint8_t foodCounts[7] = {};
        uint8_t inventoryCounts[32] = {};
        uint8_t bowlFood = 0;
        uint8_t bowlBitesRemaining = 0;
        uint32_t updatedMs = 0;
    };

    // The host owns the game state. BrainBridge only schedules requests and
    // asks the host to read or mutate that state on the UI task.
    struct HostAdapter {
        bool (*snapshot)(Snapshot& out, void* userCtx) = nullptr;
        bool (*startExpedition)(uint8_t area, void* userCtx) = nullptr;
        bool (*returnHome)(void* userCtx) = nullptr;
        bool (*inviteFriend)(void* userCtx) = nullptr;
        bool (*eat)(void* userCtx) = nullptr;
        bool (*buyFood)(uint8_t foodIndex, void* userCtx) = nullptr;
        bool (*say)(const char* text, void* userCtx) = nullptr;
        void* userCtx = nullptr;
    };

    static BrainBridge& instance();

    bool begin();
    void update(uint32_t nowMs);
    void setHost(const HostAdapter& adapter);
    // waitMs must comfortably outlast a slow UI frame (render/decode spikes);
    // the LLM-side HTTP timeout is 30000 ms, so 5000 ms leaves ample headroom.
    bool enqueue(const Request& request, Result& result,
                 uint32_t waitMs = 5000);
    bool snapshot(Snapshot& out) const;
    void setAgentAllowed(bool allowed);
    void setRuntimeState(bool playerActive, uint32_t idleSeconds,
                         bool agentAllowed);
    // True while the UI task is executing an autonomous capability.
    bool executingAutonomous() const;
    void rememberChatId(const char* channel, const char* chatId);
    bool latestChatId(char* chatId, size_t chatIdSize) const;
    void setAllowedChatId(const char* chatId);
    bool isChatAuthorized(const char* chatId) const;

#if defined(STICKMON_CLAW_INTERNAL)
    static esp_err_t collectContext(const claw_core_request_t* request,
                                    claw_core_context_t* outContext,
                                    void* userCtx);
#endif

private:
    static constexpr size_t SLOT_COUNT = 4;

    struct Slot {
        bool inUse = false;
        bool processing = false;
        bool waiter = false;
        Request request{};
        Result result{};
        SemaphoreHandle_t done = nullptr;
    };

    BrainBridge() = default;
    BrainBridge(const BrainBridge&) = delete;
    BrainBridge& operator=(const BrainBridge&) = delete;

    void refreshSnapshot(uint32_t nowMs);
    Result execute(const Request& request);
    static void setResult(Result& result, bool accepted, const char* text);
    HostAdapter hostAdapter() const;

    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    Slot slots_[SLOT_COUNT]{};
    Snapshot snapshot_{};
    HostAdapter host_{};
    char latestChatId_[96] = {};
    char allowedChatId_[96] = {};
    bool agentAllowed_ = false;
    bool executingAutonomous_ = false;
    bool begun_ = false;
};

}  // namespace Stickmon
