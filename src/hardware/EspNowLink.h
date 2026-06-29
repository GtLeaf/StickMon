#pragma once

#include <cstdint>
#include <cstring>

enum class LinkMessageType : uint8_t {
    HELLO = 0x01,
    BYE = 0x02,
    PING = 0x03,
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
};

struct LinkFrameHeader {
    uint16_t magic = 0x5AA5;
    uint8_t version = 0x02;
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
    bool isEnabled() const { return enabled; }
    Mode currentMode() const { return mode; }
    uint16_t nextSeq();

    void startHost(RoomPurpose purpose);
    void startSearch(RoomPurpose purpose);
    void stopRoom();
    void update();

    uint8_t roomCount() const { return roomCountValue; }
    const RoomEntry* roomAt(uint8_t index) const;
    bool sendJoinRequest(uint8_t index);
    bool takeJoinRequest(uint8_t outMac[6], RoomPurpose& outPurpose);
    bool sendJoinAck(const uint8_t mac[6], bool accepted);
    bool takeJoinAck(bool& accepted);
    bool connected() const { return mode == Mode::CONNECTED; }
    const uint8_t* peerMac() const { return peer; }

private:
    EspNowLink() = default;

    struct __attribute__((packed)) WirePacket {
        uint16_t magic;
        uint8_t version;
        LinkMessageType type;
        uint8_t purpose;
        uint8_t roomId;
        uint8_t accepted;
    };

    static void onReceive(const uint8_t* mac, const uint8_t* data, int len);
    void handleReceive(const uint8_t* mac, const uint8_t* data, int len);
    bool sendPacket(const uint8_t mac[6], LinkMessageType type, RoomPurpose purpose, uint8_t accepted = 0);
    void rememberRoom(const uint8_t mac[6], uint8_t roomId, RoomPurpose purpose);

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
    bool pendingAck = false;
    bool pendingAckAccepted = false;
};
