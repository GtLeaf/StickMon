#include "hardware/EspNowLink.h"
#include "platform/api/PlatformServices.h"

constexpr uint8_t EspNowLink::BROADCAST_MAC[6];

EspNowLink& EspNowLink::ins() {
    static EspNowLink instance;
    return instance;
}

bool EspNowLink::begin() {
    if (enabled) return true;
    if (!Platform::peers().enable()) {
        enabled = false;
        mode = Mode::OFF;
        return false;
    }
    enabled = true;
    mode = Mode::OFF;
    return true;
}

void EspNowLink::end() {
    if (!isEnabled()) return;
    Platform::peers().end();
    enabled = false;
    mode = Mode::OFF;
    roomCountValue = 0;
    pendingJoin = false;
    pendingAck = false;
    awaitingAck = false;
    resetSessionStateLocked();
}

bool EspNowLink::beginStub() {
    enabled = false;
    mode = Mode::OFF;
    seq = 1;
    roomCountValue = 0;
    pendingJoin = false;
    pendingAck = false;
    awaitingAck = false;
    resetSessionStateLocked();
    return true;
}

bool EspNowLink::isEnabled() const {
    bool value = enabled;
    return value;
}

EspNowLink::Mode EspNowLink::currentMode() const {
    Mode value = mode;
    return value;
}

uint16_t EspNowLink::nextSeq() {
    uint16_t value = seq++;
    if (seq == 0) seq = 1;
    return value;
}

void EspNowLink::startHost(RoomPurpose purpose) {
    if (!begin()) return;
    activePurpose = purpose;
    roomId++;
    if (roomId == 0) roomId = 1;
    mode = Mode::HOSTING;
    lastAdvertMs = 0;
    pendingJoin = false;
    pendingAck = false;
    awaitingAck = false;
    resetSessionStateLocked();
}

void EspNowLink::startSearch(RoomPurpose purpose) {
    if (!begin()) return;
    activePurpose = purpose;
    mode = Mode::SEARCHING;
    roomCountValue = 0;
    pendingAck = false;
    awaitingAck = false;
    resetSessionStateLocked();
}

void EspNowLink::stopRoom() {
    mode = Mode::OFF;
    roomCountValue = 0;
    pendingJoin = false;
    pendingAck = false;
    awaitingAck = false;
    resetSessionStateLocked();
}

void EspNowLink::update() {
    if (!isEnabled()) return;
    uint32_t now = Platform::clock().millis();

    Platform::PeerPacket received;
    while (Platform::peers().receive(received)) {
        handleReceive(received.source, received.payload,
                      static_cast<int>(received.length));
    }

    bool advertise = false;
    RoomPurpose advertisePurpose = RoomPurpose::BATTLE;
    uint8_t advertiseRoomId = 0;
    bool retransmit = false;
    TrackedSessionSend retransmitSnapshot;
    uint8_t retransmitMac[6] = {};
    if (mode == Mode::HOSTING && now - lastAdvertMs >= 500) {
        lastAdvertMs = now;
        advertise = true;
        advertisePurpose = activePurpose;
        advertiseRoomId = roomId;
    }

    for (uint8_t i = 0; i < roomCountValue;) {
        if (now - rooms[i].lastSeenMs > 3000) {
            for (uint8_t j = i + 1; j < roomCountValue; ++j) rooms[j - 1] = rooms[j];
            roomCountValue--;
        } else {
            i++;
        }
    }

    if (tracked.active && now - tracked.lastSendMs >= SESSION_RETRANSMIT_MS) {
        if (tracked.attempts > SESSION_MAX_ATTEMPTS) {
            tracked.active = false;
            sessionResultReady = true;
            sessionResultSuccess = false;
        } else {
            tracked.attempts++;
            tracked.lastSendMs = now;
            retransmit = true;
            retransmitSnapshot = tracked;
            memcpy(retransmitMac, peer, 6);
        }
    }

    if (advertise) {
        sendPacket(BROADCAST_MAC, LinkMessageType::HELLO, advertisePurpose, advertiseRoomId, 0);
    }
    if (retransmit) {
        sendSessionFrame(retransmitMac, retransmitSnapshot.type, retransmitSnapshot.seq,
                         sessionId(), retransmitSnapshot.payload, retransmitSnapshot.payloadLen);
    }
}

uint8_t EspNowLink::roomCount() const {
    uint8_t count = roomCountValue;
    return count;
}

bool EspNowLink::copyRoomAt(uint8_t index, RoomEntry& out) const {
    bool found = index < roomCountValue;
    if (found) out = rooms[index];
    return found;
}

bool EspNowLink::sendJoinRequest(uint8_t index) {
    RoomEntry room;
    if (!copyRoomAt(index, room)) return false;
    uint16_t requestSeq = nextSeq();
    activePurpose = room.purpose;
    memcpy(expectedAckMac, room.mac, sizeof(expectedAckMac));
    expectedAckRoomId = room.roomId;
    expectedAckSeq = requestSeq;
    awaitingAck = true;
    pendingAck = false;
    bool sent = sendPacket(room.mac, LinkMessageType::JOIN_REQ, room.purpose,
                           room.roomId, requestSeq);
    if (!sent) {
        awaitingAck = false;
    }
    return sent;
}

