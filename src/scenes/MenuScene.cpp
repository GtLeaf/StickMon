#include "scenes/MenuScene.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "assets/MenuAssets.h"
#include "core/UiStrings.h"
#include "core/GameEngine.h"
#include "game/Species.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

namespace {
const char* statusName(uint8_t statusBits) {
    if (statusBits & Game::STATUS_POISON) return Ui::Status::STATUS_POISON;
    if (statusBits & Game::STATUS_PARALYSIS) return Ui::Status::STATUS_PARALYSIS;
    if (statusBits & Game::STATUS_SLEEP) return Ui::Status::STATUS_SLEEP;
    if (statusBits & Game::STATUS_BURN) return Ui::Status::STATUS_BURN;
    if (statusBits & Game::STATUS_FREEZE) return Ui::Status::STATUS_FREEZE;
    if (statusBits & Game::STATUS_CONFUSION) return Ui::Status::STATUS_CONFUSION;
    return Ui::Status::STATUS_OK;
}

const char* originName(Game::Origin origin) {
    switch (origin) {
    case Game::Origin::STARTER: return Ui::Status::ORIGIN_STARTER;
    case Game::Origin::HATCHED: return Ui::Status::ORIGIN_HATCHED;
    case Game::Origin::CAPTURED: return Ui::Status::ORIGIN_CAPTURED;
    case Game::Origin::TRADED: return Ui::Status::ORIGIN_TRADED;
    case Game::Origin::GIFT: return Ui::Status::ORIGIN_GIFT;
    default: return Ui::Status::ORIGIN_UNKNOWN;
    }
}

void drawInfoRow(int x, int y, const char* value, uint16_t color = PixelRenderer::rgb(241, 242, 232)) {
    PixelRenderer::text(x, y, value, color, 1);
}

void drawStatRow(int x, int y, const char* label, uint16_t value, uint16_t color) {
    char buf[24];
    snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, label, value);
    PixelRenderer::text(x, y, buf, color, 1);
}

int textPixelWidth(const char* value) {
    int width = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(value);
    while (*p) {
        if (*p < 0x80) {
            width += *p == ' ' ? 5 : 8;
            ++p;
        } else if ((*p & 0xE0) == 0xC0) {
            width += 16;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            width += 16;
            p += 3;
        } else {
            width += 8;
            ++p;
        }
    }
    return width;
}

enum PokeBugMainIcon : int {
    POKEBUG_ICON_INFO = 0,
    POKEBUG_ICON_BOX = 1,
    POKEBUG_ICON_SOCIAL = 2,
    POKEBUG_ICON_EXPLORE = 3,
    POKEBUG_ICON_SETTINGS = 4,
    POKEBUG_ICON_BACK = 5,
    POKEBUG_ICON_DEBUG = 6,
};

int menuIconIndex(uint8_t item) {
    switch (item) {
    case 0: return POKEBUG_ICON_INFO;
    case 1: return POKEBUG_ICON_BOX;
    case 2: return POKEBUG_ICON_EXPLORE;
    case 4: return POKEBUG_ICON_SOCIAL;
    case 5: return POKEBUG_ICON_SETTINGS;
    case 6: return POKEBUG_ICON_DEBUG;
    case 7: return POKEBUG_ICON_BACK;
    case 3: // shop has no matching PokeBug icon; keep fallback block.
    default: return -1;
    }
}

uint8_t bagItemCount(uint8_t sourceIndex) {
    switch (sourceIndex) {
    case 0: return GameEngine::ins().ballCount();
    case 1: return GameEngine::ins().greatBallCount();
    case 2: return GameEngine::ins().foodCount();
    case 3: return GameEngine::ins().potionCount();
    case 4: return GameEngine::ins().superPotionCount();
    case 5: return GameEngine::ins().antidoteCount();
    case 6: return GameEngine::ins().candyCount();
    case 7: return 1;
    default: return 0;
    }
}

uint8_t bagVisibleItemCount() {
    uint8_t count = 1; // Back is always visible.
    for (uint8_t i = 0; i < 7; ++i) {
        if (bagItemCount(i) > 0) ++count;
    }
    return count;
}

uint8_t bagSourceIndexForVisible(uint8_t visibleIndex) {
    uint8_t visible = 0;
    for (uint8_t source = 0; source < 7; ++source) {
        if (bagItemCount(source) == 0) continue;
        if (visible == visibleIndex) return source;
        ++visible;
    }
    return 7;
}
}

int8_t MenuScene::lastCursor = 0;

void MenuScene::onEnter() {
    viewMode = ViewMode::MENU;
    statusPage = 0;
    statusMonsterIndex = 0;
    teamCursor = 0;
    teamActionCursor = 0;
    teamActionOpen = false;
    bagCursor = 0;
    bagConfirmOpen = false;
    bagConfirmYes = true;
    computerCursor = 0;
    storageCursor = 0;
    storageScroll = 0.0f;
    debugCursor = 0;
    debugSwitchOpen = false;
    debugSwitchFocus = 0;
    if (GameEngine::ins().previousScene() == GameEngine::ins().homeScene()) {
        cursor = 0;
    } else {
        cursor = lastCursor;
    }
    animCursor = (float)cursor;
}

void MenuScene::onExit() {
    lastCursor = cursor;
}

void MenuScene::update(uint32_t nowMs, float dtSeconds) {
    (void)nowMs;
    (void)dtSeconds;
}

