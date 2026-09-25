#include "core/VisitSessionService.h"
#include "platform/api/PlatformServices.h"
#include "platform/desktop/DesktopPlatform.h"

#include <cassert>
#include <cstdint>
#include <cstring>

namespace {

constexpr uint8_t PEER[6] = {2, 4, 6, 8, 10, 12};
constexpr size_t WIRE_SIZE = 9;
constexpr size_t SESSION_SIZE = 33;

void put16(uint8_t* bytes, uint16_t value) {
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8);
}

uint16_t get16(const uint8_t* bytes) {
    return static_cast<uint16_t>(bytes[0]) |
           static_cast<uint16_t>(bytes[1]) << 8;
}

void injectWire(DesktopPlatform& desktop, LinkMessageType type,
                uint8_t roomId, uint16_t requestSeq,
                bool accepted = false) {
    uint8_t packet[WIRE_SIZE] = {};
    put16(packet, 0x5AA5);
    packet[2] = 0x03;
    packet[3] = static_cast<uint8_t>(type);
    packet[4] = static_cast<uint8_t>(EspNowLink::RoomPurpose::VISIT);
    packet[5] = roomId;
    put16(packet + 6, requestSeq);
    packet[8] = accepted ? 1 : 0;
    assert(desktop.send(PEER, packet, sizeof(packet)));
}

void injectSession(DesktopPlatform& desktop, LinkMessageType type,
                   uint16_t seq, uint16_t sessionId,
                   const void* payload = nullptr, uint8_t payloadLen = 0) {
    uint8_t frame[SESSION_SIZE] = {};
    put16(frame, 0x5AA5);
    frame[2] = 0x03;
    frame[3] = static_cast<uint8_t>(type);
    put16(frame + 4, seq);
    put16(frame + 6, sessionId);
    frame[8] = payloadLen;
    if (payloadLen > 0) {
        assert(payload);
        std::memcpy(frame + 9, payload, payloadLen);
    }
    assert(desktop.send(PEER, frame, sizeof(frame)));
}

Platform::PeerPacket takeType(DesktopPlatform& desktop,
                              LinkMessageType type) {
    Platform::PeerPacket packet;
    while (desktop.receive(packet)) {
        if (packet.length >= 4 &&
            packet.payload[3] == static_cast<uint8_t>(type)) {
            return packet;
        }
    }
    assert(false && "expected packet was not sent");
    return {};
}

void drain(DesktopPlatform& desktop) {
    Platform::PeerPacket packet;
    while (desktop.receive(packet)) {}
}

void acknowledge(DesktopPlatform& desktop,
                 const Platform::PeerPacket& packet) {
    assert(packet.length == SESSION_SIZE);
    injectSession(desktop, LinkMessageType::SESSION_ACK,
                  get16(packet.payload + 4), get16(packet.payload + 6));
}

struct ConnectedHost {
    uint16_t sessionId = 0;
    Platform::PeerPacket firstStatus;
};

ConnectedHost connectHost(DesktopPlatform& desktop,
                          Communication::VisitSessionService& service,
                          Game::GameState& state,
                          const VisitSyncPayload& sync) {
    state.teamCount = 1;
    state.team[1] = Game::MonsterRuntime{};
    service.startHost();
    assert(service.state() ==
           Communication::VisitSessionService::State::HOSTING);

    desktop.advanceMs(500);
    service.update(desktop.millis());
    Platform::PeerPacket hello = takeType(desktop, LinkMessageType::HELLO);
    const uint8_t roomId = hello.payload[5];
    assert(service.viewModel().roomId == roomId);
    const uint16_t requestSeq = 71;
    injectWire(desktop, LinkMessageType::JOIN_REQ, roomId, requestSeq);
    service.update(desktop.millis());
    assert(service.state() == Communication::VisitSessionService::State::
                                  WAITING_HOST_DECISION);
    service.acceptIncoming(true);
    Platform::PeerPacket joinAck = takeType(desktop,
                                            LinkMessageType::JOIN_ACK);
    assert(joinAck.payload[8] == 1);
    assert(service.state() ==
           Communication::VisitSessionService::State::SYNCING);

    injectSession(desktop, LinkMessageType::VISIT_SYNC, 501, requestSeq,
                  &sync, sizeof(sync));
    service.update(desktop.millis());
    Platform::PeerPacket response = takeType(desktop,
                                             LinkMessageType::VISIT_ACCEPT);
    assert(response.payload[8] == sizeof(VisitAcceptPayload));
    assert(response.payload[9] == 1);
    assert(state.teamCount == 2);
    assert(state.team[1].origin == Game::Origin::VISITOR);
    assert(service.state() ==
           Communication::VisitSessionService::State::SYNCING);

    acknowledge(desktop, response);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::ACTIVE);
    service.update(desktop.millis());
    ConnectedHost connected;
    connected.sessionId = requestSeq;
    connected.firstStatus = takeType(desktop,
                                     LinkMessageType::VISIT_STATUS);
    return connected;
}

