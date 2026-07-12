#include "hardware/EspNowLink.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "hardware/Hal.h"

constexpr uint8_t EspNowLink::BROADCAST_MAC[6];

EspNowLink& EspNowLink::ins() {
    static EspNowLink instance;
    return instance;
}

bool EspNowLink::begin() {
    portENTER_CRITICAL(&stateMux);
    bool alreadyEnabled = enabled;
    portEXIT_CRITICAL(&stateMux);
    if (alreadyEnabled) return true;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[EspNowLink] esp_now_init failed");
        portENTER_CRITICAL(&stateMux);
        enabled = false;
        mode = Mode::OFF;
        portEXIT_CRITICAL(&stateMux);
        return false;
    }
    if (esp_now_register_recv_cb(EspNowLink::onReceive) != ESP_OK) {
        Serial.println("[EspNowLink] recv callback registration failed");
        esp_now_deinit();
        WiFi.mode(WIFI_OFF);
        return false;
    }

    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(BROADCAST_MAC)) {
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("[EspNowLink] broadcast peer setup failed");
            esp_now_unregister_recv_cb();
            esp_now_deinit();
            WiFi.mode(WIFI_OFF);
            return false;
        }
    }

    portENTER_CRITICAL(&stateMux);
    enabled = true;
    mode = Mode::OFF;
    portEXIT_CRITICAL(&stateMux);
    Serial.println("[EspNowLink] enabled");
    return true;
}

void EspNowLink::end() {
    if (!isEnabled()) return;
    esp_now_unregister_recv_cb();
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
    portENTER_CRITICAL(&stateMux);
    enabled = false;
    mode = Mode::OFF;
    roomCountValue = 0;
    pendingJoin = false;
    pendingAck = false;
    awaitingAck = false;
    portEXIT_CRITICAL(&stateMux);
}

bool EspNowLink::beginStub() {
    portENTER_CRITICAL(&stateMux);
    enabled = false;
    mode = Mode::OFF;
    seq = 1;
    roomCountValue = 0;
    pendingJoin = false;
    pendingAck = false;
    awaitingAck = false;
    portEXIT_CRITICAL(&stateMux);
    return true;
}

bool EspNowLink::isEnabled() const {
    portENTER_CRITICAL(&stateMux);
    bool value = enabled;
    portEXIT_CRITICAL(&stateMux);
    return value;
}

EspNowLink::Mode EspNowLink::currentMode() const {
    portENTER_CRITICAL(&stateMux);
    Mode value = mode;
    portEXIT_CRITICAL(&stateMux);
    return value;
}

uint16_t EspNowLink::nextSeq() {
    portENTER_CRITICAL(&stateMux);
    uint16_t value = seq++;
    if (seq == 0) seq = 1;
    portEXIT_CRITICAL(&stateMux);
    return value;
}

void EspNowLink::startHost(RoomPurpose purpose) {
    if (!begin()) return;
    portENTER_CRITICAL(&stateMux);
    activePurpose = purpose;
    roomId++;
    if (roomId == 0) roomId = 1;
    mode = Mode::HOSTING;
    lastAdvertMs = 0;
    pendingJoin = false;
    pendingAck = false;
    awaitingAck = false;
    portEXIT_CRITICAL(&stateMux);
}

void EspNowLink::startSearch(RoomPurpose purpose) {
    if (!begin()) return;
    portENTER_CRITICAL(&stateMux);
    activePurpose = purpose;
    mode = Mode::SEARCHING;
    roomCountValue = 0;
    pendingAck = false;
    awaitingAck = false;
    portEXIT_CRITICAL(&stateMux);
}

void EspNowLink::stopRoom() {
    portENTER_CRITICAL(&stateMux);
    mode = Mode::OFF;
    roomCountValue = 0;
    pendingJoin = false;
    pendingAck = false;
    awaitingAck = false;
    portEXIT_CRITICAL(&stateMux);
}

void EspNowLink::update() {
    if (!isEnabled()) return;
    uint32_t now = Hal::ins().millis();

    bool advertise = false;
    RoomPurpose advertisePurpose = RoomPurpose::BATTLE;
    uint8_t advertiseRoomId = 0;
    portENTER_CRITICAL(&stateMux);
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
    portEXIT_CRITICAL(&stateMux);

    if (advertise) {
        sendPacket(BROADCAST_MAC, LinkMessageType::HELLO, advertisePurpose, advertiseRoomId, 0);
    }
}

uint8_t EspNowLink::roomCount() const {
    portENTER_CRITICAL(&stateMux);
    uint8_t count = roomCountValue;
    portEXIT_CRITICAL(&stateMux);
    return count;
}

bool EspNowLink::copyRoomAt(uint8_t index, RoomEntry& out) const {
    portENTER_CRITICAL(&stateMux);
    bool found = index < roomCountValue;
    if (found) out = rooms[index];
    portEXIT_CRITICAL(&stateMux);
    return found;
}

bool EspNowLink::sendJoinRequest(uint8_t index) {
    RoomEntry room;
    if (!copyRoomAt(index, room)) return false;
    uint16_t requestSeq = nextSeq();
    portENTER_CRITICAL(&stateMux);
    activePurpose = room.purpose;
    memcpy(expectedAckMac, room.mac, sizeof(expectedAckMac));
    expectedAckRoomId = room.roomId;
    expectedAckSeq = requestSeq;
    awaitingAck = true;
    pendingAck = false;
    portEXIT_CRITICAL(&stateMux);
    bool sent = sendPacket(room.mac, LinkMessageType::TRADE_REQ, room.purpose,
                           room.roomId, requestSeq);
    if (!sent) {
        portENTER_CRITICAL(&stateMux);
        awaitingAck = false;
        portEXIT_CRITICAL(&stateMux);
    }
    return sent;
}

