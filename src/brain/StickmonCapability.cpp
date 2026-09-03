#include "brain/StickmonCapability.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "BrainBridge.h"
#include "StickmonClawRuntime.h"
#include "app_capabilities.h"
#include "cJSON.h"
#include "claw_cap.h"
#include "game/ExploreItemProgression.h"
#include "game/GameState.h"
#include "platform/api/PlatformServices.h"

namespace Stickmon {
namespace {

constexpr char GROUP_ID[] = "stickmon";
constexpr char INPUT_NONE[] = "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}";
constexpr char INPUT_SAY[] =
    "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\",\"maxLength\":160}},\"required\":[\"text\"],\"additionalProperties\":false}";
constexpr char INPUT_FOOD[] =
    "{\"type\":\"object\",\"properties\":{\"food\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":6}},\"required\":[\"food\"],\"additionalProperties\":false}";
// Filled by buildSchemas() from Game::EXPLORE_AREA_COUNT before registration so
// the schema cannot drift from the game rules.
char INPUT_EXPEDITION[192];

bool isAutonomyRequest(const claw_cap_call_context_t* context) {
    return context && context->channel &&
           std::strcmp(context->channel, "autonomy") == 0;
}

void notePlayerRequest(const claw_cap_call_context_t* context) {
    // A message or read request from a chat channel is still player activity,
    // even when the model ultimately decides not to mutate game state.
    if (!isAutonomyRequest(context)) {
        ClawRuntime::instance().notePlayerActivity(Platform::clock().millis());
        if (context) {
            BrainBridge::instance().rememberChatId(context->channel,
                                                   context->chat_id);
        }
    }
}

void buildSchemas() {
    std::snprintf(INPUT_EXPEDITION, sizeof(INPUT_EXPEDITION),
                  "{\"type\":\"object\",\"properties\":{\"area\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":%u}},\"required\":[\"area\"],\"additionalProperties\":false}",
                  static_cast<unsigned>(Game::EXPLORE_AREA_COUNT) - 1);
}

void writeOutput(char* output, size_t outputSize, const char* text) {
    if (!output || outputSize == 0) return;
    std::snprintf(output, outputSize, "%s", text ? text : "{}");
}

void writeResult(char* output, size_t outputSize, bool accepted,
                 const char* message) {
    cJSON* object = cJSON_CreateObject();
    if (!object) {
        writeOutput(output, outputSize, "{\"ok\":false,\"error\":\"out of memory\"}");
        return;
    }
    cJSON_AddBoolToObject(object, "ok", accepted);
    cJSON_AddStringToObject(object, accepted ? "result" : "error",
                            message ? message : "");
    char* rendered = cJSON_PrintUnformatted(object);
    if (rendered) {
        writeOutput(output, outputSize, rendered);
        std::free(rendered);
    } else {
        writeOutput(output, outputSize, "{\"ok\":false,\"error\":\"json failure\"}");
    }
    cJSON_Delete(object);
}

bool authorizedMutation(const claw_cap_call_context_t* context,
                        char* output, size_t outputSize) {
    // WeChat identity and access control are owned by the ESP-Claw channel.
    (void)context;
    (void)output;
    (void)outputSize;
    return true;
}

esp_err_t executeAction(BrainBridge::Action action, const char* inputJson,
                        const claw_cap_call_context_t* context,
                        char* output, size_t outputSize) {
    if (!output || outputSize == 0) return ESP_ERR_INVALID_ARG;
    if (action != BrainBridge::Action::SAY &&
        !authorizedMutation(context, output, outputSize)) {
        return ESP_OK;
    }

    cJSON* input = cJSON_Parse(inputJson ? inputJson : "{}");
    if (!input || !cJSON_IsObject(input)) {
        cJSON_Delete(input);
        writeResult(output, outputSize, false, "Invalid JSON input");
        return ESP_OK;
    }

    BrainBridge::Request request{};
    request.action = action;
    request.requestId = context ? context->request_id : 0;
    request.autonomous = isAutonomyRequest(context);
    notePlayerRequest(context);
    if (action == BrainBridge::Action::SAY) {
        const cJSON* text = cJSON_GetObjectItemCaseSensitive(input, "text");
        if (!cJSON_IsString(text) || !text->valuestring || !text->valuestring[0]) {
            cJSON_Delete(input);
            writeResult(output, outputSize, false, "text is required");
            return ESP_OK;
        }
        std::snprintf(request.text, sizeof(request.text), "%s", text->valuestring);
    } else if (action == BrainBridge::Action::START_EXPEDITION) {
        const cJSON* area = cJSON_GetObjectItemCaseSensitive(input, "area");
        if (!cJSON_IsNumber(area) || area->valueint < 0 || area->valueint >= Game::EXPLORE_AREA_COUNT) {
            char message[64];
            std::snprintf(message, sizeof(message), "area must be between 0 and %u",
                          static_cast<unsigned>(Game::EXPLORE_AREA_COUNT) - 1);
            cJSON_Delete(input);
            writeResult(output, outputSize, false, message);
            return ESP_OK;
        }
        request.area = static_cast<uint8_t>(area->valueint);
    } else if (action == BrainBridge::Action::BUY_FOOD) {
        const cJSON* food = cJSON_GetObjectItemCaseSensitive(input, "food");
        if (!cJSON_IsNumber(food) || food->valueint < 0 || food->valueint >= 7) {
            cJSON_Delete(input);
            writeResult(output, outputSize, false, "food must be between 0 and 6");
            return ESP_OK;
        }
        request.foodIndex = static_cast<uint8_t>(food->valueint);
    }
    cJSON_Delete(input);

    BrainBridge::Result result{};
    BrainBridge::instance().enqueue(request, result);
    writeResult(output, outputSize, result.accepted, result.text);
    return ESP_OK;
}

