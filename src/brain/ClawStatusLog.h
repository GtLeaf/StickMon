#pragma once

#include <cstddef>
#include <cstdint>

namespace Stickmon {

// Fixed-size ring buffer of timestamped status lines shared by the device
// setup page (电脑 -> ESP-Claw) and the phone setup portal. This is a pure
// data structure with no locking; the caller (ClawRuntime) serializes access.
// Messages longer than one entry are split at UTF-8 boundaries into
// continuation entries so no content is ever truncated or lost.
class ClawStatusLog {
public:
    enum class Level : uint8_t { INFO = 0, OK, WARN, ERROR };

    static constexpr size_t CAPACITY = 64;
    static constexpr size_t TEXT_LEN = 64;  // Bytes per entry, including NUL.

    struct Entry {
        uint32_t ms = 0;
        Level level = Level::INFO;
        char text[TEXT_LEN] = {};
    };

    void append(Level level, uint32_t ms, const char* text);

    // Copies up to maxCount of the newest entries (oldest first) into out.
    // Returns the number of copied entries.
    size_t copyRecent(Entry* out, size_t maxCount) const;

    // Copies stored entries newer than `since` (a previously read
    // generation()), oldest first. Returns the number of copied entries.
    size_t copySince(uint32_t since, Entry* out, size_t maxCount) const;

    // Monotonic count of appended entries; use as a change counter / cursor.
    uint32_t generation() const { return total_; }
    // Generation of the oldest stored entry; entries older than this are gone.
    uint32_t oldestGeneration() const { return total_ - size_; }
    size_t size() const { return size_; }

private:
    Entry entries_[CAPACITY]{};
    size_t head_ = 0;     // Next write position.
    size_t size_ = 0;     // Valid entries.
    uint32_t total_ = 0;  // Total appended entries (monotonic).
};

}  // namespace Stickmon
