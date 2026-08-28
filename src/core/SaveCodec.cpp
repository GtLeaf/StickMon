#include "core/SaveCodec.h"

#include <cstring>

namespace SaveCodec {
namespace {

class Writer {
public:
    Writer(uint8_t* output, size_t capacity)
        : output_(output), capacity_(capacity) {}

    bool ok() const { return ok_; }
    size_t size() const { return position_; }

    void u8(uint8_t value) { put(&value, sizeof(value)); }
    void i8(int8_t value) { u8(static_cast<uint8_t>(value)); }
    void u16(uint16_t value) {
        uint8_t bytes[2] = {
            static_cast<uint8_t>(value),
            static_cast<uint8_t>(value >> 8),
        };
        put(bytes, sizeof(bytes));
    }
    void u32(uint32_t value) {
        uint8_t bytes[4] = {
            static_cast<uint8_t>(value),
            static_cast<uint8_t>(value >> 8),
            static_cast<uint8_t>(value >> 16),
            static_cast<uint8_t>(value >> 24),
        };
        put(bytes, sizeof(bytes));
    }
    void f32(float value) {
        uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value), "float must be 32-bit");
        std::memcpy(&bits, &value, sizeof(bits));
        u32(bits);
    }

private:
    void put(const void* data, size_t length) {
        if (!ok_ || !data || length > capacity_ - position_) {
            ok_ = false;
            return;
        }
        std::memcpy(output_ + position_, data, length);
        position_ += length;
    }

    uint8_t* output_ = nullptr;
    size_t capacity_ = 0;
    size_t position_ = 0;
    bool ok_ = true;
};

class Reader {
public:
    Reader(const uint8_t* input, size_t length)
        : input_(input), length_(length) {}

    bool ok() const { return ok_; }
    size_t position() const { return position_; }

    uint8_t u8() {
        uint8_t value = 0;
        get(&value, sizeof(value));
        return value;
    }
    int8_t i8() { return static_cast<int8_t>(u8()); }
    uint16_t u16() {
        uint8_t bytes[2] = {};
        get(bytes, sizeof(bytes));
        return static_cast<uint16_t>(bytes[0]) |
               static_cast<uint16_t>(bytes[1]) << 8;
    }
    uint32_t u32() {
        uint8_t bytes[4] = {};
        get(bytes, sizeof(bytes));
        return static_cast<uint32_t>(bytes[0]) |
               static_cast<uint32_t>(bytes[1]) << 8 |
               static_cast<uint32_t>(bytes[2]) << 16 |
               static_cast<uint32_t>(bytes[3]) << 24;
    }
    float f32() {
        uint32_t bits = u32();
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

private:
    void get(void* output, size_t length) {
        if (!ok_ || !output || length > length_ - position_) {
            ok_ = false;
            return;
        }
        std::memcpy(output, input_ + position_, length);
        position_ += length;
    }

    const uint8_t* input_ = nullptr;
    size_t length_ = 0;
    size_t position_ = 0;
    bool ok_ = true;
};

uint16_t crc16(const uint8_t* bytes, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t index = 0; index < length; ++index) {
        uint8_t value = (index == 14 || index == 15) ? 0 : bytes[index];
        crc ^= value;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001)
                             : static_cast<uint16_t>(crc >> 1);
        }
    }
    return crc;
}

