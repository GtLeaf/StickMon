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
    if (enabled) return true;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[EspNowLink] esp_now_init failed");
        enabled = false;
        mode = Mode::OFF;
        return false;
    }
    esp_now_register_recv_cb(EspNowLink::onReceive);

    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(BROADCAST_MAC)) {
        esp_now_add_peer(&peerInfo);
    }

    enabled = true;
    mode = Mode::OFF;
    Serial.println("[EspNowLink] enabled");
    return true;
}

void EspNowLink::end() {
    if (!enabled) return;
    esp_now_unregister_recv_cb();
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
    enabled = false;
    mode = Mode::OFF;
    roomCountValue = 0;
    pendingJoin = false;
    pendingAck = false;
}

bool EspNowLink::beginStub() {
    enabled = false;
    mode = Mode::OFF;
    seq = 1;
    return true;
}

uint16_t EspNowLink::nextSeq() {
    return seq++;
}

void EspNowLink::startHost(RoomPurpose purpose) {
    if (!begin()) return;
    activePurpose = purpose;
    roomId++;
    if (roomId == 0) roomId = 1;
    mode = Mode::HOSTING;
    lastAdvertMs = 0;
    pendingJoin = false;
}

void EspNowLink::startSearch(RoomPurpose purpose) {
    if (!begin()) return;
    activePurpose = purpose;
    mode = Mode::SEARCHING;
    roomCountValue = 0;
    pendingAck = false;
}

void EspNowLink::stopRoom() {
    mode = Mode::OFF;
    roomCountValue = 0;
    pendingJoin = false;
    pendingAck = false;
}

void EspNowLink::update() {
    if (!enabled) return;
    uint32_t now = Hal::ins().millis();

    if (mode == Mode::HOSTING && now - lastAdvertMs >= 500) {
        lastAdvertMs = now;
        sendPacket(BROADCAST_MAC, LinkMessageType::HELLO, activePurpose);
    }

    for (uint8_t i = 0; i < roomCountValue;) {
        if (now - rooms[i].lastSeenMs > 3000) {
            for (uint8_t j = i + 1; j < roomCountValue; ++j) rooms[j - 1] = rooms[j];
            roomCountValue--;
        } else {
            i++;
        }
    }
}

const EspNowLink::RoomEntry* EspNowLink::roomAt(uint8_t index) const {
    if (index >= roomCountValue) return nullptr;
    return &rooms[index];
}

bool EspNowLink::sendJoinRequest(uint8_t index) {
    const RoomEntry* room = roomAt(index);
    if (!room) return false;
    activePurpose = room->purpose;
    return sendPacket(room->mac, LinkMessageType::TRADE_REQ, room->purpose);
}

bool EspNowLink::takeJoinRequest(uint8_t outMac[6], RoomPurpose& outPurpose) {
    if (!pendingJoin) return false;
    memcpy(outMac, pendingJoinMac, 6);
    outPurpose = pendingJoinPurpose;
    pendingJoin = false;
    return true;
}

bool EspNowLink::sendJoinAck(const uint8_t mac[6], bool accepted) {
    if (accepted) {
        memcpy(peer, mac, 6);
        mode = Mode::CONNECTED;
    }
    return sendPacket(mac, LinkMessageType::TRADE_OFFER, activePurpose, accepted ? 1 : 0);
}

bool EspNowLink::takeJoinAck(bool& accepted) {
    if (!pendingAck) return false;
    accepted = pendingAckAccepted;
    pendingAck = false;
    if (accepted) mode = Mode::CONNECTED;
    return true;
}

void EspNowLink::onReceive(const uint8_t* mac, const uint8_t* data, int len) {
    EspNowLink::ins().handleReceive(mac, data, len);
}

void EspNowLink::handleReceive(const uint8_t* mac, const uint8_t* data, int len) {
    if (!mac || !data || len < (int)sizeof(WirePacket)) return;
    WirePacket packet{};
    memcpy(&packet, data, sizeof(packet));
    if (packet.magic != 0x5AA5 || packet.version != 0x02) return;

    RoomPurpose purpose = (RoomPurpose)packet.purpose;
    if (packet.type == LinkMessageType::HELLO && mode == Mode::SEARCHING && purpose == activePurpose) {
        rememberRoom(mac, packet.roomId, purpose);
        return;
    }

    if (packet.type == LinkMessageType::TRADE_REQ && mode == Mode::HOSTING && purpose == activePurpose) {
        memcpy(pendingJoinMac, mac, 6);
        pendingJoinPurpose = purpose;
        pendingJoin = true;
        return;
    }

    if (packet.type == LinkMessageType::TRADE_OFFER && mode == Mode::SEARCHING) {
        pendingAck = true;
        pendingAckAccepted = packet.accepted != 0;
        if (pendingAckAccepted) memcpy(peer, mac, 6);
    }
}

bool EspNowLink::sendPacket(const uint8_t mac[6], LinkMessageType type, RoomPurpose purpose, uint8_t accepted) {
    if (!enabled) return false;
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peerInfo{};
        memcpy(peerInfo.peer_addr, mac, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }

    WirePacket packet{};
    packet.magic = 0x5AA5;
    packet.version = 0x02;
    packet.type = type;
    packet.purpose = (uint8_t)purpose;
    packet.roomId = roomId;
    packet.accepted = accepted;
    return esp_now_send(mac, (const uint8_t*)&packet, sizeof(packet)) == ESP_OK;
}

void EspNowLink::rememberRoom(const uint8_t mac[6], uint8_t seenRoomId, RoomPurpose purpose) {
    uint32_t now = Hal::ins().millis();
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
