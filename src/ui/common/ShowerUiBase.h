#pragma once

#include "core/UiMotion.h"
#include "ui/common/ShowerUiHost.h"

namespace Ui {

class ShowerUiBase {
public:
    explicit ShowerUiBase(ShowerHost& host) : host(host) {}
    virtual ~ShowerUiBase() = default;
    ShowerUiState state;
    virtual ShowerLayout layout() const = 0;
    virtual void render() = 0;
    void enter();
    void resetSession();
    bool onButton(const ButtonEvent& event);
    virtual UiMotion::StepResult updateMenu();
    virtual int touchTarget(int16_t, int16_t) { return -1; }
    virtual bool onTap(int) { return false; }
    virtual bool onTouch(const Platform::IInputDevice::TouchEvent&) { return false; }
    virtual void cancelTouch() {}
    virtual bool usesMotionInput() const { return true; }
    virtual ShowerMotion readMotion(uint32_t nowMs, float dtSeconds);
    virtual void moveTool(bool soap, float dtSeconds, const ShowerMotion& motion);
    void beginRinse(uint32_t nowMs);
    void updateRinseWater(uint32_t nowMs, float dtSeconds, uint8_t spawnCursor);
    void clearWater();
    void updateAtmosphere(bool target, float dtSeconds);
    bool updateEffects(uint32_t nowMs, bool atmosphereTarget, RenderDemand& demand);
    void spawnExpFloat(uint32_t amount, uint32_t nowMs);
    void spawnHeartFloat(uint32_t nowMs);
    void startHop(float heightPx, uint32_t nowMs);
    void startWiggle(uint32_t nowMs);
    void startTurn(uint32_t nowMs);

protected:
    ShowerHost& host;

private:
    void spawnWaterDrop(uint8_t spawnCursor);
    void spawnSplashDrop(float x, float y, float vx, float vy);
};

}  // namespace Ui
