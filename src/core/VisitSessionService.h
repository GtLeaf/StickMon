#pragma once

#include <cstdint>

#include "game/GameState.h"
#include "hardware/EspNowLink.h"

namespace Communication {

class VisitSessionService {
public:
    static constexpr uint16_t VISIT_DURATION_SEC = 120;

    enum class State : uint8_t {
        IDLE = 0,
        HOSTING,
        SEARCHING,
        JOINING,
        WAITING_HOST_DECISION,
        SYNCING,
        WAITING_ACCEPT,
        ACTIVE,
        ENDING,
        FAILED,
        ENDED,
    };

    struct RemotePet {
        bool known = false;
        uint16_t speciesId = 0;
        uint8_t level = 0;
        uint8_t nature = 0;
        uint8_t satiety = 0;
        uint8_t mood = 0;
        uint8_t affection = 0;
    };

    struct ViewModel {
        State state = State::IDLE;
        uint8_t roomCount = 0;
        EspNowLink::RoomEntry rooms[EspNowLink::MAX_ROOMS] = {};
        bool incomingRequest = false;
        bool localIsHost = false;
        RemotePet remote;
        uint16_t remainSec = 0;
        const char* error = nullptr;
    };

    void attach(Game::GameState* state) { gameState_ = state; }

    void startHost();
    void startSearch();
    bool selectRoom(uint8_t index);
    void acceptIncoming(bool accepted);
    void endVisit();
    void stop();
    void update(uint32_t nowMs);

    bool active() const { return state_ == State::ACTIVE; }
    bool busy() const { return state_ != State::IDLE && state_ != State::ENDED; }
    State state() const { return state_; }
    ViewModel viewModel() const;

private:
    bool localSync(VisitSyncPayload& payload) const;
    void queueMessage(LinkMessageType type, const void* payload,
                      uint8_t payloadLen);
    void queueLocalSync();
    void fail(const char* message);
    void activate(uint32_t nowMs);
    void processIncoming(uint32_t nowMs);
    void pumpOutgoing();

    Game::GameState* gameState_ = nullptr;
    State state_ = State::IDLE;
    RemotePet remote_;
    uint32_t lastPeerMs_ = 0;
    uint32_t nextPingMs_ = 0;
    uint32_t activeUntilMs_ = 0;
    uint16_t remainSec_ = 0;
    uint8_t pendingJoinMac_[6] = {};
    uint16_t pendingJoinSeq_ = 0;
    bool incomingRequest_ = false;
    bool localIsHost_ = false;
    bool syncReceived_ = false;
    bool localSyncSent_ = false;
    bool acceptSent_ = false;
    bool finishAfterSend_ = false;
    bool queuedMessage_ = false;
    LinkMessageType queuedType_ = LinkMessageType::PING;
    uint8_t queuedPayloadLen_ = 0;
    uint8_t queuedPayload_[24] = {};
    const char* error_ = nullptr;
};

}  // namespace Communication
