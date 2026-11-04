#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#include "assets/PokemonSprites.h"
#include "game/BondSystem.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
#define private public
#include "core/GameEngine.h"
#include "scenes/MenuScene.h"
#include "scenes/MainScene.h"
#undef private

#if STICKMON_CONTACTS_UI_BASELINE
#define COMPUTER(menu) (menu)
#define CONTACTS(menu) (menu)
#else
#define COMPUTER(menu) ((menu).computerUi)
#define CONTACTS(menu) ((menu).contactsUi)
#endif

#if STICKMON_CONTACTS_UI_BASELINE || STICKMON_MAIN_MENU_UI_BASELINE
#define MAIN_MENU(scene) (scene)
#else
#define MAIN_MENU(scene) ((scene).mainMenuUi)
#endif

class TouchInput final : public Platform::IInputDevice {
public:
    void update() override {}
    bool pressed(Platform::InputButton) const override { return false; }
    bool touchAvailable() const override { return STICKMON_UI_COMPACT_TOUCH; }
};

int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]);
    const auto bundle = desktop.serviceBundle();
    TouchInput input;
    Platform::Services services{bundle.lifecycle, bundle.clock, bundle.logger,
        input, bundle.display, bundle.audio, bundle.imu, bundle.power,
        bundle.blobs, bundle.resources, bundle.memory, bundle.peers};
    Platform::bind(services);
    assert(desktop.begin());
    auto& engine = GameEngine::ins();
    assert(engine.begin());
    auto& state = engine.gameState();
    MenuScene menu;
    unsigned checkpoints = 0;
    auto setup = [&](int contacts, int team = 1) {
        GameRandom::seed(1234);
        engine.contactVisit = {};
        engine.teamMemberArrivalPending = false;
        state = Game::GameState{};
        state.oobeDone = true;
        state.tutorialFlags = 0xff;
        state.teamCount = team;
        for (int i = 0; i < team; ++i) {
            state.team[i] = Game::MonsterFactory::create(i == 0 ? 1 : 25, 25);
            state.team[i].origin = Game::Origin::STARTER;
            state.team[i].metAt = 10 + i;
        }
        state.storageCount = contacts;
        const uint16_t species[] = {4,7,25,133,10,16};
        for (int i = 0; i < contacts; ++i) {
            state.storage[i] = Game::MonsterFactory::create(species[i % 6], 20 + i);
            state.storage[i].origin = Game::Origin::BEFRIENDED;
            state.storage[i].metAt = 100 + i;
            state.storage[i].bond = 25;
        }
        menu.onEnter();
#if STICKMON_UI_COMPACT_TOUCH
        // The real engine enables dynamic assets after its startup cache finishes.
        PokemonSprites::setDynamicLoadingEnabled(true);
#endif
        menu.toast = nullptr;
    };
    auto capture = [&](const std::string& name) {
        PixelRenderer::clear(0);
        menu.render();
        const auto fb = desktop.frameBuffer();
        FILE* file = std::fopen((std::string(argv[2]) + "/" + name + ".rgb565").c_str(), "wb");
        assert(file);
        std::fwrite(fb.pixels, sizeof(uint16_t), fb.width * fb.height, file);
        std::fclose(file);
    };
    auto settle = [&]() {
        for (int i = 0; i < 100; ++i) {
            desktop.advanceMs(20);
            menu.update(desktop.millis(), .02f);
        }
    };
    auto lock = [&](int slot) {
        state.storage[slot].petCountToday = Game::Bond::inviteLockMarker(
            Game::Bond::invitationDay(engine.gameMinutesTotal()));
    };
    // Pixel fixtures come from the frozen pre-extraction source tree. Private
    // fields here set visual states; the journeys below use scene entry points.
    for (int count : {0, 20}) {
        setup(count);
        menu.viewMode = MenuScene::ViewMode::COMPUTER;
        for (int row = 0; row < 2; ++row) {
            COMPUTER(menu).computerCursor = row;
            capture("computer-" + std::to_string(count) + "-" + std::to_string(row));
        }
        menu.toast = Ui::Storage::INVITE_LOCKED;
        capture("computer-notice-" + std::to_string(count));
    }
    setup(0);
    menu.viewMode = MenuScene::ViewMode::STORAGE;
    capture("contacts-empty");
    const int bonds[] = {-100,-50,0,25,50,75};
    for (int i = 0; i < 6; ++i) {
        setup(1);
        state.storage[0].bond = bonds[i];
        menu.viewMode = MenuScene::ViewMode::STORAGE;
        capture("contacts-bond-" + std::to_string(i));
    }
    setup(5);
    state.storage[0] = state.team[0];
    state.team[0].speciesId = 25;
    state.team[0].bond = 100;
    state.storage[1].origin = Game::Origin::HATCHED;
    lock(2);
    engine.contactVisit.active = true;
    engine.contactVisit.storageSlot = 3;
    menu.viewMode = MenuScene::ViewMode::STORAGE;
    for (int i = 0; i < 5; ++i) {
        CONTACTS(menu).storageCursor = i;
#if STICKMON_UI_COMPACT_TOUCH
        settle();
#endif
        capture("contacts-traits-" + std::to_string(i));
    }
    for (int slot = 0; slot < 5; ++slot) {
        CONTACTS(menu).storageCursor = slot;
#if STICKMON_UI_COMPACT_TOUCH
        settle();
#endif
        CONTACTS(menu).storageActionOpen = true;
        // Team has Status/Back; hatched and visiting have no Delete row.
        const int count = slot == 0 ? 2 : slot == 1 || slot == 3 ? 3 : 4;
        for (int action = 0; action < count; ++action) {
            CONTACTS(menu).storageActionCursor = action;
            capture("actions-" + std::to_string(slot) + "-" + std::to_string(action));
        }
    }
    setup(1);
    menu.viewMode = MenuScene::ViewMode::STORAGE;
    CONTACTS(menu).storageActionOpen = true;
    CONTACTS(menu).storageActionCursor = 1;
    CONTACTS(menu).storageInviteConfirmOpen = true;
    for (bool yes : {false,true}) {
        CONTACTS(menu).storageInviteConfirmYes = yes;
        capture(yes ? "invite-yes" : "invite-no");
    }
    CONTACTS(menu).storageInviteConfirmOpen = false;
    CONTACTS(menu).storageActionCursor = 2;
    CONTACTS(menu).storageReleaseConfirmOpen = true;
    for (bool yes : {false,true}) {
        CONTACTS(menu).storageReleaseConfirmYes = yes;
        capture(yes ? "delete-yes" : "delete-no");
    }
    CONTACTS(menu).storageReleaseConfirmOpen = false;
    CONTACTS(menu).storageActionOpen = false;
    const char* notices[] = {Ui::Storage::INVITE_REFUSED, Ui::Storage::INVITE_LOCKED,
        Ui::Storage::IN_TEAM, Ui::Storage::TEAM_FULL_TOAST, Ui::Storage::DELETE_TOAST,
        Ui::Storage::VISITING};
    for (int i = 0; i < 6; ++i) {
        menu.toast = notices[i];
        capture("notice-" + std::to_string(i));
    }
    setup(20);
    menu.viewMode = MenuScene::ViewMode::STORAGE;
    for (int row : {0,9,19,20}) {
        CONTACTS(menu).storageCursor = row;
        settle();
        capture("contacts-full-" + std::to_string(row));
    }

    const ButtonEvent confirm{0,BtnAction::PRESSED};
    const ButtonEvent next{1,BtnAction::PRESSED};
    const ButtonEvent back{1,BtnAction::LONG_PRESS};
    auto press = [&](ButtonEvent event) { assert(menu.onButton(event)); };
    auto enterContacts = [&]() {
        menu.onEnter();
        menu.toast = nullptr;
        // Main menu item 5 is Computer; navigation is identical in both UIs.
        assert(MAIN_MENU(menu).cursor == 0);
        for (int i = 0; i < 5; ++i) press(next);
        press(confirm);
        assert(menu.viewMode == MenuScene::ViewMode::COMPUTER);
        press(next); press(confirm);
        assert(menu.viewMode == MenuScene::ViewMode::STORAGE);
        assert(CONTACTS(menu).storageCursor == 0);
    };
    auto openAction = [&](int index) {
        assert(!CONTACTS(menu).storageActionOpen);
        press(confirm);
        assert(CONTACTS(menu).storageActionOpen);
        for (int i = 0; i < index; ++i) press(next);
        press(confirm);
    };
    auto sameState = [&](const Game::GameState& before) {
        assert(std::memcmp(&state, &before, sizeof(state)) == 0);
    };
    auto notice = [&](const char* text, uint32_t duration) {
        assert(menu.toast && std::strcmp(menu.toast, text) == 0);
        assert(menu.toastUntil - desktop.millis() == duration);
    };
    auto touch = [&](Platform::IInputDevice::TouchType type, int x, int y) {
        desktop.advanceMs(10);
        return menu.onTouch({type,int16_t(x),int16_t(y),desktop.millis()});
    };
    using Type = Platform::IInputDevice::TouchType;

    setup(2);
    state.storage[0] = state.team[0];
    state.team[0].speciesId = 25;
    enterContacts();
    auto before = state;
    openAction(0);
    assert(menu.viewMode == MenuScene::ViewMode::STATUS);
    assert(!menu.statusUi.statusFromStorage && menu.statusUi.statusMonsterIndex == 0);
    press(back);
    assert(menu.viewMode == MenuScene::ViewMode::STORAGE);
    sameState(before);
    press(next);
    openAction(0);
    assert(menu.statusUi.statusFromStorage && menu.statusUi.statusMonsterIndex == 1);
    press(back);
    sameState(before);
    ++checkpoints;

    setup(1); enterContacts(); before = state;
    openAction(1);
    assert(CONTACTS(menu).storageInviteConfirmOpen && CONTACTS(menu).storageInviteConfirmYes);
    press(next); press(confirm);
    assert(!CONTACTS(menu).storageInviteConfirmOpen && CONTACTS(menu).storageActionOpen);
    sameState(before);
    press(confirm); press(back);
    assert(!CONTACTS(menu).storageInviteConfirmOpen && CONTACTS(menu).storageActionOpen);
    press(back);
    assert(!CONTACTS(menu).storageActionOpen);
    press(back);
    assert(menu.viewMode == MenuScene::ViewMode::COMPUTER);
    press(back);
    assert(menu.viewMode == MenuScene::ViewMode::MENU);
    sameState(before);
    ++checkpoints;

    setup(3); enterContacts(); before = state;
    openAction(2);
    assert(CONTACTS(menu).storageReleaseConfirmOpen && !CONTACTS(menu).storageReleaseConfirmYes);
    press(confirm);
    assert(!CONTACTS(menu).storageReleaseConfirmOpen && CONTACTS(menu).storageActionOpen);
    sameState(before);
    press(confirm); press(back);
    assert(!CONTACTS(menu).storageReleaseConfirmOpen && CONTACTS(menu).storageActionOpen);
    press(back); press(next);
    const auto removed = state.storage[1].ivPacked;
    const auto replacement = state.storage[2].ivPacked;
    openAction(2); press(next); press(confirm);
    assert(state.storageCount == 2 && state.storage[1].ivPacked == replacement && replacement != removed);
    assert(CONTACTS(menu).storageCursor == 1 && !CONTACTS(menu).storageActionOpen);
    notice(Ui::Storage::DELETE_TOAST,1100);
    openAction(2); press(next); press(confirm);
    assert(state.storageCount == 1 && CONTACTS(menu).storageCursor == 0);
    openAction(2); press(next); press(confirm);
    assert(state.storageCount == 0 && CONTACTS(menu).storageCursor == 0);
    ++checkpoints;

    for (int gate = 0; gate < 3; ++gate) {
        setup(1, gate == 2 ? 2 : 1);
        if (gate == 0) lock(0);
        if (gate == 1) { engine.contactVisit.active = true; engine.contactVisit.storageSlot = 0; }
        enterContacts(); before = state;
        openAction(1);
        assert(!CONTACTS(menu).storageInviteConfirmOpen && CONTACTS(menu).storageActionOpen);
        notice(gate == 0 ? Ui::Storage::INVITE_LOCKED : gate == 1 ? Ui::Storage::VISITING : Ui::Storage::TEAM_FULL_TOAST,1100);
        sameState(before);
        ++checkpoints;
    }
    // After the confirmation opened, external changes must still be validated
    // by the real engine. Its result keeps the original 1400ms feedback.
    for (int result = 0; result < 5; ++result) {
        setup(1); enterContacts(); openAction(1);
        if (result == 0) lock(0);
        if (result == 1) state.team[state.teamCount++] = state.storage[0];
        if (result == 2) state.team[state.teamCount++] = Game::MonsterFactory::create(25,15);
        if (result == 3) {
            state.storage[0].bond = -100;
            uint32_t seed = 1;
            do { GameRandom::seed(seed++); } while (GameRandom::random(100) < 5);
            GameRandom::seed(seed - 1);
        }
        if (result == 4) {
            state.storageCount = 0;
            menu.toast = Ui::Storage::VISITING;
            menu.toastUntil = desktop.millis() + 37;
            before = state;
        }
        press(confirm);
        assert(!CONTACTS(menu).storageInviteConfirmOpen);
        const char* message = result == 0 ? Ui::Storage::INVITE_LOCKED : result == 1 ? Ui::Storage::IN_TEAM
            : result == 2 ? Ui::Storage::TEAM_FULL_TOAST : result == 3 ? Ui::Storage::INVITE_REFUSED
            : Ui::Storage::VISITING;
        notice(message,1400);
        assert(CONTACTS(menu).storageActionOpen == (result == 0 || result == 2 || result == 4));
        if (result == 3) assert(engine.contactInviteLocked(0));
        if (result == 4) sameState(before);
        ++checkpoints;
    }
    // Touch follows each family list and popup layout.
    setup(1); enterContacts(); before = state;
    const int listY = STICKMON_UI_COMPACT_TOUCH ? TouchUi::SAFE_HEADER_H + 16 : 18;
    const int inviteY = STICKMON_UI_COMPACT_TOUCH ? 122 : 57;
    const int confirmY = STICKMON_UI_COMPACT_TOUCH ? 154 : 85;
    touch(Type::DOWN,40,listY); touch(Type::CANCEL,40,listY); touch(Type::UP,40,listY);
    assert(!CONTACTS(menu).storageActionOpen);
    touch(Type::DOWN,40,listY); touch(Type::MOVE,70,listY+30); touch(Type::UP,70,listY+30);
    assert(!CONTACTS(menu).storageActionOpen);
    touch(Type::DOWN,40,listY); touch(Type::UP,40,listY);
    assert(CONTACTS(menu).storageActionOpen);
    touch(Type::UP,40,listY);
    assert(CONTACTS(menu).storageActionOpen && !CONTACTS(menu).storageInviteConfirmOpen);
    touch(Type::DOWN,170,inviteY); touch(Type::UP,170,inviteY);
    assert(CONTACTS(menu).storageInviteConfirmOpen);
    touch(Type::DOWN,170,confirmY); touch(Type::CANCEL,170,confirmY); touch(Type::UP,170,confirmY);
    assert(CONTACTS(menu).storageInviteConfirmOpen);
    touch(Type::DOWN,170,confirmY); touch(Type::UP,170,confirmY);
    assert(!CONTACTS(menu).storageInviteConfirmOpen && CONTACTS(menu).storageActionOpen);
    touch(Type::UP,170,confirmY);
    assert(!CONTACTS(menu).storageInviteConfirmOpen && CONTACTS(menu).storageActionOpen);
    sameState(before);
    ++checkpoints;

    setup(20); enterContacts();
    for (int i = 0; i < (STICKMON_UI_COMPACT_TOUCH ? 19 : 20); ++i) press(next);
    settle();
    assert(CONTACTS(menu).storageScroll > 0);
    const auto parked = menu.update(desktop.millis(),0);
    assert(!parked.redraw && parked.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE);
    // OSTB uses the same safe header back as Team; Stick retains its footer.
    const int backX = STICKMON_UI_COMPACT_TOUCH ? 48 : Hal::DISPLAY_W / 6;
    const int backY = STICKMON_UI_COMPACT_TOUCH ? 24 : Hal::DISPLAY_H - 12;
    touch(Type::DOWN,backX,backY);
    touch(Type::UP,backX,backY);
    assert(menu.viewMode == MenuScene::ViewMode::COMPUTER);
    ++checkpoints;

