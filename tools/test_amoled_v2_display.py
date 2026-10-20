#!/usr/bin/env python3
"""Exercise the real V2 transfer functions against an asynchronous 2x2 panel."""

from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class V2DisplayTests(unittest.TestCase):
    def test_dirty_rectangles_preserve_panel_pixels_and_dma_ownership(self):
        source = (ROOT / "firmware/amoled_1_8_v2/main/main.cpp").read_text()
        transfers = source[source.index("esp_err_t submitFrame("):
                           source.index("uint32_t millisNow()")]
        helper = ""
        if "void alignDisplayUpdateRegion(" in source:
            start = source.index("void alignDisplayUpdateRegion(")
            helper = source[start:source.index("esp_err_t submitFrame(", start)]
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "display.cpp"
            cpp.write_text(PREFIX + helper + transfers + SUFFIX)
            binary = Path(directory) / "display"
            subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            str(cpp), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


PREFIX = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>
constexpr uint16_t LOGICAL_WIDTH = 368, LOGICAL_HEIGHT = 448;
constexpr uint16_t PHYSICAL_WIDTH = 368, TRANSFER_LOGICAL_ROWS = 32;
constexpr size_t TRANSFER_BUFFER_COUNT = 2;
constexpr int DISPLAY_UPDATE_ALIGNMENT = 2;
using TransferBuffers = std::array<uint16_t*, TRANSFER_BUFFER_COUNT>;
using esp_err_t = int;
using esp_lcd_panel_handle_t = void*;
using SemaphoreHandle_t = void*;
constexpr int ESP_OK = 0, ESP_ERR_INVALID_ARG = 1, ESP_ERR_TIMEOUT = 2, pdTRUE = 1;
#define pdMS_TO_TICKS(ms) (ms)
struct Transfer {
    int x0, y0, x1, y1;
    const uint16_t* data;
    std::vector<uint16_t> original;
};
std::deque<Transfer> pending;
std::vector<uint16_t> panel(LOGICAL_WIDTH * LOGICAL_HEIGHT);
unsigned calls = 0;
int xSemaphoreTake(SemaphoreHandle_t, int) {
    assert(!pending.empty());
    const auto& t = pending.front();
    // Deferred DMA must still see the bytes present at submission.
    assert(std::equal(t.original.begin(), t.original.end(), t.data));
    for (int y = t.y0; y < t.y1; ++y) {
        std::copy_n(t.data + (y - t.y0) * (t.x1 - t.x0), t.x1 - t.x0,
                    panel.data() + y * LOGICAL_WIDTH + t.x0);
    }
    pending.pop_front();
    return pdTRUE;
}
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t,
                                  int x0, int y0, int x1, int y1,
                                  const void* data) {
    // CO5300 BSP's rounder contract, including every 32-row chunk.
    assert(x0 % 2 == 0 && y0 % 2 == 0 && x1 % 2 == 0 && y1 % 2 == 0);
    assert(x0 >= 0 && x1 <= LOGICAL_WIDTH && y0 >= 0 && y1 <= LOGICAL_HEIGHT);
    assert(x0 < x1 && y0 < y1 && y1 - y0 <= TRANSFER_LOGICAL_ROWS);
    assert(pending.size() < TRANSFER_BUFFER_COUNT);
    for (const auto& t : pending) assert(t.data != data);
    const auto* pixels = static_cast<const uint16_t*>(data);
    pending.push_back({x0, y0, x1, y1, pixels,
                      std::vector<uint16_t>(pixels, pixels + (x1-x0)*(y1-y0))});
    ++calls;
    return ESP_OK;
}
'''

SUFFIX = r'''
int main() {
    std::vector<uint16_t> frame(panel.size());
    for (size_t i = 0; i < frame.size(); ++i) frame[i] = (i * 71 + i / 368) % 65536;
    std::array<uint16_t, LOGICAL_WIDTH * TRANSFER_LOGICAL_ROWS> a{}, b{};
    TransferBuffers buffers{a.data(), b.data()};
    auto* done = reinterpret_cast<void*>(1);
    assert(submitFrame(nullptr, frame.data(), buffers, done) == ESP_OK);
    assert(panel == frame && pending.empty());

    // Odd bounds, odd widths, 32-row crossings, single pixels, clipped edges,
    // and repeated movement: untouched neighboring pixels must remain intact.
    const uint16_t regions[][4] = {
        {73, 133, 125, 192}, {131, 196, 159, 224}, {1, 2, 1, 2},
        {367, 368, 447, 448}, {0, 1, 0, 1}, {365, 600, 443, 600},
        {0, 368, 23, 366}, {53, 128, 79, 146}
    };
    for (const auto& r : regions) {
        for (int y = r[2]; y < std::min<int>(r[3], LOGICAL_HEIGHT); ++y)
            for (int x = r[0]; x < std::min<int>(r[1], LOGICAL_WIDTH); ++x)
                frame[y * LOGICAL_WIDTH + x] ^= 0xFFFF;
        assert(submitFrameRegion(nullptr, frame.data(), buffers, done,
                                 r[0], r[1], r[2], r[3]) == ESP_OK);
        assert(panel == frame && pending.empty());
    }
    for (int y = 23; y < 88; ++y)
        for (int x = 0; x < LOGICAL_WIDTH; ++x) frame[y * LOGICAL_WIDTH + x] ^= 0x1234;
    assert(submitFrame(nullptr, frame.data(), buffers, done, 23, 88) == ESP_OK);
    assert(panel == frame && pending.empty());
    const auto before = calls;
    assert(submitFrameRegion(nullptr, frame.data(), buffers, done, 3, 3, 1, 3) == ESP_OK);
    assert(submitFrameRegion(nullptr, frame.data(), buffers, done, 9, 3, 1, 3) == ESP_OK);
    assert(submitFrameRegion(nullptr, frame.data(), buffers, done, 500, 600, 1, 3) == ESP_OK);
    assert(submitFrame(nullptr, frame.data(), buffers, done, 3, 3) == ESP_OK);
    assert(calls == before);
    assert(submitFrame(nullptr, frame.data(), buffers, nullptr) == ESP_ERR_INVALID_ARG);
}
'''


if __name__ == "__main__":
    unittest.main()
