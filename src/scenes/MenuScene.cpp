#include "scenes/MenuScene.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "assets/MenuAssets.h"
#include "assets/PokemonSprites.h"
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

int statusPageContentHeight(uint8_t page) {
    switch (page) {
    case 0: return 129;
    case 1: return 128;
    case 2: return 192;
    case 3: return 123;
    default: return 123;
    }
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
    case 2: return POKEBUG_ICON_BOX;
    case 3: return POKEBUG_ICON_EXPLORE;
    case 5: return POKEBUG_ICON_SOCIAL;
    case 6: return POKEBUG_ICON_SETTINGS;
    case 7: return POKEBUG_ICON_DEBUG;
    case 8: return POKEBUG_ICON_BACK;
    case 1: // room has no matching PokeBug icon; keep fallback block.
    case 4: // shop has no matching PokeBug icon; keep fallback block.
    default: return -1;
    }
}

uint8_t bagItemCount(uint8_t sourceIndex) {
    switch (sourceIndex) {
    case 0: return GameEngine::ins().ballCount();
    case 1: return GameEngine::ins().greatBallCount();
    case 2: return GameEngine::ins().potionCount();
    case 3: return GameEngine::ins().superPotionCount();
    case 4: return GameEngine::ins().antidoteCount();
    case 5: return GameEngine::ins().candyCount();
    case 6: return 1;
    default: return 0;
    }
}

uint8_t bagVisibleItemCount() {
    uint8_t count = 1; // Back is always visible.
    for (uint8_t i = 0; i < 6; ++i) {
        if (bagItemCount(i) > 0) ++count;
    }
    return count;
}

uint8_t bagSourceIndexForVisible(uint8_t visibleIndex) {
    uint8_t visible = 0;
    for (uint8_t source = 0; source < 6; ++source) {
        if (bagItemCount(source) == 0) continue;
        if (visible == visibleIndex) return source;
        ++visible;
    }
    return 6;
}

uint16_t foodColor(uint8_t foodIndex) {
    switch (foodIndex) {
    case 1: return PixelRenderer::rgb(255, 138, 112);
    default: return PixelRenderer::rgb(245, 180, 87);
    }
}

uint16_t typeColor(TypeId type) {
    switch (type) {
    case TypeId::FIRE: return PixelRenderer::rgb(239, 91, 67);
    case TypeId::WATER: return PixelRenderer::rgb(67, 145, 224);
    case TypeId::GRASS: return PixelRenderer::rgb(92, 188, 92);
    case TypeId::ELECTRIC: return PixelRenderer::rgb(245, 204, 67);
    case TypeId::ICE: return PixelRenderer::rgb(115, 206, 218);
    case TypeId::FIGHTING: return PixelRenderer::rgb(190, 85, 64);
    case TypeId::POISON: return PixelRenderer::rgb(155, 95, 196);
    case TypeId::GROUND: return PixelRenderer::rgb(184, 142, 83);
    case TypeId::FLYING: return PixelRenderer::rgb(135, 166, 220);
    case TypeId::PSYCHIC: return PixelRenderer::rgb(226, 98, 168);
    case TypeId::BUG: return PixelRenderer::rgb(142, 185, 72);
    case TypeId::ROCK: return PixelRenderer::rgb(160, 136, 84);
    case TypeId::GHOST: return PixelRenderer::rgb(105, 86, 160);
    case TypeId::DRAGON: return PixelRenderer::rgb(88, 118, 210);
    case TypeId::DARK: return PixelRenderer::rgb(86, 76, 70);
    case TypeId::STEEL: return PixelRenderer::rgb(144, 158, 172);
    case TypeId::FAIRY: return PixelRenderer::rgb(232, 138, 202);
    case TypeId::NORMAL:
    default: return PixelRenderer::rgb(154, 158, 132);
    }
}

const char* abilityName(const Species& species) {
    switch (species.id) {
    case 1:
    case 2:
    case 3: return Ui::Status::ABILITY_OVERGROW;
    case 4:
    case 5:
    case 6: return Ui::Status::ABILITY_BLAZE;
    case 7:
    case 8:
    case 9: return Ui::Status::ABILITY_TORRENT;
    case 25:
    case 26:
    case 172: return Ui::Status::ABILITY_STATIC;
    case 92:
    case 93:
    case 94:
    case 380:
    case 381: return Ui::Status::ABILITY_LEVITATE;
    case 133: return Ui::Status::ABILITY_ADAPTABILITY;
    case 134: return Ui::Status::ABILITY_WATER_ABSORB;
    case 135: return Ui::Status::ABILITY_VOLT_ABSORB;
    case 136: return Ui::Status::ABILITY_FLASH_FIRE;
    case 123:
    case 212: return Ui::Status::ABILITY_TECHNICIAN;
    case 129: return Ui::Status::ABILITY_SWIFT_SWIM;
    case 130: return Ui::Status::ABILITY_INTIMIDATE;
    case 143: return Ui::Status::ABILITY_THICK_FAT;
    case 147:
    case 148: return Ui::Status::ABILITY_SHED_SKIN;
    case 149: return Ui::Status::ABILITY_INNER_FOCUS;
    case 151:
    case 196:
    case 197:
    default: return Ui::Status::ABILITY_SYNCHRONIZE;
    }
}

const char* genderName(const Species& species, const Game::MonsterRuntime& mon) {
    if (species.id == 151) return Ui::Status::GENDER_NONE;
    if (species.id == 380) return Ui::Status::GENDER_FEMALE;
    if (species.id == 381) return Ui::Status::GENDER_MALE;
    return ((mon.ivPacked ^ mon.speciesId) & 1) ? Ui::Status::GENDER_FEMALE : Ui::Status::GENDER_MALE;
}

const char* proficiencyName(uint8_t value) {
    if (value >= 90) return Ui::Status::PROF_FULL;
    if (value >= 60) return Ui::Status::PROF_HIGH;
    if (value >= 25) return Ui::Status::PROF_MID;
    return Ui::Status::PROF_LOW;
}

const char* metAreaName(uint8_t metArea) {
    if (metArea < 3) return Ui::Explore::AREA_ITEMS[metArea];
    return Ui::Status::ORIGIN_UNKNOWN;
}

void drawTypeBadge(int x, int y, TypeId type) {
    auto& c = PixelRenderer::canvas();
    c.fillRect(x, y, 34, 15, typeColor(type));
    c.drawRect(x, y, 34, 15, PixelRenderer::rgb(241, 242, 232));
    PixelRenderer::text(x + 4, y, typeName(type), PixelRenderer::rgb(15, 18, 24), 1);
}

int drawTypeBracket(int x, int y, TypeId type) {
    PixelRenderer::text(x, y, "[", PixelRenderer::rgb(241, 242, 232), 1);
    const char* name = typeName(type);
    PixelRenderer::text(x + 8, y, name, typeColor(type), 1);
    int closeX = x + 8 + textPixelWidth(name);
    PixelRenderer::text(closeX, y, "]", PixelRenderer::rgb(241, 242, 232), 1);
    return closeX + 10;
}

