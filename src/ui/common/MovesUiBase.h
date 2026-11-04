#pragma once

#include "core/Scene.h"
#include "core/features/MovesFeature.h"
#include "ui/common/MenuPageHost.h"

namespace Ui {

class MovesHost : public virtual MenuPageHost {
public:
    virtual bool forgetPageMove(uint8_t teamSlot, uint8_t moveSlot) = 0;
    virtual bool recallPageMove(uint8_t teamSlot, Game::MoveId move, uint8_t replacementSlot) = 0;
    virtual uint8_t collectPageRecallable(uint8_t teamSlot, Game::MoveId* moves, uint8_t capacity) = 0;
};

class MovesUiBase : public Scene, public Features::Moves::Flow {
public:
    explicit MovesUiBase(MovesHost& host) : host(host) {}
protected:
    MovesHost& host;
    void execute(const Features::Moves::Action& action);
    void forgetMove(uint8_t slot);
};

namespace MovesDrawing {
int textPixelWidth(const char* value);
int drawTypeBracket(int x, int y, TypeId type);
void drawProficiencyBar(int x, int y, int width, int height, uint8_t value);
}

}  // namespace Ui