bool EspNowLink::takeJoinRequest(uint8_t outMac[6], RoomPurpose& outPurpose, uint16_t& outRequestSeq) {
    bool available = pendingJoin;
    if (available) {
        memcpy(outMac, pendingJoinMac, 6);
        outPurpose = pendingJoinPurpose;
        outRequestSeq = pendingJoinSeq;
        pendingJoin = false;
    }
    return available;
}

bool EspNowLink::sendJoinAck(const uint8_t mac[6], bool accepted, uint16_t requestSeq) {
    RoomPurpose purpose = activePurpose;
    uint8_t localRoomId = roomId;
    if (accepted) {
        memcpy(peer, mac, 6);
        mode = Mode::CONNECTED;
        sessionIdValue = requestSeq;
    }
    bool sent = sendPacket(mac, LinkMessageType::JOIN_ACK, purpose,
                           localRoomId, requestSeq, accepted ? 1 : 0);
    if (!sent && accepted) {
        if (mode == Mode::CONNECTED && memcmp(peer, mac, 6) == 0) mode = Mode::HOSTING;
    }
    return sent;
}

bool EspNowLink::takeJoinAck(bool& accepted) {
    bool available = pendingAck;
    if (available) {
        accepted = pendingAckAccepted;
        pendingAck = false;
        if (accepted) mode = Mode::CONNECTED;
    }
    return available;
}

bool EspNowLink::connected() const {
    return currentMode() == Mode::CONNECTED;
}

bool EspNowLink::copyPeerMac(uint8_t outMac[6]) const {
    if (!outMac) return false;
    bool available = mode == Mode::CONNECTED;
    if (available) memcpy(outMac, peer, 6);
    return available;
}

void EspNowLink::handleReceive(const uint8_t* mac, const uint8_t* data, int len) {
    if (!mac || !data) return;
    if (len == (int)sizeof(SessionFrame)) {
        handleSessionFrame(mac, data, len);
        return;
    }
    if (len != (int)sizeof(WirePacket)) return;
    WirePacket packet{};
    memcpy(&packet, data, sizeof(packet));
    if (packet.magic != 0x5AA5 || packet.version != 0x03) return;
    if (packet.purpose > static_cast<uint8_t>(RoomPurpose::VISIT)) return;

    RoomPurpose purpose = (RoomPurpose)packet.purpose;
    uint32_t now = Platform::clock().millis();
    if (packet.type == LinkMessageType::HELLO && mode == Mode::SEARCHING &&
        purpose == activePurpose && packet.roomId != 0) {
        rememberRoomLocked(mac, packet.roomId, purpose, now);
    } else if (packet.type == LinkMessageType::JOIN_REQ && mode == Mode::HOSTING &&
               purpose == activePurpose && packet.roomId == roomId &&
               packet.requestSeq != 0 && !pendingJoin) {
        memcpy(pendingJoinMac, mac, 6);
        pendingJoinPurpose = purpose;
        pendingJoinSeq = packet.requestSeq;
        pendingJoin = true;
    } else if (packet.type == LinkMessageType::JOIN_ACK && mode == Mode::SEARCHING &&
               awaitingAck && purpose == activePurpose &&
               packet.roomId == expectedAckRoomId && packet.requestSeq == expectedAckSeq &&
               memcmp(mac, expectedAckMac, 6) == 0) {
        pendingAck = true;
        pendingAckAccepted = packet.accepted != 0;
        awaitingAck = false;
        if (pendingAckAccepted) {
            memcpy(peer, mac, 6);
            sessionIdValue = expectedAckSeq;
        }
    }
}

bool EspNowLink::sendPacket(const uint8_t mac[6], LinkMessageType type, RoomPurpose purpose,
                            uint8_t packetRoomId, uint16_t requestSeq, uint8_t accepted) {
    if (!isEnabled()) return false;
    WirePacket packet{};
    packet.magic = 0x5AA5;
    packet.version = 0x03;
    packet.type = type;
    packet.purpose = (uint8_t)purpose;
    packet.roomId = packetRoomId;
    packet.requestSeq = requestSeq;
    packet.accepted = accepted;
    return Platform::peers().send(mac, &packet, sizeof(packet));
}

void EspNowLink::rememberRoomLocked(const uint8_t mac[6], uint8_t seenRoomId,
                                    RoomPurpose purpose, uint32_t now) {
    for (uint8_t i = 0; i < roomCountValue; ++i) {
        if (memcmp(rooms[i].mac, mac, 6) == 0 && rooms[i].roomId == seenRoomId) {
            rooms[i].lastSeenMs = now;
            return;
        }
    }
    if (roomCountValue >= MAX_ROOMS) return;
    RoomEntry& entry = rooms[roomCountValue++];
    memcpy(entry.mac, mac, 6);
    entry.roomId = seenRoomId;
    entry.purpose = purpose;
    entry.lastSeenMs = now;
}