void drawStatusMonsterIcon(const Species& species, int x, int y) {
    auto& c = PixelRenderer::canvas();
    static constexpr float STATUS_ICON_SCALE = 1.2f;
    const PokemonSprites::SpriteFrame* frame = PokemonSprites::findSpeciesSprite(species.id, PokemonSprites::SpriteKind::ICON_0);
    if (frame) {
        uint8_t w = pgm_read_byte(&frame->width);
        uint8_t h = pgm_read_byte(&frame->height);
        int drawX = x - (int)roundf((w * (STATUS_ICON_SCALE - 1.0f)) * 0.5f);
        int drawY = y - (int)roundf((h * (STATUS_ICON_SCALE - 1.0f)) * 0.5f);
        if (PokemonSprites::drawFrameScaled(frame, drawX, drawY, STATUS_ICON_SCALE)) return;
    }
    c.fillRect(x + 6, y + 4, 53, 58, species.colorA);
    c.fillRect(x + 16, y + 16, 34, 36, species.colorB);
}
}

int8_t MenuScene::lastCursor = 0;

void MenuScene::onEnter() {
    resetNavigation();
    statusPage = 0;
    statusMonsterIndex = 0;
    statusFromStorage = false;
    statusScrollKey = -1;
    statusScroll = 0.0f;
    statusScrollLastMs = Hal::ins().millis();
    teamCursor = 0;
    teamActionCursor = 0;
    teamActionOpen = false;
    bagCursor = 0;
    bagConfirmOpen = false;
    bagConfirmYes = true;
    roomCursor = 0;
    foodCursor = visibleFoodIndexOf(GameEngine::ins().selectedFoodIndex());
    foodScroll = 0.0f;
    computerCursor = 0;
    storageCursor = 0;
    storageActionCursor = 0;
    storageActionOpen = false;
    storageReleaseConfirmOpen = false;
    storageReleaseConfirmYes = false;
    storageScroll = 0.0f;
    debugCategory = DebugCategory::ROOT;
    debugCursor = 0;
    debugSwitchOpen = false;
    debugSwitchFocus = 0;
    debugTimeOpen = false;
    debugTimeFocus = 0;
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

void MenuScene::resetNavigation() {
    navDepth = 0;
    viewMode = ViewMode::MENU;
}

void MenuScene::pushView(ViewMode next) {
    if (next == viewMode) return;
    if (navDepth < NAV_STACK_CAP) {
        navStack[navDepth++] = viewMode;
    } else {
        for (uint8_t i = 1; i < NAV_STACK_CAP; ++i) {
            navStack[i - 1] = navStack[i];
        }
        navStack[NAV_STACK_CAP - 1] = viewMode;
    }
    viewMode = next;
}

void MenuScene::popView() {
    teamActionOpen = false;
    storageActionOpen = false;
    storageReleaseConfirmOpen = false;
    bagConfirmOpen = false;
    debugSwitchOpen = false;
    debugTimeOpen = false;

    if (navDepth > 0) {
        viewMode = navStack[--navDepth];
    } else {
        viewMode = ViewMode::MENU;
    }
    if (viewMode == ViewMode::MENU) {
        statusFromStorage = false;
    }
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
            if (viewMode == ViewMode::STORAGE && storageReleaseConfirmOpen) {
                storageReleaseConfirmOpen = false;
                return true;
            }
            if (viewMode == ViewMode::STORAGE && storageActionOpen) {
                storageActionOpen = false;
                storageReleaseConfirmOpen = false;
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
            if (viewMode == ViewMode::DEBUG && debugTimeOpen) {
                debugTimeOpen = false;
                return true;
            }
            if (viewMode == ViewMode::DEBUG && debugCategory != DebugCategory::ROOT) {
                debugCategory = DebugCategory::ROOT;
                debugCursor = 0;
                return true;
            }
            popView();
            return true;
        }
        if (viewMode == ViewMode::ROOM && event.btn == 1 && event.action == BtnAction::PRESSED) {
            roomCursor = (roomCursor + 1) % ROOM_ITEM_COUNT;
            return true;
        }
        if (viewMode == ViewMode::ROOM && event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (roomCursor == 0) {
                pushView(ViewMode::FOOD);
                foodCursor = visibleFoodIndexOf(GameEngine::ins().selectedFoodIndex());
                foodScroll = 0.0f;
            } else if (roomCursor == ROOM_ITEM_COUNT - 1) {
                popView();
            }
            return true;
        }
        if (viewMode == ViewMode::FOOD && event.btn == 1 && event.action == BtnAction::PRESSED) {
            foodCursor = (foodCursor + 1) % FOOD_ITEM_COUNT;
            if (foodCursor == 0) foodScroll = 0.0f;
            return true;
        }
        if (viewMode == ViewMode::FOOD && event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (isFoodBackIndex(foodCursor)) {
                popView();
            } else if (GameEngine::ins().selectFood(foodCursor)) {
                toast = Ui::Room::FOOD_SELECTED;
                toastUntil = Hal::ins().millis() + 900;
            } else {
                toast = Ui::Room::FOOD_NO_STOCK;
                toastUntil = Hal::ins().millis() + 900;
            }
            return true;
        }
        if (viewMode == ViewMode::TEAM) {
            uint8_t teamCount = GameEngine::ins().gameState().teamCount;
            uint8_t rowCount = teamCount + 1;
            if (teamCursor >= rowCount) teamCursor = 0;
            if (event.btn == 1 && event.action == BtnAction::PRESSED) {
                if (teamActionOpen) {
                    teamActionCursor = (teamActionCursor + 1) % TEAM_ACTION_COUNT;
                } else {
                    teamCursor = (teamCursor + 1) % rowCount;
                }
                return true;
            }
            if (event.btn == 0 && event.action == BtnAction::PRESSED) {
                if (teamCursor >= teamCount) {
                    teamActionOpen = false;
                    popView();
                    return true;
                }
                if (!teamActionOpen) {
                    teamActionOpen = true;
                    teamActionCursor = 0;
                    return true;
                }

                if (teamActionCursor == 0) {
                    statusMonsterIndex = teamCursor;
                    statusFromStorage = false;
                    statusPage = 0;
                    statusScrollKey = -1;
                    statusScroll = 0.0f;
                    teamActionOpen = false;
                    pushView(ViewMode::STATUS);
                } else if (teamActionCursor == 1) {
                    if (GameEngine::ins().moveTeamMemberToFront(teamCursor)) {
                        teamCursor = 0;
                        statusMonsterIndex = 0;
                        toast = Ui::Menu::SWITCH_TOAST;
                        toastUntil = Hal::ins().millis() + 1100;
                    }
                    teamActionOpen = false;
                } else if (teamActionCursor == 2) {
                    bool boxFull = GameEngine::ins().gameState().storageCount >= Game::STORAGE_CAP;
                    bool lastMonster = GameEngine::ins().gameState().teamCount <= 1;
                    if (GameEngine::ins().depositTeamMemberToStorage(teamCursor)) {
                        uint8_t newTeamCount = GameEngine::ins().gameState().teamCount;
                        if (newTeamCount == 0) {
                            teamCursor = 0;
                        } else if (teamCursor >= newTeamCount) {
                            teamCursor = newTeamCount - 1;
                        }
                        statusMonsterIndex = 0;
                        toast = Ui::Team::DEPOSIT_TOAST;
                    } else if (boxFull) {
                        toast = Ui::Team::DEPOSIT_FULL_TOAST;
                    } else if (lastMonster) {
                        toast = Ui::Team::DEPOSIT_LAST_TOAST;
                    }
                    toastUntil = Hal::ins().millis() + 1100;
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
            statusScrollKey = -1;
            statusScroll = 0.0f;
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
                    if (bagConfirmSource == 2) {
                        toast = GameEngine::ins().usePotion() ? Ui::Bag::USED_POTION : Ui::Bag::HP_FULL;
                        toastUntil = Hal::ins().millis() + 1100;
                    } else if (bagConfirmSource == 3) {
                        toast = GameEngine::ins().useSuperPotion() ? Ui::Bag::USED_SUPER_POTION : Ui::Bag::HP_FULL;
                        toastUntil = Hal::ins().millis() + 1100;
                    }
                }
                bagConfirmOpen = false;
                return true;
            }
            uint8_t source = bagSourceIndexForVisible(bagCursor);
            if (source == 2) {
                bagConfirmSource = source;
                bagConfirmYes = true;
                bagConfirmOpen = true;
            } else if (source == 3) {
                bagConfirmSource = source;
                bagConfirmYes = true;
                bagConfirmOpen = true;
            } else if (source == 6) {
                popView();
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
                pushView(ViewMode::STORAGE);
                storageCursor = 0;
                storageActionCursor = 0;
                storageActionOpen = false;
                storageReleaseConfirmOpen = false;
                storageScroll = 0.0f;
            } else {
                popView();
            }
            return true;
        }
        if (viewMode == ViewMode::STORAGE && event.btn == 1 && event.action == BtnAction::PRESSED) {
            if (storageReleaseConfirmOpen) {
                storageReleaseConfirmYes = !storageReleaseConfirmYes;
                return true;
            }
            if (storageActionOpen) {
                storageActionCursor = (storageActionCursor + 1) % STORAGE_ACTION_COUNT;
                return true;
            }
            uint8_t count = GameEngine::ins().gameState().storageCount + 1;
            storageCursor = (storageCursor + 1) % count;
            if (storageCursor == 0) storageScroll = 0.0f;
            return true;
        }
        if (viewMode == ViewMode::STORAGE && event.btn == 0 && event.action == BtnAction::PRESSED) {
            uint8_t count = GameEngine::ins().gameState().storageCount + 1;
            if (storageReleaseConfirmOpen) {
                if (storageReleaseConfirmYes) {
                    if (GameEngine::ins().releaseStorageMember(storageCursor)) {
                        uint8_t newCount = GameEngine::ins().gameState().storageCount;
                        if (newCount == 0) {
                            storageCursor = 0;
                        } else if (storageCursor >= newCount) {
                            storageCursor = newCount - 1;
                        }
                        toast = Ui::Storage::RELEASE_TOAST;
                        toastUntil = Hal::ins().millis() + 1100;
                    }
                    storageActionOpen = false;
                }
                storageReleaseConfirmOpen = false;
                return true;
            }
            if (storageActionOpen) {
                if (storageCursor >= GameEngine::ins().gameState().storageCount) {
                    storageActionOpen = false;
                    return true;
                }
                if (storageActionCursor == 0) {
                    statusMonsterIndex = storageCursor;
                    statusFromStorage = true;
                    statusPage = 0;
                    statusScrollKey = -1;
                    statusScroll = 0.0f;
                    storageActionOpen = false;
                    pushView(ViewMode::STATUS);
                } else if (storageActionCursor == 1) {
                    if (GameEngine::ins().gameState().teamCount >= Game::TEAM_CAP) {
                        toast = Ui::Storage::TEAM_FULL_TOAST;
                        toastUntil = Hal::ins().millis() + 1100;
                        return true;
                    }
                    if (GameEngine::ins().withdrawStorageMemberToTeam(storageCursor)) {
                        uint8_t newCount = GameEngine::ins().gameState().storageCount;
                        if (newCount == 0) {
                            storageCursor = 0;
                        } else if (storageCursor >= newCount) {
                            storageCursor = newCount - 1;
                        }
                        toast = Ui::Storage::WITHDRAW_TOAST;
                        toastUntil = Hal::ins().millis() + 1100;
                        storageActionOpen = false;
                    }
                } else if (storageActionCursor == 2) {
                    storageReleaseConfirmOpen = true;
                    storageReleaseConfirmYes = false;
                } else {
                    storageActionOpen = false;
                }
                return true;
            }
            if (storageCursor >= count - 1) {
                popView();
                storageActionOpen = false;
                storageReleaseConfirmOpen = false;
            } else {
                storageActionOpen = true;
                storageActionCursor = 0;
                storageReleaseConfirmOpen = false;
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
        if (viewMode == ViewMode::DEBUG && debugTimeOpen && event.btn == 1 && event.action == BtnAction::PRESSED) {
            debugTimeFocus = (debugTimeFocus + 1) % DEBUG_TIME_FOCUS_COUNT;
            return true;
        }
        if (viewMode == ViewMode::DEBUG && debugTimeOpen && event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (debugTimeFocus < 4) {
                incrementDebugTimeDigit();
            } else if (debugTimeFocus == 4) {
                GameEngine::ins().debugAdvanceToTimeOfDay(debugTimeTargetMinutes());
                debugTimeOpen = false;
                toast = Ui::Debug::TIME_SET;
                toastUntil = Hal::ins().millis() + 1100;
            } else {
                debugTimeOpen = false;
            }
            return true;
        }
        if (viewMode == ViewMode::DEBUG && event.btn == 1 && event.action == BtnAction::PRESSED) {
            debugCursor = (debugCursor + 1) % debugItemCount();
            return true;
        }
        if (viewMode == ViewMode::DEBUG && event.btn == 0 && event.action == BtnAction::PRESSED) {
            handleDebugAction();
            return true;
        }
        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            popView();
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
        pushView(ViewMode::TEAM);
        statusFromStorage = false;
        teamCursor = 0;
        teamActionCursor = 0;
        teamActionOpen = false;
        return true;
    case ITEM_ROOM:
        pushView(ViewMode::ROOM);
        roomCursor = 0;
        return true;
    case ITEM_BAG:
        pushView(ViewMode::BAG);
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
        pushView(ViewMode::COMPUTER);
        computerCursor = 0;
        return true;
    case ITEM_SETTINGS:
        GameEngine::ins().requestScene(SceneID::SETTINGS);
        return true;
    case ITEM_DEBUG:
        pushView(ViewMode::DEBUG);
        debugCategory = DebugCategory::ROOT;
        debugCursor = 0;
        debugSwitchOpen = false;
        debugTimeOpen = false;
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
    if (viewMode == ViewMode::ROOM) {
        renderRoomPage();
        return;
    }
    if (viewMode == ViewMode::FOOD) {
        renderFoodPage();
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
    uint8_t rowCount = state.teamCount + 1;
    if (teamCursor >= rowCount) teamCursor = 0;

    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(10, 14, 20));
    PixelRenderer::text(8, 6, Ui::TEAM, PixelRenderer::rgb(67, 213, 224), 1);

    static constexpr int ROW_X = 8;
    static constexpr int ROW_Y = 24;
    static constexpr int ROW_W = Hal::DISPLAY_W - ROW_X * 2;
    static constexpr int ROW_H = 34;
    static constexpr int ROW_GAP = 5;
    for (uint8_t i = 0; i < rowCount; ++i) {
        int y = ROW_Y + i * (ROW_H + ROW_GAP);
        bool selected = i == teamCursor;
        if (i >= state.teamCount) {
            uint16_t color = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
            if (selected) c.fillRect(ROW_X, y + 2, 4, 18, PixelRenderer::rgb(255, 216, 72));
            PixelRenderer::text(ROW_X + 16, y + 3, Ui::BACK, color, 1);
            c.drawFastHLine(ROW_X, y + 24, ROW_W, PixelRenderer::rgb(55, 63, 76));
            continue;
        }

        const Game::MonsterRuntime& mon = state.team[i];
        const Species& species = GameEngine::ins().speciesFor(mon);
        uint16_t border = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(72, 83, 98);
        c.drawRect(ROW_X, y, ROW_W, ROW_H, border);
        if (selected) c.fillRect(ROW_X, y, 4, ROW_H, PixelRenderer::rgb(255, 216, 72));

        PixelRenderer::text(ROW_X + 10, y + 4, species.name, PixelRenderer::rgb(241, 242, 232), 1);

        uint8_t hpPct = mon.hpMax > 0 ? (uint8_t)((uint32_t)mon.hpCur * 100 / mon.hpMax) : 0;
        uint16_t hpColor = hpPct > 50 ? PixelRenderer::rgb(92, 222, 112) :
                           (hpPct > 20 ? PixelRenderer::rgb(255, 216, 72) :
                            PixelRenderer::rgb(239, 85, 85));
        int barX = ROW_X + 112;
        int barY = y + 7;
        int barW = ROW_W - 124;
        c.fillRect(barX, barY, barW, 8, PixelRenderer::rgb(82, 87, 95));
        int fillW = (barW * hpPct) / 100;
        if (fillW > 0) c.fillRect(barX, barY, fillW, 8, hpColor);
        c.drawRect(barX, barY, barW, 8, PixelRenderer::rgb(45, 48, 56));

        char buf[24];
        snprintf(buf, sizeof(buf), Ui::Status::LEVEL_FMT, mon.level);
        PixelRenderer::text(ROW_X + 10, y + 19, buf, PixelRenderer::rgb(135, 214, 238), 1);

        snprintf(buf, sizeof(buf), "%u/%u", mon.hpCur, mon.hpMax);
        PixelRenderer::text(ROW_X + 112, y + 19, buf, PixelRenderer::rgb(241, 242, 232), 1);
        if (i == 0) {
            PixelRenderer::text(ROW_X + ROW_W - 44, y + 19, Ui::Team::FIRST_BADGE,
                                PixelRenderer::rgb(255, 216, 72), 1);
        }
    }

    if (teamActionOpen && teamCursor < state.teamCount) renderTeamActionPopup();
    renderToast();
}

void MenuScene::renderTeamActionPopup() {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 142;
    static constexpr int POP_Y = 20;
    static constexpr int POP_W = 78;
    static constexpr int POP_H = 100;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(241, 242, 232));

    for (uint8_t i = 0; i < TEAM_ACTION_COUNT; ++i) {
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
    uint8_t statusCount = statusFromStorage ? state.storageCount : state.teamCount;
    if (statusCount == 0) {
        popView();
        statusMonsterIndex = 0;
        return;
    }
    if (statusMonsterIndex >= statusCount) statusMonsterIndex = 0;
    const Game::MonsterRuntime& activeMon = statusFromStorage
        ? state.storage[statusMonsterIndex]
        : state.team[statusMonsterIndex];
    const Species& mon = GameEngine::ins().speciesFor(activeMon);
    char buf[64];
    if (statusPage >= STATUS_PAGE_COUNT) statusPage = 0;

    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(10, 14, 20));

    int contentH = statusPageContentHeight(statusPage);
    int maxScroll = contentH > Hal::DISPLAY_H ? contentH - Hal::DISPLAY_H : 0;
    int scrollKey = (statusFromStorage ? 0x4000 : 0) | ((int)statusMonsterIndex << 8) | statusPage;
    updateStatusScroll(scrollKey, maxScroll);
    int scrollY = (int)statusScroll;
    auto sy = [scrollY](int y) { return y - scrollY; };

    c.setClipRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H);
    if (statusPage == 0) {
        drawStatusMonsterIcon(mon, 10, sy(19));

        int infoX = 88;
        snprintf(buf, sizeof(buf), Ui::Status::PROFILE_LINE_FMT,
                 mon.name, genderName(mon, activeMon), activeMon.level);
        PixelRenderer::text(infoX, sy(16), buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::TYPE_FMT, typeName(mon.type1), typeName(mon.type2));
        PixelRenderer::text(infoX, sy(38), buf, PixelRenderer::rgb(255, 216, 72), 1);

        PixelRenderer::text(infoX, sy(60), Ui::Status::ABILITY, PixelRenderer::rgb(67, 213, 224), 1);
        PixelRenderer::text(infoX + 42, sy(60), abilityName(mon), PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::NATURE_FMT, natureName(activeMon.nature));
        PixelRenderer::text(infoX, sy(82), buf, PixelRenderer::rgb(241, 242, 232), 1);

        c.drawFastHLine(14, sy(101), 196, PixelRenderer::rgb(55, 63, 76));
        PixelRenderer::text(16, sy(109), Ui::Status::SOURCE_INFO, PixelRenderer::rgb(67, 213, 224), 1);
        if (activeMon.origin == Game::Origin::CAPTURED && activeMon.metArea < 3) {
            PixelRenderer::text(60, sy(109), Ui::Status::SOURCE_AT, PixelRenderer::rgb(241, 242, 232), 1);
            const char* area = metAreaName(activeMon.metArea);
            PixelRenderer::text(84, sy(109), area, PixelRenderer::rgb(255, 216, 72), 1);
            PixelRenderer::text(84 + textPixelWidth(area) + 6, sy(109),
                                Ui::Status::SOURCE_MET,
                                PixelRenderer::rgb(241, 242, 232), 1);
        } else if (activeMon.origin == Game::Origin::HATCHED) {
            PixelRenderer::text(60, sy(109), Ui::Status::SOURCE_HATCHED,
                                PixelRenderer::rgb(255, 216, 72), 1);
        } else if (activeMon.origin == Game::Origin::STARTER) {
            PixelRenderer::text(60, sy(109), Ui::Status::SOURCE_STARTER,
                                PixelRenderer::rgb(255, 216, 72), 1);
        } else {
            PixelRenderer::text(60, sy(109), originName(activeMon.origin),
                                PixelRenderer::rgb(255, 216, 72), 1);
        }
    } else if (statusPage == 1) {
        PixelRenderer::text(12, sy(8), Ui::Status::CURRENT_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        snprintf(buf, sizeof(buf), Ui::Status::HP_FMT, activeMon.hpCur, activeMon.hpMax);
        PixelRenderer::text(14, sy(30), buf, PixelRenderer::rgb(92, 222, 112), 1);
        drawStatRow(128, sy(30), Ui::Status::STAT_SPA, statFor(mon, activeMon, 3), PixelRenderer::rgb(241, 242, 232));
        drawStatRow(14, sy(50), Ui::Status::STAT_ATK, statFor(mon, activeMon, 1), PixelRenderer::rgb(241, 242, 232));
        drawStatRow(128, sy(50), Ui::Status::STAT_SPD, statFor(mon, activeMon, 4), PixelRenderer::rgb(241, 242, 232));
        drawStatRow(14, sy(70), Ui::Status::STAT_DEF, statFor(mon, activeMon, 2), PixelRenderer::rgb(241, 242, 232));
        drawStatRow(128, sy(70), Ui::Status::STAT_SPE, statFor(mon, activeMon, 5), PixelRenderer::rgb(241, 242, 232));

        snprintf(buf, sizeof(buf), Ui::Status::EXP_VALUE_FMT, (unsigned long)activeMon.exp);
        PixelRenderer::text(14, sy(92), buf, PixelRenderer::rgb(241, 242, 232), 1);
        uint32_t expNext = expToNextLevel(mon.growthRate, activeMon.level, activeMon.exp);
        snprintf(buf, sizeof(buf), Ui::Status::EXP_NEXT_FMT, (unsigned long)expNext);
        PixelRenderer::text(128, sy(92), buf, PixelRenderer::rgb(241, 242, 232), 1);

        uint32_t levelExp = minimumExpForLevel(mon.growthRate, activeMon.level);
        uint32_t nextExp = activeMon.level >= Game::LEVEL_MAX
            ? activeMon.exp
            : minimumExpForLevel(mon.growthRate, activeMon.level + 1);
        uint8_t expPct = nextExp > levelExp
            ? (uint8_t)min<uint32_t>(100, (activeMon.exp - levelExp) * 100UL / (nextExp - levelExp))
            : 100;
        uint32_t maxExp = minimumExpForLevel(mon.growthRate, Game::LEVEL_MAX);
        uint8_t totalPct = maxExp > 0
            ? (uint8_t)min<uint32_t>(100, activeMon.exp * 100UL / maxExp)
            : 100;
        static constexpr int BAR_X = 14;
        static constexpr int BAR_Y = 118;
        static constexpr int BAR_W = 186;
        static constexpr int BAR_H = 9;
        int barY = sy(BAR_Y);
        c.fillRoundRect(BAR_X, barY, BAR_W, BAR_H, 2, PixelRenderer::rgb(29, 34, 42));
        int totalW = ((BAR_W - 2) * totalPct) / 100;
        if (totalW > 0) {
            c.fillRoundRect(BAR_X + 1, barY + 1, totalW, BAR_H - 2, 1,
                            PixelRenderer::rgb(112, 118, 126));
        }
        int levelW = ((BAR_W - 2) * expPct) / 100;
        if (levelW > 0) {
            c.fillRoundRect(BAR_X + 1, barY + 1, levelW, BAR_H - 2, 1,
                            PixelRenderer::rgb(255, 216, 72));
        }
        c.drawRoundRect(BAR_X, barY, BAR_W, BAR_H, 2, PixelRenderer::rgb(45, 48, 56));
    } else if (statusPage == 2) {
        const MoveInfo* basicMove = findMove(moveIdForMonster(mon, activeMon, false));
        PixelRenderer::text(12, sy(8), Ui::Status::MOVE_INFO, PixelRenderer::rgb(67, 213, 224), 1);

        TypeId basicType = basicMove ? basicMove->type : TypeId::NORMAL;
        PixelRenderer::text(14, sy(31), Ui::Status::BASIC_MOVE, PixelRenderer::rgb(67, 213, 224), 1);
        int basicNameX = drawTypeBracket(92, sy(31), basicType);
        PixelRenderer::text(basicNameX + 4, sy(31), basicMove ? basicMove->name : Ui::Status::MOVE_UNKNOWN,
                            PixelRenderer::rgb(241, 242, 232), 1);

        auto drawSpecialMove = [&](uint8_t slot, int y) {
            const MoveInfo* specialMove = findMove(specialMoveIdForMonster(activeMon, slot));
            char label[24];
            snprintf(label, sizeof(label), "%s%u", Ui::Status::SPECIAL_MOVE, (unsigned)(slot + 1));
            PixelRenderer::text(14, sy(y), label,
                                specialMove ? PixelRenderer::rgb(67, 213, 224) : PixelRenderer::rgb(156, 164, 176),
                                1);
            if (!specialMove) {
                PixelRenderer::text(104, sy(y), Ui::Status::MOVE_NOT_LEARNED,
                                    PixelRenderer::rgb(156, 164, 176), 1);
                return;
            }
            snprintf(buf, sizeof(buf), Ui::Status::PROFICIENCY_FMT,
                     proficiencyName(activeMon.proficiency));
            PixelRenderer::text(128, sy(y), buf, PixelRenderer::rgb(255, 216, 72), 1);
            int specialNameX = drawTypeBracket(18, sy(y + 20), specialMove->type);
            PixelRenderer::text(specialNameX + 4, sy(y + 20), specialMove->name,
                                PixelRenderer::rgb(241, 242, 232), 1);
            snprintf(buf, sizeof(buf), Ui::Status::POWER_FMT, specialMove->power);
            PixelRenderer::text(24, sy(y + 44), buf, PixelRenderer::rgb(255, 216, 72), 1);
            PixelRenderer::text(88, sy(y + 44),
                                specialMove->description,
                                PixelRenderer::rgb(135, 214, 238), 1);
        };
        drawSpecialMove(0, 58);
        drawSpecialMove(1, 128);
    } else if (statusPage == 3) {
        PixelRenderer::text(12, sy(8), Ui::Status::EFFORT_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        snprintf(buf, sizeof(buf), Ui::Status::TOTAL_FMT, Game::evTotal(activeMon.ev), Game::EV_TOTAL_MAX);
        drawInfoRow(126, sy(8), buf, PixelRenderer::rgb(255, 216, 72));
        drawStatRow(18, sy(34), Ui::Status::STAT_HP, activeMon.ev.hp, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, sy(52), Ui::Status::STAT_ATK, activeMon.ev.atk, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, sy(70), Ui::Status::STAT_DEF, activeMon.ev.def, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, sy(88), Ui::Status::STAT_SPA, activeMon.ev.spa, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(18, sy(106), Ui::Status::STAT_SPD, activeMon.ev.spd, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(126, sy(34), Ui::Status::STAT_SPE, activeMon.ev.spe, PixelRenderer::rgb(241, 242, 232));
    } else {
        PixelRenderer::text(12, sy(8), Ui::Status::INDIVIDUAL_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        drawStatRow(18, sy(34), Ui::Status::STAT_HP, Game::ivAt(activeMon.ivPacked, 0), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(18, sy(52), Ui::Status::STAT_ATK, Game::ivAt(activeMon.ivPacked, 1), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(18, sy(70), Ui::Status::STAT_DEF, Game::ivAt(activeMon.ivPacked, 2), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(18, sy(88), Ui::Status::STAT_SPA, Game::ivAt(activeMon.ivPacked, 3), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(18, sy(106), Ui::Status::STAT_SPD, Game::ivAt(activeMon.ivPacked, 4), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(126, sy(34), Ui::Status::STAT_SPE, Game::ivAt(activeMon.ivPacked, 5), PixelRenderer::rgb(135, 214, 238));
    }
    c.clearClipRect();

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

void MenuScene::renderRoomPage() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0x0000);

    static constexpr int ROW_X = 8;
    static constexpr int TEXT_X = 22;
    static constexpr int VALUE_X = 158;
    static constexpr int ROW_Y = 12;
    static constexpr int ROW_H = 24;
    static constexpr int SEP_W = Hal::DISPLAY_W - ROW_X * 2;

    for (uint8_t i = 0; i < ROOM_ITEM_COUNT; ++i) {
        int y = ROW_Y + i * ROW_H;
        bool selected = i == roomCursor;
        uint16_t color = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) {
            c.fillRect(ROW_X, y + 1, 4, 16, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(TEXT_X, y, Ui::Room::ITEMS[i], color, 1);
        if (i == 0) {
            char countBuf[12];
            snprintf(countBuf, sizeof(countBuf), Ui::Bag::COUNT_X_FMT, GameEngine::ins().foodCount());
            PixelRenderer::text(VALUE_X, y, countBuf,
                                selected ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                                1);
        }
        if (i + 1 < ROOM_ITEM_COUNT) {
            c.drawFastHLine(ROW_X, y + ROW_H - 4, SEP_W, PixelRenderer::rgb(55, 63, 76));
        }
    }

    PixelRenderer::text(22, 116, Ui::Room::DESCS[roomCursor], PixelRenderer::rgb(135, 214, 238), 1);
    renderToast();
}

void MenuScene::renderFoodPage() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0x0000);

    static constexpr int LEFT_W = 78;
    static constexpr int LIST_Y_START = 6;
    static constexpr int LIST_BOTTOM = Hal::DISPLAY_H - 6;
    static constexpr int VISIBLE_H = LIST_BOTTOM - LIST_Y_START;
    static constexpr int ROW_H = 22;
    static constexpr int TEXT_X = 18;

    if (foodCursor >= FOOD_ITEM_COUNT) foodCursor = 0;

    c.drawFastVLine(LEFT_W, 6, Hal::DISPLAY_H - 12, 0x7BEF);

    int totalH = FOOD_ITEM_COUNT * ROW_H;
    int maxScroll = (totalH > VISIBLE_H) ? (totalH - VISIBLE_H) : 0;
    int targetScroll = foodCursor * ROW_H - VISIBLE_H / 2 + ROW_H / 2;
    if (targetScroll < 0) targetScroll = 0;
    if (targetScroll > maxScroll) targetScroll = maxScroll;

    float diff = (float)targetScroll - foodScroll;
    if (fabsf(diff) < 0.5f) {
        foodScroll = (float)targetScroll;
    } else {
        foodScroll += diff * 0.25f;
    }

    uint8_t selectedFood = GameEngine::ins().selectedFoodIndex();
    c.setClipRect(0, LIST_Y_START, LEFT_W - 1, VISIBLE_H);
    for (uint8_t i = 0; i < FOOD_ITEM_COUNT; ++i) {
        int y = LIST_Y_START + i * ROW_H - (int)foodScroll;
        if (y + ROW_H < LIST_Y_START || y > LIST_BOTTOM) continue;
        bool selected = i == foodCursor;
        bool back = isFoodBackIndex(i);
        uint8_t stock = back ? 1 : GameEngine::ins().foodCount(i);
        uint16_t color = selected ? 0xFFE0 : (stock > 0 ? 0xFFFF : 0x7BEF);
        int textY = y + (ROW_H - 16) / 2;

        if (!back && i == selectedFood && stock > 0) {
            drawSelectionDiamond(TEXT_X - 8, textY + 8, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(TEXT_X, textY,
                            back ? Ui::BACK : Ui::Room::FOOD_NAMES[i],
                            color, 1);
    }
    c.clearClipRect();

    if (isFoodBackIndex(foodCursor)) {
        PixelRenderer::text(LEFT_W + 28, 80, Ui::Room::DESCS[ROOM_ITEM_COUNT - 1], 0x7BEF, 1);
        renderToast();
        return;
    }

    uint8_t stock = GameEngine::ins().foodCount(foodCursor);
    uint16_t iconColor = foodColor(foodCursor);
    int iconX = LEFT_W + 13;
    int iconY = 9;
    c.fillEllipse(iconX + 29, iconY + 18, 28, 11, PixelRenderer::rgb(112, 87, 70));
    c.fillEllipse(iconX + 29, iconY + 14, 23, 9, iconColor);
    c.fillCircle(iconX + 21, iconY + 11, 2, PixelRenderer::rgb(92, 151, 80));
    c.fillCircle(iconX + 37, iconY + 12, 2, PixelRenderer::rgb(178, 79, 57));

    int textX = iconX + 68;
    PixelRenderer::text(textX, iconY + 2, Ui::Room::FOOD_NAMES[foodCursor], 0xFFFF, 1);
    char countBuf[16];
    snprintf(countBuf, sizeof(countBuf), Ui::Bag::COUNT_X_FMT, stock);
    PixelRenderer::text(textX, iconY + 20, countBuf,
                        stock > 0 ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                        1);

    const char* descLines[3] = {
        Ui::Room::FOOD_DESCS[foodCursor][0],
        Ui::Room::FOOD_DESCS[foodCursor][1],
        Ui::Room::FOOD_DESCS[foodCursor][2],
    };
    renderScrollableDescription(descLines, 3, LEFT_W + 10, 58,
                                Hal::DISPLAY_W - LEFT_W - 12,
                                stock > 0 ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                                0x300 + foodCursor);
    renderToast();
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
    if (storageActionOpen && storageCursor < state.storageCount) {
        renderStorageActionPopup();
        if (storageReleaseConfirmOpen) renderStorageReleaseConfirmPopup();
    }
    renderToast();
}

void MenuScene::renderStorageActionPopup() {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 142;
    static constexpr int POP_Y = 20;
    static constexpr int POP_W = 78;
    static constexpr int POP_H = 100;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(241, 242, 232));

    for (uint8_t i = 0; i < STORAGE_ACTION_COUNT; ++i) {
        int y = POP_Y + 9 + i * 21;
        bool selected = i == storageActionCursor;
        if (selected) c.fillRect(POP_X + 5, y - 2, 4, 18, PixelRenderer::rgb(255, 216, 72));
        PixelRenderer::text(POP_X + 16, y, Ui::Storage::ACTIONS[i],
                            selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232),
                            1);
    }
}

void MenuScene::renderStorageReleaseConfirmPopup() {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 25;
    static constexpr int POP_Y = 32;
    static constexpr int POP_W = 190;
    static constexpr int POP_H = 72;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(241, 242, 232));

    int lineW = textPixelWidth(Ui::Storage::RELEASE_CONFIRM);
    int lineX = POP_X + (POP_W - lineW) / 2;
    if (lineX < POP_X + 6) lineX = POP_X + 6;
    PixelRenderer::text(lineX, POP_Y + 17, Ui::Storage::RELEASE_CONFIRM,
                        PixelRenderer::rgb(241, 242, 232), 1);

    uint16_t yesColor = storageReleaseConfirmYes ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    uint16_t noColor = !storageReleaseConfirmYes ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    PixelRenderer::text(POP_X + 60, POP_Y + 49, Ui::Storage::YES, yesColor, 1);
    PixelRenderer::text(POP_X + 130, POP_Y + 49, Ui::Storage::NO, noColor, 1);
}

void MenuScene::renderDebugPage() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0x0000);

    static constexpr int ROW_X = 8;
    static constexpr int TEXT_X = 22;
    static constexpr int ROW_Y = 0;
    static constexpr int ROW_H = 17;
    static constexpr int SEP_Y_OFFSET = 16;
    static constexpr int SEP_W = Hal::DISPLAY_W - ROW_X * 2;
    uint8_t itemCount = debugItemCount();
    if (debugCursor >= itemCount) debugCursor = 0;

    for (uint8_t i = 0; i < itemCount; ++i) {
        int y = ROW_Y + i * ROW_H;
        bool selected = i == debugCursor;
        uint16_t color = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) {
            c.fillRect(ROW_X, y, 4, 16, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(TEXT_X, y, debugItemLabel(i), color, 1);
        if (debugCategory == DebugCategory::MONSTER && i == 1) {
            const auto& state = GameEngine::ins().gameState();
            uint16_t speciesId = state.teamCount > 0 ? state.team[0].speciesId : 0;
            char idBuf[20];
            snprintf(idBuf, sizeof(idBuf), Ui::Debug::CURRENT_ID_FMT, speciesId);
            PixelRenderer::text(Hal::DISPLAY_W - textPixelWidth(idBuf) - 12, y, idBuf,
                                selected ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                                1);
        }
        if (debugCategory == DebugCategory::ENV && i == 0) {
            uint16_t minutes = GameEngine::ins().gameMinutesOfDay();
            char timeBuf[20];
            snprintf(timeBuf, sizeof(timeBuf), Ui::Debug::CURRENT_TIME_FMT, minutes / 60, minutes % 60);
            PixelRenderer::text(Hal::DISPLAY_W - textPixelWidth(timeBuf) - 12, y, timeBuf,
                                selected ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                                1);
        }
        if (debugCategory == DebugCategory::ENV && i == 1) {
            const char* value = GameEngine::ins().debugLightSourceLabel();
            PixelRenderer::text(Hal::DISPLAY_W - textPixelWidth(value) - 12, y, value,
                                selected ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                                1);
        }
        if (debugCategory == DebugCategory::MOTION && i == 0) {
            const char* value = GameEngine::ins().debugTiltControlEnabled() ? Ui::Settings::ON : Ui::Settings::OFF;
            PixelRenderer::text(Hal::DISPLAY_W - textPixelWidth(value) - 12, y, value,
                                selected ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                                1);
        }
        if (debugCategory == DebugCategory::MOTION && i == 1) {
            const char* value = GameEngine::ins().debugWalkBoundaryVisible() ? Ui::Settings::ON : Ui::Settings::OFF;
            PixelRenderer::text(Hal::DISPLAY_W - textPixelWidth(value) - 12, y, value,
                                selected ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                                1);
        }
        if (i + 1 < itemCount) {
            c.drawFastHLine(ROW_X, y + SEP_Y_OFFSET, SEP_W, PixelRenderer::rgb(55, 63, 76));
        }
    }

    if (debugSwitchOpen) renderDebugSwitchPopup();
    if (debugTimeOpen) renderDebugTimePopup();
    renderToast();
}

uint8_t MenuScene::debugItemCount() const {
    switch (debugCategory) {
    case DebugCategory::MONSTER: return DEBUG_MONSTER_ITEM_COUNT;
    case DebugCategory::RESOURCE: return DEBUG_RESOURCE_ITEM_COUNT;
    case DebugCategory::ENV: return DEBUG_ENV_ITEM_COUNT;
    case DebugCategory::MOTION: return DEBUG_MOTION_ITEM_COUNT;
    case DebugCategory::ROOT:
    default:
        return DEBUG_ROOT_ITEM_COUNT;
    }
}

const char* MenuScene::debugItemLabel(uint8_t index) const {
    if (index >= debugItemCount()) return Ui::BACK;
    switch (debugCategory) {
    case DebugCategory::MONSTER: return Ui::Debug::MONSTER_ITEMS[index];
    case DebugCategory::RESOURCE: return Ui::Debug::RESOURCE_ITEMS[index];
    case DebugCategory::ENV: return Ui::Debug::ENV_ITEMS[index];
    case DebugCategory::MOTION: return Ui::Debug::MOTION_ITEMS[index];
    case DebugCategory::ROOT:
    default:
        return Ui::Debug::ROOT_ITEMS[index];
    }
}

void MenuScene::handleDebugAction() {
    if (debugCategory == DebugCategory::ROOT) {
        switch (debugCursor) {
        case 0: debugCategory = DebugCategory::MONSTER; break;
        case 1: debugCategory = DebugCategory::RESOURCE; break;
        case 2: debugCategory = DebugCategory::ENV; break;
        case 3: debugCategory = DebugCategory::MOTION; break;
        default:
            popView();
            return;
        }
        debugCursor = 0;
        return;
    }

    if (debugCursor + 1 >= debugItemCount()) {
        debugCategory = DebugCategory::ROOT;
        debugCursor = 0;
        return;
    }

    switch (debugCategory) {
    case DebugCategory::MONSTER:
        if (debugCursor == 0) {
            GameEngine::ins().debugRecoverActiveMonster();
            toast = Ui::Debug::RECOVERED;
            toastUntil = Hal::ins().millis() + 1100;
        } else if (debugCursor == 1) {
            openDebugSwitchPopup();
        }
        break;
    case DebugCategory::RESOURCE:
        if (debugCursor == 0) {
            GameEngine::ins().addCoins(1000);
            toast = Ui::Debug::COINS_ADDED;
            toastUntil = Hal::ins().millis() + 1100;
        }
        break;
    case DebugCategory::ENV:
        if (debugCursor == 0) {
            openDebugTimePopup();
        } else if (debugCursor == 1) {
            GameEngine::ins().cycleDebugLightSource();
        }
        break;
    case DebugCategory::MOTION:
        if (debugCursor == 0) {
            GameEngine::ins().toggleDebugTiltControl();
        } else if (debugCursor == 1) {
            GameEngine::ins().toggleDebugWalkBoundary();
        }
        break;
    case DebugCategory::ROOT:
    default:
        break;
    }
}

void MenuScene::openDebugSwitchPopup() {
    debugTimeOpen = false;
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

void MenuScene::openDebugTimePopup() {
    debugSwitchOpen = false;
    uint16_t minutes = GameEngine::ins().gameMinutesOfDay();
    uint8_t hour = minutes / 60;
    uint8_t minute = minutes % 60;
    debugTimeDigits[0] = hour / 10;
    debugTimeDigits[1] = hour % 10;
    debugTimeDigits[2] = minute / 10;
    debugTimeDigits[3] = minute % 10;
    debugTimeFocus = 0;
    debugTimeOpen = true;
}

uint16_t MenuScene::debugTimeTargetMinutes() const {
    uint8_t hour = (uint8_t)(debugTimeDigits[0] * 10 + debugTimeDigits[1]);
    uint8_t minute = (uint8_t)(debugTimeDigits[2] * 10 + debugTimeDigits[3]);
    if (hour > 23) hour = 23;
    if (minute > 59) minute = 59;
    return (uint16_t)(hour * 60 + minute);
}

void MenuScene::incrementDebugTimeDigit() {
    if (debugTimeFocus == 0) {
        debugTimeDigits[0] = (debugTimeDigits[0] + 1) % 3;
        if (debugTimeDigits[0] == 2 && debugTimeDigits[1] > 3) debugTimeDigits[1] = 3;
        return;
    }
    if (debugTimeFocus == 1) {
        uint8_t maxHourOnes = debugTimeDigits[0] == 2 ? 3 : 9;
        debugTimeDigits[1] = (debugTimeDigits[1] + 1) % (maxHourOnes + 1);
        return;
    }
    if (debugTimeFocus == 2) {
        debugTimeDigits[2] = (debugTimeDigits[2] + 1) % 6;
        return;
    }
    if (debugTimeFocus == 3) {
        debugTimeDigits[3] = (debugTimeDigits[3] + 1) % 10;
    }
}

uint8_t MenuScene::visibleFoodIndexOf(uint8_t foodIndex) const {
    return foodIndex < Game::ROOM_FOOD_COUNT ? foodIndex : 0;
}

bool MenuScene::isFoodBackIndex(uint8_t index) const {
    return index >= Game::ROOM_FOOD_COUNT;
}

void MenuScene::drawSelectionDiamond(int cx, int cy, uint16_t color) {
    auto& c = PixelRenderer::canvas();
    c.fillTriangle(cx, cy - 3, cx + 3, cy, cx, cy + 3, color);
    c.fillTriangle(cx, cy - 3, cx, cy + 3, cx - 3, cy, color);
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

void MenuScene::renderDebugTimePopup() {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 30;
    static constexpr int POP_Y = 26;
    static constexpr int POP_W = 180;
    static constexpr int POP_H = 84;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(18, 22, 30));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(241, 242, 232));

    PixelRenderer::text(POP_X + 14, POP_Y + 10, Ui::Debug::SET_TIME,
                        PixelRenderer::rgb(67, 213, 224), 1);
    PixelRenderer::text(POP_X + 14, POP_Y + 32, Ui::Debug::TARGET_TIME,
                        PixelRenderer::rgb(241, 242, 232), 1);

    char digitBuf[2] = {'0', '\0'};
    for (uint8_t i = 0; i < 4; ++i) {
        int x = POP_X + 90 + i * 16 + (i >= 2 ? 8 : 0);
        bool selected = debugTimeFocus == i;
        if (selected) {
            c.drawRect(x - 3, POP_Y + 29, 14, 18, PixelRenderer::rgb(255, 216, 72));
        }
        digitBuf[0] = (char)('0' + debugTimeDigits[i]);
        PixelRenderer::text(x, POP_Y + 32, digitBuf,
                            selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232),
                            1);
    }
    PixelRenderer::text(POP_X + 122, POP_Y + 32, ":", PixelRenderer::rgb(241, 242, 232), 1);

    uint16_t yesColor = debugTimeFocus == 4 ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
    uint16_t cancelColor = debugTimeFocus == 5 ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
    if (debugTimeFocus == 4) c.fillRect(POP_X + 38, POP_Y + 60, 4, 16, PixelRenderer::rgb(255, 216, 72));
    if (debugTimeFocus == 5) c.fillRect(POP_X + 94, POP_Y + 60, 4, 16, PixelRenderer::rgb(255, 216, 72));
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

void MenuScene::updateStatusScroll(int scrollKey, int maxScroll) {
    uint32_t now = Hal::ins().millis();
    if (scrollKey != statusScrollKey) {
        statusScrollKey = scrollKey;
        statusScroll = 0.0f;
        statusScrollLastMs = now;
    }

    if (maxScroll <= 0) {
        statusScroll = 0.0f;
        statusScrollLastMs = now;
        return;
    }

    uint32_t dt = now - statusScrollLastMs;
    if (dt > 120) dt = 120;
    statusScrollLastMs = now;

    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    if (!Hal::ins().readAccel(ax, ay, az)) return;
    (void)ax;
    (void)az;

    static constexpr float DEADZONE = 0.18f;
    static constexpr float MAX_TILT = 0.75f;
    static constexpr float SPEED = 72.0f;
    if (fabsf(ay) < DEADZONE) return;
    float input = ay > 0.0f ? (ay - DEADZONE) : (ay + DEADZONE);
    input /= (MAX_TILT - DEADZONE);
    if (input > 1.0f) input = 1.0f;
    if (input < -1.0f) input = -1.0f;

    statusScroll += input * SPEED * ((float)dt / 1000.0f);
    if (statusScroll < 0.0f) statusScroll = 0.0f;
    if (statusScroll > (float)maxScroll) statusScroll = (float)maxScroll;
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