#if STICKMON_UI_COMPACT_TOUCH
    setup(1); enterContacts(); before = state;
    assert(menu.directTouchPage() && !menu.touchNavigation());
    assert(CONTACTS(menu).touchTarget(48,24) == 230);
    assert(CONTACTS(menu).touchTarget(48,48) == -1);
    for (int x : {Hal::DISPLAY_W / 6, Hal::DISPLAY_W / 2, Hal::DISPLAY_W * 5 / 6}) {
        touch(Type::DOWN,x,Hal::DISPLAY_H-12);touch(Type::UP,x,Hal::DISPLAY_H-12);
        assert(menu.viewMode == MenuScene::ViewMode::STORAGE && !CONTACTS(menu).storageActionOpen);
    }
    touch(Type::DOWN,40,listY);touch(Type::UP,40,listY);
    assert(CONTACTS(menu).storageActionOpen);
    touch(Type::DOWN,170,inviteY);touch(Type::UP,170,inviteY);
    assert(CONTACTS(menu).storageInviteConfirmOpen);
    touch(Type::DOWN,48,24);touch(Type::CANCEL,48,24);touch(Type::UP,48,24);
    assert(CONTACTS(menu).storageInviteConfirmOpen);
    touch(Type::DOWN,48,24);touch(Type::UP,48,24);
    assert(!CONTACTS(menu).storageInviteConfirmOpen && CONTACTS(menu).storageActionOpen);
    touch(Type::UP,48,24);
    assert(CONTACTS(menu).storageActionOpen);
    touch(Type::DOWN,48,24);touch(Type::UP,48,24);
    assert(!CONTACTS(menu).storageActionOpen && menu.viewMode == MenuScene::ViewMode::STORAGE);
    touch(Type::DOWN,48,24);menu.cancelTouch();touch(Type::UP,48,24);
    assert(menu.viewMode == MenuScene::ViewMode::STORAGE);
    touch(Type::DOWN,48,24);touch(Type::UP,48,24);
    assert(menu.viewMode == MenuScene::ViewMode::COMPUTER);
    sameState(before);

    setup(20); enterContacts(); before = state;
    touch(Type::DOWN,100,200);touch(Type::MOVE,100,100);
    assert(CONTACTS(menu).storageScroll == 100 && !CONTACTS(menu).storageActionOpen);
    settle();
    assert(CONTACTS(menu).storageScroll == 100);
    touch(Type::UP,100,100);settle();
    assert(CONTACTS(menu).storageScroll == 100 && !CONTACTS(menu).storageActionOpen);
    touch(Type::DOWN,100,200);touch(Type::MOVE,100,-1200);touch(Type::UP,100,100);
    settle();
    assert(CONTACTS(menu).storageScroll == 1260 && !CONTACTS(menu).storageActionOpen);
    assert(CONTACTS(menu).touchTarget(100,198) == 19);
    touch(Type::DOWN,100,198);touch(Type::UP,100,198);
    assert(CONTACTS(menu).storageCursor == 19 && CONTACTS(menu).storageActionOpen);
    touch(Type::DOWN,48,24);touch(Type::UP,48,24);
    press(next);settle();
    assert(CONTACTS(menu).storageCursor == 0 && CONTACTS(menu).storageScroll == 0);
    press(next);settle();
    assert(CONTACTS(menu).storageCursor == 1);
    CONTACTS(menu).storageCursor = 19;
    press(next);settle();
    assert(CONTACTS(menu).storageCursor == 0 && CONTACTS(menu).storageScroll == 0);
    sameState(before);

    // No synthetic Back rows remain, including physical-key wrap and empty lists.
    setup(0); enterContacts(); before = state;
    assert(CONTACTS(menu).listItemCount() == 0);
    assert(!CONTACTS(menu).onTap(0) && !menu.onButton(confirm));
    press(next);
    assert(menu.viewMode == MenuScene::ViewMode::STORAGE);
    assert(CONTACTS(menu).touchTarget(100,80) == -1);
    sameState(before);

    setup(3); enterContacts(); before = state;
    assert(CONTACTS(menu).listItemCount() == 3);
    touch(Type::DOWN,40,listY);touch(Type::UP,40,listY);
    assert(CONTACTS(menu).storageActionOpen && CONTACTS(menu).actionCount() == 3);
    for (int i=0; i<CONTACTS(menu).actionCount(); ++i)
        assert(CONTACTS(menu).actionAt(i) != Features::Contacts::Action::BACK);
    assert(!CONTACTS(menu).onTap(131));
    touch(Type::DOWN,50,150);touch(Type::UP,50,150);
    assert(!CONTACTS(menu).storageActionOpen && CONTACTS(menu).storageCursor == 0);
    touch(Type::UP,50,150);
    assert(!CONTACTS(menu).storageActionOpen);
    sameState(before);

    // Selecting Delete still uses the engine guard and its default-No confirmation.
    touch(Type::DOWN,40,listY);touch(Type::UP,40,listY);
    touch(Type::DOWN,170,162);touch(Type::UP,170,162);
    assert(CONTACTS(menu).storageReleaseConfirmOpen && !CONTACTS(menu).storageReleaseConfirmYes);
    const auto deleted = state.storage[0].ivPacked;
    touch(Type::DOWN,90,154);touch(Type::UP,90,154);
    assert(state.storageCount == 2 && state.storage[0].ivPacked != deleted);
    assert(!CONTACTS(menu).storageActionOpen && !CONTACTS(menu).storageReleaseConfirmOpen);

    setup(1);state.storage[0]=state.team[0];enterContacts();
    assert(CONTACTS(menu).actionCount() == 1);
    assert(CONTACTS(menu).actionAt(0) == Features::Contacts::Action::STATUS);
    setup(1);state.storage[0].origin=Game::Origin::HATCHED;enterContacts();
    assert(CONTACTS(menu).actionCount() == 2);
    for(int i=0;i<2;++i)assert(CONTACTS(menu).actionAt(i) != Features::Contacts::Action::DELETE);
