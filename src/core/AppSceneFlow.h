#pragma once

#include <cstdint>

namespace AppSceneFlow {

enum class Scene : uint8_t {
    HOME = 0,
    MAIN_MENU,
    EXPLORE_AREAS,
    EXPLORE_ROUTE,
    EXPLORE_MENU,
    TEAM,
    ROOM,
    SHOWER,
    BAG,
    SHOP,
    COMPUTER,
    SETTINGS,
    DEBUG,
};

enum class ExploreMenuItem : uint8_t {
    TEAM = 0,
    BAG,
    END,
    BACK,
};

struct ExploreMenuEntry {
    ExploreMenuItem item;
    Scene target;
    uint8_t iconIndex;
    const char* shortLabel;
};

inline constexpr uint8_t exploreMenuItemCount() { return 4; }

inline ExploreMenuEntry exploreMenuEntry(uint8_t index) {
    switch (index) {
    case 0:
        return {ExploreMenuItem::TEAM, Scene::TEAM, 1, "TEAM"};
    case 1:
        return {ExploreMenuItem::BAG, Scene::BAG, 3, "BAG"};
    case 2:
        return {ExploreMenuItem::END, Scene::EXPLORE_AREAS, 0, "END"};
    default:
        return {ExploreMenuItem::BACK, Scene::EXPLORE_ROUTE, 8, "BACK"};
    }
}

enum class MainMenuItem : uint8_t {
    EXPLORE = 0,
    TEAM,
    ROOM,
    BAG,
    SHOP,
    COMPUTER,
    SETTINGS,
    DEBUG,
    BACK,
};

struct MainMenuEntry {
    MainMenuItem item;
    Scene target;
    uint8_t iconIndex;
    const char* shortLabel;
};

inline constexpr uint8_t mainMenuItemCount(bool debugEnabled) {
    return debugEnabled ? 9 : 8;
}

inline MainMenuEntry mainMenuEntry(uint8_t index, bool debugEnabled) {
    if (debugEnabled && index == 7) {
        return {MainMenuItem::DEBUG, Scene::DEBUG, 7, "DEBUG"};
    }
    if ((!debugEnabled && index == 7) || (debugEnabled && index == 8)) {
        return {MainMenuItem::BACK, Scene::HOME, 8, "BACK"};
    }
    switch (index) {
    case 0:
        return {MainMenuItem::EXPLORE, Scene::EXPLORE_AREAS, 0, "EXPLORE"};
    case 1:
        return {MainMenuItem::TEAM, Scene::TEAM, 1, "TEAM"};
    case 2:
        return {MainMenuItem::ROOM, Scene::ROOM, 2, "ROOM"};
    case 3:
        return {MainMenuItem::BAG, Scene::BAG, 3, "BAG"};
    case 4:
        return {MainMenuItem::SHOP, Scene::SHOP, 4, "SHOP"};
    case 5:
        return {MainMenuItem::COMPUTER, Scene::COMPUTER, 5, "COMPUTER"};
    case 6:
        return {MainMenuItem::SETTINGS, Scene::SETTINGS, 6, "SETTINGS"};
    default:
        return {MainMenuItem::BACK, Scene::HOME, 8, "BACK"};
    }
}

class Controller {
public:
    explicit Controller(Scene initial = Scene::HOME)
        : current_(initial), menuReturn_(initial), subSceneReturn_(initial) {}

    Scene current() const { return current_; }
    Scene menuReturn() const { return menuReturn_; }
    Scene subSceneReturn() const { return subSceneReturn_; }

    void enter(Scene next) { current_ = next; }

    void goHome() {
        current_ = Scene::HOME;
        menuReturn_ = Scene::HOME;
    }

    void openMenu() {
        if (current_ != Scene::MAIN_MENU) menuReturn_ = current_;
        current_ = Scene::MAIN_MENU;
    }

    void openMenu(Scene returnTo) {
        menuReturn_ = returnTo;
        current_ = Scene::MAIN_MENU;
    }

    Scene closeMenu() {
        if (current_ == Scene::MAIN_MENU) current_ = menuReturn_;
        return current_;
    }

    void enterExploreRoute() { current_ = Scene::EXPLORE_ROUTE; }

    void openExploreMenu() {
        if (current_ == Scene::EXPLORE_ROUTE) current_ = Scene::EXPLORE_MENU;
    }

    Scene closeExploreMenu() {
        if (current_ == Scene::EXPLORE_MENU) current_ = Scene::EXPLORE_ROUTE;
        return current_;
    }

    void openSubScene(Scene target) {
        subSceneReturn_ = current_;
        current_ = target;
    }

    Scene closeSubScene() {
        current_ = subSceneReturn_;
        return current_;
    }

    void leaveExploreRoute() { current_ = Scene::EXPLORE_AREAS; }

private:
    Scene current_;
    Scene menuReturn_;
    Scene subSceneReturn_;
};

}  // namespace AppSceneFlow
