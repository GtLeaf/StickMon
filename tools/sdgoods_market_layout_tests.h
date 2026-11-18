#pragma once

// User-visible layout checks run against the real round controllers and fonts.
static void stallLayoutTests(const Game::GameState& initial,const std::string& out) {
    auto state=initial;state.teamCount=1;state.team[0]=Game::MonsterFactory::create(25,50);
    state.team[0].satiety=100;state.coins=20000;state.marketTutor={};
    Game::MarketTutor::ensurePeriod(state);
    Game::MoveId pool[Game::MarketTutor::POOL_CAP];
    const int count=Game::MarketTutor::collectPool(state.team[0],pool,Game::MarketTutor::POOL_CAP);assert(count);
    state.marketTutor.move=pool[0];state.marketTutor.taught=0;
    Ui::RoundMarketTutorUi tutor;tutor.open(state);tutor.page_=TutorPage::CONFIRM;
    tutor.transition_=Tutor::Transition::NONE;tutor.member_=0;tutor.slot_=1;
    Ui::RoundMarketShopUi shop;shop.open(state);shop.page_=Page::SHELF;shop.transition_=UI::Transition::NONE;
    assert(tutor.panelTop()==132&&shop.panelTop()==132&&"approved narrow panels start at132");
    auto& c=PixelRenderer::canvas();const auto previous=Platform::display().frameBuffer();
    std::vector<uint16_t> pixels(360*360);c.attach({pixels.data(),360,360,false});
    c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(false);c.clearClipRect();
    const auto sentinel=PixelRenderer::rgb(8,16,24),paper=PixelRenderer::rgb(248,248,224),ink=PixelRenderer::rgb(40,56,80);
    c.fillSprite(sentinel);state.coins=0;tutor.render(state);
    assert(c.readPixel(20,200)==sentinel&&"side scene remains exposed");
    assert(c.readPixel(96,282)==paper&&"action has rounded corners");
    int top=360,bottom=-1;
    for(int y=194;y<282;++y)for(int x=68;x<292;++x){const auto p=c.readPixel(x,y);
        assert(p!=PixelRenderer::rgb(183,56,56)&&"coin shortfall copy removed");
        if(p==ink){top=std::min(top,y);bottom=std::max(bottom,y);}}
    assert(bottom>=top&&std::abs((top+bottom)-474)<=2&&"body centered between header and action");
    capture(out,"layout-tutor-no-coins");renderContract(tutor,state);
    // A failed save must not cover or change the selectable move rows.
    c.attach({pixels.data(),360,360,false});c.clearClipRect();
    tutor.page_=TutorPage::TRAIN_MOVE;state.coins=20000;c.fillSprite(sentinel);tutor.render(state);
    std::vector<uint16_t> moveRows(pixels.begin()+184*360,pixels.begin()+280*360);
    tutor.error_=Ui::MarketTutorText::SAVE_FAILED;c.fillSprite(sentinel);tutor.render(state);
    assert(std::equal(moveRows.begin(),moveRows.end(),pixels.begin()+184*360)&&"training error must not overwrite move rows");
    capture(out,"layout-training-save-failed");renderContract(tutor,state);
    c.fillSprite(sentinel);shop.render(state);assert(c.readPixel(20,200)==sentinel);
    assert(c.readPixel(180,355)!=sentinel&&"shelf continues to physical bottom");
    const auto request=shop.touch({Type::DOWN,180,340,1000},state);assert(request.kind==UI::Request::NONE);
    shop.touch({Type::UP,180,340,1040},state);
    assert(shop.page()==Page::CONFIRM&&shop.selected()==2&&"bottom shelf row is selectable");
    auto singleAction=shop;
    singleAction.touch({Type::DOWN,80,300,1050},state);
    assert(singleAction.touch({Type::UP,80,300,1090},state).kind==UI::Request::NONE&&singleAction.page()==Page::CONFIRM&&"removed back button has no hit target");
    singleAction.touch({Type::DOWN,180,308,1150},state);
    assert(singleAction.touch({Type::UP,180,308,1190},state).kind==UI::Request::BUY&&singleAction.pending()&&"centered purchase requests payment");
    singleAction.committed(false);
    singleAction.touch({Type::DOWN,35,200,1250},state);
    singleAction.touch({Type::UP,35,200,1290},state);
    assert(singleAction.page()==Page::SHELF&&"outside panel returns to shelf");
    state.coins=0;
    shop.touch({Type::DOWN,232,308,1100},state);
    assert(shop.touch({Type::UP,232,308,1140},state).kind==UI::Request::NONE&&!shop.pending());
    assert(shop.error_==nullptr&&"disabled purchase has no coin-shortfall response");
    state.coins=20000;
    c.attach({pixels.data(),360,360,false});c.clearClipRect();
    const auto red=PixelRenderer::rgb(183,56,56);
    c.fillSprite(sentinel);
    Ui::RoundMarketStall::sampledText(&c,0,0,Ui::MarketShopText::STALE,int(std::strlen(Ui::MarketShopText::STALE)),16,red);
    const auto fullErrorInk=std::count(pixels.begin(),pixels.end(),red);
    shop.error_=Ui::MarketShopText::STALE;c.fillSprite(sentinel);shop.render(state);
    assert(std::count(pixels.begin(),pixels.end(),red)>=fullErrorInk&&"long stale-period error is complete");
    capture(out,"layout-shop-stale");renderContract(shop,state);
    c.attach({pixels.data(),360,360,false});c.clearClipRect();
    shop.page_=Page::ADVICE;shop.error_=Ui::MarketShopText::ADVICE_SAVE_FAILED;
    c.fillSprite(sentinel);shop.render(state);
    for(int y=0;y<360;++y)for(int x=0;x<360;++x)
        if(c.readPixel(x,y)==PixelRenderer::rgb(183,56,56))
            assert((x-180)*(x-180)+(y-180)*(y-180)<180*180&&"complete advice error stays inside circle");
    capture(out,"layout-advice-save-failed");renderContract(shop,state);
    c.attach(previous);c.setNativeText(false);c.clearClipRect();
    std::puts("round-layout: narrow/full-height panels, rounded disabled action, centered copy, bottom shelf touch OK");
}

static void stallCameraBounds(const Market::Session& session) {
    auto view=Ui::MarketWalkUi::viewport();
    const int world=view.pixel(Market::CELL_WIDTH*26);
    for(int offset:{-1000,1000}){
        view.cameraOffsetX=offset;
        assert(view.cameraX(session)>=0&&view.cameraX(session)<=world-view.width&&"stall camera stays inside authored map");
    }
}
