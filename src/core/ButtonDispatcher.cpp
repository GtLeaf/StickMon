#include "core/ButtonDispatcher.h"
#include "platform/api/PlatformServices.h"

ButtonDispatcher& ButtonDispatcher::ins() {
    static ButtonDispatcher instance;
    return instance;
}

uint8_t ButtonDispatcher::poll(ButtonEvent* events, uint8_t maxEvents) {
    uint8_t eventCount = 0;
    uint32_t now = Platform::clock().millis();

    states[0].raw =
        Platform::input().pressed(Platform::InputButton::PRIMARY);
    states[1].raw =
        Platform::input().pressed(Platform::InputButton::SECONDARY);
    processBtn(states[0], now, 0, events, maxEvents, eventCount);
    processBtn(states[1], now, 1, events, maxEvents, eventCount);
    return eventCount;
}

bool ButtonDispatcher::isDown(Platform::InputButton button) const {
    uint8_t index = static_cast<uint8_t>(button);
    return index < 2 && states[index].debounced;
}

void ButtonDispatcher::processBtn(BtnState& state, uint32_t now, uint8_t btnId,
                                  ButtonEvent* events, uint8_t maxEvents, uint8_t& eventCount) {
    if (state.raw != state.debounced && now - state.debounceTime >= DEBOUNCE_MS) {
        state.debounced = state.raw;
        state.debounceTime = now;
        emit(events, maxEvents, eventCount, btnId, state.debounced ? BtnAction::DOWN : BtnAction::UP);

        if (state.debounced) {
            state.pressTime = now;
            state.longTriggered = false;
        } else {
            if (!state.longTriggered) {
                emit(events, maxEvents, eventCount, btnId, BtnAction::PRESSED);
            }
        }
    }

    if (state.debounced && !state.longTriggered && now - state.pressTime >= longPressMs) {
        state.longTriggered = true;
        emit(events, maxEvents, eventCount, btnId, BtnAction::LONG_PRESS);
    }
}

void ButtonDispatcher::emit(ButtonEvent* events, uint8_t maxEvents, uint8_t& eventCount,
                            uint8_t btnId, BtnAction action) {
    if (eventCount >= maxEvents) return;
    events[eventCount++] = ButtonEvent{btnId, action};
}
