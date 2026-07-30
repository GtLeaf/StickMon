#include "core/Scene.h"
#include "core/UiMotion.h"

#include <cassert>
#include <climits>

int main() {
    {
        RenderDemand demand;
        SceneUpdateResult result = demand.result();
        assert(!result.redraw);
        assert(result.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE);
    }
    {
        RenderDemand demand;
        demand.wakeIn(500);
        demand.wakeIn(120);
        demand.wakeIn(300);
        SceneUpdateResult result = demand.result();
        assert(!result.redraw);
        assert(result.nextUpdateDelayMs == 120);
    }
    {
        RenderDemand demand;
        demand.animate(true, 66);
        SceneUpdateResult result = demand.result();
        assert(result.redraw);
        assert(result.nextUpdateDelayMs == 66);
    }
    {
        RenderDemand demand;
        assert(!demand.expired(true, 1000, 1250));
        SceneUpdateResult result = demand.result();
        assert(!result.redraw);
        assert(result.nextUpdateDelayMs == 250);
    }
    {
        RenderDemand demand;
        assert(demand.expired(true, 1250, 1250));
        SceneUpdateResult result = demand.result();
        assert(result.redraw);
        assert(result.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE);
    }
    {
        RenderDemand demand;
        uint32_t now = UINT32_MAX - 10U;
        uint32_t deadline = 5U;
        assert(!demand.expired(true, now, deadline));
        assert(demand.result().nextUpdateDelayMs == 16U);
    }
    {
        float value = 0.0f;
        UiMotion::StepResult step = UiMotion::lerp(value, 10.0f, 0.5f);
        assert(step.changed && step.active);
        assert(value == 5.0f);
        step = UiMotion::approach(value, 10.0f, 10.0f);
        assert(step.changed && !step.active);
        assert(value == 10.0f);
    }
    return 0;
}
