// Host checks for the Gengar stall rules and their save round trip.
#include <cassert>
#include <cstdio>
#include <cstring>
#include "core/SaveCodec.h"
#include "game/MarketTutorService.h"
#include "game/MonsterFactory.h"

using namespace Game;

static GameState party(uint16_t a, uint8_t la, uint16_t b, uint8_t lb) {
    GameState s{};
    s.oobeDone = true; s.teamCount = 2; s.coins = 5000;
    s.team[0] = MonsterFactory::create(a, la);
    s.team[1] = MonsterFactory::create(b, lb);
    s.team[0].ivPacked = 0x111; s.team[0].metAt = 10;
    s.team[1].ivPacked = 0x222; s.team[1].metAt = 20;
    return s;
}

int main() {
    // Every exported tutor reference must resolve in the actual move registry.
    for (unsigned i = 0; i < speciesCount(); ++i) {
        const auto& species = speciesTable()[i];
        for (unsigned j = 0; j < tutorMoveCountForSpecies(species); ++j)
            assert(findMove(tutorMoveForSpecies(species, j)));
    }
    // Pool rules: TM-compatible or future level-up; never recallable/known/basic.
    {
        auto s = party(133, 18, 25, 18);
        const Species* eevee = findSpecies(133);
        MoveId pool[MarketTutor::POOL_CAP];
        const uint8_t n = MarketTutor::collectPool(s.team[0], pool, MarketTutor::POOL_CAP);
        assert(n > 0);
        bool hasShadowBall = false, hasFutureLevel = false;
        for (uint8_t i = 0; i < n; ++i) {
            assert(!canRetainLevelMove(*eevee, pool[i], s.team[0].level));
            assert(!MarketTutor::knows(s.team[0], pool[i]));
            assert(!isBasicFirstMoveForSpecies(*eevee, pool[i]));
            assert(isMoveBattleSupported(pool[i]));
            hasShadowBall |= pool[i] == 247;
            hasFutureLevel |= moveLearnLevelForSpecies(*eevee, pool[i]) > s.team[0].level;
        }
        assert(hasShadowBall && hasFutureLevel);
        // Pikachu cannot learn Shadow Ball.
        assert(MarketTutor::eligibility(s.team[1], 247) == MarketTutor::Eligibility::INCOMPATIBLE);
        assert(MarketTutor::eligibility(s.team[0], 247) == MarketTutor::Eligibility::CAN_LEARN);
    }
    // Period draw: deterministic, stable through team changes, private RNG.
    {
        auto s = party(133, 18, 25, 18);
        s.gameMinutesTotal = 3 * 1440 + 60;
        assert(MarketTutor::ensurePeriod(s));
        const auto first = s.marketTutor;
        assert(first.move && first.periodDay == 3);
        assert(first.drawnSpecies == 133 || first.drawnSpecies == 25);
        const Species* drawn = findSpecies(first.drawnSpecies);
        const auto& owner = s.team[0].speciesId == first.drawnSpecies ? s.team[0] : s.team[1];
        assert(MarketTutor::teachable(*drawn, owner, first.move));
        std::swap(s.team[0], s.team[1]);
        s.gameMinutesTotal += 600;
        assert(!MarketTutor::ensurePeriod(s));
        assert(std::memcmp(&s.marketTutor, &first, sizeof(first)) == 0);
        auto again = party(133, 18, 25, 18); again.gameMinutesTotal = 3 * 1440 + 5;
        MarketTutor::ensurePeriod(again);
        assert(again.marketTutor.move == first.move);
        s.gameMinutesTotal = 4 * 1440;
        assert(MarketTutor::ensurePeriod(s) && s.marketTutor.periodDay == 4);
        // Draw coverage: both members get drawn across many days.
        bool drew[2] = {};
        for (uint32_t day = 0; day < 200; ++day) {
            auto d = party(133, 18, 25, 18); d.gameMinutesTotal = day * 1440;
            MarketTutor::ensurePeriod(d);
            assert(d.marketTutor.move);
            drew[d.marketTutor.drawnSpecies == 25] = true;
        }
        assert(drew[0] && drew[1]);
    }
    // Empty pool closes the class.
    {
        auto s = party(133, 18, 25, 18);
        s.team[0].origin = Origin::VISITOR; s.team[1].origin = Origin::VISITOR;
        MarketTutor::ensurePeriod(s);
        assert(!s.marketTutor.move);
        assert(MarketTutor::checkTeach(s, 0, 1) == MarketTutor::Result::NO_CLASS);
    }
    // Teaching: replaces the chosen slot, resets proficiency, charges once.
    {
        auto s = party(133, 18, 25, 18);
        s.team[0].move2Id = 129; s.team[0].moveProficiency[1] = 72;
        s.marketTutor.periodDay = 0; s.marketTutor.move = 247; s.marketTutor.drawnSpecies = 133;
        const uint16_t price = MarketTutor::lessonPrice(247);
        assert(price == 400);
        assert(MarketTutor::checkTeach(s, 1, 1) == MarketTutor::Result::INCOMPATIBLE);
        assert(MarketTutor::checkTeach(s, 0, 0) == MarketTutor::Result::BAD_SLOT);
        auto poor = s; poor.coins = 399;
        assert(MarketTutor::teach(poor, 0, 1) == MarketTutor::Result::NO_COINS && poor.coins == 399);
        auto hungry = s; hungry.team[0].satiety = 0;
        assert(MarketTutor::teach(hungry, 0, 1) == MarketTutor::Result::HUNGRY);
        auto fainted = s; fainted.team[0].hpCur = 0;
        assert(MarketTutor::teach(fainted, 0, 1) == MarketTutor::Result::FAINTED);
        assert(MarketTutor::teach(s, 0, 1) == MarketTutor::Result::OK);
        assert(s.team[0].move2Id == 247 && s.team[0].moveProficiency[1] == 0 && s.coins == 5000 - price);
        assert(MarketTutor::teach(s, 0, 2) == MarketTutor::Result::ALREADY_TAUGHT && s.coins == 5000 - price);
        // A taught TM move survives evolution and save sanitizing rules.
        const Species* espeon = findSpecies(196);
        if (espeon) assert(canRetainSpecialMove(*espeon, 247, 18));
        assert(canRetainSpecialMove(*findSpecies(133), 247, 18));
        // ...but is not offered by heart-scale recall.
        MoveId recall[80];
        s.team[0].move2Id = 129;
        const uint8_t r = collectRecallableMoves(*findSpecies(133), s.team[0], recall, 80);
        for (uint8_t i = 0; i < r; ++i) assert(recall[i] != 247);
    }
    // Training: identity-bound, once per period, capped at 100.
    {
        auto s = party(25, 18, 133, 18);
        s.team[0].move2Id = 486; s.team[0].moveProficiency[1] = 64;
        s.team[0].move3Id = 98; s.team[0].moveProficiency[2] = 100;
        MarketTutor::ensurePeriod(s);
        assert(MarketTutor::checkTrain(s, 0, 2) == MarketTutor::Result::MAXED);
        assert(MarketTutor::train(s, 0, 1) == MarketTutor::Result::OK);
        assert(s.team[0].moveProficiency[1] == 72 && s.coins == 4800);
        assert(MarketTutor::train(s, 0, 1) == MarketTutor::Result::ALREADY_TRAINED);
        std::swap(s.team[0], s.team[1]);
        assert(MarketTutor::checkTrain(s, 1, 1) == MarketTutor::Result::ALREADY_TRAINED);
        assert(MarketTutor::trainableMembers(s) == 1);
        s.gameMinutesTotal += 1440; MarketTutor::ensurePeriod(s);
        assert(MarketTutor::checkTrain(s, 1, 1) == MarketTutor::Result::OK);
        assert(MarketTutor::trainedProficiency(97) == 100 && MarketTutor::trainStep(97).cost == 300);
        assert(MarketTutor::trainedProficiency(39) == 49 && MarketTutor::trainedProficiency(40) == 48);
    }
    // Recall keeps the heart-scale rule.
    {
        auto s = party(133, 18, 25, 18);
        assert(MarketTutor::recall(s, 0, 28, 1) == MarketTutor::Result::NO_HEART_SCALE);
        s.bag.heartScale = 1;
        assert(MarketTutor::recall(s, 0, 28, 1) == MarketTutor::Result::OK);
        assert(s.team[0].move2Id == 28 && s.bag.heartScale == 0);
    }
    // Save codec: schema 4 round trip, schema 3 payloads decode to defaults.
    {
        auto s = party(133, 18, 25, 18);
        s.marketTutor.periodDay = 9; s.marketTutor.move = 247; s.marketTutor.drawnSpecies = 133;
        s.marketTutor.taught = 1; s.marketTutor.trainCount = 1; s.marketTutor.trained[0] = {0x111, 10};
        static uint8_t encoded[SaveCodec::MAX_ENCODED_BYTES];
        size_t length = 0;
        MainSceneViewState view{};
        assert(SaveCodec::encode(s, view, 3, encoded, sizeof(encoded), length));
        auto* decoded = new SaveCodec::Snapshot{};
        assert(SaveCodec::decode(encoded, length, *decoded));
        assert(std::memcmp(&decoded->state.marketTutor, &s.marketTutor, sizeof(s.marketTutor)) == 0);
        // Fabricate the frozen schema-3 payload by removing the v6 extension.
        constexpr size_t VIEW = 84, EXT = 4 + 2 + 2 + 1 + 1 + MARKET_TUTOR_TRAIN_CAP * 8 + 4 + 2 * MARKET_SHELF_SLOTS +
                                4 + 1 + MARKET_CARE_PERIOD_CAP * 8;
        static uint8_t legacy[SaveCodec::MAX_ENCODED_BYTES];
        const size_t cut = length - VIEW - EXT;
        std::memcpy(legacy, encoded, cut);
        std::memcpy(legacy + cut, encoded + cut + EXT, VIEW);
        const size_t legacyLength = length - EXT;
        legacy[4] = 3; legacy[SaveCodec::HEADER_BYTES + 4] = 5;
        const uint16_t payload = uint16_t(legacyLength - SaveCodec::HEADER_BYTES);
        legacy[12] = uint8_t(payload); legacy[13] = uint8_t(payload >> 8);
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < legacyLength; ++i) {
            crc ^= (i == 14 || i == 15) ? 0 : legacy[i];
            for (int b = 0; b < 8; ++b) crc = (crc & 1) ? uint16_t((crc >> 1) ^ 0xA001) : uint16_t(crc >> 1);
        }
        legacy[14] = uint8_t(crc); legacy[15] = uint8_t(crc >> 8);
        *decoded = {};
        assert(SaveCodec::decode(legacy, legacyLength, *decoded));
        assert(decoded->state.version == SAVE_VERSION && decoded->state.coins == s.coins);
        assert(decoded->state.marketTutor.periodDay == MARKET_PERIOD_NONE && !decoded->state.marketTutor.move);
        assert(decoded->state.team[0].speciesId == 133);
        delete decoded;
    }
    std::puts("market-tutor: pool draw teach train recall save OK");
}
