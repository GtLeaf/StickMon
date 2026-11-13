#pragma once
#include "game/GameState.h"

namespace Game::MarketItemCatalog {
struct Entry { ItemId id; uint16_t price; uint8_t unlockArea; const char* name; const char* description; };
inline constexpr Entry ITEMS[] = {
    {ItemId::HP_UP,600,3,"HP增强剂","提升HP努力值10点"},
    {ItemId::PROTEIN,600,3,"攻击增强剂","提升攻击努力值10点"},
    {ItemId::IRON,600,3,"防御增强剂","提升防御努力值10点"},
    {ItemId::CALCIUM,600,3,"特攻增强剂","提升特攻努力值10点"},
    {ItemId::ZINC,600,3,"特防增强剂","提升特防努力值10点"},
    {ItemId::CARBOS,600,3,"速度增强剂","提升速度努力值10点"},
    {ItemId::POMEG_BERRY,200,3,"榴石果","降低HP努力值10点 亲密提升2点"},
    {ItemId::KELPSY_BERRY,200,3,"藻根果","降低攻击努力值10点 亲密提升2点"},
    {ItemId::QUALOT_BERRY,200,3,"比巴果","降低防御努力值10点 亲密提升2点"},
    {ItemId::HONDEW_BERRY,200,3,"哈密果","降低特攻努力值10点 亲密提升2点"},
    {ItemId::GREPA_BERRY,200,3,"萄葡果","降低特防努力值10点 亲密提升2点"},
    {ItemId::TAMATO_BERRY,200,3,"茄番果","降低速度努力值10点 亲密提升2点"},
    {ItemId::MAX_REVIVE,600,4,"活力块","复活一只精灵 恢复全部HP"},
    {ItemId::LAVA_COOKIE,120,2,"釜炎仙贝","治愈异常状态"},
    {ItemId::ORAN_BERRY,20,0,"橙橙果","恢复10点HP"},
    {ItemId::SITRUS_BERRY,60,1,"文柚果","恢复30点HP"},
    {ItemId::LUM_BERRY,100,2,"木子果","治愈异常状态"},
};
inline constexpr bool contains(ItemId id) { return id >= ItemId::HP_UP && id < ItemId::COUNT; }
inline const Entry* find(ItemId id) { return contains(id) ? &ITEMS[uint8_t(id)-uint8_t(ItemId::HP_UP)] : nullptr; }
inline constexpr bool nutrient(ItemId id) { return id >= ItemId::HP_UP && id <= ItemId::CARBOS; }
inline constexpr bool resetBerry(ItemId id) { return id >= ItemId::POMEG_BERRY && id <= ItemId::TAMATO_BERRY; }
inline constexpr uint8_t statIndex(ItemId id) {
    return nutrient(id) ? uint8_t(id)-uint8_t(ItemId::HP_UP) : uint8_t(id)-uint8_t(ItemId::POMEG_BERRY);
}
}
