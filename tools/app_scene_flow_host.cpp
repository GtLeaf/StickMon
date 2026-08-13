#include <cassert>
#include <cstring>

#include "core/AppSceneFlow.h"

int main() {
    using namespace AppSceneFlow;

    assert(mainMenuItemCount(false) == 8);
    assert(mainMenuItemCount(true) == 9);

    const MainMenuItem releaseOrder[] = {
        MainMenuItem::EXPLORE,
        MainMenuItem::TEAM,
        MainMenuItem::ROOM,
        MainMenuItem::BAG,
        MainMenuItem::SHOP,
        MainMenuItem::COMPUTER,
        MainMenuItem::SETTINGS,
        MainMenuItem::BACK,
    };
    const uint8_t releaseIcons[] = {0, 1, 2, 3, 4, 5, 6, 8};
    for (uint8_t index = 0; index < mainMenuItemCount(false); ++index) {
        MainMenuEntry entry = mainMenuEntry(index, false);
        assert(entry.item == releaseOrder[index]);
        assert(entry.iconIndex == releaseIcons[index]);
        assert(entry.shortLabel && std::strlen(entry.shortLabel) > 0);
    }
    assert(mainMenuEntry(7, true).item == MainMenuItem::DEBUG);
    assert(mainMenuEntry(8, true).item == MainMenuItem::BACK);

    const ExploreMenuItem exploreOrder[] = {
        ExploreMenuItem::TEAM,
        ExploreMenuItem::BAG,
        ExploreMenuItem::END,
        ExploreMenuItem::BACK,
    };
    const Scene exploreTargets[] = {
        Scene::TEAM,
        Scene::BAG,
        Scene::EXPLORE_AREAS,
        Scene::EXPLORE_ROUTE,
    };
    const uint8_t exploreIcons[] = {1, 3, 0, 8};
    assert(exploreMenuItemCount() == 4);
    for (uint8_t index = 0; index < exploreMenuItemCount(); ++index) {
        ExploreMenuEntry entry = exploreMenuEntry(index);
        assert(entry.item == exploreOrder[index]);
        assert(entry.target == exploreTargets[index]);
        assert(entry.iconIndex == exploreIcons[index]);
        assert(entry.shortLabel && std::strlen(entry.shortLabel) > 0);
    }

    Controller flow;
    flow.openMenu();
    assert(flow.current() == Scene::MAIN_MENU);
    assert(flow.menuReturn() == Scene::HOME);
    assert(flow.closeMenu() == Scene::HOME);

    flow.enter(Scene::EXPLORE_AREAS);
    flow.openMenu(Scene::HOME);
    assert(flow.menuReturn() == Scene::HOME);
    assert(flow.closeMenu() == Scene::HOME);

    flow.enter(Scene::EXPLORE_AREAS);
    flow.enterExploreRoute();
    flow.openExploreMenu();
    assert(flow.current() == Scene::EXPLORE_MENU);
    assert(flow.closeExploreMenu() == Scene::EXPLORE_ROUTE);
    flow.openExploreMenu();
    flow.openSubScene(Scene::TEAM);
    assert(flow.current() == Scene::TEAM);
    assert(flow.subSceneReturn() == Scene::EXPLORE_MENU);
    assert(flow.closeSubScene() == Scene::EXPLORE_MENU);
    flow.openSubScene(Scene::BAG);
    assert(flow.current() == Scene::BAG);
    assert(flow.subSceneReturn() == Scene::EXPLORE_MENU);
    assert(flow.closeSubScene() == Scene::EXPLORE_MENU);
    assert(flow.closeExploreMenu() == Scene::EXPLORE_ROUTE);
    flow.openMenu();
    assert(flow.menuReturn() == Scene::EXPLORE_ROUTE);
    assert(flow.closeMenu() == Scene::EXPLORE_ROUTE);
    flow.leaveExploreRoute();
    assert(flow.current() == Scene::EXPLORE_AREAS);
    flow.goHome();
    assert(flow.current() == Scene::HOME);
    assert(flow.menuReturn() == Scene::HOME);
    flow.openMenu();
    flow.openSubScene(Scene::SHOP);
    assert(flow.subSceneReturn() == Scene::MAIN_MENU);
    assert(flow.closeSubScene() == Scene::MAIN_MENU);

    flow.openSubScene(Scene::ROOM);
    assert(flow.current() == Scene::ROOM);
    assert(flow.subSceneReturn() == Scene::MAIN_MENU);
    flow.enter(Scene::SHOWER);
    assert(flow.current() == Scene::SHOWER);
    flow.enter(Scene::ROOM);
    assert(flow.current() == Scene::ROOM);
    assert(flow.closeSubScene() == Scene::MAIN_MENU);
    return 0;
}