bool MenuScene::onButton(const ButtonEvent& event) {
    if (viewMode != ViewMode::MENU) {
        if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
            GameEngine::ins().requestScene(GameEngine::ins().homeScene());
            return true;
        }
        if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
            if (viewMode == ViewMode::TEAM && teamActionOpen) {
                teamActionOpen = false;
                return true;
            }
            if (viewMode == ViewMode::BAG && bagConfirmOpen) {
                bagConfirmOpen = false;
                return true;
            }
            if (viewMode == ViewMode::DEBUG && debugSwitchOpen) {
                debugSwitchOpen = false;
                return true;
            }
            if (viewMode == ViewMode::STORAGE) {
                viewMode = ViewMode::COMPUTER;
                return true;
            }
            viewMode = ViewMode::MENU;
            teamActionOpen = false;
            bagConfirmOpen = false;
            debugSwitchOpen = false;
            return true;
        }
        if (viewMode == ViewMode::TEAM) {
            uint8_t count = GameEngine::ins().gameState().teamCount;
            if (count == 0) return true;
            if (teamCursor >= count) teamCursor = 0;
            if (event.btn == 1 && event.action == BtnAction::PRESSED) {
                if (teamActionOpen) {
                    teamActionCursor = (teamActionCursor + 1) % 3;
                } else {
                    teamCursor = (teamCursor + 1) % count;
                }
                return true;
            }
            if (event.btn == 0 && event.action == BtnAction::PRESSED) {
                if (!teamActionOpen) {
                    teamActionOpen = true;
                    teamActionCursor = 0;
                    return true;
                }

                if (teamActionCursor == 0) {
                    statusMonsterIndex = teamCursor;
                    statusPage = 0;
                    teamActionOpen = false;
                    viewMode = ViewMode::STATUS;
                } else if (teamActionCursor == 1) {
                    if (GameEngine::ins().moveTeamMemberToFront(teamCursor)) {
                        teamCursor = 0;
                        statusMonsterIndex = 0;
                        toast = Ui::Menu::SWITCH_TOAST;
                        toastUntil = Hal::ins().millis() + 1100;
                    }
                    teamActionOpen = false;
                } else {
                    teamActionOpen = false;
                }
                return true;
            }
            return false;
        }
        if (viewMode == ViewMode::STATUS && event.btn == 1 && event.action == BtnAction::PRESSED) {
            statusPage = (statusPage + 1) % STATUS_PAGE_COUNT;
            return true;
        }
        if (viewMode == ViewMode::BAG && event.btn == 1 && event.action == BtnAction::PRESSED) {
            if (bagConfirmOpen) {
                bagConfirmYes = !bagConfirmYes;
                return true;
            }
            uint8_t visibleCount = bagVisibleItemCount();
            bagCursor = (bagCursor + 1) % visibleCount;
            if (bagCursor == 0) bagScroll = 0.0f;
            return true;
        }
        if (viewMode == ViewMode::BAG && event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (bagConfirmOpen) {
                if (bagConfirmYes) {
                    if (bagConfirmSource == 3) {
                        toast = GameEngine::ins().usePotion() ? Ui::Bag::USED_POTION : Ui::Bag::HP_FULL;
                        toastUntil = Hal::ins().millis() + 1100;
                    } else if (bagConfirmSource == 4) {
                        toast = GameEngine::ins().useSuperPotion() ? Ui::Bag::USED_SUPER_POTION : Ui::Bag::HP_FULL;
                        toastUntil = Hal::ins().millis() + 1100;
                    }
                }
                bagConfirmOpen = false;
                return true;
            }
            uint8_t source = bagSourceIndexForVisible(bagCursor);
            if (source == 3) {
                bagConfirmSource = source;
                bagConfirmYes = true;
                bagConfirmOpen = true;
            } else if (source == 4) {
                bagConfirmSource = source;
                bagConfirmYes = true;
                bagConfirmOpen = true;
            } else if (source == 7) {
                viewMode = ViewMode::MENU;
            }
            return true;
        }
        if (viewMode == ViewMode::COMPUTER && event.btn == 1 && event.action == BtnAction::PRESSED) {
            computerCursor = (computerCursor + 1) % COMPUTER_ITEM_COUNT;
            return true;
        }
        if (viewMode == ViewMode::COMPUTER && event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (computerCursor == 0) {
                GameEngine::ins().requestScene(SceneID::SOCIAL);
            } else if (computerCursor == 1) {
                viewMode = ViewMode::STORAGE;
                storageCursor = 0;
                storageScroll = 0.0f;
            } else {
                viewMode = ViewMode::MENU;
            }
            return true;
        }
        if (viewMode == ViewMode::STORAGE && event.btn == 1 && event.action == BtnAction::PRESSED) {
            uint8_t count = GameEngine::ins().gameState().storageCount + 1;
            storageCursor = (storageCursor + 1) % count;
            if (storageCursor == 0) storageScroll = 0.0f;
            return true;
        }
        if (viewMode == ViewMode::STORAGE && event.btn == 0 && event.action == BtnAction::PRESSED) {
            uint8_t count = GameEngine::ins().gameState().storageCount + 1;
            if (storageCursor >= count - 1) {
                viewMode = ViewMode::COMPUTER;
            }
            return true;
        }
        if (viewMode == ViewMode::DEBUG && debugSwitchOpen && event.btn == 1 && event.action == BtnAction::PRESSED) {
            debugSwitchFocus = (debugSwitchFocus + 1) % DEBUG_SWITCH_FOCUS_COUNT;
            return true;
        }
        if (viewMode == ViewMode::DEBUG && debugSwitchOpen && event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (debugSwitchFocus < 3) {
                debugSwitchDigits[debugSwitchFocus] = (debugSwitchDigits[debugSwitchFocus] + 1) % 10;
            } else if (debugSwitchFocus == 3) {
                if (GameEngine::ins().debugSetActiveSpecies(debugSwitchTargetId())) {
                    debugSwitchOpen = false;
                    toast = Ui::Debug::SWITCHED;
                    toastUntil = Hal::ins().millis() + 1100;
                } else {
                    toast = Ui::Debug::INVALID_ID;
                    toastUntil = Hal::ins().millis() + 1100;
                }
            } else {
                debugSwitchOpen = false;
            }
            return true;
        }
        if (viewMode == ViewMode::DEBUG && event.btn == 1 && event.action == BtnAction::PRESSED) {
            debugCursor = (debugCursor + 1) % DEBUG_ITEM_COUNT;
            return true;
        }
        if (viewMode == ViewMode::DEBUG && event.btn == 0 && event.action == BtnAction::PRESSED) {
            switch (debugCursor) {
            case 0:
                GameEngine::ins().debugRecoverActiveMonster();
                toast = Ui::Debug::RECOVERED;
                toastUntil = Hal::ins().millis() + 1100;
                break;
            case 1:
                openDebugSwitchPopup();
                break;
            case 2:
                GameEngine::ins().addCoins(1000);
                toast = Ui::Debug::COINS_ADDED;
                toastUntil = Hal::ins().millis() + 1100;
                break;
            default:
                viewMode = ViewMode::MENU;
                break;
            }
            return true;
        }
        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            viewMode = ViewMode::TEAM;
            return true;
        }
        return false;
    }

    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        cursor++;
        if (cursor >= ITEM_COUNT) {
            cursor = 0;
            animCursor = 0.0f;
        }
        return true;
    }

    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        GameEngine::ins().requestScene(GameEngine::ins().homeScene());
        return true;
    }

    if (event.btn != 0 || event.action != BtnAction::PRESSED) {
        return false;
    }

    switch (cursor) {
    case ITEM_TEAM:
        viewMode = ViewMode::TEAM;
        teamCursor = 0;
        teamActionCursor = 0;
        teamActionOpen = false;
        return true;
    case ITEM_BAG:
        viewMode = ViewMode::BAG;
        bagCursor = 0;
        bagConfirmOpen = false;
        bagScroll = 0.0f;
        return true;
    case ITEM_EXPLORE: {
        const auto& mon = GameEngine::ins().activeMonster();
        if (mon.fainted || mon.hpCur == 0) {
            toast = Ui::Menu::FAINTED_TOAST;
            toastUntil = Hal::ins().millis() + 1100;
            return true;
        }
        GameEngine::ins().requestScene(SceneID::EXPLORE);
        return true;
    }
    case ITEM_SHOP:
        GameEngine::ins().requestScene(SceneID::SHOP);
        return true;
    case ITEM_COMPUTER:
        viewMode = ViewMode::COMPUTER;
        computerCursor = 0;
        return true;
    case ITEM_SETTINGS:
        GameEngine::ins().requestScene(SceneID::SETTINGS);
        return true;
    case ITEM_DEBUG:
        viewMode = ViewMode::DEBUG;
        debugCursor = 0;
        debugSwitchOpen = false;
        return true;
    case ITEM_BACK:
        GameEngine::ins().requestScene(GameEngine::ins().homeScene());
        return true;
    default:
        return false;
    }
}

