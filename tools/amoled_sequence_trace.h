#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Host-only typed trace. The legacy digest remains available for provenance.
class AmoledSequenceTrace {
public:
    AmoledSequenceTrace(uint32_t seed, uint8_t scenario)
        : seed_(seed), scenario_(scenario), enabled_(std::getenv("STICKMON_SEQUENCE_TRACE")) {}

    void begin(int frame) {
        first_ = true;
        if (enabled_)
            std::printf("SequenceFrame {\"seed\":%u,\"scenario\":%u,\"frame\":%d,\"fields\":{",
                        seed_, scenario_, frame);
    }
    void integer(const char* name, uint32_t value) {
        addHash(value);
        if (enabled_) {
            prefix(name);
            std::printf("%u", value);
        }
    }
    void number(const char* name, float value) {
        assert(std::isfinite(value));
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        addHash(bits);
        if (enabled_) {
            prefix(name);
            // Keep a decimal point so integral float values retain their type.
            std::printf("%#.9g", double(value));
        }
    }
    void clock(const char* name, uint32_t value, uint32_t base, bool active = true) {
        addHash(value - base);
        if (enabled_) {
            prefix(name);
            // An active deadline may legitimately equal zero at clock wrap.
            std::printf("%u", active ? value - base : 0);
        }
    }
    void end() const {
        if (enabled_) std::puts("}}");
    }
    void actor(int slot) { actor_ = slot; }
    uint64_t hash() const { return hash_; }

private:
    void prefix(const char* name) {
        if (actor_ >= 0 && std::strncmp(name, "actor.", 6) == 0)
            std::printf("%s\"%s.%s\":", first_ ? "" : ",",
                        actor_ == 0 ? "main" : "companion", name + 6);
        else
            std::printf("%s\"%s\":", first_ ? "" : ",", name);
        first_ = false;
    }
    void addHash(uint32_t value) {
        for (uint8_t byte = 0; byte < 4; ++byte) {
            hash_ = (hash_ ^ (value & 255U)) * 1099511628211ULL;
            value >>= 8;
        }
    }
    uint64_t hash_ = 1469598103934665603ULL;
    uint32_t seed_;
    uint8_t scenario_;
    bool enabled_;
    bool first_ = true;
    int actor_ = -1;
};
