#include "platform/api/PlatformServices.h"
#include "platform/desktop/DesktopPlatform.h"

#include <cassert>
#include <string>

class CaptureLogger final : public Platform::ILogger {
public:
    void write(const char* data, size_t length) override { text.append(data, length); }
    void flush() override {}
    std::string text;
};

int main(int argc, char** argv) {
    assert(argc == 2);
    DesktopPlatform platform(argv[1]);
    auto& base = platform.serviceBundle();
    CaptureLogger logger;
    Platform::Services services{base.lifecycle, base.clock, logger, base.input,
        base.display, base.audio, base.imu, base.power, base.blobs, base.resources,
        base.memory, base.peers};
    Platform::bind(services);
    Platform::logf("routine %d", 7);
    Platform::logLine(" routine-line");
#if EXPECT_ROUTINE_LOGS
    assert(logger.text == "routine 7 routine-line\n");
#else
    assert(logger.text.empty());
#endif
    logger.text.clear();
    Platform::errorf("failure %d", 3);
    Platform::errorLine(" error-line");
    assert(logger.text == "failure 3 error-line\n");
    logger.text.clear();
    Platform::errorf(nullptr);
    Platform::errorLine(nullptr);
    assert(logger.text.empty());
    Platform::errorf("%s", std::string(1000, 'x').c_str());
    assert(logger.text == std::string(383, 'x'));
    return 0;
}