void writeMonster(Writer& writer, const Game::MonsterRuntime& monster) {
    writer.u16(monster.speciesId);
    writer.u8(monster.level);
    writer.u32(monster.exp);
    writer.u16(monster.hpCur);
    writer.u16(monster.hpMax);
    writer.u16(monster.move1Id);
    writer.u16(monster.move2Id);
    writer.u16(monster.move3Id);
    writer.u32(monster.ivPacked);
    writer.u8(monster.ev.hp);
    writer.u8(monster.ev.atk);
    writer.u8(monster.ev.def);
    writer.u8(monster.ev.spa);
    writer.u8(monster.ev.spd);
    writer.u8(monster.ev.spe);
    writer.u8(monster.nature);
    writer.u8(monster.affection);
    writer.u8(monster.mood);
    writer.u8(monster.satiety);
    for (uint8_t value : monster.moveProficiency) writer.u8(value);
    writer.u8(static_cast<uint8_t>(monster.majorStatus));
    writer.u8(monster.majorStatusTurns);
    writer.u8(monster.metArea);
    writer.u8(monster.petCountToday);
    writer.u8(static_cast<uint8_t>(monster.origin));
    writer.u8(monster.fainted ? 1 : 0);
    writer.i8(monster.bond);
    writer.u32(monster.metAt);
    writer.u32(monster.lastSeenAt);
    writer.u32(monster.lastPettedAt);
    writer.u32(monster.lastExploredAt);
    writer.u32(monster.lastWindowGazeAt);
}

void readMonster(Reader& reader, Game::MonsterRuntime& monster) {
    monster.speciesId = reader.u16();
    monster.level = reader.u8();
    monster.exp = reader.u32();
    monster.hpCur = reader.u16();
    monster.hpMax = reader.u16();
    monster.move1Id = reader.u16();
    monster.move2Id = reader.u16();
    monster.move3Id = reader.u16();
    monster.ivPacked = reader.u32();
    monster.ev.hp = reader.u8();
    monster.ev.atk = reader.u8();
    monster.ev.def = reader.u8();
    monster.ev.spa = reader.u8();
    monster.ev.spd = reader.u8();
    monster.ev.spe = reader.u8();
    monster.nature = reader.u8();
    monster.affection = reader.u8();
    monster.mood = reader.u8();
    monster.satiety = reader.u8();
    for (uint8_t& value : monster.moveProficiency) value = reader.u8();
    monster.majorStatus = static_cast<Game::MajorStatus>(reader.u8());
    monster.majorStatusTurns = reader.u8();
    monster.metArea = reader.u8();
    monster.petCountToday = reader.u8();
    monster.origin = static_cast<Game::Origin>(reader.u8());
    monster.fainted = reader.u8() != 0;
    monster.bond = reader.i8();
    monster.metAt = reader.u32();
    monster.lastSeenAt = reader.u32();
    monster.lastPettedAt = reader.u32();
    monster.lastExploredAt = reader.u32();
    monster.lastWindowGazeAt = reader.u32();
}