uint16_t connectVisitor(DesktopPlatform& desktop,
                        Communication::VisitSessionService& service) {
    service.startSearch();
    assert(service.state() ==
           Communication::VisitSessionService::State::SEARCHING);
    const uint8_t roomId = 93;
    injectWire(desktop, LinkMessageType::HELLO, roomId, 0);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::ROOM_LIST);
    assert(service.viewModel().roomCount == 1);
    assert(service.viewModel().rooms[0].roomId == roomId);
    assert(service.selectRoom(0));
    Platform::PeerPacket request = takeType(desktop,
                                            LinkMessageType::JOIN_REQ);
    const uint16_t requestSeq = get16(request.payload + 6);
    injectWire(desktop, LinkMessageType::JOIN_ACK, roomId, requestSeq, true);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::WAITING_ACCEPT);
    Platform::PeerPacket sync = takeType(desktop, LinkMessageType::VISIT_SYNC);
    assert(sync.payload[8] == sizeof(VisitSyncPayload));

    VisitAcceptPayload response{1, static_cast<uint8_t>(
                                       VisitHostResult::ACCEPTED)};
    acknowledge(desktop, sync);
    injectSession(desktop, LinkMessageType::VISIT_ACCEPT, 601, requestSeq,
                  &response, sizeof(response));
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::ACTIVE);
    Platform::PeerPacket ping = takeType(desktop,
                                         LinkMessageType::VISIT_PING);
    assert(ping.payload[8] == sizeof(VisitPingPayload));
    assert(get16(ping.payload + 11) == service.viewModel().remote.hpCur);
    assert(get16(ping.payload + 13) == service.viewModel().remote.hpMax);
    acknowledge(desktop, ping);
    service.update(desktop.millis());
    drain(desktop);
    return requestSeq;
}

void testHostAdmission(Communication::VisitSessionService& service,
                       Game::GameState& state) {
    state.teamCount = 2;
    service.startHost();
    assert(service.state() ==
           Communication::VisitSessionService::State::FAILED);
    assert(std::strcmp(service.viewModel().error, "TEAM NOT SOLO") == 0);
    service.stop();
}

void testTouchBeforeFrameUpdate(DesktopPlatform& desktop,
                                Communication::VisitSessionService& service,
                                Game::GameState& state) {
    state.teamCount = 1;
    uint32_t frameTime = desktop.millis();
    desktop.advanceMs(1);
    service.startHost();
    service.update(frameTime);
    assert(service.state() ==
           Communication::VisitSessionService::State::HOSTING);
    desktop.advanceMs(Communication::VisitSessionService::HOST_TIMEOUT_MS - 2);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::HOSTING);
    service.stop();
    drain(desktop);

    frameTime = desktop.millis();
    desktop.advanceMs(1);
    service.startSearch();
    service.update(frameTime);
    assert(service.state() ==
           Communication::VisitSessionService::State::SEARCHING);
    desktop.advanceMs(Communication::VisitSessionService::SEARCH_TIMEOUT_MS - 2);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::SEARCHING);
    service.stop();
    drain(desktop);
}

