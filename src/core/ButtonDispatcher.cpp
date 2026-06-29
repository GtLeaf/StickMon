#include "core/ButtonDispatcher.h"
#include "hardware/Hal.h"

ButtonDispatcher& ButtonDispatcher::ins() {
    static ButtonDispatcher instance;
    return instance;
}

uint8_t ButtonDispatcher::poll(ButtonEvent* events, uint8_t maxEvents) {
    uint8_t eventCount = 0;
    uint32_t now = Hal::ins().millis();

    states[0].raw = Hal::ins().btnA_raw();
    states[1].raw = Hal::ins().btnB_raw();
    processBtn(states[0], now, 0, events, maxEvents, eventCount);
    processBtn(states[1], now, 1, events, maxEvents, eventCount);
    return eventCount;
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
