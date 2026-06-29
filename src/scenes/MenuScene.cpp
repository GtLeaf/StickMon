#include "scenes/MenuScene.h"
#include <cstdio>
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
}

int8_t MenuScene::lastCursor = 0;

void MenuScene::onEnter() {
    viewMode = ViewMode::MENU;
    statusPage = 0;
    if (GameEngine::ins().previousScene() == GameEngine::ins().homeScene()) {
        cursor = 0;
    } else {
        cursor = lastCursor;
    }
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
            viewMode = ViewMode::MENU;
            return true;
        }
        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            viewMode = ViewMode::MENU;
            return true;
        }
        if (viewMode == ViewMode::STATUS && event.btn == 1 && event.action == BtnAction::PRESSED) {
            statusPage = (statusPage + 1) % STATUS_PAGE_COUNT;
            return true;
        }
        return false;
    }

    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        cursor++;
        if (cursor >= ITEM_COUNT) cursor = 0;
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
    case ITEM_STATUS:
        viewMode = ViewMode::STATUS;
        statusPage = 0;
        return true;
    case ITEM_BAG:
        viewMode = ViewMode::BAG;
        return true;
    case ITEM_EXPLORE:
        GameEngine::ins().requestScene(SceneID::EXPLORE);
        return true;
    case ITEM_SHOP:
        GameEngine::ins().requestScene(SceneID::SHOP);
        return true;
    case ITEM_SOCIAL:
        GameEngine::ins().requestScene(SceneID::SOCIAL);
        return true;
    case ITEM_SETTINGS:
        GameEngine::ins().requestScene(SceneID::SETTINGS);
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

    const char* title = Ui::MENU;
    if (viewMode == ViewMode::STATUS) title = Ui::STATUS;
    if (viewMode == ViewMode::BAG) title = Ui::BAG;
    renderTitleBar(title);

    if (viewMode == ViewMode::STATUS) {
        renderStatusPage();
        return;
    }
    if (viewMode == ViewMode::BAG) {
        renderBagPage();
        return;
    }

    renderMenu();
    renderToast();
}

