#include "core/VisitSessionService.h"

#include <algorithm>
#include <cstring>

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
    EspNowLink::ins().startHost(EspNowLink::RoomPurpose::VISIT);
    state_ = State::HOSTING;
    localIsHost_ = true;
    error_ = nullptr;
}

void VisitSessionService::startSearch() {
    stop();
    EspNowLink::ins().startSearch(EspNowLink::RoomPurpose::VISIT);
    state_ = State::SEARCHING;
    localIsHost_ = false;
    error_ = nullptr;
}

bool VisitSessionService::selectRoom(uint8_t index) {
    if (state_ != State::SEARCHING || !EspNowLink::ins().sendJoinRequest(index)) {
        return false;
    }
    state_ = State::JOINING;
    error_ = nullptr;
    return true;
}

void VisitSessionService::acceptIncoming(bool accepted) {
    if (state_ != State::WAITING_HOST_DECISION || !incomingRequest_) return;
    bool sent = EspNowLink::ins().sendJoinAck(
        pendingJoinMac_, accepted, pendingJoinSeq_);
    incomingRequest_ = false;
    if (!sent) {
        fail("JOIN FAILED");
        return;
    }
    if (!accepted) {
        state_ = State::HOSTING;
        return;
    }
    state_ = State::SYNCING;
    queueLocalSync();
}

void VisitSessionService::endVisit() {
    if (!EspNowLink::ins().connected()) {
        stop();
        state_ = State::ENDED;
        return;
    }
    VisitEndPayload payload{0};
    queueMessage(LinkMessageType::VISIT_END, &payload, sizeof(payload));
    finishAfterSend_ = true;
    state_ = State::ENDING;
}