#endif

    // Owned MenuScene is synchronously destroyed by a successful invitation.
    // Never read that pointer after its final onButton call returns.
    setup(1);
    state.storage[0].bond = 100;
    const auto joinedSpecies = state.storage[0].speciesId;
    engine.requestScene(SceneID::MAIN, false);
    engine.requestScene(SceneID::MENU, false);
    assert(engine.currentId == SceneID::MENU);
    Scene* owned = engine.currentScene.get();
    for (int i = 0; i < 5; ++i) assert(owned->onButton(next));
    assert(owned->onButton(confirm));
    assert(owned->onButton(next));
    assert(owned->onButton(confirm));
    assert(owned->onButton(confirm));
    assert(owned->onButton(next));
    assert(owned->onButton(confirm));
#if STICKMON_UI_COMPACT_TOUCH
    owned->onTouch({Type::DOWN,90,154,desktop.millis()});
    desktop.advanceMs(10);
    assert(owned->onTouch({Type::UP,90,154,desktop.millis()}));
#else
    assert(owned->onButton(confirm));
#endif
    assert(engine.currentId == SceneID::MAIN);
    assert(state.teamCount == 2 && state.team[1].speciesId == joinedSpecies);
    assert(!engine.consumeTeamMemberArrivalRequest());
    const auto* arrivalScene = static_cast<const MainScene*>(engine.currentScene.get());
    assert(arrivalScene->contactGuestMotion == MainScene::ContactGuestMotion::TEAM_ENTER_CROSS);
    ++checkpoints;
    std::printf("contacts-checkpoints=%u joined=%u team=%u\n", checkpoints, joinedSpecies, state.teamCount);
    // Release the active scene while its platform services are still alive.
    engine.currentScene.reset();
}