void MenuScene::renderMenu() {
    auto& c = PixelRenderer::canvas();
    const int visibleRows = 7;
    const int rowH = 27;
    const int startY = 30;
    int first = 0;
    if (cursor >= visibleRows) {
        first = cursor - visibleRows + 1;
    }

    for (int row = 0; row < visibleRows; ++row) {
        int i = first + row;
        if (i >= ITEM_COUNT) break;
        int y = startY + row * rowH;
        uint16_t color = (i == cursor) ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);

        if (i == cursor) {
            c.fillRect(4, y + 5, 5, 17, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(16, y + 5, Ui::Menu::ITEMS[i], color, 1);

        if (row < visibleRows - 1 && i < ITEM_COUNT - 1) {
            c.drawFastHLine(4, y + rowH - 1, Hal::DISPLAY_W - 8, PixelRenderer::rgb(70, 74, 84));
        }
    }

    if (first > 0) {
        c.fillTriangle(122, 29, 127, 29, 124, 25, PixelRenderer::rgb(135, 214, 238));
    }
    if (first + visibleRows < ITEM_COUNT) {
        c.fillTriangle(122, 213, 127, 213, 124, 217, PixelRenderer::rgb(135, 214, 238));
    }
}

void MenuScene::renderToast() {
    if (!toast || Hal::ins().millis() > toastUntil) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(14, 205, 107, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(22, 211, toast, PixelRenderer::rgb(255, 255, 255), 1);
}

void MenuScene::renderStatusPage() {
    if (!GameEngine::ins().gameState().oobeDone) {
        renderEggStatusPage();
        return;
    }

    auto& c = PixelRenderer::canvas();
    const Game::MonsterRuntime& activeMon = GameEngine::ins().activeMonster();
    const Species& mon = GameEngine::ins().activeSpecies();
    char buf[32];

    c.fillRect(0, 24, Hal::DISPLAY_W, Hal::DISPLAY_H - 24, PixelRenderer::rgb(10, 14, 20));

    if (statusPage == 0) {
        c.fillRect(12, 43, 28, 32, mon.colorA);
        c.fillRect(19, 52, 14, 14, mon.colorB);
        PixelRenderer::text(48, 38, mon.name, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::LEVEL_FMT, activeMon.level);
        PixelRenderer::text(48, 58, buf, PixelRenderer::rgb(241, 242, 232), 1);
        c.drawFastHLine(8, 88, 119, PixelRenderer::rgb(55, 63, 76));
        snprintf(buf, sizeof(buf), Ui::Status::HP_FMT, activeMon.hpCur, activeMon.hpMax);
        PixelRenderer::text(12, 94, buf, PixelRenderer::rgb(92, 222, 112), 1);
        snprintf(buf, sizeof(buf), Ui::Status::EXP_FMT, activeMon.exp);
        PixelRenderer::text(12, 112, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::TYPE_FMT, typeName(mon.type1), typeName(mon.type2));
        PixelRenderer::text(12, 130, buf, PixelRenderer::rgb(255, 216, 72), 1);
        snprintf(buf, sizeof(buf), Ui::Status::NATURE_FMT, natureName(activeMon.nature));
        PixelRenderer::text(12, 148, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STATUS_FMT, statusName(activeMon.statusBits));
        PixelRenderer::text(12, 166, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::CONDITION_FMT,
                 GameEngine::ins().hungerValue(), GameEngine::ins().moodValue());
        PixelRenderer::text(12, 184, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::AFFECTION_FMT, activeMon.affection, activeMon.proficiency);
        PixelRenderer::text(12, 202, buf, PixelRenderer::rgb(241, 242, 232), 1);
    } else if (statusPage == 1) {
        PixelRenderer::text(12, 42, Ui::Status::BASE_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        c.drawFastHLine(8, 68, 119, PixelRenderer::rgb(55, 63, 76));
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_HP, mon.stats.hp);
        PixelRenderer::text(12, 82, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_ATK, mon.stats.atk);
        PixelRenderer::text(12, 104, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_DEF, mon.stats.def);
        PixelRenderer::text(12, 126, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_SPA, mon.stats.spa);
        PixelRenderer::text(12, 148, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_SPD, mon.stats.spd);
        PixelRenderer::text(12, 170, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_SPE, mon.stats.spe);
        PixelRenderer::text(12, 192, buf, PixelRenderer::rgb(241, 242, 232), 1);
    } else if (statusPage == 2) {
        PixelRenderer::text(12, 42, Ui::Status::CURRENT_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        c.drawFastHLine(8, 68, 119, PixelRenderer::rgb(55, 63, 76));
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_HP, statFor(mon, activeMon, 0));
        PixelRenderer::text(12, 82, buf, PixelRenderer::rgb(92, 222, 112), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_ATK, statFor(mon, activeMon, 1));
        PixelRenderer::text(12, 104, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_DEF, statFor(mon, activeMon, 2));
        PixelRenderer::text(12, 126, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_SPA, statFor(mon, activeMon, 3));
        PixelRenderer::text(12, 148, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_SPD, statFor(mon, activeMon, 4));
        PixelRenderer::text(12, 170, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_SPE, statFor(mon, activeMon, 5));
        PixelRenderer::text(12, 192, buf, PixelRenderer::rgb(241, 242, 232), 1);
    } else if (statusPage == 3) {
        PixelRenderer::text(12, 36, Ui::Status::EFFORT_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        snprintf(buf, sizeof(buf), Ui::Status::TOTAL_FMT, Game::evTotal(activeMon.ev), Game::EV_TOTAL_MAX);
        PixelRenderer::text(12, 58, buf, PixelRenderer::rgb(255, 216, 72), 1);
        c.drawFastHLine(8, 80, 119, PixelRenderer::rgb(55, 63, 76));
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_HP, activeMon.ev.hp);
        PixelRenderer::text(12, 92, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_ATK, activeMon.ev.atk);
        PixelRenderer::text(12, 112, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_DEF, activeMon.ev.def);
        PixelRenderer::text(12, 132, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_SPA, activeMon.ev.spa);
        PixelRenderer::text(12, 152, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_SPD, activeMon.ev.spd);
        PixelRenderer::text(12, 172, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_SPE, activeMon.ev.spe);
        PixelRenderer::text(12, 192, buf, PixelRenderer::rgb(241, 242, 232), 1);
    } else if (statusPage == 4) {
        PixelRenderer::text(12, 42, Ui::Status::INDIVIDUAL_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        c.drawFastHLine(8, 68, 119, PixelRenderer::rgb(55, 63, 76));
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_HP, Game::ivAt(activeMon.ivPacked, 0));
        PixelRenderer::text(12, 82, buf, PixelRenderer::rgb(135, 214, 238), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_ATK, Game::ivAt(activeMon.ivPacked, 1));
        PixelRenderer::text(12, 104, buf, PixelRenderer::rgb(135, 214, 238), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_DEF, Game::ivAt(activeMon.ivPacked, 2));
        PixelRenderer::text(12, 126, buf, PixelRenderer::rgb(135, 214, 238), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_SPA, Game::ivAt(activeMon.ivPacked, 3));
        PixelRenderer::text(12, 148, buf, PixelRenderer::rgb(135, 214, 238), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_SPD, Game::ivAt(activeMon.ivPacked, 4));
        PixelRenderer::text(12, 170, buf, PixelRenderer::rgb(135, 214, 238), 1);
        snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, Ui::Status::STAT_SPE, Game::ivAt(activeMon.ivPacked, 5));
        PixelRenderer::text(12, 192, buf, PixelRenderer::rgb(135, 214, 238), 1);
    } else if (statusPage == 5) {
        const MoveInfo* basicMove = findMove(mon.basicMoveId);
        const MoveInfo* specialMove = findMove(mon.specialMoveId);
        PixelRenderer::text(12, 42, Ui::Status::MOVE_INFO, PixelRenderer::rgb(67, 213, 224), 1);
        c.drawFastHLine(8, 68, 119, PixelRenderer::rgb(55, 63, 76));
        PixelRenderer::text(12, 78,
                            basicMove ? basicMove->name : Ui::Status::MOVE_UNKNOWN,
                            PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::POWER_FMT, basicMove ? basicMove->power : 0);
        PixelRenderer::text(12, 98, buf, PixelRenderer::rgb(255, 216, 72), 1);
        PixelRenderer::text(12, 118,
                            basicMove ? basicMove->description : Ui::Status::MOVE_UNKNOWN,
                            PixelRenderer::rgb(135, 214, 238), 1);
        PixelRenderer::text(12, 150,
                            specialMove ? specialMove->name : Ui::Status::MOVE_UNKNOWN,
                            PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::POWER_FMT, specialMove ? specialMove->power : 0);
        PixelRenderer::text(12, 170, buf, PixelRenderer::rgb(255, 216, 72), 1);
        PixelRenderer::text(12, 190,
                            specialMove ? specialMove->description : Ui::Status::MOVE_UNKNOWN,
                            PixelRenderer::rgb(135, 214, 238), 1);
    } else {
        PixelRenderer::text(12, 42, Ui::Status::EVOLUTION, PixelRenderer::rgb(67, 213, 224), 1);
        c.drawFastHLine(8, 66, 119, PixelRenderer::rgb(55, 63, 76));
        if (mon.evolveTo == 0) {
            PixelRenderer::text(12, 86, Ui::Status::NO_EVOLVE, PixelRenderer::rgb(241, 242, 232), 1);
        } else {
            PixelRenderer::text(12, 86, evolutionMethodName(mon.evolveMethod), PixelRenderer::rgb(241, 242, 232), 1);
            if (mon.evolveMethod == EvolutionMethod::LEVEL) {
                snprintf(buf, sizeof(buf), Ui::Status::NEED_LEVEL_FMT, mon.evolveLevel);
                PixelRenderer::text(12, 110, buf, PixelRenderer::rgb(241, 242, 232), 1);
            }
            snprintf(buf, sizeof(buf), Ui::Status::ID_FMT, mon.evolveTo);
            PixelRenderer::text(12, 134, buf, PixelRenderer::rgb(135, 214, 238), 1);
        }
        PixelRenderer::text(12, 166, Ui::Status::SOURCE_INFO, PixelRenderer::rgb(67, 213, 224), 1);
        snprintf(buf, sizeof(buf), Ui::Status::SOURCE_FMT, originName(activeMon.origin));
        PixelRenderer::text(12, 188, buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::MET_AT_FMT, activeMon.caughtAt);
        PixelRenderer::text(12, 208, buf, PixelRenderer::rgb(241, 242, 232), 1);
    }

    renderPageIndicator(statusPage, STATUS_PAGE_COUNT);
}

void MenuScene::renderEggStatusPage() {
    auto& c = PixelRenderer::canvas();

    statusPage = 0;
    c.fillRect(0, 24, Hal::DISPLAY_W, Hal::DISPLAY_H - 24, PixelRenderer::rgb(10, 14, 20));
    c.fillEllipse(28, 70, 18, 26, PixelRenderer::rgb(240, 232, 184));
    c.fillEllipse(28, 71, 13, 20, PixelRenderer::rgb(255, 248, 214));
    c.drawEllipse(28, 70, 18, 26, PixelRenderer::rgb(109, 92, 62));
    c.fillCircle(20, 65, 3, PixelRenderer::rgb(255, 145, 67));
    c.fillCircle(36, 76, 3, PixelRenderer::rgb(71, 169, 226));
    PixelRenderer::text(54, 44, Ui::Status::EGG_TITLE, PixelRenderer::rgb(241, 242, 232), 1);
    c.drawFastHLine(8, 102, 119, PixelRenderer::rgb(55, 63, 76));
    PixelRenderer::text(12, 116, Ui::Status::EGG_STATE, PixelRenderer::rgb(255, 216, 72), 1);
}

void MenuScene::renderBagPage() {
    auto& c = PixelRenderer::canvas();
    char buf[32];

    c.fillRect(6, 34, 123, 160, PixelRenderer::rgb(25, 31, 40));
    c.drawRect(6, 34, 123, 160, PixelRenderer::rgb(72, 83, 98));
    PixelRenderer::text(12, 44, Ui::Bag::ITEMS, PixelRenderer::rgb(67, 213, 224), 1);
    snprintf(buf, sizeof(buf), Ui::Bag::BALL_FMT, GameEngine::ins().ballCount());
    PixelRenderer::text(12, 64, buf, PixelRenderer::rgb(241, 242, 232), 1);
    snprintf(buf, sizeof(buf), Ui::Bag::GREAT_BALL_FMT, GameEngine::ins().greatBallCount());
    PixelRenderer::text(12, 82, buf, PixelRenderer::rgb(241, 242, 232), 1);
    snprintf(buf, sizeof(buf), Ui::Bag::FOOD_FMT, GameEngine::ins().foodCount());
    PixelRenderer::text(12, 100, buf, PixelRenderer::rgb(241, 242, 232), 1);
    snprintf(buf, sizeof(buf), Ui::Bag::POTION_FMT, GameEngine::ins().potionCount());
    PixelRenderer::text(12, 118, buf, PixelRenderer::rgb(241, 242, 232), 1);
    snprintf(buf, sizeof(buf), Ui::Bag::SUPER_POTION_FMT, GameEngine::ins().superPotionCount());
    PixelRenderer::text(12, 136, buf, PixelRenderer::rgb(241, 242, 232), 1);
    snprintf(buf, sizeof(buf), Ui::Bag::ANTIDOTE_FMT, GameEngine::ins().antidoteCount());
    PixelRenderer::text(12, 154, buf, PixelRenderer::rgb(241, 242, 232), 1);
    snprintf(buf, sizeof(buf), Ui::Bag::CANDY_FMT, GameEngine::ins().candyCount());
    PixelRenderer::text(12, 172, buf, PixelRenderer::rgb(241, 242, 232), 1);

}

void MenuScene::renderTitleBar(const char* title) {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, 24, PixelRenderer::rgb(25, 25, 40));
    PixelRenderer::text(4, 4, title, PixelRenderer::rgb(67, 213, 224), 1);

    if (viewMode != ViewMode::MENU) return;
    int batt = Hal::ins().filteredBatteryLevel();
    char battBuf[8];
    snprintf(battBuf, sizeof(battBuf), Ui::Menu::TITLE_BATTERY_FMT, batt);
    PixelRenderer::text(104, 4, battBuf,
                        batt > 20 ? PixelRenderer::rgb(92, 222, 112) : PixelRenderer::rgb(239, 85, 85),
                        1);
}

void MenuScene::renderPageIndicator(uint8_t page, uint8_t count) {
    auto& c = PixelRenderer::canvas();
    const int dotW = 7;
    const int gap = 4;
    int totalW = count * dotW + (count - 1) * gap;
    int x = (Hal::DISPLAY_W - totalW) / 2;
    int y = Hal::DISPLAY_H - 11;
    for (uint8_t i = 0; i < count; ++i) {
        uint16_t color = i == page ? PixelRenderer::rgb(67, 213, 224) : PixelRenderer::rgb(92, 98, 110);
        c.fillRect(x + i * (dotW + gap), y, dotW, 5, color);
    }
}