void MenuScene::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(7, 9, 14));

    if (viewMode == ViewMode::STATUS) {
        renderStatusPage();
        return;
    }
    if (viewMode == ViewMode::TEAM) {
        renderTeamPage();
        return;
    }
    if (viewMode == ViewMode::BAG) {
        renderBagPage();
        return;
    }
    if (viewMode == ViewMode::COMPUTER) {
        renderComputerPage();
        return;
    }
    if (viewMode == ViewMode::STORAGE) {
        renderStoragePage();
        return;
    }
    if (viewMode == ViewMode::DEBUG) {
        renderDebugPage();
        return;
    }

    renderMenu();
    renderToast();
}

void MenuScene::renderMenu() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0x0000);
    int batt = Hal::ins().filteredBatteryLevel();
    char battBuf[8];
    snprintf(battBuf, sizeof(battBuf), Ui::Menu::TITLE_BATTERY_FMT, batt);
    PixelRenderer::text(Hal::DISPLAY_W - textPixelWidth(battBuf) - 6, 6, battBuf,
                        batt > 20 ? 0x07E0 : 0xF800,
                        1);

    constexpr int centerY = Hal::DISPLAY_H / 2;
    constexpr int spacing = 42;
    constexpr float lerp = 0.25f;
    constexpr int boxW = 56;
    constexpr int boxH = 32;
    constexpr int iconSlotW = 56;
    constexpr int boxX = 20;

    float target = (float)cursor;
    float diff = target - animCursor;
    if (fabsf(diff) < 0.05f) {
        animCursor = target;
    } else {
        animCursor += diff * lerp;
    }

    int order[ITEM_COUNT];
    for (int i = 0; i < ITEM_COUNT; ++i) order[i] = i;
    std::sort(order, order + ITEM_COUNT, [this](int a, int b) {
        return fabsf((float)a - animCursor) < fabsf((float)b - animCursor);
    });

    for (int k = 0; k < ITEM_COUNT; ++k) {
        int i = order[k];
        float rawOffset = (float)i - animCursor;
        int y = centerY + (int)(rawOffset * spacing);

        bool isSelected = fabsf(rawOffset) < 0.5f;
        float relScale = isSelected ? 1.15f : 1.0f;
        uint16_t descColor = isSelected ? 0xFFFF : 0x7BEF;
        int leftW = boxW;

        int iconIndex = menuIconIndex((uint8_t)i);
        if (iconIndex >= 0 && iconIndex < MenuAssets::MAIN_ICON_COUNT) {
            if (isSelected) {
                c.fillRect(boxX - 4, y - boxH / 2, 2, boxH, 0xFFE0);
            }

            uint16_t offset = pgm_read_word(&MenuAssets::MAIN_ICON_FRAMES[iconIndex].offset);
            uint16_t length = pgm_read_word(&MenuAssets::MAIN_ICON_FRAMES[iconIndex].length);
            int scaledIconW = (int)(MenuAssets::FRAME_W * relScale);
            int scaledIconH = (int)(MenuAssets::FRAME_H * relScale);
            int iconX = boxX + (iconSlotW - scaledIconW) / 2;
            int iconY = y - scaledIconH / 2;
            PixelRenderer::drawRgb565RleScaled(iconX, iconY, MenuAssets::FRAME_W, MenuAssets::FRAME_H,
                                               MenuAssets::MAIN_ICON_RLE, offset, length, relScale);
            leftW = iconSlotW;
        } else {
            int drawBoxW = (int)(boxW * relScale);
            int drawBoxH = (int)(boxH * relScale);
            uint16_t boxColor = isSelected ? 0xFFE0 : 0x7BEF;
            if (isSelected) {
                c.fillRect(boxX - 4, y - boxH / 2, 2, boxH, 0xFFE0);
            }
            c.fillRect(boxX, y - drawBoxH / 2, drawBoxW, drawBoxH, boxColor);
            leftW = drawBoxW;
        }

        const char* desc = Ui::Menu::ITEMS[i];
        int tw = textPixelWidth(desc);
        int th = 16;
        int descX = (boxX + leftW + 10 + Hal::DISPLAY_W) / 2 - 10;
        int descY = y - th / 2;
        PixelRenderer::text(descX - tw / 2, descY, desc, descColor, 1);
    }
}

