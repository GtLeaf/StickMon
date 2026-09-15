#pragma once

#include <cstdint>

#include "core/VisitTypes.h"
#include "game/GameState.h"
#include "hardware/EspNowLink.h"

namespace Communication {

class VisitSessionService {
public:
    static constexpr uint16_t VISIT_DURATION_SEC = 1800;
    static constexpr uint32_t HOST_TIMEOUT_MS = 30000;
    static constexpr uint32_t SEARCH_TIMEOUT_MS = 8000;
    static constexpr uint32_t JOIN_TIMEOUT_MS = 3000;
    static constexpr uint32_t HANDSHAKE_TIMEOUT_MS = 5000;
    static constexpr uint32_t HOST_DECISION_TIMEOUT_MS = 30000;

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
        uint8_t roomId = 0;
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
    enum class SendCompletion : uint8_t {
        NONE,
        ACTIVATE,
        FINISH,
    };

    bool localSync(VisitSyncPayload& payload) const;
    void queueMessage(LinkMessageType type, const void* payload,
                      uint8_t payloadLen,
                      SendCompletion completion = SendCompletion::NONE);
    void queueLocalSync();
    void fail(const char* message);
    void activate(uint32_t nowMs);
    void setState(State state, uint32_t nowMs);
    bool attachRemoteVisitor(const VisitSyncPayload& sync);
    void detachRemoteVisitor();
    void finish();
    void processIncoming(uint32_t nowMs);
    void pumpOutgoing();

    Game::GameState* gameState_ = nullptr;
    State state_ = State::IDLE;
    RemotePet remote_;
    uint32_t lastPeerMs_ = 0;
    uint32_t nextPingMs_ = 0;
    uint32_t activeUntilMs_ = 0;
    uint32_t stateStartedMs_ = 0;
    uint16_t remainSec_ = 0;
    uint8_t pendingJoinMac_[6] = {};
    uint16_t pendingJoinSeq_ = 0;
    bool incomingRequest_ = false;
    bool localIsHost_ = false;
    bool visitorAttached_ = false;
    bool queuedMessage_ = false;
    LinkMessageType queuedType_ = LinkMessageType::PING;
    uint8_t queuedPayloadLen_ = 0;
    uint8_t queuedPayload_[24] = {};
    SendCompletion queuedCompletion_ = SendCompletion::NONE;
    SendCompletion inFlightCompletion_ = SendCompletion::NONE;
    const char* error_ = nullptr;
};

}  // namespace Communication