esp_err_t executeGetContext(const char* input, const claw_cap_call_context_t* context,
                            char* output, size_t outputSize) {
    (void)input;
    notePlayerRequest(context);
    BrainBridge::Snapshot snapshot{};
    if (!BrainBridge::instance().snapshot(snapshot)) {
        writeResult(output, outputSize, false, "StickMon context is not ready");
        return ESP_OK;
    }
    char text[512];
    std::snprintf(text, sizeof(text),
                  "home=%s communication_slot=0 team=%u species=%u species_name=%s nature=%u nature_name=%s gender=%s level=%u hp=%u/%u satiety=%u mood=%u expedition_phase=%u area=%u unlocked_area=%u visit=%s battery=%d player_active=%s idle_seconds=%lu agent_allowed=%s action_locked=%s coins=%lu food=[%u,%u,%u,%u,%u,%u,%u] bowl_food=%u bowl_bites=%u",
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
                  static_cast<unsigned>(snapshot.explorePhase),
                  static_cast<unsigned>(snapshot.exploreArea),
                  static_cast<unsigned>(snapshot.unlockedArea),
                  snapshot.visitActive ? "true" : "false",
                  static_cast<int>(snapshot.battery),
                  snapshot.playerActive ? "true" : "false",
                  static_cast<unsigned long>(snapshot.idleSeconds),
                  snapshot.agentAllowed ? "true" : "false",
                  snapshot.actionLocked ? "true" : "false",
                  static_cast<unsigned long>(snapshot.coins),
                  static_cast<unsigned>(snapshot.foodCounts[0]),
                  static_cast<unsigned>(snapshot.foodCounts[1]),
                  static_cast<unsigned>(snapshot.foodCounts[2]),
                  static_cast<unsigned>(snapshot.foodCounts[3]),
                  static_cast<unsigned>(snapshot.foodCounts[4]),
                  static_cast<unsigned>(snapshot.foodCounts[5]),
                  static_cast<unsigned>(snapshot.foodCounts[6]),
                  static_cast<unsigned>(snapshot.bowlFood),
                  static_cast<unsigned>(snapshot.bowlBitesRemaining));
    writeResult(output, outputSize, true, text);
    return ESP_OK;
}

esp_err_t executeContext(const char* input, const claw_cap_call_context_t* context,
                         char* output, size_t outputSize) {
    return executeGetContext(input, context, output, outputSize);
}

esp_err_t executeInventory(const char* input, const claw_cap_call_context_t* context,
                           char* output, size_t outputSize) {
    (void)input;
    notePlayerRequest(context);
    BrainBridge::Snapshot snapshot{};
    if (!BrainBridge::instance().snapshot(snapshot)) {
        writeResult(output, outputSize, false, "StickMon inventory is not ready");
        return ESP_OK;
    }
    char text[384] = {};
    int used = std::snprintf(text, sizeof(text),
                             "coins=%lu food=[%u,%u,%u,%u,%u,%u,%u] items=",
                             static_cast<unsigned long>(snapshot.coins),
                             static_cast<unsigned>(snapshot.foodCounts[0]),
                             static_cast<unsigned>(snapshot.foodCounts[1]),
                             static_cast<unsigned>(snapshot.foodCounts[2]),
                             static_cast<unsigned>(snapshot.foodCounts[3]),
                             static_cast<unsigned>(snapshot.foodCounts[4]),
                             static_cast<unsigned>(snapshot.foodCounts[5]),
                             static_cast<unsigned>(snapshot.foodCounts[6]));
    for (uint8_t index = 0;
         used > 0 && index < static_cast<uint8_t>(Game::ItemId::COUNT) &&
         static_cast<size_t>(used) < sizeof(text) - 12;
         ++index) {
        if (snapshot.inventoryCounts[index] == 0) continue;
        used += std::snprintf(text + used, sizeof(text) - static_cast<size_t>(used),
                              "%u:%u,", static_cast<unsigned>(index),
                              static_cast<unsigned>(snapshot.inventoryCounts[index]));
    }
    writeResult(output, outputSize, true, text);
    return ESP_OK;
}

esp_err_t executeSay(const char* input, const claw_cap_call_context_t* context,
                     char* output, size_t outputSize) {
    return executeAction(BrainBridge::Action::SAY, input, context, output, outputSize);
}

