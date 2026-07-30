#pragma once

#include <cstddef>
#include <cstdint>

namespace Game {

using MoveId = uint16_t;

static constexpr uint8_t TEAM_CAP = 2;
static constexpr uint8_t STORAGE_CAP = 20;
static constexpr uint8_t ITEM_STACK_CAP = 99;
static constexpr uint32_t SAVE_MAGIC = 0x534D4F4E; // SMON
static constexpr uint16_t SAVE_VERSION = 1;
static constexpr uint8_t STAT_COUNT = 6;
static constexpr uint8_t NATURE_COUNT = 25;
static constexpr uint8_t LEVEL_MAX = 100;
static constexpr uint8_t MOVE_SLOT_COUNT = 3;
static constexpr uint8_t MOVE_PROFICIENCY_MAX = 100;
static constexpr uint8_t IV_MAX = 31;
static constexpr uint8_t EV_MAX = 252;
static constexpr uint16_t EV_TOTAL_MAX = 510;
static constexpr uint32_t HATCH_MAGIC = 0x48415443; // HATC
static constexpr uint8_t ROOM_FOOD_COUNT = 7;
static constexpr uint8_t SOAP_VARIANT_COUNT = 3;
static constexpr uint8_t ROOM_BOWL_CAPACITY = 1;
static constexpr uint8_t ROOM_NORMAL_FOOD_INDEX = 0;
static constexpr uint8_t ROOM_NORMAL_FOOD_BITES = 3;
static constexpr uint8_t ROOM_TASTY_FOOD_INDEX = 1;
static constexpr uint8_t ROOM_SWEET_FOOD_INDEX = 2;
static constexpr uint8_t ROOM_SPICY_FOOD_INDEX = 3;
static constexpr uint8_t ROOM_SOUR_FOOD_INDEX = 4;
static constexpr uint8_t ROOM_BITTER_FOOD_INDEX = 5;
static constexpr uint8_t ROOM_DRY_FOOD_INDEX = 6;
static constexpr uint8_t EXPLORE_AREA_COUNT = 6;
static constexpr uint8_t FRIENDSHIP_PITY_TRACKED_COUNT = 38;
static constexpr uint8_t DAILY_CANDY_PURCHASE_CAP = 1;
static constexpr uint32_t INITIAL_COINS = 1000;
static constexpr uint32_t INITIAL_GAME_MINUTES = 7UL * 60UL;
static constexpr uint8_t MET_AREA_STARTER = 0xFD;
static constexpr uint8_t MET_AREA_HATCHED = 0xFE;
static constexpr uint8_t MET_AREA_UNKNOWN = 0xFF;

enum class ItemId : uint8_t {
    NORMAL_FOOD = 0,
    POTION,
    SUPER_POTION,
    ANTIDOTE,
    CANDY,
    TASTY_FOOD,
    SWEET_FOOD,
    SPICY_FOOD,
    SOUR_FOOD,
    BITTER_FOOD,
    DRY_FOOD,
    PARALYZE_HEAL,
    AWAKENING,
    BURN_HEAL,
    ICE_HEAL,
    MAX_POTION,
    FULL_RESTORE,
    FULL_HEAL,
    FIRE_STONE,
    WATER_STONE,
    THUNDER_STONE,
    REVIVE,
    MAX_REPEL,
    HONEY,
    NUGGET,
    BIG_PEARL,
    STAR_PIECE,
    SOAP_0,
    SOAP_1,
    SOAP_2,
    HEART_SCALE,
    COUNT,
};
static_assert(
    static_cast<uint8_t>(ItemId::SOAP_2) -
        static_cast<uint8_t>(ItemId::SOAP_0) + 1 ==
        SOAP_VARIANT_COUNT,
    "soap item ids must stay contiguous");

// Maps food ItemIds to RoomState::food slots; returns -1 for non-food items.
inline int8_t foodIndexForItemId(ItemId item) {
    switch (item) {
    case ItemId::NORMAL_FOOD: return ROOM_NORMAL_FOOD_INDEX;
    case ItemId::TASTY_FOOD: return (int8_t)ROOM_TASTY_FOOD_INDEX;
    case ItemId::SWEET_FOOD: return (int8_t)ROOM_SWEET_FOOD_INDEX;
    case ItemId::SPICY_FOOD: return (int8_t)ROOM_SPICY_FOOD_INDEX;
    case ItemId::SOUR_FOOD: return (int8_t)ROOM_SOUR_FOOD_INDEX;
    case ItemId::BITTER_FOOD: return (int8_t)ROOM_BITTER_FOOD_INDEX;
    case ItemId::DRY_FOOD: return (int8_t)ROOM_DRY_FOOD_INDEX;
    default: return -1;
    }
}

// Inverse of foodIndexForItemId; returns COUNT for out-of-range slots.
inline ItemId itemIdForFoodIndex(uint8_t foodIndex) {
    switch (foodIndex) {
    case ROOM_NORMAL_FOOD_INDEX: return ItemId::NORMAL_FOOD;
    case ROOM_TASTY_FOOD_INDEX: return ItemId::TASTY_FOOD;
    case ROOM_SWEET_FOOD_INDEX: return ItemId::SWEET_FOOD;
    case ROOM_SPICY_FOOD_INDEX: return ItemId::SPICY_FOOD;
    case ROOM_SOUR_FOOD_INDEX: return ItemId::SOUR_FOOD;
    case ROOM_BITTER_FOOD_INDEX: return ItemId::BITTER_FOOD;
    case ROOM_DRY_FOOD_INDEX: return ItemId::DRY_FOOD;
    default: return ItemId::COUNT;
    }
}

inline int8_t soapIndexForItemId(ItemId item) {
    uint8_t value = static_cast<uint8_t>(item);
    uint8_t first = static_cast<uint8_t>(ItemId::SOAP_0);
    uint8_t last = static_cast<uint8_t>(ItemId::SOAP_2);
    return value >= first && value <= last ? static_cast<int8_t>(value - first) : -1;
}

inline ItemId itemIdForSoapIndex(uint8_t soapIndex) {
    if (soapIndex >= SOAP_VARIANT_COUNT) return ItemId::COUNT;
    return static_cast<ItemId>(
        static_cast<uint8_t>(ItemId::SOAP_0) + soapIndex);
}

enum class MajorStatus : uint8_t {
    NONE = 0,
    POISON,
    TOXIC,
    PARALYSIS,
    SLEEP,
    BURN,
    FREEZE,
};

enum class Origin : uint8_t {
    STARTER = 0,
    HATCHED,
    BEFRIENDED,
    TRADED,
    GIFT,
    VISITOR,
    UNKNOWN = 0xFF,
};

struct StatLine {
    uint8_t hp = 0;
    uint8_t atk = 0;
    uint8_t def = 0;
    uint8_t spa = 0;
    uint8_t spd = 0;
    uint8_t spe = 0;
};

inline uint8_t ivAt(uint32_t packed, uint8_t index) {
    if (index >= STAT_COUNT) return 0;
    return (packed >> (index * 5)) & 0x1F;
}

inline void setIv(uint32_t& packed, uint8_t index, uint8_t value) {
    if (index >= STAT_COUNT) return;
    uint32_t shift = index * 5;
    packed &= ~(0x1FUL << shift);
    packed |= ((uint32_t)(value > IV_MAX ? IV_MAX : value) & 0x1F) << shift;
}

inline uint8_t evAt(const StatLine& ev, uint8_t index) {
    switch (index) {
    case 0: return ev.hp;
    case 1: return ev.atk;
    case 2: return ev.def;
    case 3: return ev.spa;
    case 4: return ev.spd;
    case 5: return ev.spe;
    default: return 0;
    }
}

inline uint16_t evTotal(const StatLine& ev) {
    return ev.hp + ev.atk + ev.def + ev.spa + ev.spd + ev.spe;
}

inline uint8_t roomFoodBitesPerServing(uint8_t foodIndex) {
    return foodIndex == ROOM_NORMAL_FOOD_INDEX ? ROOM_NORMAL_FOOD_BITES : 1;
}

struct MonsterRuntime {
    uint16_t speciesId = 1;
    uint8_t level = 5;
    uint32_t exp = 0;
    uint16_t hpCur = 20;
    uint16_t hpMax = 20;
    MoveId move1Id = 0;
    MoveId move2Id = 0;
    MoveId move3Id = 0;
    uint32_t ivPacked = 0;
    StatLine ev;
    uint8_t nature = 0;
    uint8_t affection = 30;
    uint8_t mood = 70;
    uint8_t satiety = 75;
    uint8_t moveProficiency[MOVE_SLOT_COUNT] = {};
    MajorStatus majorStatus = MajorStatus::NONE;
    uint8_t majorStatusTurns = 0;
    uint8_t metArea = MET_AREA_UNKNOWN;
    uint8_t petCountToday = 0;
    Origin origin = Origin::STARTER;
    bool fainted = false;
    int8_t bond = 100;
    uint32_t metAt = 0;
    uint32_t lastSeenAt = 0;
    uint32_t lastPettedAt = 0;
    uint32_t lastExploredAt = 0;
    uint32_t lastWindowGazeAt = 0;
};

static_assert(sizeof(MonsterRuntime) == 64,
              "MonsterRuntime layout is part of the v1 save format");
static_assert(offsetof(MonsterRuntime, bond) == 43,
              "bond must reuse the v1 padding byte to preserve save size");

struct BagState {
    uint8_t paralyzeHeal = 0;
    uint8_t awakening = 0;
    uint8_t burnHeal = 0;
    uint8_t iceHeal = 0;
    uint8_t potion = 2;
    uint8_t superPotion = 0;
    uint8_t antidote = 0;
    uint8_t candy = 1;
    uint8_t soap[SOAP_VARIANT_COUNT] = {};
    uint8_t maxPotion = 0;
    uint8_t fullRestore = 0;
    uint8_t fullHeal = 0;
    uint8_t fireStone = 0;
    uint8_t waterStone = 0;
    uint8_t thunderStone = 0;
    uint8_t revive = 0;
    uint8_t maxRepel = 0;
    uint8_t honey = 0;
    uint8_t nugget = 0;
    uint8_t bigPearl = 0;
    uint8_t starPiece = 0;
    uint8_t heartScale = 0;
};
static_assert(sizeof(BagState) == 24,
              "heart scale must reuse the existing GameState alignment byte");

struct RoomState {
    uint8_t food[ROOM_FOOD_COUNT] = {2, 0};
    uint8_t selectedFood = 0;
    uint8_t bowlFood = 0;
    uint8_t bowlCount = 0;
    uint8_t bowlBitesRemaining = 0;
    uint8_t roomStyle = 0;
    uint8_t activeToy = 0;
    uint8_t ownedToys = 0;
    uint16_t ownedFurniture = 0;
    uint16_t placedFurniture = 0;
};

struct PlayerSettings {
    uint8_t brightness = 128;
    uint8_t speedIndex = 0; // 1x, 2x, 4x, 8x
    uint16_t longPressMs = 500;
    uint16_t doubleClickMs = 300;
    uint8_t volume = 50;
    bool voiceCallEnabled = false;
    bool vibrationOn = false;
    uint8_t idleTimeoutIndex = 2; // 0=30s, 1=2min, 2=5min, 3=10min, 4=never
    bool leftHanded = false;
    uint8_t language = 0; // 0 = zh-CN
};

struct GameState {
    uint32_t magic = SAVE_MAGIC;
    uint16_t version = SAVE_VERSION;
    uint16_t checksum = 0;
    bool oobeDone = false;
    uint16_t hatchSeconds = 180;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 1;
    MonsterRuntime team[TEAM_CAP];
    uint8_t storageCount = 0;
    MonsterRuntime storage[STORAGE_CAP];
    BagState bag;
    RoomState room;
    uint32_t coins = INITIAL_COINS;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint16_t careExpToday = 0;
    uint16_t careDay = 0;
    uint8_t pairMoodRewardsToday = 0;
    uint8_t candyPurchasesToday = 0;
    uint32_t gameMinutesTotal = INITIAL_GAME_MINUTES;
    bool pendingLevelUp = false;
    uint8_t pendingLevelUpLevel = 0;
    bool pendingMoveLearn = false;
    uint8_t pendingMoveSlot = 0;
    MoveId pendingMoveId = 0;
    uint16_t pendingMoveCursor = 0;
    PlayerSettings settings;
    // 栖息地轮换：每区域活跃池重抽计数，击败区域头目 +1（§7.2/§7.6）
    uint8_t explorePoolRerollCounts[EXPLORE_AREA_COUNT] = {};
    // 结交累积：按白名单物种独立记录，索引见 FriendshipPity.h（§7.9.4）
    uint8_t friendshipPityFailCounts[FRIENDSHIP_PITY_TRACKED_COUNT] = {};
    // 特殊精灵：bit0=卡比兽，bit1=拉帝亚斯，bit2=拉帝欧斯（§八）
    uint8_t specialBossDefeatedMask = 0;
    // [0]=拉帝亚斯，[1]=拉帝欧斯；游荡战结束后递增
    uint8_t roamingRerollCounts[2] = {};
};
static_assert(sizeof(GameState) == 1560,
              "v1 save size changed; update the save contract deliberately");
static_assert(offsetof(GameState, room) == 1452,
              "BagState must not shift the v1 room/save layout");

struct HatchProgress {
    uint32_t magic = HATCH_MAGIC;
    uint16_t checksum = 0;
    uint16_t elapsedSeconds = 0;
    uint16_t pokeCount = 0;
    uint16_t wipeCount = 0;
};

} // namespace Game
