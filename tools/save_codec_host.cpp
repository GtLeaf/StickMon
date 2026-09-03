#include "core/SaveCodec.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

Game::GameState makeState() {
    Game::GameState state;
    state.oobeDone = true;
    state.hatchSeconds = 1234;
    state.activeSlot = 1;
    state.teamCount = 2;
    state.team[0].speciesId = 25;
    state.team[0].level = 22;
    state.team[0].exp = 98765;
    state.team[0].hpCur = 71;
    state.team[0].hpMax = 88;
    state.team[0].ivPacked = 0x01234567;
    state.team[0].majorStatus = Game::MajorStatus::PARALYSIS;
    state.team[0].gender = static_cast<uint8_t>(Game::Gender::FEMALE);
    state.team[0].bond = -12;
    state.team[0].metAt = 0x01020304;
    state.team[1].speciesId = 133;
    state.team[1].origin = Game::Origin::BEFRIENDED;
    state.team[1].gender = static_cast<uint8_t>(Game::Gender::MALE);
    state.storageCount = 1;
    state.storage[0].speciesId = 4;
    state.storage[0].gender = static_cast<uint8_t>(Game::Gender::FEMALE);
    state.bag.candy = 7;
    state.bag.thunderStone = 2;
    state.bag.soap[2] = 3;
    state.room.food[1] = 4;
    state.room.selectedFood = 1;
    state.room.bowlFood = 1;
    state.room.bowlBitesRemaining = 1;
    state.coins = 0x10203040;
    state.gameMinutesTotal = 456789;
    state.pendingLevelUp = true;
    state.pendingLevelUpLevel = 23;
    state.pendingMoveLearn = true;
    state.pendingMoveSlot = 1;
    state.pendingMoveId = 85;
    state.pendingMoveCursor = 6;
    state.settings.brightness = 201;
    state.settings.speedIndex = 3;
    state.settings.longPressMs = 777;
    state.settings.doubleClickMs = 222;
    state.settings.volume = 66;
    state.settings.voiceCallEnabled = true;
    state.settings.vibrationOn = true;
    state.settings.leftHanded = true;
    state.explorePoolRerollCounts[2] = 4;
    state.friendshipPityFailCounts[3] = 5;
    state.specialBossDefeatedMask = 0x15;
    state.roamingRerollCounts[1] = 2;
    state.tutorialFlags = 0xA5;
    state.normalBossPitySlotIndex = 33;
    state.normalBossMissCount[4] = 9;
    return state;
}

MainSceneViewState makeView() {
    MainSceneViewState view;
    view.valid = true;
    view.speciesId = 25;
    view.monsterX = 71.5f;
    view.monsterY = 82.25f;
    view.targetX = 90.0f;
    view.targetY = 83.0f;
    view.aiMode = 2;
    view.pmdAction = 1;
    view.pmdDirection = 6;
    view.pmdFrame = 3;
    view.facingRight = false;
    view.faintRestActive = true;
    view.nextDecisionRemainingMs = 12345;
    view.postFeedAwakeRemainingMs = 54321;
    view.secondary.valid = true;
    view.secondary.speciesId = 133;
    view.secondary.ivPacked = 0x00112233;
    view.secondary.metAt = 0x11121314;
    view.secondary.nature = 4;
    view.secondary.metArea = 2;
    view.secondary.origin = static_cast<uint8_t>(Game::Origin::BEFRIENDED);
    view.secondary.x = 132.0f;
    view.secondary.y = 90.0f;
    view.secondary.targetX = 131.0f;
    view.secondary.targetY = 91.0f;
    view.secondary.sleepX = 140.0f;
    view.secondary.sleepY = 100.0f;
    view.secondary.state = 4;
    view.secondary.direction = 1;
    view.secondary.frameIndex = 7;
    view.secondary.facingRight = false;
    view.secondary.sleepSpotValid = true;
    view.secondary.stateRemainingMs = 8888;
    view.secondary.foodRetryRemainingMs = 9999;
    return view;
}