void writeState(Writer& writer, const Game::GameState& state) {
    writer.u32(Game::SAVE_MAGIC);
    writer.u16(Game::SAVE_VERSION);
    writer.u8(state.oobeDone ? 1 : 0);
    writer.u16(state.hatchSeconds);
    writer.u8(state.activeSlot);
    writer.u8(state.teamCount);
    for (const Game::MonsterRuntime& monster : state.team) writeMonster(writer, monster);
    writer.u8(state.storageCount);
    for (const Game::MonsterRuntime& monster : state.storage) writeMonster(writer, monster);

    writer.u8(state.bag.paralyzeHeal);
    writer.u8(state.bag.awakening);
    writer.u8(state.bag.burnHeal);
    writer.u8(state.bag.iceHeal);
    writer.u8(state.bag.potion);
    writer.u8(state.bag.superPotion);
    writer.u8(state.bag.antidote);
    writer.u8(state.bag.candy);
    for (uint8_t value : state.bag.soap) writer.u8(value);
    writer.u8(state.bag.maxPotion);
    writer.u8(state.bag.fullRestore);
    writer.u8(state.bag.fullHeal);
    writer.u8(state.bag.fireStone);
    writer.u8(state.bag.waterStone);
    writer.u8(state.bag.thunderStone);
    writer.u8(state.bag.revive);
    writer.u8(state.bag.maxRepel);
    writer.u8(state.bag.honey);
    writer.u8(state.bag.nugget);
    writer.u8(state.bag.bigPearl);
    writer.u8(state.bag.starPiece);
    writer.u8(state.bag.heartScale);

    for (uint8_t value : state.room.food) writer.u8(value);
    writer.u8(state.room.selectedFood);
    writer.u8(state.room.bowlFood);
    writer.u8(state.room.bowlCount);
    writer.u8(state.room.bowlBitesRemaining);
    writer.u8(state.room.roomStyle);
    writer.u8(state.room.activeToy);
    writer.u8(state.room.ownedToys);
    writer.u16(state.room.ownedFurniture);
    writer.u16(state.room.placedFurniture);

    writer.u32(state.coins);
    writer.u16(state.stepsToday);
    writer.u16(state.walkExpToday);
    writer.u16(state.careExpToday);
    writer.u16(state.careDay);
    writer.u8(state.pairMoodRewardsToday);
    writer.u8(state.candyPurchasesToday);
    writer.u32(state.gameMinutesTotal);
    writer.u8(state.pendingLevelUp ? 1 : 0);
    writer.u8(state.pendingLevelUpLevel);
    writer.u8(state.pendingMoveLearn ? 1 : 0);
    writer.u8(state.pendingMoveSlot);
    writer.u16(state.pendingMoveId);
    writer.u16(state.pendingMoveCursor);

    writer.u8(state.settings.brightness);
    writer.u8(state.settings.speedIndex);
    writer.u16(state.settings.longPressMs);
    writer.u16(state.settings.doubleClickMs);
    writer.u8(state.settings.volume);
    writer.u8(state.settings.voiceCallEnabled ? 1 : 0);
    writer.u8(state.settings.vibrationOn ? 1 : 0);
    writer.u8(state.settings.idleTimeoutIndex);
    writer.u8(state.settings.leftHanded ? 1 : 0);
    writer.u8(state.settings.language);

    for (uint8_t value : state.explorePoolRerollCounts) writer.u8(value);
    for (uint8_t value : state.friendshipPityFailCounts) writer.u8(value);
    writer.u8(state.specialBossDefeatedMask);
    for (uint8_t value : state.roamingRerollCounts) writer.u8(value);
    writer.u8(state.tutorialFlags);
    writer.u32(state.normalBossPitySlotIndex);
    for (uint8_t value : state.normalBossMissCount) writer.u8(value);
}

