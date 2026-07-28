#pragma once

#include <cstdint>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

enum class LinkMessageType : uint8_t {
    HELLO = 0x01,
    BYE = 0x02,
    PING = 0x03,
    JOIN_REQ = 0x04,
    JOIN_ACK = 0x05,
    BATTLE_REQ = 0x10,
    BATTLE_ACK = 0x11,
    BATTLE_TURN = 0x12,
    BATTLE_END = 0x13,
    TRADE_REQ = 0x20,
    TRADE_OFFER = 0x21,
    TRADE_LOCK = 0x22,
    TRADE_COMMIT = 0x23,
    TRADE_CANCEL = 0x24,
    EVOLVE_BIND = 0x30,
    EVOLVE_ACK = 0x31,
    EVENT_BEACON = 0x40,
    EVENT_REWARD = 0x41,
    VISIT_SYNC = 0x50,
    VISIT_ACCEPT = 0x51,
    VISIT_PING = 0x52,
    VISIT_STATUS = 0x53,
    VISIT_RECALL = 0x54,
    VISIT_END = 0x55,
    SESSION_ACK = 0x5F,
};

struct __attribute__((packed)) VisitSyncPayload {
    uint16_t speciesId;
    uint8_t level;
    uint8_t nature;
    uint8_t satiety;
    uint8_t mood;
    uint8_t affection;
};

struct __attribute__((packed)) VisitAcceptPayload {
    uint8_t accepted;  // 0/1
    uint8_t reason;    // 0=ok 1=通讯录已满 2=无精灵
};

struct __attribute__((packed)) VisitPingPayload {
    uint8_t satiety;
    uint8_t mood;
};

struct __attribute__((packed)) VisitStatusPayload {
    uint8_t active;
    uint16_t remainSec;
};

struct __attribute__((packed)) VisitEndPayload {
    uint8_t reason;  // 0=召回 1=断线
};

struct LinkFrameHeader {
    uint16_t magic = 0x5AA5;
    uint8_t version = 0x03;
    LinkMessageType type = LinkMessageType::PING;
    uint8_t flags = 0;
    uint16_t seq = 0;
    uint16_t sessionId = 0;
    uint8_t payloadLen = 0;
};

class EspNowLink {
public:
    static constexpr uint8_t MAX_ROOMS = 4;
    static constexpr uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    // The current protocol only establishes an ephemeral UI session. Any frame
    // that mutates save data must add authenticated pairing before release.

    enum class Mode : uint8_t {
        OFF,
        HOSTING,
        SEARCHING,
        CONNECTED,
    };

    enum class RoomPurpose : uint8_t {
        BATTLE = 0,
        TRADE = 1,
        EVOLVE = 2,
        GIFT = 3,
        VISIT = 4,
    };

    struct RoomEntry {
        uint8_t mac[6] = {};
        uint8_t roomId = 0;
        RoomPurpose purpose = RoomPurpose::BATTLE;
        uint32_t lastSeenMs = 0;
    };

    static EspNowLink& ins();

    bool begin();
    void end();
    bool beginStub();
    bool isEnabled() const;
    Mode currentMode() const;
    uint16_t nextSeq();

    void startHost(RoomPurpose purpose);
    void startSearch(RoomPurpose purpose);
    void stopRoom();
    void update();

    uint8_t roomCount() const;
    bool copyRoomAt(uint8_t index, RoomEntry& out) const;
    bool sendJoinRequest(uint8_t index);
    bool takeJoinRequest(uint8_t outMac[6], RoomPurpose& outPurpose, uint16_t& outRequestSeq);
    bool sendJoinAck(const uint8_t mac[6], bool accepted, uint16_t requestSeq);
    bool takeJoinAck(bool& accepted);
    bool connected() const;
    bool copyPeerMac(uint8_t outMac[6]) const;

    uint16_t sessionId() const;
    bool sendSessionMessage(LinkMessageType type, const void* payload, uint8_t payloadLen);
    bool sessionSendBusy() const;
    bool takeSessionSendResult(bool& success);
    bool takeSessionMessage(LinkMessageType& type, uint8_t* payload, uint8_t& payloadLen);

private:
    EspNowLink() = default;

    static constexpr uint8_t SESSION_PAYLOAD_CAP = 24;
    static constexpr uint32_t SESSION_RETRANSMIT_MS = 300;
    static constexpr uint8_t SESSION_MAX_ATTEMPTS = 5;

    struct __attribute__((packed)) WirePacket {
        uint16_t magic;
        uint8_t version;
        LinkMessageType type;
        uint8_t purpose;
        uint8_t roomId;
        uint16_t requestSeq;
        uint8_t accepted;
    };

    struct __attribute__((packed)) SessionFrame {
        uint16_t magic;
        uint8_t version;
        uint8_t type;
        uint16_t seq;
        uint16_t sessionId;
        uint8_t payloadLen;
        uint8_t payload[SESSION_PAYLOAD_CAP];
    };

    struct TrackedSessionSend {
        bool active = false;
        LinkMessageType type = LinkMessageType::PING;
        uint16_t seq = 0;
        uint8_t attempts = 0;
        uint8_t payloadLen = 0;
        uint32_t lastSendMs = 0;
        uint8_t payload[SESSION_PAYLOAD_CAP] = {};
    };

    static bool isSessionType(LinkMessageType type);
    static void onReceive(const uint8_t* mac, const uint8_t* data, int len);
    void handleReceive(const uint8_t* mac, const uint8_t* data, int len);
    void handleSessionFrame(const uint8_t* mac, const uint8_t* data, int len);
    bool sendPacket(const uint8_t mac[6], LinkMessageType type, RoomPurpose purpose,
                    uint8_t packetRoomId, uint16_t requestSeq, uint8_t accepted = 0);
    bool sendSessionFrame(const uint8_t mac[6], LinkMessageType type, uint16_t frameSeq,
                          uint16_t frameSessionId, const void* payload, uint8_t payloadLen);
    void rememberRoomLocked(const uint8_t mac[6], uint8_t roomId, RoomPurpose purpose, uint32_t nowMs);
    void resetSessionStateLocked();

    bool enabled = false;
    Mode mode = Mode::OFF;
    uint16_t seq = 1;
    uint8_t roomId = 1;
    RoomPurpose activePurpose = RoomPurpose::BATTLE;
    uint32_t lastAdvertMs = 0;
    RoomEntry rooms[MAX_ROOMS];
    uint8_t roomCountValue = 0;
    uint8_t peer[6] = {};
    bool pendingJoin = false;
    uint8_t pendingJoinMac[6] = {};
    RoomPurpose pendingJoinPurpose = RoomPurpose::BATTLE;
    uint16_t pendingJoinSeq = 0;
    bool pendingAck = false;
    bool pendingAckAccepted = false;
    bool awaitingAck = false;
    uint8_t expectedAckMac[6] = {};
    uint8_t expectedAckRoomId = 0;
    uint16_t expectedAckSeq = 0;
    uint16_t sessionIdValue = 0;
    TrackedSessionSend tracked;
    bool sessionResultReady = false;
    bool sessionResultSuccess = false;
    bool rxPending = false;
    LinkMessageType rxType = LinkMessageType::PING;
    uint8_t rxPayloadLen = 0;
    uint8_t rxPayload[SESSION_PAYLOAD_CAP] = {};
    bool lastRxValid = false;
    uint16_t lastRxSeq = 0;
    mutable portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
};