void testStageTimeouts(DesktopPlatform& desktop,
                       Communication::VisitSessionService& service,
                       Game::GameState& state) {
    state.teamCount = 1;
    service.startHost();
    desktop.advanceMs(Communication::VisitSessionService::HOST_TIMEOUT_MS);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::FAILED);
    service.stop();
    drain(desktop);

    service.startHost();
    desktop.advanceMs(500);
    service.update(desktop.millis());
    Platform::PeerPacket hostHello = takeType(desktop,
                                              LinkMessageType::HELLO);
    injectWire(desktop, LinkMessageType::JOIN_REQ, hostHello.payload[5], 69);
    service.update(desktop.millis());
    assert(service.state() == Communication::VisitSessionService::State::
                                  WAITING_HOST_DECISION);
    desktop.advanceMs(
        Communication::VisitSessionService::HOST_DECISION_TIMEOUT_MS);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::HOSTING);
    service.stop();
    drain(desktop);

    service.startHost();
    desktop.advanceMs(500);
    service.update(desktop.millis());
    hostHello = takeType(desktop, LinkMessageType::HELLO);
    injectWire(desktop, LinkMessageType::JOIN_REQ, hostHello.payload[5], 70);
    service.update(desktop.millis());
    service.acceptIncoming(true);
    takeType(desktop, LinkMessageType::JOIN_ACK);
    desktop.advanceMs(
        Communication::VisitSessionService::HANDSHAKE_TIMEOUT_MS);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::FAILED);
    service.stop();
    drain(desktop);

    service.startSearch();
    desktop.advanceMs(Communication::VisitSessionService::SEARCH_TIMEOUT_MS);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::FAILED);
    service.stop();
    drain(desktop);

    service.startSearch();
    injectWire(desktop, LinkMessageType::HELLO, 31, 0);
    service.update(desktop.millis());
    assert(service.selectRoom(0));
    Platform::PeerPacket timedRequest = takeType(desktop,
                                                 LinkMessageType::JOIN_REQ);
    desktop.advanceMs(Communication::VisitSessionService::JOIN_TIMEOUT_MS);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::SEARCHING);
    assert(std::strcmp(service.viewModel().error, "JOIN TIMEOUT") == 0);
    injectWire(desktop, LinkMessageType::JOIN_ACK, 31,
               get16(timedRequest.payload + 6), true);
    service.update(desktop.millis());
    assert(!EspNowLink::ins().connected());
    assert(service.state() ==
           Communication::VisitSessionService::State::SEARCHING);
    service.stop();
    drain(desktop);

    service.startSearch();
    injectWire(desktop, LinkMessageType::HELLO, 32, 0);
    service.update(desktop.millis());
    assert(service.selectRoom(0));
    Platform::PeerPacket request = takeType(desktop,
                                            LinkMessageType::JOIN_REQ);
    const uint16_t requestSeq = get16(request.payload + 6);
    injectWire(desktop, LinkMessageType::JOIN_ACK, 32, requestSeq, true);
    service.update(desktop.millis());
    takeType(desktop, LinkMessageType::VISIT_SYNC);
    desktop.advanceMs(
        Communication::VisitSessionService::HANDSHAKE_TIMEOUT_MS);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::FAILED);
    service.stop();
    drain(desktop);
}

