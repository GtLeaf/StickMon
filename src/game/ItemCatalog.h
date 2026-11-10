// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "game/ContentData.h"
#include "game/GameState.h"
namespace ItemCatalog {
struct Entry {
    Game::ItemId id;uint16_t buyPrice,sellPrice;uint8_t unlockArea;
    const char* name;const char* description;
};
template<class V> inline void contentFields(V& v,Entry& x) {v(x.id,x.buyPrice,x.sellPrice,x.unlockArea,x.name,x.description);}
#if STICKMON_EXTERNAL_CONTENT
inline const ContentData::Table<Entry,size_t(Game::ItemId::COUNT)> ITEMS{50};
inline const Entry* find(Game::ItemId id) {
    for(const auto& e:ITEMS) if(e.id==id)return &e;
    return nullptr;
}
#endif
}