esp_err_t executeExpedition(const char* input, const claw_cap_call_context_t* context,
                            char* output, size_t outputSize) {
    return executeAction(BrainBridge::Action::START_EXPEDITION, input, context, output, outputSize);
}

esp_err_t executeReturnHome(const char* input, const claw_cap_call_context_t* context,
                            char* output, size_t outputSize) {
    return executeAction(BrainBridge::Action::RETURN_HOME, input, context, output, outputSize);
}

esp_err_t executeInviteFriend(const char* input, const claw_cap_call_context_t* context,
                              char* output, size_t outputSize) {
    return executeAction(BrainBridge::Action::INVITE_FRIEND, input, context, output, outputSize);
}

esp_err_t executeEat(const char* input, const claw_cap_call_context_t* context,
                     char* output, size_t outputSize) {
    return executeAction(BrainBridge::Action::EAT, input, context, output, outputSize);
}

esp_err_t executeBuyFood(const char* input, const claw_cap_call_context_t* context,
                         char* output, size_t outputSize) {
    return executeAction(BrainBridge::Action::BUY_FOOD, input, context, output, outputSize);
}

const claw_cap_descriptor_t DESCRIPTORS[] = {
    {"stickmon_get_context", "Get StickMon context", "stickmon",
     "Read current pet status and activity state.", CLAW_CAP_KIND_CALLABLE,
     CLAW_CAP_FLAG_CALLABLE_BY_LLM, INPUT_NONE, nullptr, nullptr, nullptr,
     executeContext},
    {"stickmon_get_inventory", "Get inventory", "stickmon",
     "Read coins, food stock, and non-empty bag item counts.", CLAW_CAP_KIND_CALLABLE,
     CLAW_CAP_FLAG_CALLABLE_BY_LLM, INPUT_NONE, nullptr, nullptr, nullptr,
     executeInventory},
    {"stickmon_say", "Speak as the pet", "stickmon",
     "Deliver a short message to the pet presentation layer.", CLAW_CAP_KIND_CALLABLE,
     CLAW_CAP_FLAG_CALLABLE_BY_LLM, INPUT_SAY, nullptr, nullptr, nullptr,
     executeSay},
    {"stickmon_start_expedition", "Start expedition", "stickmon",
     "Start a deterministic StickMon expedition in an unlocked area.", CLAW_CAP_KIND_CALLABLE,
     CLAW_CAP_FLAG_CALLABLE_BY_LLM | CLAW_CAP_FLAG_RESTRICTED, INPUT_EXPEDITION,
     nullptr, nullptr, nullptr, executeExpedition},
    {"stickmon_return_home", "Return home", "stickmon",
     "Recall the pet from an active expedition.", CLAW_CAP_KIND_CALLABLE,
     CLAW_CAP_FLAG_CALLABLE_BY_LLM | CLAW_CAP_FLAG_RESTRICTED, INPUT_NONE,
     nullptr, nullptr, nullptr, executeReturnHome},
    {"stickmon_invite_friend", "Invite a friend", "stickmon",
     "Open an ESP-NOW room so a nearby StickMon can join for a visit.", CLAW_CAP_KIND_CALLABLE,
     CLAW_CAP_FLAG_CALLABLE_BY_LLM | CLAW_CAP_FLAG_RESTRICTED, INPUT_NONE,
     nullptr, nullptr, nullptr, executeInviteFriend},
    {"stickmon_eat", "Eat food", "stickmon",
     "Place available food in the bowl and let the pet eat one serving.", CLAW_CAP_KIND_CALLABLE,
     CLAW_CAP_FLAG_CALLABLE_BY_LLM | CLAW_CAP_FLAG_RESTRICTED, INPUT_NONE,
     nullptr, nullptr, nullptr, executeEat},
    {"stickmon_buy_food", "Buy ordinary food", "stickmon",
     "Buy one unlocked food item using game coins. Food index 0-6 follows the live context list.", CLAW_CAP_KIND_CALLABLE,
     CLAW_CAP_FLAG_CALLABLE_BY_LLM | CLAW_CAP_FLAG_RESTRICTED, INPUT_FOOD,
     nullptr, nullptr, nullptr, executeBuyFood},
};

const claw_cap_group_t GROUP = {
    GROUP_ID,
    "StickMon",
    "1.0.0",
    DESCRIPTORS,
    sizeof(DESCRIPTORS) / sizeof(DESCRIPTORS[0]),
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

esp_err_t registerGroup(const app_claw_config_t* config,
                        const app_claw_storage_paths_t* paths) {
    (void)config;
    (void)paths;
    return claw_cap_register_group(&GROUP);
}

}  // namespace

esp_err_t registerStickmonCapability() {
    buildSchemas();
    static const app_capability_external_group_t external = {
        GROUP_ID, "StickMon", true, nullptr, registerGroup};
    return app_capabilities_register_external_group(&external);
}

}  // namespace Stickmon
