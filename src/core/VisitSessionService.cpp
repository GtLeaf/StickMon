#include "core/VisitSessionService.h"

#include <algorithm>
#include <cstring>

#include "game/MonsterFactory.h"
#include "platform/api/PlatformServices.h"

namespace Communication {
namespace {

constexpr uint32_t PING_INTERVAL_MS = 2000;
constexpr uint32_t PEER_TIMEOUT_MS = 7000;

}  // namespace

void VisitSessionService::startHost() {
    stop();
    if (!gameState_ || gameState_->teamCount == 0) {
        fail("NO PET");
        return;
    }
    if (gameState_->teamCount != 1) {
        fail("TEAM NOT SOLO");
        return;
    }
    EspNowLink::ins().startHost(EspNowLink::RoomPurpose::VISIT);
    if (!EspNowLink::ins().isEnabled()) {
        fail("LINK UNAVAILABLE");
        return;
    }
    localIsHost_ = true;
    error_ = nullptr;
    setState(State::HOSTING, Platform::clock().millis());
}

void VisitSessionService::startSearch() {
    stop();
    if (!gameState_ || gameState_->teamCount == 0) {
        fail("NO PET");
        return;
    }
    EspNowLink::ins().startSearch(EspNowLink::RoomPurpose::VISIT);
    if (!EspNowLink::ins().isEnabled()) {
        fail("LINK UNAVAILABLE");
        return;
    }
    localIsHost_ = false;
    error_ = nullptr;
    setState(State::SEARCHING, Platform::clock().millis());
}

bool VisitSessionService::selectRoom(uint8_t index) {
    if (state_ != State::SEARCHING ||
        !EspNowLink::ins().sendJoinRequest(index)) {
        return false;
    }
    setState(State::JOINING, Platform::clock().millis());
    error_ = nullptr;
    return true;
}

void VisitSessionService::acceptIncoming(bool accepted) {
    if (state_ != State::WAITING_HOST_DECISION || !incomingRequest_) return;
    const uint32_t nowMs = Platform::clock().millis();
    bool sent = EspNowLink::ins().sendJoinAck(
        pendingJoinMac_, accepted, pendingJoinSeq_);
    incomingRequest_ = false;
    if (!sent) {
        fail("JOIN FAILED");
        return;
    }
    if (!accepted) {
        setState(State::HOSTING, nowMs);
        return;
    }
    setState(State::SYNCING, nowMs);
}

void VisitSessionService::endVisit() {
    if (state_ == State::ENDING) return;
    if (!EspNowLink::ins().connected()) {
        finish();
        return;
    }
    if (localIsHost_) {
        VisitEndPayload payload{0};
        queueMessage(LinkMessageType::VISIT_END, &payload, sizeof(payload),
                     SendCompletion::FINISH);
        detachRemoteVisitor();
    } else {
        queueMessage(LinkMessageType::VISIT_RECALL, nullptr, 0,
                     SendCompletion::FINISH);
    }
    setState(State::ENDING, Platform::clock().millis());
}

void VisitSessionService::stop() {
    detachRemoteVisitor();
    EspNowLink::ins().stopRoom();
    state_ = State::IDLE;
    remote_ = RemotePet{};
    lastPeerMs_ = 0;
    nextPingMs_ = 0;
    activeUntilMs_ = 0;
    stateStartedMs_ = 0;
    remainSec_ = 0;
    std::memset(pendingJoinMac_, 0, sizeof(pendingJoinMac_));
    pendingJoinSeq_ = 0;
    incomingRequest_ = false;
    localIsHost_ = false;
    visitorAttached_ = false;
    queuedMessage_ = false;
    queuedPayloadLen_ = 0;
    queuedCompletion_ = SendCompletion::NONE;
    inFlightCompletion_ = SendCompletion::NONE;
    error_ = nullptr;
}

bool VisitSessionService::localSync(VisitSyncPayload& payload) const {
    if (!gameState_ || gameState_->teamCount == 0) return false;
    const Game::MonsterRuntime& pet = gameState_->team[0];
    payload.speciesId = pet.speciesId;
    payload.level = pet.level;
    payload.nature = pet.nature;
    payload.satiety = pet.satiety;
    payload.mood = pet.mood;
    payload.affection = pet.affection;
    return true;
}

void VisitSessionService::queueMessage(LinkMessageType type,
                                       const void* payload,
                                       uint8_t payloadLen,
                                       SendCompletion completion) {
    if (payloadLen > sizeof(queuedPayload_) ||
        (payloadLen > 0 && !payload)) {
        return;
    }
    queuedType_ = type;
    queuedPayloadLen_ = payloadLen;
    if (payloadLen > 0) {
        std::memcpy(queuedPayload_, payload, payloadLen);
    }
    queuedMessage_ = true;
    queuedCompletion_ = completion;
}

void VisitSessionService::queueLocalSync() {
    VisitSyncPayload payload{};
    if (localSync(payload)) {
        queueMessage(LinkMessageType::VISIT_SYNC, &payload, sizeof(payload));
    }
}

void VisitSessionService::fail(const char* message) {
    detachRemoteVisitor();
    EspNowLink::ins().stopRoom();
    state_ = State::FAILED;
    error_ = message;
    queuedMessage_ = false;
    queuedCompletion_ = SendCompletion::NONE;
    inFlightCompletion_ = SendCompletion::NONE;
}

void VisitSessionService::activate(uint32_t nowMs) {
    setState(State::ACTIVE, nowMs);
    activeUntilMs_ = localIsHost_
        ? nowMs + VISIT_DURATION_SEC * 1000UL
        : 0;
    remainSec_ = VISIT_DURATION_SEC;
    lastPeerMs_ = nowMs;
    nextPingMs_ = nowMs;
    error_ = nullptr;
}

void VisitSessionService::setState(State state, uint32_t nowMs) {
    state_ = state;
    stateStartedMs_ = nowMs;
}

bool VisitSessionService::attachRemoteVisitor(const VisitSyncPayload& sync) {
    if (!gameState_ || gameState_->teamCount != 1) return false;
    gameState_->team[1] = Game::MonsterFactory::create(sync.speciesId,
                                                       sync.level);
    Game::MonsterRuntime& visitor = gameState_->team[1];
    visitor.nature = sync.nature;
    visitor.satiety = sync.satiety;
    visitor.mood = sync.mood;
    visitor.affection = sync.affection;
    visitor.origin = Game::Origin::VISITOR;
    gameState_->teamCount = 2;
    gameState_->activeSlot = 0;
    visitorAttached_ = true;
    return true;
}

void VisitSessionService::detachRemoteVisitor() {
    if (!visitorAttached_ || !gameState_) return;
    if (gameState_->teamCount >= 2 &&
        gameState_->team[1].origin == Game::Origin::VISITOR) {
        gameState_->team[1] = Game::MonsterRuntime{};
        gameState_->teamCount = 1;
        gameState_->activeSlot = 0;
    }
    visitorAttached_ = false;
}

void VisitSessionService::finish() {
    detachRemoteVisitor();
    EspNowLink::ins().stopRoom();
    state_ = State::ENDED;
    queuedMessage_ = false;
    queuedCompletion_ = SendCompletion::NONE;
    inFlightCompletion_ = SendCompletion::NONE;
    error_ = nullptr;
}

void VisitSessionService::processIncoming(uint32_t nowMs) {
    EspNowLink& link = EspNowLink::ins();
    EspNowLink::RoomPurpose requestPurpose = EspNowLink::RoomPurpose::VISIT;
    if (state_ == State::HOSTING && link.takeJoinRequest(
            pendingJoinMac_, requestPurpose, pendingJoinSeq_)) {
        if (requestPurpose == EspNowLink::RoomPurpose::VISIT) {
            incomingRequest_ = true;
            setState(State::WAITING_HOST_DECISION, nowMs);
        }
    }

    bool accepted = false;
    if (state_ == State::JOINING && link.takeJoinAck(accepted)) {
        if (!accepted) {
            fail("JOIN DECLINED");
        } else {
            queueLocalSync();
            setState(State::WAITING_ACCEPT, nowMs);
        }
    }

    LinkMessageType type = LinkMessageType::PING;
    uint8_t payload[24] = {};
    uint8_t payloadLen = 0;
    while (link.takeSessionMessage(type, payload, payloadLen)) {
        lastPeerMs_ = nowMs;
        if (type == LinkMessageType::VISIT_SYNC &&
            payloadLen == sizeof(VisitSyncPayload) && localIsHost_ &&
            state_ == State::SYNCING) {
            VisitSyncPayload sync{};
            std::memcpy(&sync, payload, sizeof(sync));
            remote_.known = true;
            remote_.speciesId = sync.speciesId;
            remote_.level = sync.level;
            remote_.nature = sync.nature;
            remote_.satiety = sync.satiety;
            remote_.mood = sync.mood;
            remote_.affection = sync.affection;

            VisitHostResult result = VisitHostResult::ACCEPTED;
            if (!gameState_ || gameState_->teamCount == 0) {
                result = VisitHostResult::NO_MONSTER;
            } else if (gameState_->teamCount != 1) {
                result = VisitHostResult::TEAM_NOT_SOLO;
            } else if (!attachRemoteVisitor(sync)) {
                result = VisitHostResult::TEAM_NOT_SOLO;
            }
            VisitAcceptPayload response{
                result == VisitHostResult::ACCEPTED ? uint8_t{1} : uint8_t{0},
                static_cast<uint8_t>(result)};
            queueMessage(LinkMessageType::VISIT_ACCEPT, &response,
                         sizeof(response),
                         response.accepted ? SendCompletion::ACTIVATE
                                           : SendCompletion::FINISH);
            if (!response.accepted) setState(State::ENDING, nowMs);
        } else if (type == LinkMessageType::VISIT_ACCEPT &&
                   payloadLen == sizeof(VisitAcceptPayload) &&
                   !localIsHost_ && state_ == State::WAITING_ACCEPT) {
            VisitAcceptPayload response{};
            std::memcpy(&response, payload, sizeof(response));
            if (response.accepted != 0) {
                activate(nowMs);
            } else {
                switch (static_cast<VisitHostResult>(response.reason)) {
                case VisitHostResult::STORAGE_FULL:
                    fail("HOST FULL");
                    break;
                case VisitHostResult::TEAM_NOT_SOLO:
                    fail("HOST TEAM BUSY");
                    break;
                case VisitHostResult::NO_MONSTER:
                default:
                    fail("HOST NO PET");
                    break;
                }
            }
        } else if (type == LinkMessageType::VISIT_PING &&
                   payloadLen == sizeof(VisitPingPayload) && localIsHost_ &&
                   state_ == State::ACTIVE) {
            VisitPingPayload ping{};
            std::memcpy(&ping, payload, sizeof(ping));
            remote_.satiety = ping.satiety;
            remote_.mood = ping.mood;
            if (visitorAttached_ && gameState_ &&
                gameState_->teamCount >= 2 &&
                gameState_->team[1].origin == Game::Origin::VISITOR) {
                gameState_->team[1].satiety = ping.satiety;
                gameState_->team[1].mood = ping.mood;
            }
        } else if (type == LinkMessageType::VISIT_STATUS &&
                   payloadLen == sizeof(VisitStatusPayload) &&
                   !localIsHost_ && state_ == State::ACTIVE) {
            VisitStatusPayload status{};
            std::memcpy(&status, payload, sizeof(status));
            remainSec_ = status.remainSec;
            if (status.active == 0) finish();
        } else if (type == LinkMessageType::VISIT_RECALL && localIsHost_ &&
                   state_ == State::ACTIVE) {
            VisitEndPayload end{0};
            queueMessage(LinkMessageType::VISIT_END, &end, sizeof(end),
                         SendCompletion::FINISH);
            detachRemoteVisitor();
            setState(State::ENDING, nowMs);
        } else if (type == LinkMessageType::VISIT_END && !localIsHost_) {
            finish();
        }
    }
}

void VisitSessionService::pumpOutgoing() {
    EspNowLink& link = EspNowLink::ins();
    bool sendResult = false;
    if (link.takeSessionSendResult(sendResult)) {
        if (!sendResult) {
            fail("LINK TIMEOUT");
            return;
        }
        SendCompletion completion = inFlightCompletion_;
        inFlightCompletion_ = SendCompletion::NONE;
        if (completion == SendCompletion::ACTIVATE) {
            activate(Platform::clock().millis());
        } else if (completion == SendCompletion::FINISH) {
            finish();
            return;
        }
    }
    if (!queuedMessage_ || link.sessionSendBusy() || !link.connected()) return;
    if (link.sendSessionMessage(queuedType_, queuedPayload_,
                                queuedPayloadLen_)) {
        queuedMessage_ = false;
        inFlightCompletion_ = queuedCompletion_;
        queuedCompletion_ = SendCompletion::NONE;
    }
}

void VisitSessionService::update(uint32_t nowMs) {
    EspNowLink::ins().update();
    if (state_ == State::IDLE || state_ == State::FAILED ||
        state_ == State::ENDED) {
        return;
    }

    processIncoming(nowMs);
    if (state_ == State::ACTIVE) {
        if (localIsHost_ &&
            static_cast<int32_t>(nowMs - activeUntilMs_) >= 0) {
            endVisit();
        } else {
            if (localIsHost_) {
                remainSec_ = static_cast<uint16_t>(
                    (activeUntilMs_ - nowMs + 999UL) / 1000UL);
            }
            if (static_cast<int32_t>(nowMs - nextPingMs_) >= 0 &&
                !queuedMessage_) {
                if (localIsHost_) {
                    VisitStatusPayload status{1, remainSec_};
                    queueMessage(LinkMessageType::VISIT_STATUS, &status,
                                 sizeof(status));
                } else {
                    VisitPingPayload ping{};
                    if (gameState_ && gameState_->teamCount > 0) {
                        ping.satiety = gameState_->team[0].satiety;
                        ping.mood = gameState_->team[0].mood;
                    }
                    queueMessage(LinkMessageType::VISIT_PING, &ping,
                                 sizeof(ping));
                }
                nextPingMs_ = nowMs + PING_INTERVAL_MS;
            }
        }
        if (state_ == State::ACTIVE &&
            nowMs - lastPeerMs_ > PEER_TIMEOUT_MS) {
            fail("LINK LOST");
            return;
        }
    } else {
        uint32_t timeoutMs = 0;
        const char* timeoutError = "LINK TIMEOUT";
        switch (state_) {
        case State::HOSTING:
            timeoutMs = HOST_TIMEOUT_MS;
            timeoutError = "HOST TIMEOUT";
            break;
        case State::SEARCHING:
            timeoutMs = SEARCH_TIMEOUT_MS;
            timeoutError = "SEARCH TIMEOUT";
            break;
        case State::JOINING:
            timeoutMs = JOIN_TIMEOUT_MS;
            break;
        case State::SYNCING:
        case State::WAITING_ACCEPT:
            timeoutMs = HANDSHAKE_TIMEOUT_MS;
            break;
        case State::WAITING_HOST_DECISION:
            if (nowMs - stateStartedMs_ >= HOST_DECISION_TIMEOUT_MS) {
                EspNowLink::ins().sendJoinAck(
                    pendingJoinMac_, false, pendingJoinSeq_);
                incomingRequest_ = false;
                setState(State::HOSTING, nowMs);
            }
            break;
        case State::ENDING:
            if (nowMs - stateStartedMs_ >= HANDSHAKE_TIMEOUT_MS) {
                finish();
                return;
            }
            break;
        default:
            break;
        }
        if (timeoutMs > 0 && nowMs - stateStartedMs_ >= timeoutMs) {
            fail(timeoutError);
            return;
        }
    }
    pumpOutgoing();
}

VisitSessionService::ViewModel VisitSessionService::viewModel() const {
    ViewModel model;
    model.state = state_;
    if (localIsHost_ && state_ != State::IDLE) {
        model.roomId = EspNowLink::ins().currentRoomId();
    }
    model.incomingRequest = incomingRequest_;
    model.localIsHost = localIsHost_;
    model.remote = remote_;
    if (!localIsHost_ && gameState_ && gameState_->teamCount > 0 &&
        (state_ == State::ACTIVE || state_ == State::ENDING)) {
        const Game::MonsterRuntime& pet = gameState_->team[0];
        model.remote.known = true;
        model.remote.speciesId = pet.speciesId;
        model.remote.level = pet.level;
        model.remote.nature = pet.nature;
        model.remote.satiety = pet.satiety;
        model.remote.mood = pet.mood;
        model.remote.affection = pet.affection;
    }
    model.remainSec = remainSec_;
    model.error = error_;
    if (state_ == State::SEARCHING || state_ == State::JOINING) {
        EspNowLink::RoomEntry room;
        for (uint8_t index = 0; index < EspNowLink::MAX_ROOMS; ++index) {
            if (!EspNowLink::ins().copyRoomAt(index, room)) break;
            model.rooms[model.roomCount++] = room;
        }
    }
    return model;
}

}  // namespace Communication
