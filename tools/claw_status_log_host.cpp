// Host harness for ClawStatusLog. Protocol (one command per argv[1]):
//   fill <n>      append n entries "entry-<i>" and print size/generation
//   recent <n>    print the newest n entries, oldest first
//   since <g>     print entries newer than generation g
//   long          append one 130-byte ASCII message, print all entries
//   utf8          append a Chinese message that crosses the entry boundary,
//                 print all entries (continuation entries must not split a
//                 multi-byte character)
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "brain/ClawStatusLog.h"

namespace {

void printEntry(const Stickmon::ClawStatusLog::Entry& entry) {
    std::printf("%u|%u|%s\n", static_cast<unsigned>(entry.ms),
                static_cast<unsigned>(entry.level), entry.text);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    Stickmon::ClawStatusLog log;

    if (std::strcmp(argv[1], "fill") == 0 && argc == 3) {
        const int count = std::atoi(argv[2]);
        for (int i = 0; i < count; ++i) {
            char text[32];
            std::snprintf(text, sizeof(text), "entry-%d", i);
            log.append(Stickmon::ClawStatusLog::Level::INFO,
                       static_cast<uint32_t>(1000 + i), text);
        }
        std::printf("size=%u gen=%u oldest=%u\n",
                    static_cast<unsigned>(log.size()), log.generation(),
                    log.oldestGeneration());
        return 0;
    }
    if (std::strcmp(argv[1], "recent") == 0 && argc == 3) {
        for (int i = 0; i < 70; ++i) {
            char text[32];
            std::snprintf(text, sizeof(text), "entry-%d", i);
            log.append(Stickmon::ClawStatusLog::Level::INFO,
                       static_cast<uint32_t>(1000 + i), text);
        }
        Stickmon::ClawStatusLog::Entry out[Stickmon::ClawStatusLog::CAPACITY];
        const size_t count =
            log.copyRecent(out, static_cast<size_t>(std::atoi(argv[2])));
        std::printf("count=%u\n", static_cast<unsigned>(count));
        for (size_t i = 0; i < count; ++i) printEntry(out[i]);
        return 0;
    }
    if (std::strcmp(argv[1], "since") == 0 && argc == 3) {
        for (int i = 0; i < 70; ++i) {
            char text[32];
            std::snprintf(text, sizeof(text), "entry-%d", i);
            log.append(Stickmon::ClawStatusLog::Level::INFO,
                       static_cast<uint32_t>(1000 + i), text);
        }
        Stickmon::ClawStatusLog::Entry out[Stickmon::ClawStatusLog::CAPACITY];
        const size_t count = log.copySince(
            static_cast<uint32_t>(std::atoi(argv[2])), out,
            Stickmon::ClawStatusLog::CAPACITY);
        std::printf("count=%u\n", static_cast<unsigned>(count));
        for (size_t i = 0; i < count; ++i) printEntry(out[i]);
        return 0;
    }
    if (std::strcmp(argv[1], "long") == 0) {
        char text[131];
        for (int i = 0; i < 130; ++i) text[i] = static_cast<char>('a' + i % 26);
        text[130] = '\0';
        log.append(Stickmon::ClawStatusLog::Level::WARN, 7, text);
        Stickmon::ClawStatusLog::Entry out[8];
        const size_t count = log.copyRecent(out, 8);
        std::printf("count=%u\n", static_cast<unsigned>(count));
        for (size_t i = 0; i < count; ++i) printEntry(out[i]);
        return 0;
    }
    if (std::strcmp(argv[1], "utf8") == 0) {
        // 22 Chinese characters = 66 bytes: crosses the 63-byte entry limit
        // in the middle of a character.
        const char* text =
            "热点已启动微信二维码已生成等待手机扫码确认登录成功";
        log.append(Stickmon::ClawStatusLog::Level::OK, 9, text);
        Stickmon::ClawStatusLog::Entry out[8];
        const size_t count = log.copyRecent(out, 8);
        std::printf("count=%u\n", static_cast<unsigned>(count));
        for (size_t i = 0; i < count; ++i) printEntry(out[i]);
        return 0;
    }
    return 2;
}