void MenuScene::renderToast() {
    if (!toast || Hal::ins().millis() > toastUntil) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(70, 108, 100, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(78, 110, toast, PixelRenderer::rgb(255, 255, 255), 1);
}

void MenuScene::renderTeamPage() {
    auto& c = PixelRenderer::canvas();
    const auto& state = GameEngine::ins().gameState();
    if (state.teamCount == 0) {
        c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(10, 14, 20));
        PixelRenderer::text(16, 58, Ui::Team::EMPTY, PixelRenderer::rgb(241, 242, 232), 1);
        return;
    }
    if (teamCursor >= state.teamCount) teamCursor = 0;

    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(10, 14, 20));
    PixelRenderer::text(8, 6, Ui::TEAM, PixelRenderer::rgb(67, 213, 224), 1);

    static constexpr int ROW_X = 8;
    static constexpr int ROW_Y = 28;
    static constexpr int ROW_W = Hal::DISPLAY_W - ROW_X * 2;
    static constexpr int ROW_H = 42;
    for (uint8_t i = 0; i < state.teamCount; ++i) {
        int y = ROW_Y + i * (ROW_H + 8);
        const Game::MonsterRuntime& mon = state.team[i];
        const Species& species = GameEngine::ins().speciesFor(mon);
        bool selected = i == teamCursor;
        uint16_t border = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(72, 83, 98);
        c.drawRect(ROW_X, y, ROW_W, ROW_H, border);
        if (selected) c.fillRect(ROW_X, y, 4, ROW_H, PixelRenderer::rgb(255, 216, 72));

        PixelRenderer::text(ROW_X + 10, y + 5, species.name, PixelRenderer::rgb(241, 242, 232), 1);

        uint8_t hpPct = mon.hpMax > 0 ? (uint8_t)((uint32_t)mon.hpCur * 100 / mon.hpMax) : 0;
        uint16_t hpColor = hpPct > 50 ? PixelRenderer::rgb(92, 222, 112) :
                           (hpPct > 20 ? PixelRenderer::rgb(255, 216, 72) :
                            PixelRenderer::rgb(239, 85, 85));
        int barX = ROW_X + 112;
        int barY = y + 8;
        int barW = ROW_W - 124;
        c.fillRect(barX, barY, barW, 8, PixelRenderer::rgb(82, 87, 95));
        int fillW = (barW * hpPct) / 100;
        if (fillW > 0) c.fillRect(barX, barY, fillW, 8, hpColor);
        c.drawRect(barX, barY, barW, 8, PixelRenderer::rgb(45, 48, 56));

        char buf[24];
        snprintf(buf, sizeof(buf), Ui::Status::LEVEL_FMT, mon.level);
        PixelRenderer::text(ROW_X + 10, y + 22, buf, PixelRenderer::rgb(135, 214, 238), 1);

        snprintf(buf, sizeof(buf), "%u/%u", mon.hpCur, mon.hpMax);
        PixelRenderer::text(ROW_X + 112, y + 22, buf, PixelRenderer::rgb(241, 242, 232), 1);
        if (i == 0) {
            PixelRenderer::text(ROW_X + ROW_W - 44, y + 22, Ui::Team::FIRST_BADGE,
                                PixelRenderer::rgb(255, 216, 72), 1);
        }
    }

    if (teamActionOpen) renderTeamActionPopup();
    renderToast();
}

void MenuScene::renderTeamActionPopup() {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 142;
    static constexpr int POP_Y = 26;
    static constexpr int POP_W = 78;
    static constexpr int POP_H = 76;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(241, 242, 232));

    for (uint8_t i = 0; i < 3; ++i) {
        int y = POP_Y + 9 + i * 21;
        bool selected = i == teamActionCursor;
        if (selected) c.fillRect(POP_X + 5, y - 2, 4, 18, PixelRenderer::rgb(255, 216, 72));
        PixelRenderer::text(POP_X + 16, y, Ui::Team::ACTIONS[i],
                            selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232),
                            1);
    }
}

