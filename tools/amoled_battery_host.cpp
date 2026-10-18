#include "AmoledApp.h"
#include "HomeScreen.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include "ui/UiCommon.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace AmoledV1 {
namespace {
class BatteryPower final : public Platform::IPowerDevice {
public:
    int percent = 65;
    size_t samples = 0;
    int batteryLevel() override { ++samples; return percent; }
    Platform::WakeReason wakeReason() const override { return Platform::WakeReason::NORMAL; }
    uint32_t hardwareRandom() override { return 123; }
    size_t externalMemorySize() const override { return 0; }
    size_t externalMemoryFree() const override { return 0; }
    void restart() override {}
    void enterDeepSleep(uint64_t, bool) override {}
};
}

struct AmoledBatteryTestAccess {
    static void check(DesktopPlatform& desktop, Canvas565& canvas) {
        auto& original = desktop.serviceBundle();
        BatteryPower power;
        Platform::Services services{
            original.lifecycle, original.clock, original.logger, original.input,
            original.display, original.audio, original.imu, power,
            original.blobs, original.resources, original.memory, original.peers};
        Platform::bind(services);
        {
            AmoledApp app;
            const uint32_t start = desktop.millis();
            app.begin(start);
            assert(app.sceneFlow.current() == AppSceneFlow::Scene::HOME);
            assert(power.samples == 1 && app.batteryPercent == 65);
            app.render(canvas);
            assert(canvas.readPixel(324, 20) == UiCommon::rgb(92, 213, 139));
            assert(canvas.readPixel(344, 20) != UiCommon::rgb(92, 213, 139));
            app.markRendered();
            power.percent = 10;
            app.updateBattery(start + 4999);
            assert(power.samples == 1 && !app.needsRender());
            app.updateBattery(start + 5000);
            assert(power.samples == 2 && app.needsRender());
            assert(app.renderRowBegin() == 0 && app.renderRowEnd() == 48);
            const std::vector<uint16_t> before(canvas.rawPixels(),
                canvas.rawPixels() + canvas.pixelCount());
            app.render(canvas);
            assert(std::equal(before.begin() + 48 * 368, before.end(),
                              canvas.rawPixels() + 48 * 368));
            assert(canvas.readPixel(324, 20) == UiCommon::rgb(238, 91, 91));
            assert(canvas.readPixel(338, 20) != UiCommon::rgb(92, 213, 139));
            app.markRendered();
            app.updateBattery(start + 10000);
            assert(power.samples == 3 && !app.needsRender());
            for (int invalid : {-1, 101, 255}) {
                power.percent = invalid;
                app.onWake(start + 11000);
                app.render(canvas);
                assert(app.batteryPercent == -1);
                assert(canvas.readPixel(333, 23) == UiCommon::rgb(142, 157, 155));
                assert(canvas.readPixel(324, 20) != UiCommon::rgb(92, 213, 139));
                app.markRendered();
            }
            power.percent = 100;
            app.onWake(start + 12000);
            app.render(canvas);
            assert(canvas.readPixel(347, 20) == UiCommon::rgb(92, 213, 139));
            // Battery changes on other pages must not invalidate their header.
            app.sceneFlow.openMenu();
            app.markRendered();
            power.percent = 0;
            app.updateBattery(start + 17000);
            assert(app.batteryPercent == 0 && !app.needsRender());
            app.sceneFlow.goHome();
            app.requestFullRender();
            app.render(canvas);
            assert(canvas.readPixel(324, 20) != UiCommon::rgb(238, 91, 91));
            // Unsigned elapsed time keeps polling correct across millis wrap.
            app.lastBatterySampleMs = 0xFFFFFF00U;
            power.percent = 50;
            app.updateBattery(0xFFFFFF00U + 5000U);
            assert(app.batteryPercent == 50);
        }
        Platform::bind(original);
    }
};
}

void checkBatteryDisplay(DesktopPlatform& desktop, Canvas565& canvas) {
    AmoledV1::AmoledBatteryTestAccess::check(desktop, canvas);
}
