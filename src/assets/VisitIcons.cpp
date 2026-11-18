#include "assets/VisitIcons.h"
#include "assets/VisitIconsData.h"
#include "presentation/Canvas565.h"
#include <algorithm>
namespace VisitIcons {
void draw(Canvas565 &canvas, Kind kind, int centerX, int top) {
    const unsigned index = static_cast<unsigned>(kind);
    if (index >= sizeof(Data::FRAMES) / sizeof(Data::FRAMES[0]))
        return;
    const auto &frame = Data::FRAMES[index];
    unsigned position = 0;
    for (unsigned offset = frame.offset; offset < frame.offset + frame.length; offset += 2) {
        unsigned count = Data::RUNS[offset];
        const auto color = Data::RUNS[offset + 1];
        if (!color) {
            position += count;
            continue;
        }
        while (count) {
            const unsigned x = position % frame.width, y = position / frame.width;
            const unsigned length = std::min(count, unsigned(frame.width) - x);
            canvas.drawFastHLine(centerX - frame.width / 2 + int(x), top + int(y), length,
                                 Data::PALETTE[color]);
            position += length;
            count -= length;
        }
    }
}
} // namespace VisitIcons