void MenuScene::renderStatusPage() {
    if (!GameEngine::ins().gameState().oobeDone) {
        renderEggStatusPage();
        return;
    }

    auto& c = PixelRenderer::canvas();
    const auto& state = GameEngine::ins().gameState();
    if (statusMonsterIndex >= state.teamCount) statusMonsterIndex = 0;
    const Game::MonsterRuntime& activeMon = state.team[statusMonsterIndex];
    const Species& mon = GameEngine::ins().speciesFor(activeMon);
    char buf[32];

    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(10, 14, 20));
    c.drawFastHLine(6, 22, 216, PixelRenderer::rgb(55, 63, 76));

    if (statusPage == 0) {
        PixelRenderer::text(12, 7, mon.name, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::LEVEL_FMT, activeMon.level);
        drawInfoRow(150, 7, buf);
        snprintf(buf, sizeof(buf), Ui::Status::HP_FMT, activeMon.hpCur, activeMon.hpMax);
        drawInfoRow(12, 32, buf, PixelRenderer::rgb(92, 222, 112));
        snprintf(buf, sizeof(buf), Ui::Status::EXP_FMT, (unsigned long)activeMon.exp);
        drawInfoRow(12, 50, buf);
        snprintf(buf, sizeof(buf), Ui::Status::TYPE_FMT, typeName(mon.type1), typeName(mon.type2));
        drawInfoRow(12, 68, buf, PixelRenderer::rgb(255, 216, 72));
        snprintf(buf, sizeof(buf), Ui::Status::NATURE_FMT, natureName(activeMon.nature));
        drawInfoRow(132, 32, buf);
        snprintf(buf, sizeof(buf), Ui::Status::STATUS_FMT, statusName(activeMon.statusBits));
        drawInfoRow(132, 50, buf);
        snprintf(buf, sizeof(buf), Ui::Status::CONDITION_FMT,
                 activeMon.satiety, activeMon.mood);
        drawInfoRow(132, 68, buf);
        snprintf(buf, sizeof(buf), Ui::Status::AFFECTION_FMT, activeMon.affection, activeMon.proficiency);
        drawInfoRow(12, 88, buf);
    } else if (statusPage == 1) {
        PixelRenderer::text(12, 8, Ui::Status::BASE_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        drawStatRow(18, 34, Ui::Status::STAT_HP, mon.stats.hp, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, 52, Ui::Status::STAT_ATK, mon.stats.atk, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, 70, Ui::Status::STAT_DEF, mon.stats.def, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, 88, Ui::Status::STAT_SPA, mon.stats.spa, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, 106, Ui::Status::STAT_SPD, mon.stats.spd, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(126, 34, Ui::Status::STAT_SPE, mon.stats.spe, PixelRenderer::rgb(241, 242, 232));
    } else if (statusPage == 2) {
        PixelRenderer::text(12, 8, Ui::Status::CURRENT_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        drawStatRow(18, 34, Ui::Status::STAT_HP, statFor(mon, activeMon, 0), PixelRenderer::rgb(92, 222, 112));
        drawStatRow(18, 52, Ui::Status::STAT_ATK, statFor(mon, activeMon, 1), PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, 70, Ui::Status::STAT_DEF, statFor(mon, activeMon, 2), PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, 88, Ui::Status::STAT_SPA, statFor(mon, activeMon, 3), PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, 106, Ui::Status::STAT_SPD, statFor(mon, activeMon, 4), PixelRenderer::rgb(241, 242, 232));
        drawStatRow(126, 34, Ui::Status::STAT_SPE, statFor(mon, activeMon, 5), PixelRenderer::rgb(241, 242, 232));
    } else if (statusPage == 3) {
        PixelRenderer::text(12, 8, Ui::Status::EFFORT_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        snprintf(buf, sizeof(buf), Ui::Status::TOTAL_FMT, Game::evTotal(activeMon.ev), Game::EV_TOTAL_MAX);
        drawInfoRow(126, 8, buf, PixelRenderer::rgb(255, 216, 72));
        drawStatRow(18, 34, Ui::Status::STAT_HP, activeMon.ev.hp, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, 52, Ui::Status::STAT_ATK, activeMon.ev.atk, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, 70, Ui::Status::STAT_DEF, activeMon.ev.def, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, 88, Ui::Status::STAT_SPA, activeMon.ev.spa, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, 106, Ui::Status::STAT_SPD, activeMon.ev.spd, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(126, 34, Ui::Status::STAT_SPE, activeMon.ev.spe, PixelRenderer::rgb(241, 242, 232));
    } else if (statusPage == 4) {
        PixelRenderer::text(12, 8, Ui::Status::INDIVIDUAL_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        drawStatRow(18, 34, Ui::Status::STAT_HP, Game::ivAt(activeMon.ivPacked, 0), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(18, 52, Ui::Status::STAT_ATK, Game::ivAt(activeMon.ivPacked, 1), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(18, 70, Ui::Status::STAT_DEF, Game::ivAt(activeMon.ivPacked, 2), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(18, 88, Ui::Status::STAT_SPA, Game::ivAt(activeMon.ivPacked, 3), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(18, 106, Ui::Status::STAT_SPD, Game::ivAt(activeMon.ivPacked, 4), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(126, 34, Ui::Status::STAT_SPE, Game::ivAt(activeMon.ivPacked, 5), PixelRenderer::rgb(135, 214, 238));
    } else if (statusPage == 5) {
        const MoveInfo* basicMove = findMove(mon.basicMoveId);
        const MoveInfo* specialMove = findMove(mon.specialMoveId);
        PixelRenderer::text(12, 8, Ui::Status::MOVE_INFO, PixelRenderer::rgb(67, 213, 224), 1);
        PixelRenderer::text(18, 30, Ui::Status::BASIC_MOVE, PixelRenderer::rgb(67, 213, 224), 1);
        PixelRenderer::text(18, 48,
                            basicMove ? basicMove->name : Ui::Status::MOVE_UNKNOWN,
                            PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::POWER_FMT, basicMove ? basicMove->power : 0);
        PixelRenderer::text(126, 48, buf, PixelRenderer::rgb(255, 216, 72), 1);
        PixelRenderer::text(18, 66,
                            basicMove ? basicMove->description : Ui::Status::MOVE_UNKNOWN,
                            PixelRenderer::rgb(135, 214, 238), 1);
        PixelRenderer::text(18, 82, Ui::Status::SPECIAL_MOVE, PixelRenderer::rgb(67, 213, 224), 1);
        PixelRenderer::text(18, 100,
                            specialMove ? specialMove->name : Ui::Status::MOVE_UNKNOWN,
                            PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::POWER_FMT, specialMove ? specialMove->power : 0);
        PixelRenderer::text(126, 100, buf, PixelRenderer::rgb(255, 216, 72), 1);
        PixelRenderer::text(18, 118,
                            specialMove ? specialMove->description : Ui::Status::MOVE_UNKNOWN,
                            PixelRenderer::rgb(135, 214, 238), 1);
    } else {
        PixelRenderer::text(12, 8, Ui::Status::EVOLUTION, PixelRenderer::rgb(67, 213, 224), 1);
        if (mon.evolveTo == 0) {
            PixelRenderer::text(18, 52, Ui::Status::NO_EVOLVE, PixelRenderer::rgb(241, 242, 232), 1);
        } else {
            PixelRenderer::text(18, 52, evolutionMethodName(mon.evolveMethod), PixelRenderer::rgb(241, 242, 232), 1);
            if (mon.evolveMethod == EvolutionMethod::LEVEL) {
                snprintf(buf, sizeof(buf), Ui::Status::NEED_LEVEL_FMT, mon.evolveLevel);
                PixelRenderer::text(18, 70, buf, PixelRenderer::rgb(241, 242, 232), 1);
            }
            snprintf(buf, sizeof(buf), Ui::Status::ID_FMT, mon.evolveTo);
            PixelRenderer::text(18, 88, buf, PixelRenderer::rgb(135, 214, 238), 1);
        }
        PixelRenderer::text(126, 8, Ui::Status::SOURCE_INFO, PixelRenderer::rgb(67, 213, 224), 1);
        snprintf(buf, sizeof(buf), Ui::Status::SOURCE_FMT, originName(activeMon.origin));
        PixelRenderer::text(126, 52, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::MET_AT_FMT, activeMon.caughtAt);
        PixelRenderer::text(126, 70, buf, PixelRenderer::rgb(241, 242, 232), 1);
    }

    renderPageIndicator(statusPage, STATUS_PAGE_COUNT);
}

void MenuScene::renderEggStatusPage() {
    auto& c = PixelRenderer::canvas();

    statusPage = 0;
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(10, 14, 20));
    c.fillEllipse(44, 63, 19, 28, PixelRenderer::rgb(240, 232, 184));
    c.fillEllipse(44, 64, 14, 21, PixelRenderer::rgb(255, 248, 214));
    c.drawEllipse(44, 63, 19, 28, PixelRenderer::rgb(109, 92, 62));
    c.fillCircle(36, 57, 3, PixelRenderer::rgb(255, 145, 67));
    c.fillCircle(52, 70, 3, PixelRenderer::rgb(71, 169, 226));
    PixelRenderer::text(88, 38, Ui::Status::EGG_TITLE, PixelRenderer::rgb(241, 242, 232), 1);
    c.drawFastHLine(84, 65, 122, PixelRenderer::rgb(55, 63, 76));
    PixelRenderer::text(88, 78, Ui::Status::EGG_STATE, PixelRenderer::rgb(255, 216, 72), 1);
}

void MenuScene::renderBagPage() {
    auto& c = PixelRenderer::canvas();
    BagRow rows[BAG_ITEM_COUNT];
    uint8_t visibleCount = collectVisibleBagRows(rows, BAG_ITEM_COUNT);
    if (bagCursor >= visibleCount) bagCursor = visibleCount - 1;

    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(10, 14, 20));
    renderSplitList(rows, visibleCount);
    renderBagDetail(rows[bagCursor]);
    if (bagConfirmOpen) renderBagConfirmPopup();
    renderToast();
}

void MenuScene::renderBagConfirmPopup() {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 25;
    static constexpr int POP_Y = 32;
    static constexpr int POP_W = 190;
    static constexpr int POP_H = 72;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(241, 242, 232));

    char line[48];
    snprintf(line, sizeof(line), Ui::Bag::USE_CONFIRM_FMT, GameEngine::ins().activeSpecies().name);
    int lineW = textPixelWidth(line);
    int lineX = POP_X + (POP_W - lineW) / 2;
    if (lineX < POP_X + 6) lineX = POP_X + 6;
    PixelRenderer::text(lineX, POP_Y + 17, line, PixelRenderer::rgb(241, 242, 232), 1);

    uint16_t yesColor = bagConfirmYes ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    uint16_t noColor = !bagConfirmYes ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    PixelRenderer::text(POP_X + 60, POP_Y + 49, Ui::Bag::YES, yesColor, 1);
    PixelRenderer::text(POP_X + 130, POP_Y + 49, Ui::Bag::NO, noColor, 1);
}

void MenuScene::renderComputerPage() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0x0000);

    static constexpr int ROW_X = 8;
    static constexpr int TEXT_X = 22;
    static constexpr int ROW_Y = 18;
    static constexpr int ROW_H = 30;
    static constexpr int SEP_Y_OFFSET = 23;
    static constexpr int SEP_W = Hal::DISPLAY_W - ROW_X * 2;

    for (uint8_t i = 0; i < COMPUTER_ITEM_COUNT; ++i) {
        int y = ROW_Y + i * ROW_H;
        bool selected = i == computerCursor;
        uint16_t color = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) {
            c.fillRect(ROW_X, y - 1, 4, 18, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(TEXT_X, y, Ui::Computer::ITEMS[i], color, 1);
        if (i == 1) {
            char countBuf[16];
            snprintf(countBuf, sizeof(countBuf), Ui::Computer::STORAGE_COUNT_FMT,
                     GameEngine::ins().gameState().storageCount);
            PixelRenderer::text(Hal::DISPLAY_W - textPixelWidth(countBuf) - 12, y, countBuf,
                                selected ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                                1);
        }
        if (i + 1 < COMPUTER_ITEM_COUNT) {
            c.drawFastHLine(ROW_X, y + SEP_Y_OFFSET, SEP_W, PixelRenderer::rgb(55, 63, 76));
        }
    }
}

void MenuScene::renderStoragePage() {
    auto& c = PixelRenderer::canvas();
    const auto& state = GameEngine::ins().gameState();
    uint8_t rowCount = state.storageCount + 1;
    if (storageCursor >= rowCount) storageCursor = rowCount - 1;

    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0x0000);

    static constexpr int ROW_X = 8;
    static constexpr int TEXT_X = 18;
    static constexpr int LIST_Y_START = 6;
    static constexpr int LIST_BOTTOM = Hal::DISPLAY_H - 6;
    static constexpr int VISIBLE_H = LIST_BOTTOM - LIST_Y_START;
    static constexpr int ROW_H = 24;
    static constexpr int SEP_W = Hal::DISPLAY_W - ROW_X * 2;

    int totalH = rowCount * ROW_H;
    int maxScroll = (totalH > VISIBLE_H) ? (totalH - VISIBLE_H) : 0;
    int targetScroll = storageCursor * ROW_H - VISIBLE_H / 2 + ROW_H / 2;
    if (targetScroll < 0) targetScroll = 0;
    if (targetScroll > maxScroll) targetScroll = maxScroll;

    float diff = (float)targetScroll - storageScroll;
    if (fabsf(diff) < 0.5f) {
        storageScroll = (float)targetScroll;
    } else {
        storageScroll += diff * 0.25f;
    }

    if (state.storageCount == 0) {
        PixelRenderer::text(74, 44, Ui::Computer::STORAGE_EMPTY, 0x7BEF, 1);
    }

    c.setClipRect(0, LIST_Y_START, Hal::DISPLAY_W, VISIBLE_H);
    for (uint8_t i = 0; i < rowCount; ++i) {
        int y = LIST_Y_START + i * ROW_H - (int)storageScroll;
        if (y + ROW_H < LIST_Y_START || y > LIST_BOTTOM) continue;
        bool selected = i == storageCursor;
        uint16_t color = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) {
            c.fillRect(ROW_X, y + 2, 4, 18, PixelRenderer::rgb(255, 216, 72));
        }

        if (i < state.storageCount) {
            const Game::MonsterRuntime& mon = state.storage[i];
            const Species& species = GameEngine::ins().speciesFor(mon);
            PixelRenderer::text(TEXT_X, y + 3, species.name, color, 1);

            char buf[20];
            snprintf(buf, sizeof(buf), Ui::Common::LEVEL_FMT, mon.level);
            PixelRenderer::text(112, y + 3, buf, selected ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF, 1);
            snprintf(buf, sizeof(buf), "%u/%u", mon.hpCur, mon.hpMax);
            PixelRenderer::text(158, y + 3, buf, selected ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF, 1);
        } else {
            PixelRenderer::text(TEXT_X, y + 3, Ui::BACK, color, 1);
        }

        if (i + 1 < rowCount) {
            c.drawFastHLine(ROW_X, y + ROW_H - 1, SEP_W, PixelRenderer::rgb(55, 63, 76));
        }
    }
    c.clearClipRect();
}

void MenuScene::renderDebugPage() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0x0000);

    static constexpr int ROW_X = 8;
    static constexpr int TEXT_X = 22;
    static constexpr int ROW_Y = 12;
    static constexpr int ROW_H = 28;
    static constexpr int SEP_Y_OFFSET = 22;
    static constexpr int SEP_W = Hal::DISPLAY_W - ROW_X * 2;

    for (uint8_t i = 0; i < DEBUG_ITEM_COUNT; ++i) {
        int y = ROW_Y + i * ROW_H;
        bool selected = i == debugCursor;
        uint16_t color = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) {
            c.fillRect(ROW_X, y - 1, 4, 18, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(TEXT_X, y, Ui::Debug::ITEMS[i], color, 1);
        if (i == 1) {
            const auto& state = GameEngine::ins().gameState();
            uint16_t speciesId = state.teamCount > 0 ? state.team[0].speciesId : 0;
            char idBuf[20];
            snprintf(idBuf, sizeof(idBuf), Ui::Debug::CURRENT_ID_FMT, speciesId);
            PixelRenderer::text(Hal::DISPLAY_W - textPixelWidth(idBuf) - 12, y, idBuf,
                                selected ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                                1);
        }
        if (i + 1 < DEBUG_ITEM_COUNT) {
            c.drawFastHLine(ROW_X, y + SEP_Y_OFFSET, SEP_W, PixelRenderer::rgb(55, 63, 76));
        }
    }

    if (debugSwitchOpen) renderDebugSwitchPopup();
    renderToast();
}

void MenuScene::openDebugSwitchPopup() {
    const auto& state = GameEngine::ins().gameState();
    uint16_t speciesId = state.teamCount > 0 ? state.team[0].speciesId : 1;
    if (speciesId > 999) speciesId = 999;
    debugSwitchDigits[0] = (speciesId / 100) % 10;
    debugSwitchDigits[1] = (speciesId / 10) % 10;
    debugSwitchDigits[2] = speciesId % 10;
    debugSwitchFocus = 0;
    debugSwitchOpen = true;
}

uint16_t MenuScene::debugSwitchTargetId() const {
    return (uint16_t)(debugSwitchDigits[0] * 100 + debugSwitchDigits[1] * 10 + debugSwitchDigits[2]);
}

void MenuScene::renderDebugSwitchPopup() {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 34;
    static constexpr int POP_Y = 26;
    static constexpr int POP_W = 172;
    static constexpr int POP_H = 84;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(18, 22, 30));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(241, 242, 232));

    PixelRenderer::text(POP_X + 14, POP_Y + 10, Ui::Debug::SWITCH_MON,
                        PixelRenderer::rgb(67, 213, 224), 1);
    PixelRenderer::text(POP_X + 14, POP_Y + 32, Ui::Debug::INPUT_ID,
                        PixelRenderer::rgb(241, 242, 232), 1);

    char digitBuf[2] = {'0', '\0'};
    for (uint8_t i = 0; i < 3; ++i) {
        int x = POP_X + 68 + i * 18;
        bool selected = debugSwitchFocus == i;
        if (selected) {
            c.drawRect(x - 3, POP_Y + 29, 14, 18, PixelRenderer::rgb(255, 216, 72));
        }
        digitBuf[0] = (char)('0' + debugSwitchDigits[i]);
        PixelRenderer::text(x, POP_Y + 32, digitBuf,
                            selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232),
                            1);
    }

    uint16_t yesColor = debugSwitchFocus == 3 ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
    uint16_t cancelColor = debugSwitchFocus == 4 ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
    if (debugSwitchFocus == 3) c.fillRect(POP_X + 38, POP_Y + 60, 4, 16, PixelRenderer::rgb(255, 216, 72));
    if (debugSwitchFocus == 4) c.fillRect(POP_X + 94, POP_Y + 60, 4, 16, PixelRenderer::rgb(255, 216, 72));
    PixelRenderer::text(POP_X + 48, POP_Y + 60, Ui::Debug::YES, yesColor, 1);
    PixelRenderer::text(POP_X + 104, POP_Y + 60, Ui::Debug::CANCEL, cancelColor, 1);
}

