#pragma once

#include <cstddef>

#include "core/Scene.h"
#include "core/features/BagFeature.h"
#if defined(STICKMON_UI_COMPACT_TOUCH) && STICKMON_UI_COMPACT_TOUCH
#include "core/TouchScroll.h"
#include "ui/compact_touch/ItemTouchUi.h"
#endif

// One selected implementation owns presentation and interaction for this feature.
class BagUi {
public:
    using Context = Features::Bag::Context;
    using Row = Features::Bag::Row;
    struct Action {
        enum class Kind : uint8_t { NONE, BACK, HOME, USE, RECALL, SELECT_TARGET, USE_DIRECT, UNUSABLE };
        bool changed = false;
        Kind kind = Kind::NONE;
        uint8_t source = Features::Bag::SOURCE_BACK;
        bool backTutorial = false;
        bool navigationTutorial = false;
        Game::ItemId item = Game::ItemId::COUNT;
    };

    uint8_t cursor = 0;
    uint8_t confirmSource = 0;
    bool confirmOpen = false;
    bool confirmYes = true;
#if defined(STICKMON_UI_COMPACT_TOUCH) && STICKMON_UI_COMPACT_TOUCH
    float gridScroll = 0.0f;
    bool detailOpen = false;
    TouchScroll touch;
    ItemTouchUi::Grid grid(const Game::GameState& state, bool battle) const;
#else
    float scroll = 0.0f;
    int descriptionKey = -1;
    float descriptionScroll = 0.0f;
    uint32_t descriptionLastMs = 0;
#endif

    static constexpr bool directTouch() {
#if defined(STICKMON_UI_COMPACT_TOUCH) && STICKMON_UI_COMPACT_TOUCH
        return true;
#else
        return false;
#endif
    }
    void reset();
    void enter();
    void leave();
    void cancelTouch();
    SceneUpdateResult update(const Game::GameState& state, const Context& context, uint32_t nowMs);
    void render(const Game::GameState& state, const Context& context);
    void renderToast(const char* value);
    Action button(const ButtonEvent& event, const Game::GameState& state, const Context& context);
    Action tap(int target, const Game::GameState& state, const Context& context);
    Action onTouch(const Platform::IInputDevice::TouchEvent& event,
                   const Game::GameState& state, const Context& context);
    int target(int16_t x, int16_t y, const Game::GameState& state, const Context& context) const;
    void afterUse(const Game::GameState& state, const Context& context);
    static const char* notice(const Features::Bag::Outcome& outcome, char* buffer, size_t capacity);

private:
    Action activate(const Game::GameState& state, const Context& context);
    void renderConfirm(const Game::GameState& state, const Context& context);
    static int textWidth(const char* value);
    static int wrappedText(int x, int y, const char* value, int width, uint16_t color);
};