void testManualDecisionAndRoomList(
    DesktopPlatform& desktop, Communication::VisitSessionService& service,
    Game::GameState& state) {
    state.teamCount = 1;
    service.startSearch();
    injectWire(desktop, LinkMessageType::HELLO, 45, 0);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::ROOM_LIST);
    for (int index = 0; index < 18; ++index) {
        desktop.advanceMs(500);
        injectWire(desktop, LinkMessageType::HELLO, 45, 0);
        service.update(desktop.millis());
    }
    assert(service.state() ==
           Communication::VisitSessionService::State::ROOM_LIST);
    assert(service.selectRoom(0));
    Platform::PeerPacket request = takeType(desktop, LinkMessageType::JOIN_REQ);
    const uint16_t requestSeq = get16(request.payload + 6);
    desktop.advanceMs(25000);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::JOINING);
    injectWire(desktop, LinkMessageType::JOIN_ACK, 45, requestSeq, true);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::WAITING_ACCEPT);
    takeType(desktop, LinkMessageType::VISIT_SYNC);
    service.stop();
    drain(desktop);

    service.startSearch();
    injectWire(desktop, LinkMessageType::HELLO, 46, 0);
    service.update(desktop.millis());
    assert(service.selectRoom(0));
    request = takeType(desktop, LinkMessageType::JOIN_REQ);
    injectWire(desktop, LinkMessageType::JOIN_ACK, 46,
               get16(request.payload + 6), false);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::ROOM_LIST);
    assert(std::strcmp(service.viewModel().error, "JOIN DECLINED") == 0);
    service.stop();
    drain(desktop);

    service.startHost();
    desktop.advanceMs(500);
    service.update(desktop.millis());
    Platform::PeerPacket hello = takeType(desktop, LinkMessageType::HELLO);
    injectWire(desktop, LinkMessageType::JOIN_REQ, hello.payload[5], 99);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::WAITING_HOST_DECISION);
    desktop.advanceMs(25000);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::WAITING_HOST_DECISION);
    service.acceptIncoming(true);
    Platform::PeerPacket ack = takeType(desktop, LinkMessageType::JOIN_ACK);
    assert(ack.payload[8] == 1);
    service.stop();
    drain(desktop);

    service.startHost();
    desktop.advanceMs(500);
    service.update(desktop.millis());
    hello = takeType(desktop, LinkMessageType::HELLO);
    injectWire(desktop, LinkMessageType::JOIN_REQ, hello.payload[5], 100);
    service.update(desktop.millis());
    service.stop();
    ack = takeType(desktop, LinkMessageType::JOIN_ACK);
    assert(ack.payload[8] == 0);
    drain(desktop);
}

void testHostRejectsLateTeamChange(
    DesktopPlatform& desktop, Communication::VisitSessionService& service,
    Game::GameState& state) {
    state.teamCount = 1;
    service.startHost();
    desktop.advanceMs(500);
    service.update(desktop.millis());
    Platform::PeerPacket hello = takeType(desktop, LinkMessageType::HELLO);
    const uint16_t requestSeq = 72;
    injectWire(desktop, LinkMessageType::JOIN_REQ, hello.payload[5],
               requestSeq);
    service.update(desktop.millis());
    service.acceptIncoming(true);
    takeType(desktop, LinkMessageType::JOIN_ACK);

    state.teamCount = 2;
    state.team[1].origin = Game::Origin::BEFRIENDED;
    VisitSyncPayload sync{25, 18, 3, 44, 55, 66};
    injectSession(desktop, LinkMessageType::VISIT_SYNC, 502, requestSeq,
                  &sync, sizeof(sync));
    service.update(desktop.millis());
    Platform::PeerPacket response = takeType(desktop,
                                             LinkMessageType::VISIT_ACCEPT);
    assert(response.payload[9] == 0);
    assert(response.payload[10] ==
           static_cast<uint8_t>(VisitHostResult::TEAM_NOT_SOLO));
    assert(state.teamCount == 2);
    assert(state.team[1].origin == Game::Origin::BEFRIENDED);
    acknowledge(desktop, response);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::ENDED);
    state.teamCount = 1;
    service.stop();
    drain(desktop);
}