uint8_t MenuScene::collectVisibleBagRows(BagRow* rows, uint8_t maxRows) const {
    if (!rows || maxRows == 0) return 0;
    struct SourceRow {
        uint8_t count;
        uint16_t color;
    };
    SourceRow sources[] = {
        {GameEngine::ins().ballCount(), PixelRenderer::rgb(239, 85, 85)},
        {GameEngine::ins().greatBallCount(), PixelRenderer::rgb(67, 124, 230)},
        {GameEngine::ins().foodCount(), PixelRenderer::rgb(247, 167, 74)},
        {GameEngine::ins().potionCount(), PixelRenderer::rgb(92, 222, 112)},
        {GameEngine::ins().superPotionCount(), PixelRenderer::rgb(135, 214, 238)},
        {GameEngine::ins().antidoteCount(), PixelRenderer::rgb(174, 115, 228)},
        {GameEngine::ins().candyCount(), PixelRenderer::rgb(255, 216, 72)},
        {1, PixelRenderer::rgb(123, 125, 123)},
    };

    uint8_t count = 0;
    for (uint8_t source = 0; source < BAG_ITEM_COUNT - 1 && count < maxRows; ++source) {
        if (sources[source].count == 0) continue;
        rows[count++] = {source, sources[source].count, sources[source].color};
    }
    if (count < maxRows) {
        rows[count++] = {BAG_ITEM_COUNT - 1, sources[BAG_ITEM_COUNT - 1].count,
                         sources[BAG_ITEM_COUNT - 1].color};
    }
    return count;
}