bool EspNowLink::takeJoinRequest(uint8_t outMac[6], RoomPurpose& outPurpose, uint16_t& outRequestSeq) {
    portENTER_CRITICAL(&stateMux);
    bool available = pendingJoin;
    if (available) {
        memcpy(outMac, pendingJoinMac, 6);
        outPurpose = pendingJoinPurpose;
        outRequestSeq = pendingJoinSeq;
        pendingJoin = false;
    }
    portEXIT_CRITICAL(&stateMux);
    return available;
}

bool EspNowLink::sendJoinAck(const uint8_t mac[6], bool accepted, uint16_t requestSeq) {
    portENTER_CRITICAL(&stateMux);
    RoomPurpose purpose = activePurpose;
    uint8_t localRoomId = roomId;
    if (accepted) {
        memcpy(peer, mac, 6);
        mode = Mode::CONNECTED;
    }
    portEXIT_CRITICAL(&stateMux);
    bool sent = sendPacket(mac, LinkMessageType::TRADE_OFFER, purpose,
                           localRoomId, requestSeq, accepted ? 1 : 0);
    if (!sent && accepted) {
        portENTER_CRITICAL(&stateMux);
        if (mode == Mode::CONNECTED && memcmp(peer, mac, 6) == 0) mode = Mode::HOSTING;
        portEXIT_CRITICAL(&stateMux);
    }
    return sent;
}

bool EspNowLink::takeJoinAck(bool& accepted) {
    portENTER_CRITICAL(&stateMux);
    bool available = pendingAck;
    if (available) {
        accepted = pendingAckAccepted;
        pendingAck = false;
        if (accepted) mode = Mode::CONNECTED;
    }
    portEXIT_CRITICAL(&stateMux);
    return available;
}

bool EspNowLink::connected() const {
    return currentMode() == Mode::CONNECTED;
}

bool EspNowLink::copyPeerMac(uint8_t outMac[6]) const {
    if (!outMac) return false;
    portENTER_CRITICAL(&stateMux);
    bool available = mode == Mode::CONNECTED;
    if (available) memcpy(outMac, peer, 6);
    portEXIT_CRITICAL(&stateMux);
    return available;
}

void EspNowLink::onReceive(const uint8_t* mac, const uint8_t* data, int len) {
    EspNowLink::ins().handleReceive(mac, data, len);
}

void EspNowLink::handleReceive(const uint8_t* mac, const uint8_t* data, int len) {
    if (!mac || !data || len != (int)sizeof(WirePacket)) return;
    WirePacket packet{};
    memcpy(&packet, data, sizeof(packet));
    if (packet.magic != 0x5AA5 || packet.version != 0x03) return;
    if (packet.purpose > static_cast<uint8_t>(RoomPurpose::GIFT)) return;

    RoomPurpose purpose = (RoomPurpose)packet.purpose;
    uint32_t now = Hal::ins().millis();
    portENTER_CRITICAL(&stateMux);
    if (packet.type == LinkMessageType::HELLO && mode == Mode::SEARCHING &&
        purpose == activePurpose && packet.roomId != 0) {
        rememberRoomLocked(mac, packet.roomId, purpose, now);
    } else if (packet.type == LinkMessageType::TRADE_REQ && mode == Mode::HOSTING &&
               purpose == activePurpose && packet.roomId == roomId &&
               packet.requestSeq != 0 && !pendingJoin) {
        memcpy(pendingJoinMac, mac, 6);
        pendingJoinPurpose = purpose;
        pendingJoinSeq = packet.requestSeq;
        pendingJoin = true;
    } else if (packet.type == LinkMessageType::TRADE_OFFER && mode == Mode::SEARCHING &&
               awaitingAck && purpose == activePurpose &&
               packet.roomId == expectedAckRoomId && packet.requestSeq == expectedAckSeq &&
               memcmp(mac, expectedAckMac, 6) == 0) {
        pendingAck = true;
        pendingAckAccepted = packet.accepted != 0;
        awaitingAck = false;
        if (pendingAckAccepted) memcpy(peer, mac, 6);
    }
    portEXIT_CRITICAL(&stateMux);
}

bool EspNowLink::sendPacket(const uint8_t mac[6], LinkMessageType type, RoomPurpose purpose,
                            uint8_t packetRoomId, uint16_t requestSeq, uint8_t accepted) {
    if (!isEnabled()) return false;
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peerInfo{};
        memcpy(peerInfo.peer_addr, mac, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        if (esp_now_add_peer(&peerInfo) != ESP_OK) return false;
    }

    WirePacket packet{};
    packet.magic = 0x5AA5;
    packet.version = 0x03;
    packet.type = type;
    packet.purpose = (uint8_t)purpose;
    packet.roomId = packetRoomId;
    packet.requestSeq = requestSeq;
    packet.accepted = accepted;
    return esp_now_send(mac, (const uint8_t*)&packet, sizeof(packet)) == ESP_OK;
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