void testHostRecallAndDuration(
    DesktopPlatform& desktop, Communication::VisitSessionService& service,
    Game::GameState& state) {
    VisitSyncPayload sync{25, 18, 3, 44, 55, 66};
    ConnectedHost host = connectHost(desktop, service, state, sync);
    assert(state.team[1].speciesId == 25);
    assert(state.team[1].level == 18);
    assert(state.team[1].nature == 3);
    assert(state.team[1].satiety == 44);
    assert(state.team[1].mood == 55);
    assert(state.team[1].affection == 66);
    acknowledge(desktop, host.firstStatus);
    service.update(desktop.millis());
    drain(desktop);

    injectSession(desktop, LinkMessageType::VISIT_RECALL, 503,
                  host.sessionId);
    service.update(desktop.millis());
    assert(state.teamCount == 1);
    assert(service.state() ==
           Communication::VisitSessionService::State::ENDING);
    assert(service.takeHostRecall());
    assert(!service.takeHostRecall());
    Platform::PeerPacket end = takeType(desktop, LinkMessageType::VISIT_END);
    acknowledge(desktop, end);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::ENDED);
    service.stop();
    assert(!service.takeHostRecall());
    drain(desktop);

    host = connectHost(desktop, service, state, sync);
    acknowledge(desktop, host.firstStatus);
    service.update(desktop.millis());
    drain(desktop);
    desktop.advanceMs(
        Communication::VisitSessionService::VISIT_DURATION_SEC * 1000UL);
    VisitPingPayload keepAlive{40, 50, 35, 50};
    injectSession(desktop, LinkMessageType::VISIT_PING, 504,
                  host.sessionId, &keepAlive, sizeof(keepAlive));
    service.update(desktop.millis());
    assert(state.teamCount == 1);
    end = takeType(desktop, LinkMessageType::VISIT_END);
    acknowledge(desktop, end);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::ENDED);
    service.stop();
    drain(desktop);
}

void testVisitorHealthSync(DesktopPlatform& desktop,
                           Communication::VisitSessionService& service,
                           Game::GameState& state) {
    VisitSyncPayload sync{25, 18, 3, 44, 55, 66};
    ConnectedHost host = connectHost(desktop, service, state, sync);
    assert(!service.visitorHealthKnown());
    acknowledge(desktop, host.firstStatus);
    service.update(desktop.millis());
    drain(desktop);

    const uint8_t legacyPing[2] = {70, 80};
    injectSession(desktop, LinkMessageType::VISIT_PING, 705,
                  host.sessionId, legacyPing, sizeof(legacyPing));
    service.update(desktop.millis());
    assert(state.team[1].satiety == 70 && state.team[1].mood == 80);
    assert(!service.visitorHealthKnown());

    VisitPingPayload ping{71, 81, 35, 50};
    injectSession(desktop, LinkMessageType::VISIT_PING, 706,
                  host.sessionId, &ping, sizeof(ping));
    service.update(desktop.millis());
    assert(service.visitorHealthKnown());
    assert(service.viewModel().remote.hpCur == 35);
    assert(service.viewModel().remote.hpMax == 50);
    assert(state.team[1].hpCur == 35 && state.team[1].hpMax == 50);

    ping.hpCur = 0;
    injectSession(desktop, LinkMessageType::VISIT_PING, 707,
                  host.sessionId, &ping, sizeof(ping));
    service.update(desktop.millis());
    assert(state.team[1].hpCur == 0 && state.team[1].fainted);
    service.stop();
    drain(desktop);
}

