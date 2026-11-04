#include "game/AuthoredNpc.h"
#include "game/ExploreMapGenerator.h"
#include "game/MarketMapData.h"
#include "ui/common/MarketView.h"
#include <cassert>

int main() {
    // Include ordinary exploration first: its defaults must not resize market.
    static_assert(AuthoredNpc::WIDTH==16&&AuthoredNpc::HEIGHT==12);
    static_assert(Market::CELL_WIDTH==18&&Market::CELL_HEIGHT==17);
    static_assert(sizeof(ExploreMapGenerator::Map{}.layers[0])/sizeof(uint16_t)==192);
    Market::Data d{};
    d.entry=Market::node(18,Market::HEIGHT-1);d.roads.set(d.entry);
    d.residentCount=1;d.residents[0].species=94;
    auto& r=d.residents[0];r.activity.spawn=Market::CELLS-1;
    r.navigation.set(Market::cellNode(r.activity.spawn));
    Market::Session s;assert(s.reset(d,38,25,0,0));
    const auto p=s.pose(0);
    assert(p.x==17.5f*26&&p.y==16.5f*26); // Index 305 must survive uint8 truncation.
    auto& player=const_cast<Market::Actor&>(s.actor(1));
    player.stage=2;player.from=player.to=Market::node(Market::WIDTH-2,Market::HEIGHT-2);
    const Ui::MarketView::Viewport v{360,360,1,true,true};
    assert(v.cameraX(s)==108&&v.cameraY(s)==82);
    assert(s.playerPose().x-v.cameraX(s)==347&&s.playerPose().y-v.cameraY(s)==347);
    const Ui::MarketView::Viewport portrait{368,448,1.5f,false,true,2};
    assert(portrait.pixel(26)==39&&portrait.pixel(Market::CELL_WIDTH*26)==702);
    assert(portrait.pixel(Market::CELL_HEIGHT*26)==663);
    assert(portrait.cameraX(s)==334&&portrait.cameraY(s)==215);
    assert(portrait.pixel(s.playerPose().x)-portrait.cameraX(s)==349);
    assert(portrait.pixel(s.playerPose().y)-portrait.cameraY(s)==429);
    player.from=player.to=Market::node(1,1);
    assert(portrait.cameraX(s)==0&&portrait.cameraY(s)==0);
    // Exact final-row and final-column tile values are retained in the data.
    const auto& authored=MarketMapData::get();
    assert(authored.map.layers[0][305]!=0);
}