void MenuScene::renderSplitList(const BagRow* rows, uint8_t count) {
    if (!rows || count == 0) return;
    auto& c = PixelRenderer::canvas();
    static constexpr int LEFT_W = 78;
    static constexpr int LIST_Y_START = 6;
    static constexpr int LIST_BOTTOM = Hal::DISPLAY_H - 6;
    static constexpr int VISIBLE_H = LIST_BOTTOM - LIST_Y_START;
    static constexpr int ROW_H = 22;
    static constexpr int TEXT_X = 12;

    c.drawFastVLine(LEFT_W, 6, Hal::DISPLAY_H - 12, 0x7BEF);

    int totalH = count * ROW_H;
    int maxScroll = (totalH > VISIBLE_H) ? (totalH - VISIBLE_H) : 0;
    int targetScroll = bagCursor * ROW_H - VISIBLE_H / 2 + ROW_H / 2;
    if (targetScroll < 0) targetScroll = 0;
    if (targetScroll > maxScroll) targetScroll = maxScroll;

    float diff = (float)targetScroll - bagScroll;
    if (fabsf(diff) < 0.5f) {
        bagScroll = (float)targetScroll;
    } else {
        bagScroll += diff * 0.25f;
    }

    c.setClipRect(0, LIST_Y_START, LEFT_W - 1, VISIBLE_H);
    for (uint8_t i = 0; i < count; ++i) {
        int y = LIST_Y_START + i * ROW_H - (int)bagScroll;
        if (y + ROW_H < LIST_Y_START || y > LIST_BOTTOM) continue;
        bool selected = i == bagCursor;
        uint16_t color = selected ? 0xFFE0 : 0xFFFF;
        int textY = y + (ROW_H - 16) / 2;
        PixelRenderer::text(TEXT_X, textY, Ui::Bag::NAMES[rows[i].source], color, 1);
    }
    c.clearClipRect();
}