void readState(Reader& reader, Game::GameState& state) {
    state = Game::GameState{};
    uint32_t magic = reader.u32();
    uint16_t version = reader.u16();
    state.magic = magic;
    state.version = version;
    state.oobeDone = reader.u8() != 0;
    state.hatchSeconds = reader.u16();
    state.activeSlot = reader.u8();
    state.teamCount = reader.u8();
    for (Game::MonsterRuntime& monster : state.team) readMonster(reader, monster);
    state.storageCount = reader.u8();
    for (Game::MonsterRuntime& monster : state.storage) readMonster(reader, monster);

    state.bag.paralyzeHeal = reader.u8();
    state.bag.awakening = reader.u8();
    state.bag.burnHeal = reader.u8();
    state.bag.iceHeal = reader.u8();
    state.bag.potion = reader.u8();
    state.bag.superPotion = reader.u8();
    state.bag.antidote = reader.u8();
    state.bag.candy = reader.u8();
    for (uint8_t& value : state.bag.soap) value = reader.u8();
    state.bag.maxPotion = reader.u8();
    state.bag.fullRestore = reader.u8();
    state.bag.fullHeal = reader.u8();
    state.bag.fireStone = reader.u8();
    state.bag.waterStone = reader.u8();
    state.bag.thunderStone = reader.u8();
    state.bag.revive = reader.u8();
    state.bag.maxRepel = reader.u8();
    state.bag.honey = reader.u8();
    state.bag.nugget = reader.u8();
    state.bag.bigPearl = reader.u8();
    state.bag.starPiece = reader.u8();
    state.bag.heartScale = reader.u8();

    for (uint8_t& value : state.room.food) value = reader.u8();
    state.room.selectedFood = reader.u8();
    state.room.bowlFood = reader.u8();
    state.room.bowlCount = reader.u8();
    state.room.bowlBitesRemaining = reader.u8();
    state.room.roomStyle = reader.u8();
    state.room.activeToy = reader.u8();
    state.room.ownedToys = reader.u8();
    state.room.ownedFurniture = reader.u16();
    state.room.placedFurniture = reader.u16();

    state.coins = reader.u32();
    state.stepsToday = reader.u16();
    state.walkExpToday = reader.u16();
    state.careExpToday = reader.u16();
    state.careDay = reader.u16();
    state.pairMoodRewardsToday = reader.u8();
    state.candyPurchasesToday = reader.u8();
    state.gameMinutesTotal = reader.u32();
    state.pendingLevelUp = reader.u8() != 0;
    state.pendingLevelUpLevel = reader.u8();
    state.pendingMoveLearn = reader.u8() != 0;
    state.pendingMoveSlot = reader.u8();
    state.pendingMoveId = reader.u16();
    state.pendingMoveCursor = reader.u16();

    state.settings.brightness = reader.u8();
    state.settings.speedIndex = reader.u8();
    state.settings.longPressMs = reader.u16();
    state.settings.doubleClickMs = reader.u16();
    state.settings.volume = reader.u8();
    state.settings.voiceCallEnabled = reader.u8() != 0;
    state.settings.vibrationOn = reader.u8() != 0;
    state.settings.idleTimeoutIndex = reader.u8();
    state.settings.leftHanded = reader.u8() != 0;
    state.settings.language = reader.u8();

    for (uint8_t& value : state.explorePoolRerollCounts) value = reader.u8();
    for (uint8_t& value : state.friendshipPityFailCounts) value = reader.u8();
    state.specialBossDefeatedMask = reader.u8();
    for (uint8_t& value : state.roamingRerollCounts) value = reader.u8();
    state.tutorialFlags = reader.u8();
    state.normalBossPitySlotIndex = reader.u32();
    for (uint8_t& value : state.normalBossMissCount) value = reader.u8();
    state.checksum = 0;
}

void writeView(Writer& writer, const MainSceneViewState& view) {
    writer.u8(view.valid ? 1 : 0);
    writer.u16(view.speciesId);
    writer.f32(view.monsterX);
    writer.f32(view.monsterY);
    writer.f32(view.targetX);
    writer.f32(view.targetY);
    writer.u8(view.aiMode);
    writer.u8(view.pmdAction);
    writer.u8(view.pmdDirection);
    writer.u8(view.pmdFrame);
    writer.u8(view.facingRight ? 1 : 0);
    writer.u8(view.faintRestActive ? 1 : 0);
    writer.u32(view.nextDecisionRemainingMs);
    writer.u32(view.postFeedAwakeRemainingMs);
    const SecondarySceneViewState& secondary = view.secondary;
    writer.u8(secondary.valid ? 1 : 0);
    writer.u16(secondary.speciesId);
    writer.u32(secondary.ivPacked);
    writer.u32(secondary.metAt);
    writer.u8(secondary.nature);
    writer.u8(secondary.metArea);
    writer.u8(secondary.origin);
    writer.f32(secondary.x);
    writer.f32(secondary.y);
    writer.f32(secondary.targetX);
    writer.f32(secondary.targetY);
    writer.f32(secondary.sleepX);
    writer.f32(secondary.sleepY);
    writer.u8(secondary.state);
    writer.u8(secondary.direction);
    writer.u8(secondary.frameIndex);
    writer.u8(secondary.facingRight ? 1 : 0);
    writer.u8(secondary.sleepSpotValid ? 1 : 0);
    writer.u32(secondary.stateRemainingMs);
    writer.u32(secondary.foodRetryRemainingMs);
}

