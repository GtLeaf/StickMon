#pragma once
#include "presentation/PixelRenderer.h"

namespace Ui::MarketStallIcons {
inline void draw(bool advice,int cx,int cy) {
        // Small pixel artwork shares the item icons' native 2px grid.
        static constexpr const char* bag[] = {
            "....................", ".......######.......", "......##....##......", ".....##......##.....",
            ".....##......##.....", "...##############...", "...#TTTTTTTTTTTT#...", "...#T##########T#...",
            "...#TT#TTTTTT#TT#...", "...#TT#TTTTTT#TT#...", "...#TTTTTTTTTTTT#...", "...#TTTTTTTTTTTT#...",
            "...#TTTTTTTTTTTT#...", "...#TTTTTTTTTTTT#...", "...#TTTTTTTTTTTT#...", "...#TTTTTTTTTTTT#...",
            "...#LLLLLLLLLLLL#...", "...##############...", "....................", "...................."};
        static constexpr const char* note[] = {
            ".......######.......", "....####TTTT####....", "...#LLL######LLL#...", "...#LPPPPPPPPPPL#...",
            "...#LPPPPPPPPPPL#...", "...#LPSSPPSSSSPL#...", "...#LPSSPPSSSSPL#...", "...#LPPPPPPPPPPL#...",
            "...#LPSSPPSSSSPL#...", "...#LPSSPPSSSSPL#...", "...#LPPPPPPPPPPL#...", "...#LPSSPPSSSSPL#...",
            "...#LPSSPPSSSSPL#...", "...#LPPPPPPPPPPL#...", "...#LPPPPPPPPPPL#...", "...#LLLLLLLLLLLL#...",
            "....############....", "....................", "....................", "...................."};
        const auto art = advice ? note : bag;
        auto& c = PixelRenderer::canvas();
        for (int y=0;y<20;++y) for (int x=0;x<20;++x) {
            const char p=art[y][x];
            if (p=='.') continue;
            const auto color = p=='#' ? PixelRenderer::rgb(40,56,80) : p=='T' ? PixelRenderer::rgb(196,152,88) :
                p=='L' ? PixelRenderer::rgb(160,112,64) : p=='P' ? PixelRenderer::rgb(248,240,208) : PixelRenderer::rgb(104,128,112);
            c.fillRect(cx-20+x*2,cy-20+y*2,2,2,color);
        }
}
} // namespace Ui::MarketStallIcons
