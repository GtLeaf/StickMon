#pragma once

#include <cstdint>
#include "platform/api/PlatformServices.h"

enum class BtnAction : uint8_t {
    DOWN,
    UP,
    PRESSED,
    LONG_PRESS,
};

struct ButtonEvent {
    uint8_t btn;      // 0=A, 1=B
    BtnAction action;
};

class ButtonDispatcher {
public:
    static constexpr uint8_t MAX_EVENTS_PER_POLL = 6;

    static ButtonDispatcher& ins();
    void setLongPressMs(uint16_t value) { longPressMs = value; }
    uint8_t poll(ButtonEvent* events, uint8_t maxEvents);
    bool isDown(Platform::InputButton button) const;

private:
    struct BtnState {
        bool raw = false;
        bool debounced = false;
        uint32_t debounceTime = 0;
        uint32_t pressTime = 0;
        bool longTriggered = false;
    };

    BtnState states[2];

    static constexpr uint32_t DEBOUNCE_MS = 20;
    uint16_t longPressMs = 500;

    void processBtn(BtnState& state, uint32_t now, uint8_t btnId,
                    ButtonEvent* events, uint8_t maxEvents, uint8_t& eventCount);
    static void emit(ButtonEvent* events, uint8_t maxEvents, uint8_t& eventCount,
                     uint8_t btnId, BtnAction action);
};