void readView(Reader& reader, MainSceneViewState& view) {
    view = MainSceneViewState{};
    view.valid = reader.u8() != 0;
    view.speciesId = reader.u16();
    view.monsterX = reader.f32();
    view.monsterY = reader.f32();
    view.targetX = reader.f32();
    view.targetY = reader.f32();
    view.aiMode = reader.u8();
    view.pmdAction = reader.u8();
    view.pmdDirection = reader.u8();
    view.pmdFrame = reader.u8();
    view.facingRight = reader.u8() != 0;
    view.faintRestActive = reader.u8() != 0;
    view.nextDecisionRemainingMs = reader.u32();
    view.postFeedAwakeRemainingMs = reader.u32();
    SecondarySceneViewState& secondary = view.secondary;
    secondary.valid = reader.u8() != 0;
    secondary.speciesId = reader.u16();
    secondary.ivPacked = reader.u32();
    secondary.metAt = reader.u32();
    secondary.nature = reader.u8();
    secondary.metArea = reader.u8();
    secondary.origin = reader.u8();
    secondary.x = reader.f32();
    secondary.y = reader.f32();
    secondary.targetX = reader.f32();
    secondary.targetY = reader.f32();
    secondary.sleepX = reader.f32();
    secondary.sleepY = reader.f32();
    secondary.state = reader.u8();
    secondary.direction = reader.u8();
    secondary.frameIndex = reader.u8();
    secondary.facingRight = reader.u8() != 0;
    secondary.sleepSpotValid = reader.u8() != 0;
    secondary.stateRemainingMs = reader.u32();
    secondary.foodRetryRemainingMs = reader.u32();
}

} // namespace

size_t encodedSizeUpperBound() { return MAX_ENCODED_BYTES; }

bool encode(const Game::GameState& state,
            const MainSceneViewState& view,
            uint32_t sequence,
            uint8_t* output,
            size_t capacity,
            size_t& written) {
    written = 0;
    if (!output || capacity < HEADER_BYTES) return false;

    Writer writer(output, capacity);
    writer.u32(MAGIC);
    writer.u16(SCHEMA_VERSION);
    writer.u16(0);
    writer.u32(sequence);
    writer.u16(0);
    writer.u16(0);
    writeState(writer, state);
    writeView(writer, view);
    if (!writer.ok() || writer.size() > 0xFFFFU) return false;

    uint16_t payloadLength = static_cast<uint16_t>(writer.size() - HEADER_BYTES);
    output[12] = static_cast<uint8_t>(payloadLength);
    output[13] = static_cast<uint8_t>(payloadLength >> 8);
    uint16_t checksum = crc16(output, writer.size());
    output[14] = static_cast<uint8_t>(checksum);
    output[15] = static_cast<uint8_t>(checksum >> 8);
    written = writer.size();
    return true;
}

bool decode(const uint8_t* input,
            size_t length,
            Snapshot& snapshot,
            uint32_t* sequence) {
    if (!input || length < HEADER_BYTES || length > MAX_ENCODED_BYTES ||
        crc16(input, length) !=
            static_cast<uint16_t>(input[14] | static_cast<uint16_t>(input[15]) << 8)) {
        return false;
    }

    Reader header(input, length);
    if (header.u32() != MAGIC || header.u16() != SCHEMA_VERSION ||
        header.u16() != 0) {
        return false;
    }
    uint32_t storedSequence = header.u32();
    uint16_t payloadLength = header.u16();
    header.u16();
    if (!header.ok() || payloadLength != length - HEADER_BYTES) return false;

    Reader reader(input + HEADER_BYTES, payloadLength);
    readState(reader, snapshot.state);
    readView(reader, snapshot.view);
    if (!reader.ok() || reader.position() != payloadLength ||
        snapshot.state.magic != Game::SAVE_MAGIC ||
        snapshot.state.version != Game::SAVE_VERSION ||
        snapshot.state.teamCount > Game::TEAM_CAP ||
        snapshot.state.storageCount > Game::STORAGE_CAP) {
        return false;
    }
    if (sequence) *sequence = storedSequence;
    return true;
}

} // namespace SaveCodec
