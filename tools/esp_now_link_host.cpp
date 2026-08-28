#include "hardware/EspNowLink.h"
#include "platform/api/PlatformServices.h"
#include "platform/desktop/DesktopPlatform.h"

#include <cassert>
#include <cstdint>
#include <cstring>

namespace {

constexpr uint8_t BROADCAST[6] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

void put16(uint8_t* bytes, uint16_t value) {
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8);
}

uint16_t get16(const uint8_t* bytes) {
    return static_cast<uint16_t>(bytes[0]) |
           static_cast<uint16_t>(bytes[1]) << 8;
}

void writeJoinAck(DesktopPlatform& desktop, uint8_t roomId,
                  uint16_t requestSeq) {
    uint8_t packet[9] = {};
    put16(packet, 0x5AA5);
    packet[2] = 0x03;
    packet[3] = static_cast<uint8_t>(LinkMessageType::JOIN_ACK);
    packet[4] = static_cast<uint8_t>(EspNowLink::RoomPurpose::VISIT);
    packet[5] = roomId;
    put16(packet + 6, requestSeq);
    packet[8] = 1;
    assert(desktop.send(BROADCAST, packet, sizeof(packet)));
}

}  // namespace

int main() {
    DesktopPlatform desktop(".");
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());

    EspNowLink& link = EspNowLink::ins();
    assert(link.begin());
    link.startHost(EspNowLink::RoomPurpose::VISIT);
    desktop.advanceMs(500);
    link.update();

    Platform::PeerPacket hello;
    assert(desktop.receive(hello));
    assert(hello.length == 9);
    assert(hello.payload[0] == 0xA5 && hello.payload[1] == 0x5A);
    assert(hello.payload[2] == 0x03);
    assert(hello.payload[3] == static_cast<uint8_t>(LinkMessageType::HELLO));

    link.startSearch(EspNowLink::RoomPurpose::VISIT);
    assert(desktop.send(BROADCAST, hello.payload, hello.length));
    desktop.advanceMs(1);
    link.update();
    assert(link.roomCount() == 1);
    assert(link.sendJoinRequest(0));

    Platform::PeerPacket request;
    assert(desktop.receive(request));
    assert(request.length == 9);
    assert(request.payload[3] ==
           static_cast<uint8_t>(LinkMessageType::JOIN_REQ));
    writeJoinAck(desktop, request.payload[5], get16(request.payload + 6));
    link.update();
    bool accepted = false;
    assert(link.takeJoinAck(accepted) && accepted && link.connected());

    VisitPingPayload ping{73, 88};
    assert(link.sendSessionMessage(LinkMessageType::VISIT_PING,
                                   &ping, sizeof(ping)));
    Platform::PeerPacket session;
    assert(desktop.receive(session));
    assert(session.length == 33);
    assert(session.payload[0] == 0xA5 && session.payload[1] == 0x5A);
    assert(session.payload[2] == 0x03);
    assert(session.payload[3] ==
           static_cast<uint8_t>(LinkMessageType::VISIT_PING));
    assert(session.payload[8] == sizeof(ping));
    assert(session.payload[9] == ping.satiety &&
           session.payload[10] == ping.mood);

    uint8_t ack[33] = {};
    put16(ack, 0x5AA5);
    ack[2] = 0x03;
    ack[3] = static_cast<uint8_t>(LinkMessageType::SESSION_ACK);
    ack[4] = session.payload[4];
    ack[5] = session.payload[5];
    ack[6] = session.payload[6];
    ack[7] = session.payload[7];
    assert(desktop.send(BROADCAST, ack, sizeof(ack)));
    link.update();
    bool sent = false;
    assert(link.takeSessionSendResult(sent) && sent);

    link.end();
    return 0;
}
