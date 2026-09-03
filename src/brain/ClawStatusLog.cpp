#include "brain/ClawStatusLog.h"

#include <cstring>

namespace Stickmon {

void ClawStatusLog::append(Level level, uint32_t ms, const char* text) {
    if (!text) return;
    const char* cursor = text;
    while (true) {
        size_t chunk = std::strlen(cursor);
        if (chunk > TEXT_LEN - 1) {
            chunk = TEXT_LEN - 1;
            // Never split inside a multi-byte UTF-8 character: move the split
            // point back in front of the current character's lead byte.
            while (chunk > 0 &&
                   (static_cast<uint8_t>(cursor[chunk]) & 0xC0) == 0x80) {
                --chunk;
            }
        }
        Entry& entry = entries_[head_];
        entry.ms = ms;
        entry.level = level;
        std::memcpy(entry.text, cursor, chunk);
        entry.text[chunk] = '\0';
        head_ = (head_ + 1) % CAPACITY;
        if (size_ < CAPACITY) ++size_;
        ++total_;
        cursor += chunk;
        if (*cursor == '\0') return;
    }
}

size_t ClawStatusLog::copyRecent(Entry* out, size_t maxCount) const {
    if (!out || maxCount == 0) return 0;
    const size_t count = size_ < maxCount ? size_ : maxCount;
    const size_t first = (head_ + CAPACITY - count) % CAPACITY;
    for (size_t i = 0; i < count; ++i) {
        out[i] = entries_[(first + i) % CAPACITY];
    }
    return count;
}

size_t ClawStatusLog::copySince(uint32_t since, Entry* out,
                                size_t maxCount) const {
    if (!out || maxCount == 0) return 0;
    const uint32_t oldest = oldestGeneration();
    size_t skip = since <= oldest ? 0 : static_cast<size_t>(since - oldest);
    if (skip > size_) skip = size_;
    const size_t count =
        (size_ - skip) < maxCount ? (size_ - skip) : maxCount;
    const size_t first = (head_ + CAPACITY - size_ + skip) % CAPACITY;
    for (size_t i = 0; i < count; ++i) {
        out[i] = entries_[(first + i) % CAPACITY];
    }
    return count;
}

}  // namespace Stickmon
