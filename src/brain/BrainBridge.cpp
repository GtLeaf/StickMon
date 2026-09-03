#include "brain/BrainBridge.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "game/GameState.h"
#include "game/ExploreItemProgression.h"
#include "platform/api/PlatformServices.h"

namespace Stickmon {
namespace {

const char* explorePhaseName(uint8_t phase) {
    switch (phase) {
    case 1: return "departing";
    case 2: return "active";
    case 3: return "returning";
    case 4: return "returning_fainted";
    case 0:
    default: return "home";
    }
}

}  // namespace

BrainBridge& BrainBridge::instance() {
    static BrainBridge bridge;
    return bridge;
}

bool BrainBridge::begin() {
    if (begun_) return true;
    for (Slot& slot : slots_) {
        slot.done = xSemaphoreCreateBinary();
        if (!slot.done) {
            for (Slot& created : slots_) {
                if (created.done) vSemaphoreDelete(created.done);
                created.done = nullptr;
            }
            return false;
        }
    }
    begun_ = true;
    return true;
}

void BrainBridge::setHost(const HostAdapter& adapter) {
    portENTER_CRITICAL(&mux_);
    host_ = adapter;
    portEXIT_CRITICAL(&mux_);
}

BrainBridge::HostAdapter BrainBridge::hostAdapter() const {
    HostAdapter adapter{};
    portENTER_CRITICAL(&mux_);
    adapter = host_;
    portEXIT_CRITICAL(&mux_);
    return adapter;
}

void BrainBridge::setResult(Result& result, bool accepted, const char* text) {
    result.accepted = accepted;
    std::snprintf(result.text, sizeof(result.text), "%s", text ? text : "");
}

bool BrainBridge::enqueue(const Request& request, Result& result,
                          uint32_t waitMs) {
    if (!begun_) {
        setResult(result, false, "StickMon brain is not ready");
        return false;
    }

    Slot* selected = nullptr;
    portENTER_CRITICAL(&mux_);
    for (Slot& slot : slots_) {
        if (!slot.inUse) {
            xSemaphoreTake(slot.done, 0);
            slot.inUse = true;
            slot.processing = false;
            slot.waiter = true;
            slot.request = request;
            slot.result = Result{};
            selected = &slot;
            break;
        }
    }
    portEXIT_CRITICAL(&mux_);

    if (!selected) {
        setResult(result, false, "StickMon brain is busy");
        return false;
    }

    // A timeout does not mean the action was dropped: once update() claims the
    // slot it still runs to completion. Host callbacks are retry-safe (they
    // reject duplicate state transitions), so an LLM retry here is harmless.
    if (xSemaphoreTake(selected->done, pdMS_TO_TICKS(waitMs)) != pdTRUE) {
        portENTER_CRITICAL(&mux_);
        selected->waiter = false;
        if (!selected->processing) selected->inUse = false;
        portEXIT_CRITICAL(&mux_);
        setResult(result, false, "StickMon action timed out");
        return false;
    }

    portENTER_CRITICAL(&mux_);
    result = selected->result;
    selected->waiter = false;
    selected->inUse = false;
    portEXIT_CRITICAL(&mux_);
    return result.accepted;
}

void BrainBridge::refreshSnapshot(uint32_t nowMs) {
    Snapshot value{};
    HostAdapter host = hostAdapter();
    if (host.snapshot) host.snapshot(value, host.userCtx);
    value.updatedMs = nowMs;

    portENTER_CRITICAL(&mux_);
    snapshot_ = value;
    portEXIT_CRITICAL(&mux_);
}

BrainBridge::Result BrainBridge::execute(const Request& request) {
    Result result{};
    bool allowed = false;
    portENTER_CRITICAL(&mux_);
    allowed = agentAllowed_;
    portEXIT_CRITICAL(&mux_);
    if (request.autonomous && !allowed) {
        setResult(result, false, "Agent control is paused while the player is active");
        return result;
    }
    HostAdapter host = hostAdapter();
    portENTER_CRITICAL(&mux_);
    executingAutonomous_ = request.autonomous;
    portEXIT_CRITICAL(&mux_);
    switch (request.action) {
    case Action::START_EXPEDITION:
        if (request.area >= Game::EXPLORE_AREA_COUNT) {
            setResult(result, false, "Invalid expedition area");
        } else if (host.startExpedition &&
                   host.startExpedition(request.area, host.userCtx)) {
            setResult(result, true, "Expedition started");
        } else {
            setResult(result, false, host.startExpedition
                                      ? "Expedition is unavailable right now"
                                      : "StickMon host is not ready");
        }
        break;
    case Action::RETURN_HOME:
        if (host.returnHome && host.returnHome(host.userCtx)) {
            setResult(result, true, "Returning home");
        } else {
            setResult(result, false, host.returnHome
                                      ? "The pet is already home"
                                      : "StickMon host is not ready");
        }
        break;
    case Action::INVITE_FRIEND:
        if (host.inviteFriend && host.inviteFriend(host.userCtx)) {
            setResult(result, true, "Friend invitation room opened");
        } else {
            setResult(result, false, host.inviteFriend
                                      ? "A friend invitation is unavailable right now"
                                      : "StickMon host is not ready");
        }
        break;
    case Action::EAT:
        if (host.eat && host.eat(host.userCtx)) {
            setResult(result, true, "The pet ate");
        } else {
            setResult(result, false, host.eat
                                      ? "Food is unavailable right now"
                                      : "StickMon host is not ready");
        }
        break;
    case Action::BUY_FOOD:
        if (request.foodIndex >= 7) {
            setResult(result, false, "Invalid food index");
        } else if (host.buyFood && host.buyFood(request.foodIndex, host.userCtx)) {
            setResult(result, true, "Food purchased");
        } else {
            setResult(result, false, host.buyFood
                                      ? "Food could not be purchased"
                                      : "StickMon host is not ready");
        }
        break;
    case Action::SAY:
        if (host.say && host.say(request.text, host.userCtx)) {
            setResult(result, true, "Message delivered to the pet");
        } else {
            setResult(result, false, host.say
                                      ? "The pet could not display that message"
                                      : "StickMon host is not ready");
        }
        break;
    }
    portENTER_CRITICAL(&mux_);
    executingAutonomous_ = false;
    portEXIT_CRITICAL(&mux_);
    return result;
}

void BrainBridge::update(uint32_t nowMs) {
    if (!begun_) return;
    refreshSnapshot(nowMs);

    for (Slot& slot : slots_) {
        Request request{};
        portENTER_CRITICAL(&mux_);
        if (!slot.inUse || slot.processing) {
            portEXIT_CRITICAL(&mux_);
            continue;
        }
        slot.processing = true;
        request = slot.request;
        portEXIT_CRITICAL(&mux_);

        Result result = execute(request);
        portENTER_CRITICAL(&mux_);
        slot.result = result;
        slot.processing = false;
        bool waiter = slot.waiter;
        if (!waiter) slot.inUse = false;
        portEXIT_CRITICAL(&mux_);
        xSemaphoreGive(slot.done);
    }
}

bool BrainBridge::snapshot(Snapshot& out) const {
    if (!begun_) return false;
    portENTER_CRITICAL(&mux_);
    out = snapshot_;
    portEXIT_CRITICAL(&mux_);
    return true;
}

void BrainBridge::setAgentAllowed(bool allowed) {
    portENTER_CRITICAL(&mux_);
    agentAllowed_ = allowed;
    portEXIT_CRITICAL(&mux_);
}

void BrainBridge::setRuntimeState(bool playerActive, uint32_t idleSeconds,
                                  bool agentAllowed) {
    portENTER_CRITICAL(&mux_);
    snapshot_.playerActive = playerActive;
    snapshot_.idleSeconds = idleSeconds;
    snapshot_.agentAllowed = agentAllowed;
    portEXIT_CRITICAL(&mux_);
}

bool BrainBridge::executingAutonomous() const {
    bool value = false;
    portENTER_CRITICAL(&mux_);
    value = executingAutonomous_;
    portEXIT_CRITICAL(&mux_);
    return value;
}

void BrainBridge::rememberChatId(const char* channel, const char* chatId) {
    if (!channel || std::strcmp(channel, "wechat") != 0 ||
        !chatId || !chatId[0]) {
        return;
    }
    portENTER_CRITICAL(&mux_);
    std::snprintf(latestChatId_, sizeof(latestChatId_), "%s", chatId);
    portEXIT_CRITICAL(&mux_);
}

bool BrainBridge::latestChatId(char* chatId, size_t chatIdSize) const {
    if (!chatId || chatIdSize == 0) return false;
    portENTER_CRITICAL(&mux_);
    std::snprintf(chatId, chatIdSize, "%s", latestChatId_);
    portEXIT_CRITICAL(&mux_);
    return chatId[0] != '\0';
}

void BrainBridge::setAllowedChatId(const char* chatId) {
    portENTER_CRITICAL(&mux_);
    std::snprintf(allowedChatId_, sizeof(allowedChatId_), "%s", chatId ? chatId : "");
    portEXIT_CRITICAL(&mux_);
}

bool BrainBridge::isChatAuthorized(const char* chatId) const {
    if (!chatId) return false;
    char allowed[sizeof(allowedChatId_)] = {};
    portENTER_CRITICAL(&mux_);
    std::snprintf(allowed, sizeof(allowed), "%s", allowedChatId_);
    portEXIT_CRITICAL(&mux_);
    return allowed[0] != '\0' && std::strcmp(allowed, chatId) == 0;
}

esp_err_t BrainBridge::collectContext(const claw_core_request_t* request,
                                      claw_core_context_t* outContext,
                                      void* userCtx) {
    (void)request;
    BrainBridge* bridge = static_cast<BrainBridge*>(userCtx);
    if (!bridge || !outContext) return ESP_ERR_INVALID_ARG;

    Snapshot snapshot{};
    if (!bridge->snapshot(snapshot)) return ESP_ERR_INVALID_STATE;

    int needed = std::snprintf(
        nullptr, 0,
        "StickMon state: home=%s, communication_slot=0, team_count=%u, "
        "species_id=%u, species_name=%s, nature_id=%u, nature_name=%s, gender=%s, "
        "level=%u, "
        "hp=%u/%u, satiety=%u, mood=%u, expedition=%s, area=%u, unlocked_area=%u, visit_active=%s, "
        "battery=%d%%, player_active=%s, idle_seconds=%lu, agent_allowed=%s, "
        "action_locked=%s, coins=%lu, bowl_food=%u, bowl_bites=%u.",
        snapshot.oobeDone ? "ready" : "setup_required",
        static_cast<unsigned>(snapshot.teamCount),
        static_cast<unsigned>(snapshot.speciesId),
        snapshot.speciesName[0] ? snapshot.speciesName : "unknown",
        static_cast<unsigned>(snapshot.nature),
        snapshot.natureName[0] ? snapshot.natureName : "unknown",
        Game::genderName(snapshot.gender),
        static_cast<unsigned>(snapshot.level),
        static_cast<unsigned>(snapshot.hp),
        static_cast<unsigned>(snapshot.hpMax),
        static_cast<unsigned>(snapshot.satiety),
        static_cast<unsigned>(snapshot.mood),
        explorePhaseName(snapshot.explorePhase),
        static_cast<unsigned>(snapshot.exploreArea),
        static_cast<unsigned>(snapshot.unlockedArea),
        snapshot.visitActive ? "true" : "false",
        static_cast<int>(snapshot.battery),
        snapshot.playerActive ? "true" : "false",
        static_cast<unsigned long>(snapshot.idleSeconds),
        snapshot.agentAllowed ? "true" : "false",
        snapshot.actionLocked ? "true" : "false",
        static_cast<unsigned long>(snapshot.coins),
        static_cast<unsigned>(snapshot.bowlFood),
        static_cast<unsigned>(snapshot.bowlBitesRemaining));
    if (needed < 0) return ESP_FAIL;

    char* content = static_cast<char*>(std::calloc(1, static_cast<size_t>(needed) + 1));
    if (!content) return ESP_ERR_NO_MEM;
    std::snprintf(content, static_cast<size_t>(needed) + 1,
                  "StickMon state: home=%s, communication_slot=0, team_count=%u, "
                  "species_id=%u, species_name=%s, nature_id=%u, nature_name=%s, gender=%s, "
                  "level=%u, "
                  "hp=%u/%u, satiety=%u, mood=%u, expedition=%s, area=%u, unlocked_area=%u, visit_active=%s, "
                  "battery=%d%%, player_active=%s, idle_seconds=%lu, agent_allowed=%s, "
                  "action_locked=%s, coins=%lu, bowl_food=%u, bowl_bites=%u.",
                  snapshot.oobeDone ? "ready" : "setup_required",
                  static_cast<unsigned>(snapshot.teamCount),
                  static_cast<unsigned>(snapshot.speciesId),
                  snapshot.speciesName[0] ? snapshot.speciesName : "unknown",
                  static_cast<unsigned>(snapshot.nature),
                  snapshot.natureName[0] ? snapshot.natureName : "unknown",
                  Game::genderName(snapshot.gender),
                  static_cast<unsigned>(snapshot.level),
                  static_cast<unsigned>(snapshot.hp),
                  static_cast<unsigned>(snapshot.hpMax),
                  static_cast<unsigned>(snapshot.satiety),
                  static_cast<unsigned>(snapshot.mood),
                  explorePhaseName(snapshot.explorePhase),
                  static_cast<unsigned>(snapshot.exploreArea),
                  static_cast<unsigned>(snapshot.unlockedArea),
                  snapshot.visitActive ? "true" : "false",
                  static_cast<int>(snapshot.battery),
                  snapshot.playerActive ? "true" : "false",
                  static_cast<unsigned long>(snapshot.idleSeconds),
                  snapshot.agentAllowed ? "true" : "false",
                  snapshot.actionLocked ? "true" : "false",
                  static_cast<unsigned long>(snapshot.coins),
                  static_cast<unsigned>(snapshot.bowlFood),
                  static_cast<unsigned>(snapshot.bowlBitesRemaining));
    outContext->kind = CLAW_CORE_CONTEXT_KIND_SYSTEM_PROMPT;
    outContext->content = content;
    return ESP_OK;
}

}  // namespace Stickmon