bool EspNowLink::isSessionType(LinkMessageType type) {
    uint8_t value = static_cast<uint8_t>(type);
    return value >= static_cast<uint8_t>(LinkMessageType::VISIT_SYNC) &&
           value <= static_cast<uint8_t>(LinkMessageType::SESSION_ACK);
}

uint16_t EspNowLink::sessionId() const {
    uint16_t value = sessionIdValue;
    return value;
}

bool EspNowLink::sendSessionMessage(LinkMessageType type, const void* payload, uint8_t payloadLen) {
    if (!isSessionType(type) || type == LinkMessageType::SESSION_ACK) return false;
    if (payloadLen > SESSION_PAYLOAD_CAP) return false;
    if (payloadLen > 0 && !payload) return false;

    uint8_t target[6] = {};
    uint16_t frameSeq = 0;
    uint16_t frameSessionId = 0;
    uint32_t now = Platform::clock().millis();
    bool available = mode == Mode::CONNECTED && !tracked.active;
    if (available) {
        frameSeq = seq++;
        if (seq == 0) seq = 1;
        frameSessionId = sessionIdValue;
        memcpy(target, peer, 6);
        tracked.active = true;
        tracked.type = type;
        tracked.seq = frameSeq;
        tracked.attempts = 1;
        tracked.payloadLen = payloadLen;
        tracked.lastSendMs = now;
        if (payloadLen > 0) memcpy(tracked.payload, payload, payloadLen);
    }
    if (!available) return false;

    // The tracked slot drives retransmission, so a transient send failure here
    // is recovered by update() instead of being reported to the caller.
    sendSessionFrame(target, type, frameSeq, frameSessionId, payload, payloadLen);
    return true;
}

bool EspNowLink::sessionSendBusy() const {
    bool busy = tracked.active;
    return busy;
}

bool EspNowLink::takeSessionSendResult(bool& success) {
    bool available = sessionResultReady;
    if (available) {
        success = sessionResultSuccess;
        sessionResultReady = false;
    }
    return available;
}

bool EspNowLink::takeSessionMessage(LinkMessageType& type, uint8_t* payload, uint8_t& payloadLen) {
    bool available = rxPending;
    if (available) {
        type = rxType;
        payloadLen = rxPayloadLen;
        if (payload && rxPayloadLen > 0) memcpy(payload, rxPayload, rxPayloadLen);
        rxPending = false;
    }
    return available;
}

void EspNowLink::handleSessionFrame(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != (int)sizeof(SessionFrame)) return;
    SessionFrame frame{};
    memcpy(&frame, data, sizeof(frame));
    if (frame.magic != 0x5AA5 || frame.version != 0x03) return;
    LinkMessageType type = static_cast<LinkMessageType>(frame.type);
    if (!isSessionType(type)) return;
    if (frame.payloadLen > SESSION_PAYLOAD_CAP) return;

    bool sendAck = false;
    uint16_t ackSeq = 0;
    uint16_t ackSessionId = 0;
    uint8_t target[6] = {};
    bool valid = mode == Mode::CONNECTED && sessionIdValue != 0 &&
                 frame.sessionId == sessionIdValue && memcmp(mac, peer, 6) == 0;
    if (valid) {
        if (type == LinkMessageType::SESSION_ACK) {
            if (tracked.active && tracked.seq == frame.seq) {
                tracked.active = false;
                sessionResultReady = true;
                sessionResultSuccess = true;
            }
        } else {
            memcpy(target, peer, 6);
            sendAck = true;
            ackSeq = frame.seq;
            ackSessionId = sessionIdValue;
            bool duplicate = lastRxValid && lastRxSeq == frame.seq;
            if (!duplicate) {
                lastRxValid = true;
                lastRxSeq = frame.seq;
                if (!rxPending) {
                    rxType = type;
                    rxPayloadLen = frame.payloadLen;
                    if (frame.payloadLen > 0) memcpy(rxPayload, frame.payload, frame.payloadLen);
                    rxPending = true;
                }
            }
        }
    }

    if (sendAck) {
        sendSessionFrame(target, LinkMessageType::SESSION_ACK, ackSeq, ackSessionId, nullptr, 0);
    }
}

bool EspNowLink::sendSessionFrame(const uint8_t mac[6], LinkMessageType type, uint16_t frameSeq,
                                  uint16_t frameSessionId, const void* payload, uint8_t payloadLen) {
    if (!isEnabled()) return false;
    SessionFrame frame{};
    frame.magic = 0x5AA5;
    frame.version = 0x03;
    frame.type = static_cast<uint8_t>(type);
    frame.seq = frameSeq;
    frame.sessionId = frameSessionId;
    frame.payloadLen = payloadLen;
    if (payloadLen > 0 && payload) memcpy(frame.payload, payload, payloadLen);
    return Platform::peers().send(mac, &frame, sizeof(frame));
}

void EspNowLink::resetSessionStateLocked() {
    sessionIdValue = 0;
    tracked = TrackedSessionSend{};
    sessionResultReady = false;
    sessionResultSuccess = false;
    rxPending = false;
    rxPayloadLen = 0;
    lastRxValid = false;
    lastRxSeq = 0;
}