void VisitSessionService::stop() {
    EspNowLink::ins().stopRoom();
    state_ = State::IDLE;
    remote_ = RemotePet{};
    lastPeerMs_ = 0;
    nextPingMs_ = 0;
    activeUntilMs_ = 0;
    remainSec_ = 0;
    std::memset(pendingJoinMac_, 0, sizeof(pendingJoinMac_));
    pendingJoinSeq_ = 0;
    incomingRequest_ = false;
    localIsHost_ = false;
    syncReceived_ = false;
    localSyncSent_ = false;
    acceptSent_ = false;
    finishAfterSend_ = false;
    queuedMessage_ = false;
    queuedPayloadLen_ = 0;
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

void VisitSessionService::queueMessage(LinkMessageType type, const void* payload,
                                       uint8_t payloadLen) {
    if (payloadLen > sizeof(queuedPayload_) ||
        (payloadLen > 0 && !payload)) return;
    queuedType_ = type;
    queuedPayloadLen_ = payloadLen;
    if (payloadLen > 0) std::memcpy(queuedPayload_, payload, payloadLen);
    queuedMessage_ = true;
}

void VisitSessionService::queueLocalSync() {
    VisitSyncPayload payload{};
    if (localSync(payload)) {
        queueMessage(LinkMessageType::VISIT_SYNC, &payload, sizeof(payload));
    }
}

void VisitSessionService::fail(const char* message) {
    EspNowLink::ins().stopRoom();
    state_ = State::FAILED;
    error_ = message;
    queuedMessage_ = false;
    finishAfterSend_ = false;
}

void VisitSessionService::activate(uint32_t nowMs) {
    state_ = State::ACTIVE;
    activeUntilMs_ = nowMs + VISIT_DURATION_SEC * 1000UL;
    remainSec_ = VISIT_DURATION_SEC;
    lastPeerMs_ = nowMs;
    nextPingMs_ = nowMs;
    error_ = nullptr;
}

void VisitSessionService::processIncoming(uint32_t nowMs) {
    EspNowLink& link = EspNowLink::ins();
    EspNowLink::RoomPurpose requestPurpose = EspNowLink::RoomPurpose::VISIT;
    if (state_ == State::HOSTING && link.takeJoinRequest(
            pendingJoinMac_, requestPurpose, pendingJoinSeq_)) {
        // The purpose is already constrained by startHost(VISIT). Avoid
        // accepting silently in the transport layer: the UI must confirm it.
        if (requestPurpose == EspNowLink::RoomPurpose::VISIT) {
            incomingRequest_ = true;
            state_ = State::WAITING_HOST_DECISION;
        }
    }

    bool accepted = false;
    if (state_ == State::JOINING && link.takeJoinAck(accepted)) {
        if (!accepted) {
            fail("JOIN DECLINED");
        } else {
            state_ = State::SYNCING;
            queueLocalSync();
        }
    }

    LinkMessageType type = LinkMessageType::PING;
    uint8_t payload[24] = {};
    uint8_t payloadLen = 0;
    while (link.takeSessionMessage(type, payload, payloadLen)) {
        lastPeerMs_ = nowMs;
        if (type == LinkMessageType::VISIT_SYNC &&
            payloadLen == sizeof(VisitSyncPayload)) {
            VisitSyncPayload sync{};
            std::memcpy(&sync, payload, sizeof(sync));
            remote_.known = true;
            remote_.speciesId = sync.speciesId;
            remote_.level = sync.level;
            remote_.nature = sync.nature;
            remote_.satiety = sync.satiety;
            remote_.mood = sync.mood;
            remote_.affection = sync.affection;
            syncReceived_ = true;
            if (localIsHost_ && !acceptSent_) {
                VisitAcceptPayload accept{1, 0};
                queueMessage(LinkMessageType::VISIT_ACCEPT,
                             &accept, sizeof(accept));
                acceptSent_ = true;
                activate(nowMs);
            } else if (!localIsHost_ && state_ == State::SYNCING) {
                state_ = State::WAITING_ACCEPT;
            }
        } else if (type == LinkMessageType::VISIT_ACCEPT &&
                   payloadLen == sizeof(VisitAcceptPayload)) {
            VisitAcceptPayload accept{};
            std::memcpy(&accept, payload, sizeof(accept));
            if (accept.accepted != 0) activate(nowMs);
            else fail("VISIT DECLINED");
        } else if (type == LinkMessageType::VISIT_PING &&
                   payloadLen == sizeof(VisitPingPayload)) {
            VisitPingPayload ping{};
            std::memcpy(&ping, payload, sizeof(ping));
            remote_.satiety = ping.satiety;
            remote_.mood = ping.mood;
            if (state_ == State::ACTIVE) {
                VisitStatusPayload status{
                    1, static_cast<uint16_t>(remainSec_)};
                queueMessage(LinkMessageType::VISIT_STATUS,
                             &status, sizeof(status));
            }
        } else if (type == LinkMessageType::VISIT_STATUS &&
                   payloadLen == sizeof(VisitStatusPayload)) {
            VisitStatusPayload status{};
            std::memcpy(&status, payload, sizeof(status));
            remainSec_ = status.remainSec;
            if (status.active == 0) state_ = State::ENDED;
        } else if (type == LinkMessageType::VISIT_END) {
            EspNowLink::ins().stopRoom();
            state_ = State::ENDED;
            error_ = nullptr;
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
        if (finishAfterSend_) {
            finishAfterSend_ = false;
            link.stopRoom();
            state_ = State::ENDED;
            return;
        }
    }
    if (!queuedMessage_ || link.sessionSendBusy() || !link.connected()) return;
    if (link.sendSessionMessage(queuedType_, queuedPayload_, queuedPayloadLen_)) {
        queuedMessage_ = false;
        localSyncSent_ = localSyncSent_ ||
            queuedType_ == LinkMessageType::VISIT_SYNC;
    }
}

void VisitSessionService::update(uint32_t nowMs) {
    EspNowLink::ins().update();
    if (state_ == State::IDLE || state_ == State::FAILED ||
        state_ == State::ENDED) return;

    processIncoming(nowMs);
    if (state_ == State::ACTIVE) {
        if (static_cast<int32_t>(nowMs - activeUntilMs_) >= 0) {
            endVisit();
        } else {
            remainSec_ = static_cast<uint16_t>(
                (activeUntilMs_ - nowMs + 999UL) / 1000UL);
            if (static_cast<int32_t>(nowMs - nextPingMs_) >= 0 &&
                !queuedMessage_) {
                VisitPingPayload ping{};
                if (gameState_ && gameState_->teamCount > 0) {
                    ping.satiety = gameState_->team[0].satiety;
                    ping.mood = gameState_->team[0].mood;
                }
                queueMessage(LinkMessageType::VISIT_PING,
                             &ping, sizeof(ping));
                nextPingMs_ = nowMs + PING_INTERVAL_MS;
            }
        }
        if (nowMs - lastPeerMs_ > PEER_TIMEOUT_MS) {
            fail("LINK LOST");
            return;
        }
    }
    pumpOutgoing();
}

VisitSessionService::ViewModel VisitSessionService::viewModel() const {
    ViewModel model;
    model.state = state_;
    model.incomingRequest = incomingRequest_;
    model.localIsHost = localIsHost_;
    model.remote = remote_;
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