void testVisitorDepartureSignal(
    DesktopPlatform& desktop, Communication::VisitSessionService& service,
    Game::GameState& state) {
    VisitSyncPayload sync{25, 18, 3, 44, 55, 66};
    state.teamCount = 1;
    service.startHost();
    desktop.advanceMs(500);
    service.update(desktop.millis());
    Platform::PeerPacket hello = takeType(desktop, LinkMessageType::HELLO);
    const uint8_t roomId = hello.payload[5];
    injectWire(desktop, LinkMessageType::JOIN_REQ, roomId, 707);
    service.update(desktop.millis());
    service.acceptIncoming(true);
    takeType(desktop, LinkMessageType::JOIN_ACK);
    injectSession(desktop, LinkMessageType::VISIT_SYNC, 709, 707,
                  &sync, sizeof(sync));
    service.update(desktop.millis());
    Platform::PeerPacket accept = takeType(desktop,
                                           LinkMessageType::VISIT_ACCEPT);
    assert(service.state() ==
           Communication::VisitSessionService::State::SYNCING);
    injectSession(desktop, LinkMessageType::VISIT_DEPARTED, 710, 707);
    service.update(desktop.millis());
    assert(!service.visitorArrivalReady());
    acknowledge(desktop, accept);
    service.update(desktop.millis());
    assert(service.visitorArrivalReady());
    service.stop();
    drain(desktop);

    state.teamCount = 1;
    ConnectedHost host = connectHost(desktop, service, state, sync);
    assert(!service.visitorArrivalReady());
    acknowledge(desktop, host.firstStatus);
    service.update(desktop.millis());
    drain(desktop);
    injectSession(desktop, LinkMessageType::VISIT_DEPARTED, 710,
                  host.sessionId);
    service.update(desktop.millis());
    assert(service.visitorArrivalReady());
    service.stop();
    drain(desktop);

    state.teamCount = 1;
    connectVisitor(desktop, service);
    assert(!service.visitorDeparted());
    service.markVisitorDeparted();
    service.update(desktop.millis());
    assert(service.visitorDeparted());
    Platform::PeerPacket departure =
        takeType(desktop, LinkMessageType::VISIT_DEPARTED);
    assert(departure.payload[8] == 0);
    acknowledge(desktop, departure);
    service.update(desktop.millis());
    service.stop();
    drain(desktop);
}

void testVisitorAuthorityAndRecall(
    DesktopPlatform& desktop, Communication::VisitSessionService& service,
    Game::GameState& state) {
    state.teamCount = 1;
    state.team[0].speciesId = 7;
    state.team[0].level = 9;
    state.team[0].satiety = 61;
    state.team[0].mood = 72;
    state.team[0].affection = 83;
    state.team[0].hpCur = 35;
    state.team[0].hpMax = 50;
    uint16_t sessionId = connectVisitor(desktop, service);
    assert(service.viewModel().remainSec ==
           Communication::VisitSessionService::VISIT_DURATION_SEC);

    VisitStatusPayload status{1, 1234};
    injectSession(desktop, LinkMessageType::VISIT_STATUS, 602, sessionId,
                  &status, sizeof(status));
    service.update(desktop.millis());
    assert(service.viewModel().remainSec == 1234);
    desktop.advanceMs(1000);
    service.update(desktop.millis());
    assert(service.viewModel().remainSec == 1234);
    drain(desktop);

    service.endVisit();
    service.update(desktop.millis());
    Platform::PeerPacket recall = takeType(desktop,
                                           LinkMessageType::VISIT_RECALL);
    assert(recall.payload[8] == 0);
    acknowledge(desktop, recall);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::ENDED);
    service.stop();
    drain(desktop);
}

void testEndingTimeout(DesktopPlatform& desktop,
                       Communication::VisitSessionService& service,
                       Game::GameState& state) {
    state.teamCount = 1;
    connectVisitor(desktop, service);
    service.endVisit();
    service.update(desktop.millis());
    takeType(desktop, LinkMessageType::VISIT_RECALL);
    desktop.advanceMs(
        Communication::VisitSessionService::HANDSHAKE_TIMEOUT_MS);
    service.update(desktop.millis());
    assert(service.state() ==
           Communication::VisitSessionService::State::ENDED);
    service.stop();
    drain(desktop);
}

}  // namespace

int main() {
    DesktopPlatform desktop(".");
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());

    Game::GameState state;
    Communication::VisitSessionService service;
    service.attach(&state);

    testHostAdmission(service, state);
    testTouchBeforeFrameUpdate(desktop, service, state);
    testStageTimeouts(desktop, service, state);
    testManualDecisionAndRoomList(desktop, service, state);
    testHostRejectsLateTeamChange(desktop, service, state);
    testHostRecallAndDuration(desktop, service, state);
    testVisitorHealthSync(desktop, service, state);
    testVisitorDepartureSignal(desktop, service, state);
    testVisitorAuthorityAndRecall(desktop, service, state);
    testEndingTimeout(desktop, service, state);

    service.stop();
    EspNowLink::ins().end();
    return 0;
}