void MenuScene::renderBagDetail(const BagRow& row) {
    auto& c = PixelRenderer::canvas();
    static constexpr int LEFT_W = 78;
    static constexpr int RIGHT_X = LEFT_W + 8;
    static constexpr int ICON_W = 58;
    static constexpr int ICON_H = 28;
    int iconX = RIGHT_X + 5;
    int iconY = 8;

    if (row.source < BAG_ITEM_COUNT - 1) {
        int textX = iconX + ICON_W + 5;
        int nameY = iconY + 2;
        int storageY = nameY + 16;

        c.fillRect(iconX, iconY, ICON_W, ICON_H, row.color);
        c.fillRect(iconX + 18, iconY + 8, 22, 12, PixelRenderer::rgb(241, 242, 232));

        PixelRenderer::text(textX, nameY, Ui::Bag::NAMES[row.source], 0xFFFF, 1);
        char buf[16];
        snprintf(buf, sizeof(buf), Ui::Bag::COUNT_X_FMT, row.count);
        PixelRenderer::text(textX, storageY, buf,
                            row.count > 0 ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                            1);

        int descX = RIGHT_X + 2;
        int descY = iconY + ICON_H + 12;
        int descW = Hal::DISPLAY_W - descX - 2;
        const char* descLines[3] = {
            Ui::Bag::DESCS[row.source][0],
            Ui::Bag::DESCS[row.source][1],
            Ui::Bag::DESCS[row.source][2],
        };
        renderScrollableDescription(descLines, 3, descX, descY, descW,
                                    row.count > 0 ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                                    0x200 + row.source);
    } else {
        PixelRenderer::text(RIGHT_X + 20, 80, Ui::Bag::DESCS[row.source][0], 0x7BEF, 1);
    }
}

void MenuScene::updateDescriptionScroll(int scrollKey, int maxScroll) {
    if (scrollKey != descScrollKey) {
        descScrollKey = scrollKey;
        descScroll = 0.0f;
        descScrollLastMs = Hal::ins().millis();
    }

    if (maxScroll <= 0) {
        descScroll = 0.0f;
        descScrollLastMs = Hal::ins().millis();
        return;
    }

    uint32_t now = Hal::ins().millis();
    uint32_t dt = now - descScrollLastMs;
    if (dt > 120) dt = 120;
    descScrollLastMs = now;
    descScroll += 28.0f * ((float)dt / 1000.0f);
    if (descScroll > (float)(maxScroll + 18)) descScroll = 0.0f;
}

void MenuScene::renderScrollableDescription(const char* const* lines, int lineCount,
                                            int x, int y, int w, uint16_t color,
                                            int scrollKey) {
    if (lineCount <= 0 || w <= 0) return;

    int maxLineW = 0;
    for (int i = 0; i < lineCount; ++i) {
        int lineW = textPixelWidth(lines[i]);
        if (lineW > maxLineW) maxLineW = lineW;
    }
    int maxScroll = maxLineW > w ? maxLineW - w + 4 : 0;
    updateDescriptionScroll(scrollKey, maxScroll);

    auto& c = PixelRenderer::canvas();
    c.setClipRect(x, y - 1, w, lineCount * 16 + 2);
    for (int i = 0; i < lineCount; ++i) {
        PixelRenderer::text(x - (int)descScroll, y + i * 16, lines[i], color, 1);
    }
    c.clearClipRect();
}

void MenuScene::renderPageIndicator(uint8_t page, uint8_t count) {
    auto& c = PixelRenderer::canvas();
    const int dotW = 4;
    const int gap = 4;
    int totalH = count * 5 + (count - 1) * gap;
    int x = Hal::DISPLAY_W - 10;
    int y = (Hal::DISPLAY_H - totalH) / 2;
    for (uint8_t i = 0; i < count; ++i) {
        uint16_t color = i == page ? PixelRenderer::rgb(67, 213, 224) : PixelRenderer::rgb(92, 98, 110);
        c.fillRect(x, y + i * (5 + gap), dotW, 5, color);
    }
}
