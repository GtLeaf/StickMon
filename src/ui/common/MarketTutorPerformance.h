#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "assets/PokemonSprites.h"
#include "game/Species.h"
#include "presentation/PixelRenderer.h"

// Presentation only. Transactions and the performance clock belong to the
// caller. Learning and training each have three user-selected styles.
namespace Ui::MarketTutorPerformance {
    enum class Style : uint8_t {
        REMOTE, MELEE, SELF, LEARN_REMOTE, LEARN_MELEE, LEARN_SELF
    };
    inline constexpr uint32_t TRAINING_MS = 4800, LEARNING_MS = 5600;
    // A presentation choice only; do not alter contact flags or battle rules.
    inline Style trainingStyle(const MoveInfo* move) {
        if (!move) return Style::REMOTE;
        if (move->damageClass == DamageClass::STATUS) return Style::SELF;
        // Seed Bomb and Smack Down are thrown attacks even though the current
        // imported flags mark them as contact. Keep that data issue separate.
        if (move->id == 402 || move->id == 479) return Style::REMOTE;
        return move->flags & MOVE_FLAG_CONTACT ? Style::MELEE : Style::REMOTE;
    }
    inline Style learningStyle(const MoveInfo* move) {
        switch (trainingStyle(move)) {
        case Style::MELEE: return Style::LEARN_MELEE;
        case Style::SELF: return Style::LEARN_SELF;
        default: return Style::LEARN_REMOTE;
        }
    }
    namespace detail {
        inline constexpr double PI = 3.141592653589793;
        inline constexpr uint16_t rgb(int r, int g, int b) {
            return uint16_t(((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3));
        }
        inline constexpr uint16_t ink = rgb(40, 56, 80), purple = rgb(138, 92, 200);
        inline constexpr uint16_t dark = rgb(70, 40, 112), gold = rgb(240, 168, 48);
        inline constexpr uint16_t white = rgb(255, 255, 240);
        inline constexpr uint16_t energy = rgb(112, 184, 216);
        inline constexpr uint16_t shadow = rgb(186, 196, 160);
        inline Canvas565& c() {
            return PixelRenderer::canvas();
        }
        inline double p(double t, double a, double b) {
            return std::clamp((t-a)/(b-a), 0.0, 1.0);
        }
        inline int lerp(int a, int b, double q) {
            return int(std::lround(a+(b-a)*q));
        }
        inline int hop(double t, double a, double b, int h) {
            return int(-h*std::sin(PI*p(t, a, b)));
        }
        inline void ring(int x, int y, int r, uint16_t color, int thick = 2) {
            if (r<1)return;
            for (int i = 0;i<thick;++i)c().drawCircle(x, y, r+i, color);
        }
        inline void star(int x, int y, int r, uint16_t color) {
            c().drawFastHLine(x-r, y, 2*r+1, color);
            c().drawFastVLine(x, y-r, 2*r+1, color);
            c().fillCircle(x, y, 2, color);
        }
        inline void burst(int x, int y, double q, uint16_t color, int count = 7) {
            if (q<0||q>1)return;
            for (int i = 0;i<count;++i) {
                double a = i*2*PI/count;
                int rad = lerp(6, 34, q);
                int px = x+int(std::cos(a)*rad), py = y+int(std::sin(a)*rad);
                star(px, py, q>.75?2:4, color);
            }
        }
        inline void ball(int x, int y, uint16_t color, int r, bool left, double q) {
            for (int i = 3;i>0;--i)c().fillCircle(x+(left?1:-1)*i*7, y, i==1?3:2, color);
            c().fillCircle(x, y, r+2, dark);
            c().fillCircle(x, y, r, color);
            c().fillCircle(x-2, y-2, std::max(2, r/3), white);
            if (q>.5)star(x, y, r+4, color);
        }
        inline void charge(int x, int y, double q, uint16_t color, double phase) {
            ring(x, y, lerp(4, 14, q), color);
            for (int i = 0;i<5;++i) {
                double a = phase*5+i*2*PI/5;
                int rad = lerp(27, 12, q);
                c().fillCircle(x+int(std::cos(a)*rad), y+int(std::sin(a)*rad), 2, color);
            }
            c().fillCircle(x, y, lerp(2, 7, q), color);
        }
        inline void actor(int x, int ground, int lift, uint16_t id, bool right, int pose) {
            c().fillEllipse(x, ground, 22, 5, shadow);
            PokemonSprites::WalkingAnimation a{};
            if (!PokemonSprites::walkingAnimation(id, right?PokemonSprites::WalkDirection::RIGHT:PokemonSprites::WalkDirection::LEFT, a)) return;
            auto kind = PokemonSprites::SpriteKind(uint16_t(a.base)+pose%a.frameCount);
            auto* frame = PokemonSprites::findCachedSpeciesSprite(id, kind);
            if (!frame) frame = PokemonSprites::findSpeciesSprite(id, kind);
            if (!frame) return;
            float scale = std::min(2.0f, 80.0f/std::max(frame->width, frame->height));
            int sy = int(ground+lift-(PokemonSprites::frameGroundOffsetY(frame)+frame->height/2.0f)*scale);
            PokemonSprites::drawFrameScaled(frame, int(x-frame->width*scale/2), sy, scale, a.flipX, true);
        }
        inline void approval(double t, double a, double b) {
            double q = p(t, a, b);
            if (t<a||t>b)return;
            star(90, 287-int(q*10), 4, gold);
            star(278, 280-int(q*10), 5, gold);
        }
        inline void remote(double t, uint16_t partner) {
            int gh = 0, ph = 0, gpose = 0, ppose = 0;
            if (t>.7&&t<1.5) {
                ph = hop(t, .7, 1.5, 5);
                ppose = int(t*10)%3;
            }
            if (t>2.65&&t<3.45) {
                ph = hop(t, 2.65, 3.45, 7);
                ppose = int(t*10)%3;
            }
            if (t>4.1) {
                gh = hop(t, 4.1, 4.5, 7);
                ph = hop(t, 4.2, 4.6, 12);
                gpose = int(t*8)%3;
            }
            actor(90, 366, gh, 94, true, gpose);
            actor(278, 366, ph, partner, false, ppose);
            if (t<1.85) {
                double q = p(t, 0, .55);
                ring(178, 330, lerp(2, 19, q), purple, 3);
                ring(178, 330, lerp(1, 10, q), purple);
                c().fillCircle(178, 330, 3, purple);
            }
            if (t>=.65&&t<1.4)charge(242, 330, p(t, .65, 1.4), energy, t);
            if (t>=1.4&&t<1.85)ball(lerp(242, 178, p(t, 1.4, 1.85)), 330, energy, 8, true, 0);
            if (t>=1.85&&t<2.35)burst(178, 330, p(t, 1.85, 2.35), gold);
            if (t>=2.4&&t<3.9) {
                ring(178, 330, lerp(3, 13, p(t, 2.4, 2.65)), purple, 3);
                ring(178, 330, 6, purple);
                c().fillCircle(178, 330, 3, purple);
            }
            if (t>=2.65&&t<3.4)charge(242, 330, p(t, 2.65, 3.4), energy, t);
            if (t>=3.4&&t<3.9)ball(lerp(242, 178, p(t, 3.4, 3.9)), 330, energy, 11, true, 0);
            if (t>=3.9&&t<4.45)burst(178, 330, p(t, 3.9, 4.45), gold);
            approval(t, 4.15, 4.8);
        }
        inline void melee(double t, uint16_t partner) {
            int px = 278, py = 0, pose = 0;
            if (t < .6) px = lerp(278, 286, p(t, 0, .6));
            else if (t < 1.15) {
                px = lerp(286, 205, p(t, .6, 1.15));
                pose = int((t - .6) * 14) % 3;
            } else if (t < 1.35) {
                px = 205; py = hop(t, 1.15, 1.35, 4); pose = 1;
            } else if (t < 1.9) {
                px = lerp(205, 278, p(t, 1.35, 1.9)); pose = int(t * 12) % 3;
            } else if (t >= 2.6 && t < 3.15) {
                px = lerp(278, 200, p(t, 2.6, 3.15)); pose = int(t * 14) % 3;
            } else if (t >= 3.15 && t < 3.4) {
                px = 200; py = hop(t, 3.15, 3.4, 7); pose = 1;
            } else if (t >= 3.4 && t < 3.95) {
                px = lerp(200, 278, p(t, 3.4, 3.95)); pose = int(t * 12) % 3;
            }
            if (t >= 4.2) py = hop(t, 4.2, 4.6, 10);
            int tx = 180;
            if (t >= 1.2 && t < 1.55) tx -= int(5 * std::sin(PI * p(t, 1.2, 1.55)));
            if (t >= 3.2 && t < 3.65) tx -= int(8 * std::sin(PI * p(t, 3.2, 3.65)));
            if (t < 4.2) {
                const int r = lerp(1, 18, p(t, 0, .5));
                c().fillRect(tx - 2, 330, 5, 36, shadow);
                c().fillEllipse(tx, 367, 14, 3, shadow);
                c().fillCircle(tx, 330, r + 2, dark);
                c().fillCircle(tx, 330, r, rgb(232, 216, 192));
                ring(tx, 330, std::max(1, r - 4), purple);
                c().fillCircle(tx, 330, 3, purple);
            }
            const int gh = t > 4.15 ? hop(t, 4.15, 4.6, 7) : 0;
            actor(90, 366, gh, 94, true, 0);
            actor(px, 366, py, partner, false, pose);
            if (t >= 1.2 && t < 1.55) {
                const double q = p(t, 1.2, 1.55);
                ring(tx, 330, lerp(6, 24, q), energy);
                star(tx + 8, 325, 4, white);
            }
            if (t >= 3.2 && t < 3.65) {
                const double q = p(t, 3.2, 3.65);
                ring(tx, 330, lerp(4, 28, q), energy, 3);
                burst(tx, 330, q, gold, 5);
            }
            if (t >= 4.2 && t < 4.65) burst(180, 330, p(t, 4.2, 4.65), gold);
            approval(t, 4.25, 4.8);
        }
        inline uint16_t fade(uint16_t color, double q) {
            q = std::clamp(q, 0.0, 1.0);
            const int r = ((color >> 11) & 31) * 255 / 31;
            const int g = ((color >> 5) & 63) * 255 / 63;
            const int b = (color & 31) * 255 / 31;
            return rgb(lerp(r, 248, q), lerp(g, 248, q), lerp(b, 224, q));
        }
        inline void self(double t, uint16_t partner) {
            constexpr int cx = 278, cy = 329;
            int lift = 0, pose = 0;
            if (t >= .6 && t < 1.4) {
                lift = int(3 * std::sin(PI * p(t, .6, 1.4))); pose = int(t * 6) % 3;
            }
            if (t >= 2.6 && t < 3.3) {
                lift = int(3 * std::sin(PI * p(t, 2.6, 3.3))); pose = int(t * 6) % 3;
            }
            if (t >= 4.25) lift = hop(t, 4.25, 4.65, 8);
            // First cast: loose points gather into a wavering ring.
            if (t < 2.0) {
                const double gather = p(t, .45, 1.3), disperse = p(t, 1.65, 2.0);
                for (int i = 0; i < 9; ++i) {
                    const double a = i * 2 * PI / 9 + t * .6;
                    const int rad = lerp(39, 27, gather) + int(3 * std::sin(t * 17 + i));
                    const int x = cx + int(std::cos(a) * rad);
                    const int y = cy + int(std::sin(a) * rad * .85);
                    c().fillCircle(x, y, i % 3 == 0 ? 3 : 2, fade(energy, disperse));
                }
                if (t >= .8) ring(cx, cy, 27 + int(std::sin(t * 18) * 2), fade(energy, .35 + disperse * .65), 1);
            }
            // Second cast: a complete, steady halo.
            if (t >= 2.6 && t < 4.25) {
                const int rad = lerp(40, 28, p(t, 2.6, 3.3));
                ring(cx, cy, rad, energy, 2);
                if (t >= 3.3) {
                    ring(cx, cy, 32, fade(energy, .55), 1);
                    for (int i = 0; i < 4; ++i) {
                        const double q = std::fmod(t * 1.2 + i * .25, 1.0);
                        c().fillCircle(cx - 25 + i * 16, lerp(357, 290, q), 2, fade(energy, q * .5));
                    }
                }
            }
            if (t >= 4.25) {
                const double q = p(t, 4.25, 4.8);
                ring(cx, cy, lerp(28, 38, q), fade(energy, q), 1);
                burst(cx, 310, q, fade(gold, q * .5), 5);
            }
            const int gh = (t >= 2.05 && t < 2.5) ? hop(t, 2.05, 2.5, 4) :
                           t >= 4.25 ? hop(t, 4.25, 4.65, 6) : 0;
            actor(90, 366, gh, 94, true, 0);
            actor(cx, 366, lift, partner, false, pose);
            approval(t, 4.3, 4.8);
        }
        namespace learning {
            inline void question(double t,double a,double b){if(t<a||t>=b)return;const bool native=c().nativeText();c().setNativeText(true);PixelRenderer::text(272,252,"?",ink);c().setNativeText(native);}
            inline void pulseTarget(double t,double a,double b){if(t<a||t>=b)return;ring(178,330,lerp(19,27,p(t,a,b)),purple);}
            inline void remote(double t, uint16_t partner){
             int gh=0,ph=0,gpose=0,ppose=0,gx=90;
             if(t>=.4&&t<1.4){gh=hop(t,.4,1.4,7);gpose=int(t*10)%3;}
             if(t>=1.4&&t<2.3){ph=hop(t,1.4,2.3,4);ppose=int(t*10)%3;}
             if(t>=2.3&&t<2.5)gx+=int(std::sin(t*30)*3);
             if(t>=2.5&&t<3.5){gh=hop(t,2.5,3.5,8);gpose=int(t*10)%3;}
             if(t>=3.5&&t<4.6){ph=hop(t,3.5,4.6,8);ppose=int(t*10)%3;}
             if(t>=4.8){gh=hop(t,4.8,5.25,9);ph=hop(t,4.8,5.25,10);}
             actor(gx,366,gh,94,true,gpose);actor(278,366,ph,partner,false,ppose);
             if(t<4.45){int r=lerp(2,19,p(t,0,.4));ring(178,330,r,purple,2);ring(178,330,std::max(1,r/2),purple);c().fillCircle(178,330,3,purple);}
             if(t>=.4&&t<.9)charge(126,330,p(t,.4,.9),purple,t);
             if(t>=.9&&t<1.25)ball(lerp(126,178,p(t,.9,1.25)),330,purple,8,false,0);
             pulseTarget(t,1.25,1.4);
             // The first attempt is too weak to reach the target.
             if(t>=1.4&&t<2.0)charge(242,330,p(t,1.4,2.0)*.4,energy,t);
             if(t>=2.0&&t<2.35){double q=p(t,2.0,2.35);c().fillCircle(lerp(242,222,q),lerp(330,359,q),lerp(4,1,q),energy);}
             question(t,2.15,2.5);
             // A slower, complete demonstration makes the second attempt clear.
             if(t>=2.5&&t<2.95)charge(126,330,p(t,2.5,2.95),purple,t);
             if(t>=2.95&&t<3.35)ball(lerp(126,178,p(t,2.95,3.35)),330,purple,9,false,0);
             pulseTarget(t,3.35,3.5);
             if(t>=3.5&&t<4.0)charge(242,330,p(t,3.5,4.0),energy,t);
             if(t>=4.0&&t<4.45)ball(lerp(242,178,p(t,4.0,4.45)),330,energy,9,true,0);
             if(t>=4.45&&t<4.95)burst(178,330,p(t,4.45,4.95),gold);
             approval(t,4.8,5.6);
            }
            inline void melee(double t, uint16_t partner){
             int gx=90,px=278,gh=0,ph=0,gpose=0,ppose=0,tx=180;
             // Teacher strikes and fully returns before the learner moves.
             if(t>=.4&&t<.75){gx=lerp(90,150,p(t,.4,.75));gpose=int(t*14)%3;}
             else if(t>=.75&&t<.9){gx=150;gh=hop(t,.75,.9,4);gpose=1;}
             else if(t>=.9&&t<1.25){gx=lerp(150,90,p(t,.9,1.25));gpose=int(t*12)%3;}
             // Learner stops at the edge of the target on the first try.
             if(t>=1.4&&t<1.8){px=lerp(278,221,p(t,1.4,1.8));ppose=int(t*12)%3;}
             else if(t>=1.8&&t<2.0){px=221;ph=hop(t,1.8,2.0,2);}
             else if(t>=2.0&&t<2.4){px=lerp(221,278,p(t,2.0,2.4));ppose=int(t*10)%3;}
             if(t>=2.5&&t<2.85){gx=lerp(90,154,p(t,2.5,2.85));gpose=int(t*14)%3;}
             else if(t>=2.85&&t<3.0){gx=154;gh=hop(t,2.85,3.0,5);gpose=1;}
             else if(t>=3.0&&t<3.35){gx=lerp(154,90,p(t,3.0,3.35));gpose=int(t*12)%3;}
             if(t>=3.5&&t<3.95){px=lerp(278,200,p(t,3.5,3.95));ppose=int(t*14)%3;}
             else if(t>=3.95&&t<4.15){px=200;ph=hop(t,3.95,4.15,6);ppose=1;}
             else if(t>=4.15&&t<4.55){px=lerp(200,278,p(t,4.15,4.55));ppose=int(t*12)%3;}
             if(t>=.8&&t<1.15)tx+=int(5*std::sin(PI*p(t,.8,1.15)));
             if(t>=1.85&&t<2.1)tx-=int(2*std::sin(PI*p(t,1.85,2.1)));
             if(t>=2.9&&t<3.25)tx+=int(6*std::sin(PI*p(t,2.9,3.25)));
             if(t>=4.0&&t<4.4)tx-=int(8*std::sin(PI*p(t,4.0,4.4)));
             if(t<4.6){int r=lerp(1,18,p(t,0,.4));c().fillRect(tx-2,330,5,36,shadow);c().fillEllipse(tx,367,14,3,shadow);c().fillCircle(tx,330,r+2,dark);c().fillCircle(tx,330,r,rgb(232,216,192));ring(tx,330,std::max(1,r-4),purple);c().fillCircle(tx,330,3,purple);}
             if(t>=4.8){gh=hop(t,4.8,5.25,9);ph=hop(t,4.8,5.25,10);}
             actor(gx,366,gh,94,true,gpose);actor(px,366,ph,partner,false,ppose);
             if(t>=.8&&t<1.15)ring(tx,330,lerp(5,24,p(t,.8,1.15)),purple,2);
             if(t>=1.85&&t<2.1)star(tx+16,330,2,energy);
             question(t,2.15,2.5);
             if(t>=2.9&&t<3.25)ring(tx,330,lerp(5,26,p(t,2.9,3.25)),purple,2);
             if(t>=4.0&&t<4.4){double q=p(t,4.0,4.4);ring(tx,330,lerp(4,28,q),energy,3);burst(tx,330,q,gold,5);}
             if(t>=4.6&&t<5.05)burst(180,330,p(t,4.6,5.05),gold);
             approval(t,4.8,5.6);
            }
            inline void halo(int x,double t,double a,double b,uint16_t color,bool steady){
             if(t<a||t>=b)return;
             double q=p(t,a,b),gather=std::min(1.0,q*2.5),endFade=p(q,.78,1.0);
             int rad=lerp(40,28,gather);uint16_t col=fade(color,endFade);
             if(steady){ring(x,329,rad,col,2);if(gather>=1)ring(x,329,32,fade(color,.55+endFade*.45),1);}
             for(int i=0;i<9;++i){double angle=i*2*PI/9+t*.5;int r=rad+(steady?0:int(std::sin(t*17+i)*4));c().fillCircle(x+int(std::cos(angle)*r),329+int(std::sin(angle)*r*.85),i%3==0?3:2,col);}
             if(!steady&&q>.35&&q<.8){for(int i=0;i<4;++i){double angle=t*5+i*1.4;c().drawCircle(x+int(std::cos(angle)*29),329+int(std::sin(angle)*25),2,col);}}
            }
            inline void self(double t, uint16_t partner){
             int gh=0,ph=0,gpose=0,ppose=0;
             if(t>=.4&&t<1.4){gh=hop(t,.4,1.4,4);gpose=int(t*6)%3;}
             if(t>=1.4&&t<2.5){ph=int(2*std::sin(PI*p(t,1.4,2.5)));ppose=int(t*6)%3;}
             if(t>=2.5&&t<3.5){gh=hop(t,2.5,3.5,4);gpose=int(t*6)%3;}
             if(t>=3.5&&t<4.6){ph=hop(t,3.5,4.6,3);ppose=int(t*6)%3;}
             halo(90,t,.4,1.4,purple,true);
             halo(278,t,1.4,2.5,energy,false);
             halo(90,t,2.5,3.5,purple,true);
             halo(278,t,3.5,4.8,energy,true);
             if(t>=4.8&&t<5.3)burst(278,310,p(t,4.8,5.3),gold,5);
             if(t>=4.8){gh=hop(t,4.8,5.25,8);ph=hop(t,4.8,5.25,10);}
             actor(90,366,gh,94,true,gpose);actor(278,366,ph,partner,false,ppose);
             approval(t,4.85,5.6);
            }
        } // namespace learning
    }
    // namespace detail
    inline void render(Style style, uint32_t elapsedMs, uint16_t partner) {
        const double t = elapsedMs / 1000.0;
        switch (style) {
        case Style::LEARN_REMOTE: detail::learning::remote(t, partner); break;
        case Style::LEARN_MELEE: detail::learning::melee(t, partner); break;
        case Style::LEARN_SELF: detail::learning::self(t, partner); break;
        case Style::MELEE: detail::melee(t, partner); break;
        case Style::SELF: detail::self(t, partner); break;
        default: detail::remote(t, partner); break;
        }
    }
}
// namespace Ui::MarketTutorPerformance