void verifyRoundTrip() {
    Game::GameState state = makeState();
    MainSceneViewState view = makeView();
    uint8_t encoded[SaveCodec::MAX_ENCODED_BYTES] = {};
    size_t length = 0;
    assert(SaveCodec::encode(state, view, 0x12345678, encoded,
                             sizeof(encoded), length));
    assert(length > SaveCodec::HEADER_BYTES);
    assert(encoded[0] == 'S' && encoded[1] == 'V' &&
           encoded[2] == 'C' && encoded[3] == '2');
    assert(encoded[4] == 1 && encoded[5] == 0);
    assert(encoded[8] == 0x78 && encoded[9] == 0x56 &&
           encoded[10] == 0x34 && encoded[11] == 0x12);
    uint16_t payloadLength = static_cast<uint16_t>(encoded[12]) |
                             static_cast<uint16_t>(encoded[13]) << 8;
    assert(payloadLength == length - SaveCodec::HEADER_BYTES);

    SaveCodec::Snapshot decoded;
    uint32_t sequence = 0;
    assert(SaveCodec::decode(encoded, length, decoded, &sequence));
    assert(sequence == 0x12345678);
    assert(decoded.state.oobeDone && decoded.state.teamCount == 2);
    assert(decoded.state.team[0].speciesId == 25);
    assert(decoded.state.team[0].gender ==
           static_cast<uint8_t>(Game::Gender::FEMALE));
    assert(decoded.state.team[1].gender ==
           static_cast<uint8_t>(Game::Gender::MALE));
    assert(decoded.state.storage[0].gender ==
           static_cast<uint8_t>(Game::Gender::FEMALE));
    assert(decoded.state.team[0].ivPacked == 0x01234567);
    assert(decoded.state.team[0].bond == -12);
    assert(decoded.state.bag.candy == 7 && decoded.state.bag.soap[2] == 3);
    assert(decoded.state.coins == 0x10203040);
    assert(decoded.state.pendingMoveId == 85);
    assert(decoded.state.settings.voiceCallEnabled);
    assert(decoded.state.normalBossMissCount[4] == 9);
    assert(decoded.view.valid && decoded.view.speciesId == 25);
    assert(std::fabs(decoded.view.monsterX - 71.5f) < 0.001f);
    assert(decoded.view.secondary.valid &&
           decoded.view.secondary.speciesId == 133);
    assert(decoded.view.secondary.sleepSpotValid);

    uint8_t reencoded[SaveCodec::MAX_ENCODED_BYTES] = {};
    size_t reencodedLength = 0;
    assert(SaveCodec::encode(decoded.state, decoded.view, sequence,
                             reencoded, sizeof(reencoded), reencodedLength));
    assert(reencodedLength == length);
    assert(std::memcmp(encoded, reencoded, length) == 0);
}

void verifyRejectsCorruptionAndBadCapacity() {
    Game::GameState state = makeState();
    MainSceneViewState view = makeView();
    uint8_t encoded[SaveCodec::MAX_ENCODED_BYTES] = {};
    size_t length = 0;
    assert(SaveCodec::encode(state, view, 1, encoded, sizeof(encoded), length));
    assert(!SaveCodec::encode(state, view, 1, encoded,
                              SaveCodec::HEADER_BYTES - 1, length));

    SaveCodec::Snapshot snapshot;
    uint8_t saved = encoded[length - 1];
    encoded[length - 1] ^= 0x80;
    assert(!SaveCodec::decode(encoded, length, snapshot));
    encoded[length - 1] = saved;
    assert(!SaveCodec::decode(encoded, length - 1, snapshot));
    assert(!SaveCodec::decode(encoded, SaveCodec::HEADER_BYTES - 1,
                              snapshot));
}

}  // namespace

int main() {
    verifyRoundTrip();
    verifyRejectsCorruptionAndBadCapacity();
    return 0;
}
